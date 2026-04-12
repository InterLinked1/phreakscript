/*
*  FSK util for Asterisk
*
*  Copyright (C) 2013-2021 Alessandro Carminati <alessandro.carminati@gmail.com>
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*! \file
*
* \brief App to Send/Receive fsk audio stream (Bell103)
*
* \author Alessandro Carminati <alessandro.carminati@gmail.com>
*
* \ingroup applications
*/

/*** MODULEINFO
	<defaultenabled>no</defaultenabled>
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
						<para>Bell 103 modem (300 baud), originating side</para>
					</enum>
					<enum name="202">
						<para>Bell 202 modem (1200 baud)</para>
					</enum>
				</enumlist>
			</parameter>
		</syntax>
		<description>
			<para>SendFSK() is an utility to send digital messages over an audio channel</para>
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
						<para>Bell 103 modem (300 baud), terminating side</para>
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
			<para>ReceiveFSK() is an utility to receive digital messages from an audio channel</para>
			<para>This application will answer the channel if it has not yet been answered.</para>
		</description>
		<see-also>
			<ref type="application">SendFSK</ref>
		</see-also>
	</application>
***/

enum read_option_flags {
	OPT_HANGOUT    = (1 << 0),
	OPT_SILENCE    = (1 << 1),
};

AST_APP_OPTIONS(read_app_options, {
	AST_APP_OPTION('h', OPT_HANGOUT),
	AST_APP_OPTION('s', OPT_SILENCE),
});

struct receive_buffer_s {
	int ptr;
	int quitoncarrierlost;
	int FSK_eof;
	char *buffer;
};

struct transmit_buffer_s {
	int ptr;
	int bytes2send;
	int current_bit_no;
	char *buffer;
};

typedef struct transmit_buffer_s transmit_buffer_t;
typedef struct receive_buffer_s  receive_buffer_t;

static const char app_fskTX[] = "SendFSK";
static const char app_fskRX[] = "ReceiveFSK";

static void rx_status(void *user_data, int status){
	receive_buffer_t *data;

	data = (receive_buffer_t *) user_data;
	ast_debug(1, "FSK rx status is %s (%d)\n", signal_status_to_str(status), status);
	if ((status == -1) && (data->quitoncarrierlost)) {
		data->FSK_eof = 1;
	}
}

static void get_bit(void *user_data, int bit)
{
	receive_buffer_t *data;

	data=(receive_buffer_t *) user_data;
	if (bit < 0) {
		rx_status(user_data, bit);
		return;
	}

	ast_debug(1, "Got '%c' on the stream\n", (char) bit & 0xff);
	*(data->buffer + data->ptr++)=(char) bit & 0xff;
}

static int put_bit(transmit_buffer_t *user_data)
{
	int8_t data;

	if (user_data->ptr <= user_data->bytes2send) {
		if ( (user_data->current_bit_no != 0) && (user_data->current_bit_no != 9) ) {
			data = *((int8_t *)user_data->buffer + user_data->ptr) & (1 << (user_data->current_bit_no - 1));
		} else if (user_data->current_bit_no != 9) {
			data = 0;
		} else {
			data = 1;
		}
		++user_data->current_bit_no;
		if (user_data->current_bit_no == 10) {
			user_data->ptr++;
		}
		if (user_data->current_bit_no == 10) {
			user_data->current_bit_no = 0;
		}
		return data != 0 ? 1 : 0;
	} else {
		return 1;
	}
}

static int fskTX_exec(struct ast_channel *chan, const char *data) { /* SendFSK */
	typedef struct ast_frame ast_frame_t;
	char *argcopy = NULL;
	fsk_tx_state_t *caller_tx;
	transmit_buffer_t *out;
	int16_t caller_amp[BLOCK_LEN];
	ast_frame_t *fr;
	struct ast_frame f = {
		.frametype = AST_FRAME_VOICE,
		.src = "SendFSK",
		.datalen = BLOCK_LEN * 2,
		.samples = BLOCK_LEN,
		.data.ptr = &caller_amp,
	};
	struct ast_format * native_format;
	unsigned int sampling_rate;
	struct ast_format * write_format;
	int samples;
	int modem;
	int res = 0;

	AST_DECLARE_APP_ARGS(arglist,
		AST_APP_ARG(modem);
		AST_APP_ARG(data);
	);

	native_format = ast_format_cap_get_format(ast_channel_nativeformats(chan), 0);
	sampling_rate = ast_format_get_sample_rate(native_format);
	write_format = ast_format_cache_get_slin_by_rate(sampling_rate);
	f.subclass.format = write_format;

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "SendFSK requires an argument\n");
		return -1;
	}
	if (chan == NULL) {
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

	out = (transmit_buffer_t *) ast_malloc(sizeof(*out));
	out->buffer = (char *) arglist.data;
	out->bytes2send = strlen(arglist.data);
	out->current_bit_no = 0;
	out->ptr = 0;
	memset(caller_amp, 0, sizeof(*caller_amp));
	caller_tx = fsk_tx_init(NULL, &preset_fsk_specs[modem], (get_bit_func_t) &put_bit, out);
	while (out->ptr < out->bytes2send) {
		res = ast_waitfor(chan, 1000);
		fr = ast_read(chan);
		if (!fr) {
			ast_debug(1, "Null == hangup() detected\n");
			res = -1;
			break;
		}
		if (fr->frametype == AST_FRAME_DTMF) {
			ast_debug(1, "User pressed a key\n");
		}
		samples = fsk_tx(caller_tx, caller_amp, BLOCK_LEN);
		if ((res = ast_write(chan, &f)) < 0) {
			ast_debug(1, "Failed to write %d samples\n", samples);
			res = -1;
			ast_frfree(fr);
			break;
		}
	}
	memset(caller_amp, 0, sizeof(caller_amp));
	res = ast_waitfor(chan, -1);
	fr = ast_read(chan);
	if (fr != NULL) {
		if (ast_write(chan, &f) < 0) {
			res = -1;
			ast_frfree(fr);
		}
	} else {
		ast_log(LOG_WARNING, "ast_read returned NULL value.\n");
	}
	ast_debug(1, "SendFSK Completed.\n");
	return 0;
}

static int fskRX_exec(struct ast_channel *chan, const char *data) { /* ReceiveFSK */
	fsk_rx_state_t *caller_rx;
	receive_buffer_t *in;
	char *argcopy = NULL;
	struct ast_frame *f;
	struct ast_flags flags = {0};
	struct ast_silence_generator *silgen = NULL;
	int16_t output_frame[BLOCK_LEN];
	int modem;
	int silence_flag = 0;
	int res = 0;

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
	if (chan == NULL)          {
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

	modem = FSK_BELL103CH2;
	if (!ast_strlen_zero(arglist.modem)) {
		if (!strcmp(arglist.modem, "103")) {
			modem = FSK_BELL103CH2;
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
	in = (receive_buffer_t *) ast_malloc(sizeof(*in));

	if (!ast_strlen_zero(arglist.options)) {
		ast_debug(1, "This instance has flags\n");
		ast_app_parse_options(read_app_options, &flags, NULL, arglist.options);
		if (ast_test_flag(&flags, OPT_HANGOUT )) {
			in->quitoncarrierlost = 0;
		}
		if (ast_test_flag(&flags, OPT_SILENCE )) {
			silence_flag = 1;
		}
	}

	in->FSK_eof = 0;
	in->quitoncarrierlost = 1;

	in->buffer = (char *) ast_malloc(65536);
	memset(in->buffer, 0, 65536); /* Reserve 64KB space for receive buffer and set to 0 its pointer. */
	in->ptr = 0;
	ast_debug(1, "output buffer allocated\n");

	if (silence_flag) {
		silgen = ast_channel_start_silence_generator(chan);
	}
	caller_rx = fsk_rx_init(NULL, &preset_fsk_specs[modem], FSK_FRAME_MODE_8N1_FRAMES, get_bit, in);
	fsk_rx_set_modem_status_handler(caller_rx, rx_status, (void *) in);
	while (ast_waitfor(chan, -1) > -1) {
		f = ast_read(chan);
		if (!f) {
			res = -1;
			break;
		}
		if (f->frametype == AST_FRAME_VOICE){
			fsk_rx(caller_rx, f->data.ptr, f->samples);
		}
		if (in->FSK_eof != 0) {
			ast_log(LOG_NOTICE, "FSK_eof\n");
			break;
		}
		f->subclass.format = ast_format_slin;
		f->datalen = BLOCK_LEN;
		f->samples = BLOCK_LEN / 2;
		f->offset = AST_FRIENDLY_OFFSET;
		f->src = __PRETTY_FUNCTION__;
		f->data.ptr = &output_frame;
		if (ast_write(chan, f) < 0) {
			res = -1;
			ast_frfree(f);
			break;
		}
		ast_frfree(f);
	}
	if (!f) {
		ast_debug(1, "Got hangup\n");
		res = -1;
	}
	ast_debug(1, "received buffer is: %s\n", in->buffer);
	pbx_builtin_setvar_helper(chan, arglist.variable, in->buffer);
	ast_free(in->buffer);
	if (silgen) {
		ast_channel_stop_silence_generator(chan, silgen);
	}
	ast_free(in);
	return 0;
}

static int unload_module(void) {
	int res;

	res = ast_unregister_application(app_fskTX);
	res |= ast_unregister_application(app_fskRX);

	return res;
}

static int load_module(void) {
	int res;

	res = ast_register_application_xml(app_fskTX, fskTX_exec);
	res |= ast_register_application_xml(app_fskRX, fskRX_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "FSK utility application");
