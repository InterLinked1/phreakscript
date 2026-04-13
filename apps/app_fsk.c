/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2013-2021 Alessandro Carminati <alessandro.carminati@gmail.com>
 * Copyright (C) 2026 Naveen Albert <asterisk@phreaknet.org>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

/*! \file
 *
 * \brief FSK applications
 *
 * \author Alessandro Carminati <alessandro.carminati@gmail.com>
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<depend>spandsp</depend>
	<support_level>extended</support_level>
 ***/

/* Needed for spandsp headers */
#define ASTMM_LIBC ASTMM_IGNORE
#include "asterisk.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <pthread.h>
#include <errno.h>

#define SPANDSP_EXPOSE_INTERNAL_STRUCTURES
#include <spandsp.h>
#include <spandsp/version.h>

#ifndef FSK_FRAME_MODE_8N1_FRAMES /* was removed from spandsp */
#define FSK_FRAME_MODE_8N1_FRAMES 10
#endif

#define BLOCK_LEN           160

#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/logger.h"
#include "asterisk/app.h"
#include "asterisk/dsp.h"
#include "asterisk/manager.h"
#include "asterisk/format_cache.h"

/*** DOCUMENTATION
	<application name="SendFSK" language="en_US">
		<synopsis>
			Send FSK message over audio channel.
		</synopsis>
		<syntax>
			<parameter name="modem" required="no">
				<para>Name of modem protocol to use. Default is Bell 103.</para>
				<enumlist>
					<enum name="103">
						<para>Bell 103 modem (300 baud), originating</para>
					</enum>
					<enum name="202">
						<para>Bell 202 modem (1200 baud)</para>
					</enum>
				</enumlist>
			</parameter>
			<parameter name="data" required="yes">
				<para>The data to send</para>
			</parameter>
		</syntax>
		<description>
			<para>Sends digital messages over an audio channel</para>
		</description>
		<see-also>
			<ref type="application">ReceiveFSK</ref>
		</see-also>
	</application>
	<application name="ReceiveFSK" language="en_US">
		<synopsis>
			Receive FSK message from audio channel.
		</synopsis>
		<syntax>
			<parameter name="variable" required="yes">
				<para>Name of variable in which to save the received data.</para>
			</parameter>
			<parameter name="modem" required="no">
				<para>Name of modem protocol to use. Default is Bell 103.</para>
				<enumlist>
					<enum name="103">
						<para>Bell 103 modem (300 baud), originating</para>
					</enum>
					<enum name="202">
						<para>Bell 202 modem (1200 baud)</para>
					</enum>
				</enumlist>
			</parameter>
			<parameter name="options" required="no">
				<optionlist>
					<option name="h">
						<para>Receive frames until it gets a hangup. Default behaviour is to stop receiving on carrier loss.</para>
					</option>
					<option name="s">
						<para>Generate silence back to caller. Default behaviour is generate no stream. This can cause some applications to misbehave.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Receives digital messages from an audio channel</para>
			<para>This application will answer the channel if it has not yet been answered.</para>
		</description>
		<see-also>
			<ref type="application">SendFSK</ref>
		</see-also>
	</application>
***/

enum read_option_flags {
	OPT_HANGOUT = (1 << 0),
	OPT_SILENCE = (1 << 1),
};

AST_APP_OPTIONS(read_app_options, {
	AST_APP_OPTION('h', OPT_HANGOUT),
	AST_APP_OPTION('s', OPT_SILENCE),
});

struct receive_buffer {
	int ptr;
	unsigned int quitoncarrierlost:1;
	unsigned int fsk_eof:1;
	unsigned int got_data:1;
	char *buffer;
};

struct transmit_buffer {
	int ptr;
	int bytes2send;
	int current_bit_no;
	char *buffer;
};

static const char app_fsk_tx[] = "SendFSK";
static const char app_fsk_rx[] = "ReceiveFSK";

static void rx_status(void *user_data, int status)
{
	struct receive_buffer *data = user_data;
	ast_debug(1, "FSK rx status is %s (%d)\n", signal_status_to_str(status), status);
	if (status == -1 && data->quitoncarrierlost) {
		data->fsk_eof = 1;
	}
}

static void get_bit(void *user_data, int bit)
{
	char c;
	struct receive_buffer *data = user_data;
	if (bit < 0) {
		rx_status(user_data, bit);
		return;
	}
	c = (char) bit & 0xff;
	data->got_data = 1;
	ast_debug(2, "Got %d '%c' on the stream\n", c, isprint(c) ? c : ' ');
	*(data->buffer + data->ptr++) = c;
}

static int put_bit(struct transmit_buffer *user_data)
{
	int8_t data;

	if (user_data->ptr <= user_data->bytes2send) {
		if (user_data->current_bit_no == 0) {
			data = 0; /* start bit */
		} else if (user_data->current_bit_no <= 8) {
			data = *((int8_t *) user_data->buffer + user_data->ptr) & (1 << (user_data->current_bit_no - 1));
		} else {
			data = 1; /* stop bit */
		}
		++user_data->current_bit_no;
		if (user_data->current_bit_no == 10) {
			user_data->ptr++;
			user_data->current_bit_no = 0;
		}
		return data != 0 ? 1 : 0;
	} else {
		return 1;
	}
}

static int fsk_tx_exec(struct ast_channel *chan, const char *data)
{
	char *argcopy = NULL;
	fsk_tx_state_t *caller_tx;
	struct transmit_buffer *out;
	int16_t caller_amp[BLOCK_LEN];
	struct ast_frame *fr;
	struct ast_frame f = {
		.frametype = AST_FRAME_VOICE,
		.src = "SendFSK",
		.datalen = BLOCK_LEN * 2,
		.samples = BLOCK_LEN,
		.data.ptr = &caller_amp,
	};
	int samples;
	int modem;
	int res = 0;

	AST_DECLARE_APP_ARGS(arglist,
		AST_APP_ARG(modem);
		AST_APP_ARG(data);
	);

	f.subclass.format = ast_format_slin;

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "SendFSK requires an argument\n");
		return -1;
	}
	if (!chan) {
		ast_log(LOG_ERROR, "SendFSK channel is NULL. Giving up.\n");
		return -1;
	}

	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(arglist, argcopy);

	modem = FSK_BELL103CH1;
	if (!ast_strlen_zero(arglist.modem)) {
		if (!strcmp(arglist.modem, "103")) {
			modem = FSK_BELL103CH1;
		} else if (!strcmp(arglist.modem, "202")) {
			modem = FSK_BELL202;
		} else {
			ast_log(LOG_WARNING, "Unknown modem protocol: %s\n", arglist.modem);
			return -1;
		}
	}

	ast_debug(1, "Modem channel is '%s'\n", preset_fsk_specs[modem].name);

	if ((res = ast_set_write_format(chan, ast_format_slin)) < 0) {
		ast_log(LOG_ERROR, "Unable to set channel to linear mode\n");
		return -1;
	}

	out = ast_calloc(1, sizeof(*out));
	if (!out) {
		return -1;
	}
	out->buffer = arglist.data;
	out->bytes2send = strlen(arglist.data);

	memset(caller_amp, 0, sizeof(*caller_amp));

	caller_tx = fsk_tx_init(NULL, &preset_fsk_specs[modem], (get_bit_func_t) &put_bit, out);
	if (!caller_tx) {
		ast_free(out);
		return -1;
	}

	/* Send a few frames of mark tone first, to allow receiver to synchronize */
	for (samples = 0; samples < 7; samples++) {
		res = ast_waitfor(chan, -1);
		fr = ast_read(chan);
		if (!fr) {
			ast_debug(2, "User hung up\n");
			goto abort;
		}
		ast_frfree(fr);
		fsk_tx(caller_tx, caller_amp, BLOCK_LEN);
		if (ast_write(chan, &f) < 0) {
			ast_debug(1, "Failed to write mark tone\n");
			goto abort;
		}
	}

	/* Send the payload */
	while (out->ptr < out->bytes2send) {
		res = ast_waitfor(chan, 1000);
		fr = ast_read(chan);
		if (!fr) {
			ast_debug(2, "User hung up\n");
			goto abort;
		}
		if (fr->frametype == AST_FRAME_DTMF) {
			ast_debug(1, "User pressed a key\n");
		}
		ast_frfree(fr);
		samples = fsk_tx(caller_tx, caller_amp, BLOCK_LEN);
		if ((res = ast_write(chan, &f)) < 0) {
			ast_debug(1, "Failed to write %d samples\n", samples);
			goto abort;
		}
	}

	/* Send a few extra frames of mark tone to allow receiver to reliably receive last bit of data */
	for (samples = 0; samples < 4; samples++) {
		res = ast_waitfor(chan, -1);
		fr = ast_read(chan);
		if (!fr) {
			ast_debug(2, "User hung up\n");
			goto abort;
		}
		ast_frfree(fr);
		fsk_tx(caller_tx, caller_amp, BLOCK_LEN);
		if (ast_write(chan, &f) < 0) {
			ast_debug(1, "Failed to write mark tone\n");
			goto abort;
		}
	}

	ast_debug(1, "SendFSK Completed.\n");
	ast_free(out);
	fsk_tx_free(caller_tx);
	return 0;

abort:
	ast_free(out);
	fsk_tx_free(caller_tx);
	return -1;
}

static int fsk_rx_exec(struct ast_channel *chan, const char *data)
{
	fsk_rx_state_t *caller_rx;
	struct receive_buffer *in;
	char *argcopy = NULL;
	struct ast_frame *f;
	struct ast_flags flags = {0};
	struct ast_silence_generator *silgen = NULL;
	int16_t output_frame[BLOCK_LEN];
	int modem;
	int silence_flag = 0;
	int res = 0;
	int retries = 0;
	int eof_count = 0;

	AST_DECLARE_APP_ARGS(arglist,
		AST_APP_ARG(variable);
		AST_APP_ARG(modem);
		AST_APP_ARG(options);
	);

	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(arglist, argcopy);
	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "ReceiveFSK requires at least a variable as argument\n");
		return -1;
	}
	if (!chan) {
		ast_log(LOG_ERROR, "ReceiveFSK channel is NULL. Giving up.\n");
		return -1;
	}
	if (ast_channel_state(chan) != AST_STATE_UP) { /* answer channel if unanswered */
		res = ast_answer(chan);
		if (res) {
			ast_log(LOG_WARNING, "Failed to answer channel\n");
			return -1;
		}
	}

	modem = FSK_BELL103CH1;
	if (!ast_strlen_zero(arglist.modem)) {
		if (!strcmp(arglist.modem, "103")) {
			modem = FSK_BELL103CH1;
		} else if (!strcmp(arglist.modem, "202")) {
			modem = FSK_BELL202;
		} else {
			ast_log(LOG_WARNING, "Unknown modem protocol: %s\n", arglist.modem);
			return -1;
		}
	}

	pbx_builtin_setvar_helper(chan, arglist.variable, ""); /* initialize variable */
	ast_debug(1, "Modem channel is '%s'\n", preset_fsk_specs[modem].name);
	if ((res = ast_set_read_format(chan, ast_format_slin)) < 0) {
		ast_log(LOG_WARNING, "Unable to set channel to linear mode, giving up\n");
		return -1;
	}

	memset(output_frame, 0, sizeof(output_frame));
	in = ast_calloc(1, sizeof(*in));
	if (!in) {
		return -1;
	}

	in->quitoncarrierlost = 1;

	if (!ast_strlen_zero(arglist.options)) {
		ast_app_parse_options(read_app_options, &flags, NULL, arglist.options);
		if (ast_test_flag(&flags, OPT_HANGOUT )) {
			in->quitoncarrierlost = 0;
		}
		if (ast_test_flag(&flags, OPT_SILENCE )) {
			silence_flag = 1;
		}
	}

	in->buffer = ast_calloc(1, 65536); /* Reserve 64KB space for receive buffer. */
	if (!in->buffer) {
		ast_free(in);
		return -1;
	}
	if (silence_flag) {
		silgen = ast_channel_start_silence_generator(chan);
	}
	caller_rx = fsk_rx_init(NULL, &preset_fsk_specs[modem], FSK_FRAME_MODE_8N1_FRAMES, get_bit, in);
	if (!caller_rx) {
		ast_free(in->buffer);
		ast_free(in);
		return -1;
	}
	fsk_rx_set_modem_status_handler(caller_rx, rx_status, (void *) in);
	while (ast_waitfor(chan, -1) > -1) {
		f = ast_read(chan);
		if (!f) {
			ast_debug(1, "No more frames to read\n");
			goto done; /* Caller hung up immediately, but there might be data to read */
		}
		if (f->frametype == AST_FRAME_VOICE) {
			fsk_rx(caller_rx, f->data.ptr, f->samples);
		}
		if (in->fsk_eof) {
			if (!in->got_data) {
				/* Don't abort immediately if there's no carrier present yet,
				 * the sender may not have started sending yet...
				 * to a point, if we never get anything after a while (5 seconds), abort anyways. */
				if (++retries > 250) {
					ast_log(LOG_WARNING, "No FSK data received\n");
					ast_frfree(f);
					goto done; /* This is normal success */
				}
				ast_debug(4, "fsk_eof, but no data received yet, waiting...\n");
				eof_count = in->fsk_eof = 0; /* Reset */
			} else {
				eof_count++;
				ast_debug(3, "FSK eof count %d\n", eof_count);
				/* Require 2 carrier downs in a row to signify EOF
				 * Note that rx_status only fires when receiving FSK audio,
				 * but even if we really only get one carrier down,
				 * this still works because the next loop iteration here,
				 * fsk_eof will still be set true and we'll break,
				 * i.e. SpanDSP telling us carrier down + next frame is not FSK. */
				if (eof_count > 1) {
					ast_debug(1, "FSK carrier seems to have dropped\n");
					ast_frfree(f);
					goto done; /* This is typically normal success */
				}
			}
		} else {
			eof_count = 0;
		}
		f->subclass.format = ast_format_slin;
		f->datalen = BLOCK_LEN;
		f->samples = BLOCK_LEN / 2;
		f->offset = AST_FRIENDLY_OFFSET;
		f->src = __PRETTY_FUNCTION__;
		f->data.ptr = &output_frame;
		if (ast_write(chan, f) < 0) {
			ast_debug(1, "No more frames to read\n");
			ast_frfree(f);
			goto abort;
		}
	}
	if (!f) {
		ast_debug(1, "Got hangup\n");
		goto abort;
	}

done:
	ast_debug(1, "received buffer is: %s\n", in->buffer);
	pbx_builtin_setvar_helper(chan, arglist.variable, in->buffer);
	if (silgen) {
		ast_channel_stop_silence_generator(chan, silgen);
	}
	ast_free(in->buffer);
	ast_free(in);
	fsk_rx_free(caller_rx);
	return 0;

abort:
	ast_free(in->buffer);
	ast_free(in);
	fsk_rx_free(caller_rx);
	return -1;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(app_fsk_tx);
	res |= ast_unregister_application(app_fsk_rx);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_register_application_xml(app_fsk_tx, fsk_tx_exec);
	res |= ast_register_application_xml(app_fsk_rx, fsk_rx_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "FSK send/receive applications");
