/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2023, Naveen Albert
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
 * \brief Application to partially play a sound file
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
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/mod_format.h" /* expose ast_filestream */

/*** DOCUMENTATION
	<application name="PartialPlayback" language="en_US">
		<synopsis>
			Play a file, between optionally specified start and end offsets.
		</synopsis>
		<syntax>
			<parameter name="filename" required="true">
				<para>File to play. Do not include extension.</para>
			</parameter>
			<parameter name="start">
				<para>Starting time. Default is start of file.</para>
			</parameter>
			<parameter name="end">
				<para>Ending time. Default is end of file.</para>
			</parameter>
		</syntax>
		<description>
			<para>Plays back a given filename (do not include extension).</para>
			<para>This application does not automatically answer the channel.</para>
			<para>This application sets the following channel variable upon completion:</para>
			<variablelist>
				<variable name="PLAYBACKSTATUS">
					<para>The status of the playback attempt.</para>
					<value name="SUCCESS"/>
					<value name="FAILED"/>
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">Playback</ref>
			<ref type="application">ControlPlayback</ref>
			<ref type="agi">stream file</ref>
			<ref type="agi">control stream file</ref>
			<ref type="manager">ControlPlayback</ref>
		</see-also>
	</application>
 ***/

static char *app = "PartialPlayback";

static int playback_exec(struct ast_channel *chan, const char *data)
{
	int res = 0;
	int start = 0, end = 0;
	char *tmp;
	const char *orig_chan_name = NULL;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(filename);
		AST_APP_ARG(start);
		AST_APP_ARG(end);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "%s requires an argument (filename)\n", app);
		return -1;
	}

	tmp = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, tmp);

	if (ast_strlen_zero(args.filename)) {
		ast_log(LOG_ERROR, "%s requires a filename\n", app);
		return -1;
	}

	if (!ast_strlen_zero(args.start)) {
		start = atoi(args.start);
		if (start < 0) {
			ast_log(LOG_ERROR, "Invalid start time: %s\n", args.start);
			return -1;
		}
	}
	if (!ast_strlen_zero(args.end)) {
		end = atoi(args.end);
	}

	if (ast_test_flag(ast_channel_flags(chan), AST_FLAG_MASQ_NOSTREAM)) {
		orig_chan_name = ast_strdupa(ast_channel_name(chan));
	}

	ast_stopstream(chan);
	res = ast_streamfile(chan, args.filename, ast_channel_language(chan));
	if (res) {
		goto end;
	}

	if (start) {
		/* Seek to starting position - based on waitstream_control in file.c */
		int eoftest;
		/* We're fast forwarding from the beginning, so this is a convenient wrapper
		 * to seek to the right spot, using ms instead of samples. */
		ast_stream_fastforward(ast_channel_stream(chan), start);
		eoftest = fgetc(ast_channel_stream(chan)->f);
		if (feof(ast_channel_stream(chan)->f)) {
			ast_stream_rewind(ast_channel_stream(chan), start);
		} else {
			ungetc(eoftest, ast_channel_stream(chan)->f);
		}
	}

	if (end) {
		/* Play until we get to end time */
		while (ast_channel_stream(chan)) {
			/* Based on waitstream_core in file.c */
			int res;
			int ms;
			struct ast_frame *fr;
			off_t offset;
			int ms_len;

			if (orig_chan_name && strcasecmp(orig_chan_name, ast_channel_name(chan))) {
				ast_stopstream(chan);
				res = -1;
				break;
			}
			ms = ast_sched_wait(ast_channel_sched(chan));
			if (ms < 0 && !ast_channel_timingfunc(chan)) {
				ast_stopstream(chan);
				break;
			}
			if (ms < 0) {
				ms = 1000;
			}
			res = ast_waitfor(chan, ms);
			if (res < 0) {
				ast_log(LOG_WARNING, "ast_waitfor failed (%s)\n", strerror(errno));
				break;
			}
			fr = ast_read(chan);
			if (!fr) {
				ast_debug(3, "Channel %s did not return a frame, must've hung up\n", ast_channel_name(chan));
				break;
			}
			switch (fr->frametype) {
				case AST_FRAME_CONTROL:
					switch (fr->subclass.integer) {
						case AST_CONTROL_HANGUP:
							goto end;
						default:
							break;
					}
				default:
					break;
			}
			/* Is our time yet come? */
			offset = ast_tellstream(ast_channel_stream(chan));
			ms_len = offset / (ast_format_get_sample_rate(ast_channel_stream(chan)->fmt->format) / 1000);
			if (ms_len >= end) {
				ast_debug(1, "Stopping stream because we reached %d ms\n", end);
				break;
			}
			ast_sched_runq(ast_channel_sched(chan));
		}
	} else {
		/* Play the remainder */
		res = ast_waitstream(chan, "");
	}
	ast_stopstream(chan);

end:
	if (res && !ast_check_hangup(chan)) {
		ast_log(LOG_WARNING, "%s failed on %s for %s\n", app, ast_channel_name(chan), args.filename);
	}

	pbx_builtin_setvar_helper(chan, "PLAYBACKSTATUS", res ? "FAILED" : "SUCCESS");
	return res;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, playback_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Partial playback application");
