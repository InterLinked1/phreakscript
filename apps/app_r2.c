/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2026, Naveen Albert
 *
 * Naveen Albert <asterisk@phreaknet.org>
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
 * \brief R2 (Regional System No. 2) incoming and outgoing register applications
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/channel.h"
#include "asterisk/dsp.h"
#include "asterisk/app.h"
#include "asterisk/module.h"
#include "asterisk/indications.h"
#include "asterisk/conversions.h"

/*** DOCUMENTATION
	<application name="R2Incoming" language="en_US">
		<since>
			<version>24.0.0</version>
		</since>
		<synopsis>
			Act as an incoming R2 register to receive signaling information
		</synopsis>
		<syntax>
			<parameter name="variable" required="true">
				<para>The input digits will be stored in the given
				<replaceable>variable</replaceable> name.</para>
			</parameter>
			<parameter name="maxdigits">
				<para>An optional maximum number of digits to receive.</para>
			</parameter>
			<parameter name="timeout">
				<para>The number of seconds to wait for all digits, if greater than <literal>0</literal>.
				Can be floating point. Default is no timeout.</para>
			</parameter>
		</syntax>
		<description>
			<para>Acts as an incoming R2 register to receive signaling information conveyed via R2 signaling digits.</para>
			<para>This application expects to use compelled signaling to talk to an outgoing R2 register.
			For a low-level receiver, use <literal>ReceiveR2</literal> instead.</para>
			<para>Digits are read into the given <replaceable>variable</replaceable>.</para>
			<para>This application does not automatically answer the channel and should be preceded with
			<literal>Answer</literal> or <literal>Progress</literal> as needed.</para>
			<variablelist>
				<variable name="RECEIVER2STATUS">
					<para>This is the status of the receive operation.</para>
					<value name="START" />
					<value name="ERROR" />
					<value name="HANGUP" />
					<value name="MAXDIGITS" />
					<value name="TIMEOUT" />
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">R2Outgoing</ref>
			<ref type="application">ReceiveR2</ref>
			<ref type="application">SendR2</ref>
		</see-also>
	</application>
	<application name="R2Outgoing" language="en_US">
		<since>
			<version>24.0.0</version>
		</since>
		<synopsis>
			Act as an outgoing R2 register to transmit signaling information
		</synopsis>
		<syntax>
			<parameter name="digits" required="true">
				<para>List of digits 0-9,BCDEF to send.</para>
			</parameter>
		</syntax>
		<description>
			<para>Acts as an outgoing R2 register to transmit signaling information conveyed via R2 signaling digits.</para>
			<para>This application expects to use compelled signaling to talk to an incoming R2 register.
			For a low-level sender, use <literal>SendR2</literal> instead.</para>
		</description>
		<see-also>
			<ref type="application">R2Incoming</ref>
			<ref type="application">ReceiveR2</ref>
			<ref type="application">SendR2</ref>
		</see-also>
	</application>
	<application name="ReceiveR2" language="en_US">
		<since>
			<version>24.0.0</version>
		</since>
		<synopsis>
			Detects R2 digits on a channel and saves them to a variable.
		</synopsis>
		<syntax>
			<parameter name="variable" required="true">
				<para>The input digits will be stored in the given
				<replaceable>variable</replaceable> name.</para>
			</parameter>
			<parameter name="startdigit">
				<para>Consider the transmission finished after receiving this digit.</para>
			</parameter>
			<parameter name="maxdigits">
				<para>An optional maximum number of digits to receive.</para>
			</parameter>
			<parameter name="timeout">
				<para>The number of seconds to wait for all digits, if greater than <literal>0</literal>.
				Can be floating point. Default is no timeout.</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="b">
						<para>Listen for backward register signals. Default is listen for forward register signals.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Detects R2 forward or backward register signals and saves them to a channel variable.</para>
			<para>This is a low-level signaling application which can be used to receive raw R2
			forward or backward register signals. It does not use compelled signaling.</para>
			<para><literal>Answer</literal> or <literal>Progress</literal> as needed.</para>
			<variablelist>
				<variable name="RECEIVER2STATUS">
					<para>This is the status of the receive operation.</para>
					<value name="START" />
					<value name="ERROR" />
					<value name="HANGUP" />
					<value name="MAXDIGITS" />
					<value name="TIMEOUT" />
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">SendR2</ref>
			<ref type="application">R2Incoming</ref>
			<ref type="application">R2Outoing</ref>
		</see-also>
	</application>
	<application name="SendR2" language="en_US">
		<since>
			<version>24.0.0</version>
		</since>
		<synopsis>
			Sends R2 digits on the current or specified channel.
		</synopsis>
		<syntax>
			<parameter name="digits" required="true">
				<para>List of digits 0-9,BCDEF to send.</para>
			</parameter>
			<parameter name="duration_ms" required="false">
				<para>Duration of each numeric digit (defaults to 65ms).</para>
			</parameter>
			<parameter name="timeout_ms" required="false">
				<para>Amount of time to wait in ms between tones. (defaults to 60ms).</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="b">
						<para>Send backward register signals. Default is send forward register signals.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Send specified R2 register signals.</para>
			<para>This is a low-level signaling application which can be used to
			send raw R2 forward or backward register signals. It does not use compelled signaling.</para>
		</description>
		<see-also>
			<ref type="application">ReceiveR2</ref>
			<ref type="application">R2Incoming</ref>
			<ref type="application">R2Outoing</ref>
		</see-also>
	</application>
 ***/

enum read_option_flags {
	OPT_BACKWARD = (1 << 0),
};

AST_APP_OPTIONS(r2_app_options, {
	AST_APP_OPTION('b', OPT_BACKWARD),
});

static const char *incoming_name = "R2Incoming";
static const char *outgoing_name = "R2Outgoing";
static const char *receiver_name = "ReceiveR2";
static const char *sender_name = "SendR2";

/* Tone defaults for non-compelled signaling */
#define R2_TONE_LENGTH 65
#define R2_BETWEEN_MS 55

#define VALID_R2_CHAR "1234567890BCDEF"

#define R2_A1 '1' /* 1, send next digit (n + 1) */

/*! \note FIXME At the moment, there is a strange issue where when using compelled signaling,
 * once the incoming register sends the backward tone, things just fall apart.
 * Instead of DSP processing every 20 ms, we start processing about every 1 ms,
 * and we lose the ability to detect while also sending a tone.
 * Weird thing is it works the first time.
 *
 * For that reason, there are artificial sleeps currently baked in.
 * This mostly works, although it makes compelled signaling somewhat slow and awkward,
 * and it's actually faster to use ReceiveR2 and SendR2 (which use fixed tone lengths).
 * Once this is fixed, this hack can be removed. */
#define DOUBLE_DSP_ISSUE

enum r2_direction {
	R2_FORWARD,
	R2_BACKWARD,
};

static int r2_start_signal(struct ast_channel *chan, enum r2_direction dir, char digit)
{
	int number;
	static const char * const r2_forward_tones[] = {
		"1380+1500", /* 1 */
		"1380+1620", /* 2 */
		"1500+1620", /* 3 */
		"1380+1740", /* 4 */
		"1500+1740", /* 5 */
		"1620+1740", /* 6 */
		"1380+1860", /* 7 */
		"1500+1860", /* 8 */
		"1620+1860", /* 9 */
		"1740+1860", /* 10 */
		"1380+1980", /* 11 */
		"1500+1980", /* 12 */
		"1620+1980", /* 13 */
		"1740+1980", /* 14 */
		"1860+1980", /* 15 */
	};
	static const char * const r2_backward_tones[] = {
		"1140+1020", /* 1 */
		"1140+900", /* 2 */
		"1020+900", /* 3 */
		"1140+780", /* 4 */
		"1020+780", /* 5 */
		"900+780", /* 6 */
		"1140+660", /* 7 */
		"1020+660", /* 8 */
		"900+660", /* 9 */
		"780+660", /* 10 */
		"1140+540", /* 11 */
		"1020+540", /* 12 */
		"900+540", /* 13 */
		"780+540", /* 14 */
		"660+540", /* 15 */
	};

	/* Translate digit to R2 tone number */
	switch (digit) {
	case '0':
		number = 10;
		break;
	case 'B':
		number = 11;
		break;
	case 'C':
		number = 12;
		break;
	case 'D':
		number = 13;
		break;
	case 'E':
		number = 14;
		break;
	case 'F':
		number = 15;
		break;
	default:
		if (digit < '0' || digit > '9') {
			ast_log(LOG_WARNING, "Unable to generate R2 forward tone '%c' for '%s'\n", digit, ast_channel_name(chan));
			return 0;
		}
		number = digit - '0';
	}

	/* The array starts at 1, so 1-index: */
	ast_playtones_start(chan, 0, dir == R2_FORWARD ? r2_forward_tones[number - 1] : r2_backward_tones[number - 1], 0);
	return 0;
}

static int r2_stop_signal(struct ast_channel *chan)
{
	if (ast_channel_generator(chan)) {
		ast_playtones_stop(chan);
		return 0;
	}
	return -1;
}

static int r2_start_silence(struct ast_channel *chan)
{
	if (!strncasecmp(ast_channel_name(chan), "DAHDI", 5)) {
		/* DAHDI channels have constant timing, so no silence generation is necessary for correctness. */
		return 0;
	}
	/* For non-hardware channels, generate silence to ensure channels don't stall */
	return ast_playtones_start(chan, 0, "0", 0);
}

/*! \brief Receive a single R2 digit */
static int r2_receive_digit(struct ast_channel *chan, struct ast_dsp *dsp, int timeout)
{
	struct ast_frame *frame = NULL;
	struct timeval start;
	int remaining_time = timeout;

	start = ast_tvnow();

	while (remaining_time > 0) {
		int res;
		remaining_time = ast_remaining_ms(start, timeout);
		if (remaining_time <= 0) {
			ast_debug(5, "R2 digit receive timer expired (time remaining: %d/%d)\n", remaining_time, timeout);
			return -1;
		}
		res = ast_waitfor(chan, 1000);
		if (res > 0) {
			frame = ast_read(chan);
			if (!frame) {
				ast_debug(1, "Channel '%s' did not return a frame; probably hung up.\n", ast_channel_name(chan));
				return -1;
			} else if (frame->frametype == AST_FRAME_VOICE) {
				frame = ast_dsp_process(chan, dsp, frame);
				if (frame->frametype == AST_FRAME_DTMF) {
					char result = frame->subclass.integer;
					ast_frfree(frame);
					return result; /* Stop once we receive a digit */
				}
			}
		} else {
			ast_debug(3, "No activity (%d), channel hung up?\n", res);
			return -1; /* Hangup */
		}
	}
	ast_debug(3, "No time remaining\n");
	return -1;
}

/*! \retval 0 once tone stops, -1 on failure */
static int r2_receive_cessation(struct ast_channel *chan, struct ast_dsp *dsp, int digit, int timeout)
{
	struct ast_frame *frame = NULL;
	struct timeval start;
	int remaining_time = timeout;
	int last_silent = 0;

	start = ast_tvnow();

	while (remaining_time > 0) {
		int res;
		remaining_time = ast_remaining_ms(start, timeout);
		if (remaining_time <= 0) {
			ast_debug(5, "R2 digit receive timer expired (time remaining: %d/%d)\n", remaining_time, timeout);
			return -1;
		}
		res = ast_waitfor(chan, 1000);
		if (res > 0) {
			frame = ast_read(chan);
			if (!frame) {
				ast_debug(1, "Channel '%s' did not return a frame; probably hung up.\n", ast_channel_name(chan));
				return -1;
			} else if (frame->frametype == AST_FRAME_VOICE) {
				frame = ast_dsp_process(chan, dsp, frame);
				if (frame->frametype == AST_FRAME_DTMF) {
					int result = frame->subclass.integer;
					ast_frfree(frame);
					last_silent = 0;
					if (result != digit) {
						/* We shouldn't receive an altogether different signal without the existing signal first stopping */
						ast_log(LOG_WARNING, "Received R2 signal '%c' differs from previously received signal '%c'\n", result, digit);
					}
					continue; /* Ignore continuation of tone */
				} else if (frame->frametype != AST_FRAME_DTMF_BEGIN) {
					ast_frfree(frame);
					if (last_silent) {
						/* Two consecutive frames with no tone, tone stopped */
						return 0;
					}
					last_silent = 1;
				}
			}
		} else {
			ast_debug(3, "No activity (%d), channel hung up?\n", res);
			return -1; /* Hangup */
		}
	}
	ast_debug(3, "No time remaining\n");
	return -1;
}

static int r2_incoming(struct ast_channel *chan, char *buf, int buflen, int timeout, int maxdigits)
{
	struct ast_dsp *dsp;
	int remaining_time = timeout;
	int digits_read = 0;
	char *str = buf;
	int discrete = 0, features = 0;
	int res = 0;

	if (!discrete) {
		/* For compelled signaling, we need to set the relax flag for matches to be returned more readily */
		features |= DSP_DIGITMODE_RELAXDTMF;
	}

	if (!(dsp = ast_dsp_new())) {
		ast_log(LOG_WARNING, "Unable to allocate DSP!\n");
		pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "ERROR");
		return -1;
	}
	ast_dsp_set_features(dsp, DSP_FEATURE_DIGIT_DETECT);
	ast_dsp_set_digitmode(dsp, DSP_DIGITMODE_R2_FORWARD | features);

	*str = '\0'; /* start with empty output buffer */

	for (;;) {
		int digit;
		if ((maxdigits && digits_read >= maxdigits) || digits_read >= (buflen - 1)) { /* we don't have room to store any more digits (very unlikely to happen for a legitimate reason) */
			pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "MAXDIGITS");
			break;
		}
		r2_start_silence(chan);
		if (!discrete) {
			ast_dsp_digitreset(dsp);
		}
		digit = r2_receive_digit(chan, dsp, 5000);
		if (digit < 0) {
			res = -1; /* Hangup */
			pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "HANGUP");
			r2_stop_signal(chan);
			break;
		}
		if (!digit) {
			res = 0; /* Timeout */
			pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "TIMEOUT");
			r2_stop_signal(chan);
			break;
		}
		ast_debug(3, "----> '%c' (digit received)\n", digit);
		*str++ = digit;
		*str = '\0';
		digits_read++;

		/* Once the incoming register gets a signal, send the backward signal */
		ast_debug(3, "<---- '%c' (sending ACK, waiting for digit stop)\n", R2_A1);
		r2_start_signal(chan, R2_BACKWARD, R2_A1);

		/* Wait for forward signal to cease */
#ifndef DOUBLE_DSP_ISSUE
		if (r2_receive_cessation(chan, dsp, digit, 5000)) {
			res = 0;
			pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "TIMEOUT");
			r2_stop_signal(chan);
			break;
		}
#else
		/* For some reason, while we're sending the backward tone,
		 * things don't work if we simultaneously try to do DSP to see if we're still receiving tone.
		 * Need to pause longer than sender to avoid re-detecting the previous digit. */
		ast_safe_sleep(chan, 100);
#endif

		/* Stop sending backward signal */
		ast_debug(3, "----> (digit stopped)\n");
		ast_debug(3, "<---- (stopping ACK)\n");
		r2_stop_signal(chan);
#ifdef DOUBLE_DSP_ISSUE
		ast_safe_sleep(chan, 60);
#endif
	}
	ast_dsp_free(dsp);
	ast_debug(3, "channel '%s' - event loop stopped { timeout: %d, remaining_time: %d }\n", ast_channel_name(chan), timeout, remaining_time);
	return res;
}

static int r2_outgoing(struct ast_channel *chan, const char *digits)
{
	const char *ptr;
	struct ast_dsp *dsp;
	int res = 0;
	int discrete = 0;
	int features = 0;

	if (!discrete) {
		/* For compelled signaling, we need to set the relax flag for matches to be returned more readily */
		features |= DSP_DIGITMODE_RELAXDTMF;
	}

	/* Start detection for receiving the backward signals */
	if (!(dsp = ast_dsp_new())) {
		ast_log(LOG_WARNING, "Unable to allocate DSP!\n");
		pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "ERROR");
		return -1;
	}

	ast_dsp_set_features(dsp, DSP_FEATURE_DIGIT_DETECT);
	ast_dsp_set_digitmode(dsp, DSP_DIGITMODE_R2_BACKWARD | features);

	for (ptr = digits; *ptr; ptr++) {
		if (strchr(VALID_R2_CHAR, *ptr)) { /* Character represents valid R2 MF */
			int digit = *ptr;

			/* Q.440 4.1.4 System R2 compelled signaling method:
			 * - upon seizure, outgoing R2 register starts sending first forward interregister signal
			 * - incoming R2 register detects signal, sends backward interregister signal to ACK
			 * - outgoing R2 register recognizes ACK and stops sending forward signal
			 * - incoming R2 register recognizes absence of tone and stops backward signal
			 * - outgoing R2 register recognizes cessation of ACK and, if needed, send next signal. */

			/* Start the forward inter-register signal. */
			ast_debug(3, "====> '%c' (sending digit, waiting for ACK)\n", digit);
			r2_start_signal(chan, R2_FORWARD, digit);

			/* Wait for a backward signal. */
receive:
			if (!discrete) {
				ast_dsp_digitreset(dsp);
			}
			digit = r2_receive_digit(chan, dsp, 5000); /* Wait until we get a backward tone */

			/* Stop tone once we get a backward signal (or fail to) */
			if (digit > 0) {
				if (digit != R2_A1) {
					ast_debug(3, "<==== '%c' (non-ACK)\n", digit);
					goto receive; /* Keep receiving */
				}
				ast_debug(3, "<==== '%c' (received ACK)\n", digit);
			}
			/* If we got an ACK, we can stop */
			ast_debug(3, "====> (stopped digit)\n");
			r2_stop_signal(chan);

			if (digit < 0) {
				res = -1;
				goto r2_stream_cleanup;
			}
			if (!digit) {
				ast_log(LOG_WARNING, "No backward signal received from far end\n");
				res = 0;
				goto r2_stream_cleanup;
			}

			/* Wait until tone stops */
			r2_start_silence(chan);
			res = r2_receive_cessation(chan, dsp, digit, 5000);
			if (res) {
				res = -1;
				goto r2_stream_cleanup;
			}
			ast_debug(3, "<==== (ACK stopped)\n");
			/* Now, we can proceed to the next digit if needed. */
#ifdef DOUBLE_DSP_ISSUE
			ast_safe_sleep(chan, 60); /* any less is not reliable */
#endif
		} else {
			ast_log(LOG_WARNING, "Illegal R2 character '%c' in string. (%s allowed)\n", *ptr, VALID_R2_CHAR);
		}
	}

r2_stream_cleanup:
	ast_dsp_free(dsp);
	return res;
}

static int r2_incoming_exec(struct ast_channel *chan, const char *data)
{
#define BUFFER_SIZE 256
	char tmp[BUFFER_SIZE] = "";
	int to = 15000;
	double tosec;
	char *argcopy = NULL;
	int res, maxdigits = 0;

	AST_DECLARE_APP_ARGS(arglist,
		AST_APP_ARG(variable);
		AST_APP_ARG(maxdigits);
		AST_APP_ARG(timeout);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "R2Incoming requires an argument (variable)\n");
		return -1;
	}

	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(arglist, argcopy);

	if (!ast_strlen_zero(arglist.timeout)) {
		tosec = atof(arglist.timeout);
		if (tosec <= 0) {
			to = 0;
		} else {
			to = tosec * 1000.0;
		}
	}

	if (ast_strlen_zero(arglist.variable)) {
		ast_log(LOG_WARNING, "R2Incoming requires arguments\n");
		return -1;
	}
	if (!ast_strlen_zero(arglist.maxdigits)) {
		maxdigits = atoi(arglist.maxdigits);
		if (maxdigits <= 0) {
			ast_log(LOG_WARNING, "Invalid maximum number of digits, ignoring: '%s'\n", arglist.maxdigits);
			maxdigits = 0;
		}
	}

	res = r2_incoming(chan, tmp, BUFFER_SIZE, to, maxdigits);

	pbx_builtin_setvar_helper(chan, arglist.variable, tmp);
	if (!ast_strlen_zero(tmp)) {
		ast_verb(3, "R2 digits received: '%s'\n", tmp);
	} else if (!res) { /* if channel hung up, don't print anything out */
		ast_verb(3, "No R2 digits received.\n");
	}
	return res;
}

static int r2outgoing_exec(struct ast_channel *chan, const char *vdata)
{
	char *data;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(digits);
	);

	if (ast_strlen_zero(vdata)) {
		ast_log(LOG_WARNING, "R2Outgoing requires an argument\n");
		return 0;
	}

	data = ast_strdupa(vdata);
	AST_STANDARD_APP_ARGS(args, data);

	if (ast_strlen_zero(args.digits)) {
		ast_log(LOG_WARNING, "The digits argument is required (%s)\n", VALID_R2_CHAR);
		return 0;
	}
	return r2_outgoing(chan, args.digits);
}

static int read_r2_digits(struct ast_channel *chan, char *buf, int buflen, int timeout, int maxdigits, char startdigit, enum r2_direction dir)
{
	struct ast_dsp *dsp;
	int remaining_time = timeout;
	int digits_read = 0;
	char *str = buf;
	int res = 0;

	if (!(dsp = ast_dsp_new())) {
		ast_log(LOG_WARNING, "Unable to allocate DSP!\n");
		pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "ERROR");
		return -1;
	}
	ast_dsp_set_features(dsp, DSP_FEATURE_DIGIT_DETECT);
	ast_dsp_set_digitmode(dsp, (dir == R2_BACKWARD ? DSP_DIGITMODE_R2_BACKWARD : DSP_DIGITMODE_R2_FORWARD));

	*str = '\0'; /* start with empty output buffer */

	for (;;) {
		int digit;
		if ((maxdigits && digits_read >= maxdigits) || digits_read >= (buflen - 1)) { /* we don't have room to store any more digits (very unlikely to happen for a legitimate reason) */
			pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "MAXDIGITS");
			break;
		}
		digit = r2_receive_digit(chan, dsp, timeout);
		if (digit < 0) {
			res = -1; /* Hangup */
			pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "HANGUP");
			break;
		}
		if (!digit) {
			res = 0; /* Timeout */
			pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "TIMEOUT");
			break;
		}
		ast_debug(1, "Received R2 %s digit '%c'\n", dir == R2_BACKWARD ? "backward" : "forward", digit);
		*str++ = digit;
		*str = '\0';
		digits_read++;
		if (startdigit && digit == startdigit) {
			pbx_builtin_setvar_helper(chan, "RECEIVER2STATUS", "START");
			break;
		}
	}
	ast_dsp_free(dsp);
	ast_debug(3, "channel '%s' - event loop stopped { timeout: %d, remaining_time: %d }\n", ast_channel_name(chan), timeout, remaining_time);
	return res;
}

static int r2_send_tone(struct ast_channel *chan, enum r2_direction dir, char digit, int duration)
{
	ast_debug(3, "Playing R2 %s tone '%c'\n", dir == R2_FORWARD ? "forward" : "backward", digit);
	r2_start_signal(chan, dir, digit);
	if (ast_safe_sleep(chan, duration)) {
		return -1;
	}
	r2_stop_signal(chan);
	return 0;
}

static int r2_send_tones(struct ast_channel *chan, enum r2_direction dir, const char *digits, int between, unsigned int duration)
{
	const char *ptr;
	int res;
	struct ast_silence_generator *silgen;

	/* Need a quiet time before sending digits. */
	silgen = ast_channel_start_silence_generator(chan);
	res = ast_safe_sleep(chan, 4 * R2_TONE_LENGTH);
	if (silgen) {
		ast_channel_stop_silence_generator(chan, silgen);
	}
	if (res) {
		return -1;
	}

	for (ptr = digits; *ptr; ptr++) {
		if (strchr(VALID_R2_CHAR, *ptr)) { /* Character represents valid R2 MF */
			if (r2_send_tone(chan, dir, *ptr, duration)) {
				break;
			}
			/* Send the inter-digit pause as audio */
			ast_playtones_start(chan, 0, "0", 0);
			if (ast_safe_sleep(chan, between)) {
				return -1;
			}
			ast_playtones_stop(chan);
		} else {
			ast_log(LOG_WARNING, "Illegal R2 character '%c' in string. (%s allowed)\n", *ptr, VALID_R2_CHAR);
		}
	}

	return 0;
}

static int receive_r2_exec(struct ast_channel *chan, const char *data)
{
	char tmp[BUFFER_SIZE] = "";
	int to = 15000;
	double tosec;
	struct ast_flags flags = {0};
	char *argcopy = NULL;
	int res, maxdigits = 0;
	char startdigit;

	AST_DECLARE_APP_ARGS(arglist,
		AST_APP_ARG(variable);
		AST_APP_ARG(startdigit);
		AST_APP_ARG(maxdigits);
		AST_APP_ARG(timeout);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "ReceiveR2 requires an argument (variable)\n");
		return -1;
	}

	argcopy = ast_strdupa(data);

	AST_STANDARD_APP_ARGS(arglist, argcopy);

	if (!ast_strlen_zero(arglist.options)) {
		ast_app_parse_options(r2_app_options, &flags, NULL, arglist.options);
	}

	if (!ast_strlen_zero(arglist.timeout)) {
		tosec = atof(arglist.timeout);
		if (tosec <= 0) {
			to = 0;
		} else {
			to = tosec * 1000.0;
		}
	}

	if (ast_strlen_zero(arglist.variable)) {
		ast_log(LOG_WARNING, "ReceiveR2 requires arguments\n");
		return -1;
	}
	if (!ast_strlen_zero(arglist.maxdigits)) {
		maxdigits = atoi(arglist.maxdigits);
		if (maxdigits <= 0) {
			ast_log(LOG_WARNING, "Invalid maximum number of digits, ignoring: '%s'\n", arglist.maxdigits);
			maxdigits = 0;
		}
	}
	startdigit = !ast_strlen_zero(arglist.startdigit) ? arglist.startdigit[0] : 0;

	res = read_r2_digits(chan, tmp, BUFFER_SIZE, to, maxdigits, startdigit, ast_test_flag(&flags, OPT_BACKWARD) ? R2_BACKWARD : R2_FORWARD);

	pbx_builtin_setvar_helper(chan, arglist.variable, tmp);
	if (!ast_strlen_zero(tmp)) {
		ast_verb(3, "R2 digits received: '%s'\n", tmp);
	} else if (!res) { /* if channel hung up, don't print anything out */
		ast_verb(3, "No R2 digits received.\n");
	}
	return res;
}

static int send_r2_exec(struct ast_channel *chan, const char *vdata)
{
	char *data;
	struct ast_flags flags = {0};
	int dinterval = 0, duration = 0;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(digits);
		AST_APP_ARG(duration);
		AST_APP_ARG(dinterval);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(vdata)) {
		ast_log(LOG_WARNING, "SendR2 requires an argument\n");
		return 0;
	}

	data = ast_strdupa(vdata);
	AST_STANDARD_APP_ARGS(args, data);

	if (ast_strlen_zero(args.digits)) {
		ast_log(LOG_WARNING, "The digits argument is required (%s)\n", VALID_R2_CHAR);
		return 0;
	}
	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(r2_app_options, &flags, NULL, args.options);
	}
	if (!ast_strlen_zero(args.dinterval)) {
		ast_app_parse_timelen(args.dinterval, &dinterval, TIMELEN_MILLISECONDS);
	}
	if (!ast_strlen_zero(args.duration)) {
		ast_app_parse_timelen(args.duration, &duration, TIMELEN_MILLISECONDS);
	}
	dinterval = dinterval <= 0 ? R2_BETWEEN_MS : dinterval;
	duration = duration <= 0 ? R2_TONE_LENGTH : duration;

	return r2_send_tones(chan, ast_test_flag(&flags, OPT_BACKWARD) ? R2_BACKWARD : R2_FORWARD, args.digits, dinterval, duration);
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(incoming_name);
	res |= ast_unregister_application(outgoing_name);
	res |= ast_unregister_application(receiver_name);
	res |= ast_unregister_application(sender_name);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_register_application_xml(incoming_name, r2_incoming_exec);
	res |= ast_register_application_xml(outgoing_name, r2outgoing_exec);
	res |= ast_register_application_xml(receiver_name, receive_r2_exec);
	res |= ast_register_application_xml(sender_name, send_r2_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "R2 Signaling Applications");
