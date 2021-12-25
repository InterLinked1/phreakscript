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
 * \brief Trivial application to loop playback of a sound file
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
#include "asterisk/conversions.h"

/*** DOCUMENTATION
	<application name="LoopPlayback" language="en_US">
		<synopsis>
			Loops audio playback indefinitely or a certain number of times
		</synopsis>
		<syntax>
			<parameter name="filenames" required="true" argsep="&amp;">
				<argument name="filename" required="true" />
				<argument name="filename2" multiple="true" />
			</parameter>
			<parameter name="times" required="false">
				<para>Number of times to play specified file(s).
				If omitted, playback will loop "forever", i.e.
				until the channel hangs up or it is redirected to
				a different application. Default is 0 (indefinitely).</para>
			</parameter>
		</syntax>
		<description>
			<para>Plays back given filenames (do not put extension of wav/alaw, etc).
			If the file is non-existent it will fail.</para>
			<para>Options available with <literal>Playback</literal> are not available with
			this application.</para>
			<para>This application does not automatically answer the channel and should
			be preceded by <literal>Progress</literal> or <literal>Answer</literal> as
			appropriate.</para>
			<para>This application sets the following channel variable upon completion:</para>
			<variablelist>
				<variable name="LOOPPLAYBACKSTATUS">
					<para>The status of the playback attempt as a text string.</para>
					<value name="SUCCESS"/>
					<value name="FAILED"/>
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">Playback</ref>
			<ref type="application">ControlPlayback</ref>
		</see-also>
	</application>
 ***/

static char *app = "LoopPlayback";

static int playback_exec(struct ast_channel *chan, const char *data)
{
	int res = 0, mres = 0, loops = 0, plays = 0;
	char *tmp;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(filenames);
		AST_APP_ARG(loops);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "LoopPlayback requires an argument (filename)\n");
		return -1;
	}

	tmp = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, tmp);

	if (!ast_strlen_zero(args.loops) && (ast_str_to_int(args.loops, &loops) || loops < 0)) {
		ast_log(LOG_WARNING, "Invalid number of loops: %s\n", args.loops);
		return -1;
	}

	/* do not automatically answer the channel */

	ast_stopstream(chan);

	while (!res && !mres && (loops == 0 || plays++ < loops)) {
		char *back = ast_strdup(args.filenames); /* because we could be iterating for a LONG time, strdupa is a BAD idea, we could blow the stack */
		char *front;

		if (loops > 0) {
			ast_debug(1, "Looping playback of file(s) '%s', iteration %d of %d\n", back, plays, loops);
		}

		while (!res && (front = strsep(&back, "&"))) {
			res = ast_streamfile(chan, front, ast_channel_language(chan));
			if (!res) {
				res = ast_waitstream(chan, "");
				ast_stopstream(chan);
			}
			if (res) {
				if (!ast_check_hangup(chan)) {
					ast_log(LOG_WARNING, "LoopPlayback failed on %s for %s\n", ast_channel_name(chan), (char *)data);
				}
				res = 0;
				mres = 1;
			}
		}
		ast_free(back);
	}

	pbx_builtin_setvar_helper(chan, "LOOPPLAYBACKSTATUS", mres ? "FAILED" : "SUCCESS");
	return res;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(app);

	return res;
}

static int load_module(void)
{
	return ast_register_application_xml(app, playback_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Playback Loop Application");
