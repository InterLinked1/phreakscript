/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2021, Naveen Albert
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
 * \brief Frame applications
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
#include "asterisk/frame.h"
#include "asterisk/pbx.h"
#include "asterisk/format_cache.h"
#include "asterisk/channel.h"
#include "asterisk/dsp.h"
#include "asterisk/app.h"
#include "asterisk/module.h"
#include "asterisk/indications.h"
#include "asterisk/conversions.h"

/*** DOCUMENTATION
	<application name="WaitForFrame" language="en_US">
		<synopsis>
			Waits for a given frame type to be received on a channel.
		</synopsis>
		<syntax>
			<parameter name="frame" required="true">
				<para>The type of frame for which to wait.</para>
				<para>The following frame types may be used:</para>
				<enumlist>
					<enum name = "DTMF_BEGIN" />
					<enum name = "DTMF_END" />
					<enum name = "VOICE" />
					<enum name = "VIDEO" />
					<enum name = "NULL" />
					<enum name = "IAX" />
					<enum name = "TEXT" />
					<enum name = "TEXT_DATA" />
					<enum name = "IMAGE" />
					<enum name = "HTML" />
					<enum name = "CNG" />
					<enum name = "MODEM" />
				</enumlist>
				<para>The following CONTROL frames may also be used:</para>
				<enumlist>
					<enum name = "RING" />
					<enum name = "RINGING" />
					<enum name = "ANSWER" />
					<enum name = "BUSY" />
					<enum name = "TAKEOFFHOOK" />
					<enum name = "OFFHOOK" />
					<enum name = "CONGESTION" />
					<enum name = "FLASH" />
					<enum name = "WINK" />
					<enum name = "PROGRESS" />
					<enum name = "PROCEEDING" />
					<enum name = "HOLD" />
					<enum name = "UNHOLD" />
				</enumlist>
			</parameter>
			<parameter name="timeout">
				<para>The number of seconds to wait for the specified type of
				frame to be received, if greater than <literal>0</literal>.
				Can be floating point. Default is no timeout (wait forever).</para>
			</parameter>
			<parameter name="times">
				<para>The number of frames of this type that must be received
				before dialplan execution continues, if the timer has not yet
				expired.</para>
			</parameter>
			<parameter name="file">
				<para>Optional audio file to play in a loop while application is running.</para>
			</parameter>
		</syntax>
		<description>
			<para>Waits for a specified frame type to be received before
			dialplan execution continues, with a configurable timeout. This
			is useful if the channel needs to wait for a certain type of control
			frame to be received in order for call setup or progression to
			continue.</para>
			<para>If a digit is dialed during execution and a single-digit extension
			matches in the current context, execution will continue there.</para>
			<variablelist>
				<variable name="WAITFORFRAMESTATUS">
					<para>This is the status of waiting for the frame.</para>
					<value name="SUCCESS" />
					<value name="DIGIT" />
					<value name="ERROR" />
					<value name="HANGUP" />
					<value name="TIMEOUT" />
				</variable>
			</variablelist>
			<example title="Inpulsing to a switch using EM signaling">
			exten => _X!,1,Progress()
				same => n,WaitForFrame(WINK,10) ; wait up to 10s for a wink on the channel
				same => n,GotoIf($["${WAITFORFRAMESTATUS}" != "SUCCESS"]?fail,s,1)
				same => n,SendMF(*${EXTEN}#)
			</example>
		</description>
		<see-also>
			<ref type="application">SendFrame</ref>
			<ref type="application">ReceiveMF</ref>
			<ref type="application">SendMF</ref>
		</see-also>
	</application>
	<application name="SendFrame" language="en_US">
		<synopsis>
			Sends an arbitrary frame on a channel.
		</synopsis>
		<syntax>
			<parameter name="frame" required="true">
				<para>The type of frame to send.</para>
				<para>The following frame types may be sent:</para>
				<enumlist>
					<enum name = "DTMF_BEGIN" />
					<enum name = "DTMF_END" />
					<enum name = "VOICE" />
					<enum name = "VIDEO" />
					<enum name = "NULL" />
					<enum name = "IAX" />
					<enum name = "TEXT" />
					<enum name = "TEXT_DATA" />
					<enum name = "IMAGE" />
					<enum name = "HTML" />
					<enum name = "CNG" />
					<enum name = "MODEM" />
				</enumlist>
				<para>The following CONTROL frames may be sent:</para>
				<enumlist>
					<enum name = "TAKEOFFHOOK" />
					<enum name = "OFFHOOK" />
					<enum name = "FLASH" />
					<enum name = "WINK" />
					<enum name = "HOLD" />
					<enum name = "UNHOLD" />
				</enumlist>
			</parameter>
		</syntax>
		<description>
			<para>Sends an arbitrary frame on a channel.</para>
			<example title="Send Wink">
			same => n,SendFrame(WINK)
			</example>
		</description>
		<see-also>
			<ref type="application">WaitForFrame</ref>
			<ref type="application">ReceiveMF</ref>
			<ref type="application">SendMF</ref>
		</see-also>
	</application>
 ***/

static char *wait_app = "WaitForFrame";
static char *send_app = "SendFrame";

static struct {
	enum ast_frame_type type;
	const char *str;
} frametype2str[] = {
	{ AST_FRAME_VOICE,   "VOICE" },
	{ AST_FRAME_VIDEO,   "VIDEO" },
	{ AST_FRAME_DTMF_BEGIN,   "DTMF_BEGIN" },
	{ AST_FRAME_DTMF_END,   "DTMF_END" },
	{ AST_FRAME_IAX,   "IAX" },
	{ AST_FRAME_NULL,   "NULL" },
	{ AST_FRAME_TEXT,   "TEXT" },
	{ AST_FRAME_TEXT_DATA,   "TEXT_DATA" },
	{ AST_FRAME_IMAGE,   "IMAGE" },
	{ AST_FRAME_HTML,   "HTML" },
	{ AST_FRAME_CNG,   "CNG" },
	{ AST_FRAME_MODEM,   "MODEM" },
};

static struct {
	int type;
	const char *str;
} controlframetype2str[] = {
	{ AST_CONTROL_RING,   "RING" },
	{ AST_CONTROL_RINGING,   "RINGING" },
	{ AST_CONTROL_ANSWER,   "ANSWER" },
	{ AST_CONTROL_BUSY,   "BUSY" },
	{ AST_CONTROL_TAKEOFFHOOK,   "TAKEOFFHOOK" },
	{ AST_CONTROL_OFFHOOK,   "OFFHOOK" },
	{ AST_CONTROL_CONGESTION,   "CONGESTION" },
	{ AST_CONTROL_FLASH,   "FLASH" },
	{ AST_CONTROL_WINK,   "WINK" },
	{ AST_CONTROL_PROGRESS,   "PROGRESS" },
	{ AST_CONTROL_PROCEEDING,   "PROCEEDING" },
	{ AST_CONTROL_HOLD,   "HOLD" },
	{ AST_CONTROL_UNHOLD,   "UNHOLD" },
};

static int sendframe_exec(struct ast_channel *chan, const char *data)
{
	int subtype = 0, i = 0;
	unsigned short int validframe = 0;
	char *argcopy = NULL;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(frametype);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "SendFrame requires an argument (frametype)\n");
		return -1;
	}
	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, argcopy);
	if (ast_strlen_zero(args.frametype)) {
		ast_log(LOG_WARNING, "Invalid! Usage: SendFrame(frametype)\n");
		return -1;
	}
	for (i = 0; i < ARRAY_LEN(frametype2str); i++) {
		if (strcasestr(args.frametype, frametype2str[i].str)) {
			/* Send a non-control frame */
			int res;
			struct ast_frame f = { frametype2str[i].type, };
			f.subclass.integer = '0';
			ast_channel_lock(chan);
			res = ast_write(chan, &f);
			ast_channel_unlock(chan);
			if (res) {
				ast_log(LOG_WARNING, "Failed to write %s frame on %s\n", frametype2str[i].str, ast_channel_name(chan));
			}
			return 0;
		}
	}
	/* Send a control frame */
	for (i = 0; i < ARRAY_LEN(controlframetype2str); i++) {
		if (strcasestr(args.frametype, controlframetype2str[i].str)) {
			subtype = controlframetype2str[i].type;
			validframe = 1;
		}
	}
	if (!validframe) {
		ast_log(LOG_WARNING, "Unsupported frame type: %s\n", args.frametype);
		return -1;
	}

	ast_indicate(chan, subtype);

	return 0;
}

static int waitframe_exec(struct ast_channel *chan, const char *data)
{
	int timeout = 0, hits = 0, reqmatches = 1;
	double tosec;
	struct timeval start;
	struct ast_frame *frame;
	int remaining_time = timeout, subtype = 0, i = 0;
	unsigned short int validframe = 0;
	enum ast_frame_type waitframe;
	char *argcopy = NULL;
	int res = 0;
	char *audiofile = NULL;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(frametype);
		AST_APP_ARG(timeout);
		AST_APP_ARG(reqmatches);
		AST_APP_ARG(file);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "WaitForFrame requires an argument (frametype)\n");
		return -1;
	}

	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, argcopy);

	if (ast_strlen_zero(args.frametype)) {
		ast_log(LOG_WARNING, "Invalid! Usage: WaitForFrame(frametype[,timeout])\n");
		return -1;
	}
	audiofile = args.file;

	for (i = 0; i < ARRAY_LEN(frametype2str); i++) {
		if (strcasestr(args.frametype, frametype2str[i].str)) {
			waitframe = frametype2str[i].type;
			validframe = 1;
		}
	}
	for (i = 0; i < ARRAY_LEN(controlframetype2str); i++) {
		if (strcasestr(args.frametype, controlframetype2str[i].str)) {
			waitframe = AST_FRAME_CONTROL;
			subtype = controlframetype2str[i].type;
			validframe = 1;
		}
	}
	if (!validframe) {
		ast_log(LOG_WARNING, "Unsupported frame type: %s\n", args.frametype);
		return -1;
	}
	if (!ast_strlen_zero(args.timeout)) {
		tosec = atof(args.timeout);
		if (tosec <= 0) {
			timeout = 0;
		} else {
			timeout = tosec * 1000.0;
		}
	}
	if (!ast_strlen_zero(args.reqmatches) && (ast_str_to_int(args.reqmatches, &reqmatches) || reqmatches < 1)) {
		ast_log(LOG_WARNING, "Invalid number of times: %s\n", args.reqmatches);
		pbx_builtin_setvar_helper(chan, "WAITFORFRAMESTATUS", "ERROR");
		return -1;
	}
	start = ast_tvnow();

	if (audiofile) {
		ast_streamfile(chan, audiofile, ast_channel_language(chan));
	}

	do {
		if (timeout > 0) {
			remaining_time = ast_remaining_ms(start, timeout);
			if (remaining_time <= 0) {
				pbx_builtin_setvar_helper(chan, "WAITFORFRAMESTATUS", "TIMEOUT");
				break;
			}
		}
		if (ast_waitfor(chan, 1000) > 0) {
			frame = ast_read(chan);
			if (!frame) {
				ast_debug(1, "Channel '%s' did not return a frame; probably hung up.\n", ast_channel_name(chan));
				pbx_builtin_setvar_helper(chan, "WAITFORFRAMESTATUS", "HANGUP");
				res = -1;
				break;
			} else if (frame->frametype == waitframe && (frame->frametype != AST_FRAME_CONTROL ||
				subtype == frame->subclass.integer)) {
				if (++hits >= reqmatches) {
					pbx_builtin_setvar_helper(chan, "WAITFORFRAMESTATUS", "SUCCESS");
					break;
				}
			} else if (frame->frametype == AST_FRAME_DTMF_END) {
				char exten[2];
				char digit = frame->subclass.integer;
				*exten = digit;
				*(exten + 1) = '\0';
				if (ast_exists_extension(chan, ast_channel_context(chan), exten, 1, NULL)) {
					pbx_builtin_setvar_helper(chan, "WAITFORFRAMESTATUS", "DIGIT");
					ast_explicit_goto(chan, NULL, exten, 1);
					break;
				}
			}
			if (audiofile && ast_channel_streamid(chan) == -1 && ast_channel_timingfunc(chan) == NULL) {
				/* Stream ended, start it again */
				ast_streamfile(chan, audiofile, ast_channel_language(chan));
			}
		} else {
			pbx_builtin_setvar_helper(chan, "WAITFORFRAMESTATUS", "HANGUP");
		}
	} while (timeout == 0 || remaining_time > 0);

	if (!res && audiofile) {
		ast_stopstream(chan);
	}

	return res;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(wait_app);
	res |= ast_unregister_application(send_app);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_register_application_xml(wait_app, waitframe_exec);
	res |= ast_register_application_xml(send_app, sendframe_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Frame applications");
