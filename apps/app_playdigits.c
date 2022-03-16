/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2022, Naveen Albert
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
 * \brief Play digits application
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

/*** DOCUMENTATION
	<application name="PlayDigits" language="en_US">
		<synopsis>
			Plays a DTMF or MF sequence using audio files
		</synopsis>
		<syntax>
			<parameter name="directory" required="true">
				<para>Directory containing the audio set to be used.</para>
				<para>Do NOT terminate with a trailing slash.</para>
			</parameter>
			<parameter name="number" required="true">
				<para>The number to play to the caller.</para>
			</parameter>
			<parameter name="separationtime" required="false">
				<para>Pause duration, in seconds, between digits.</para>
				<para>Default is 250 ms.</para>
			</parameter>
			<parameter name="options" required="no">
				<optionlist>
					<option name="a" argsep="&amp;">
						<para>Custom file(s) to play before digits, relative
						to the specified directory.</para>
					</option>
					<option name="b" argsep="&amp;">
						<para>Custom file(s) to play between digits.</para>
					</option>
					<option name="m">
						<para>Custom prefix for digits. Default is none.</para>
					</option>
					<option name="n">
						<para>Custom suffix for digits. Default is none.</para>
					</option>
					<option name="p">
						<para>Custom file to play for pound.</para>
					</option>
					<option name="s">
						<para>Custom file to play for star.</para>
					</option>
					<option name="z" argsep="&amp;">
						<para>Custom file(s) to play after digits, relative
						to the specified directory.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Plays a DTMF or MF signaling sequence using audio files
			rather than pure tones.</para>
			<para>Numeric digits must have file names corresponding to
			the digit to be played.</para>
		</description>
		<see-also>
			<ref type="application">SendDTMF</ref>
			<ref type="application">SendMF</ref>
			<ref type="application">SayTelephoneNumber</ref>
			<ref type="application">PlayTones</ref>
			<ref type="application">Playback</ref>
		</see-also>
	</application>
 ***/

static char *app = "PlayDigits";

static char *filter_number(char *str, int *length, int allowalpha)
{
	char *new = NULL;
	int i, j, len = strlen(str);
	*length = 0;
	new = ast_malloc(len + 1); /* null terminator */
	if (!new) {
		ast_log(LOG_WARNING, "Memory allocation failed\n");
		return new;
	}
	for (i = j = 0; i < len; i++) {
		if ((allowalpha && isalnum(str[i])) || isdigit(str[i]) || str[i] == '*' || str[i] == '#') {
			new[j++] = str[i];
			if (length) {
				(*length)++;
			}
		}
	}
	new[j] = '\0';
	return new;
}

enum play_option_flags {
	OPT_BETWEEN    = (1 << 0),
	OPT_POUND      = (1 << 1),
	OPT_STAR       = (1 << 2),
	OPT_PRE        = (1 << 3),
	OPT_POST       = (1 << 4),
	OPT_PREFIX     = (1 << 5),
	OPT_SUFFIX     = (1 << 6),
};

enum {
	OPT_ARG_BETWEEN,
	OPT_ARG_POUND,
	OPT_ARG_STAR,
	OPT_ARG_PRE,
	OPT_ARG_POST,
	OPT_ARG_PREFIX,
	OPT_ARG_SUFFIX,
	/* Must be the last element */
	OPT_ARG_ARRAY_SIZE,
};

AST_APP_OPTIONS(play_app_options, {
	AST_APP_OPTION_ARG('a', OPT_PRE, OPT_ARG_PRE),
	AST_APP_OPTION_ARG('b', OPT_BETWEEN, OPT_ARG_BETWEEN),
	AST_APP_OPTION_ARG('m', OPT_PREFIX, OPT_ARG_PREFIX),
	AST_APP_OPTION_ARG('n', OPT_SUFFIX, OPT_ARG_SUFFIX),
	AST_APP_OPTION_ARG('p', OPT_POUND, OPT_ARG_POUND),
	AST_APP_OPTION_ARG('s', OPT_STAR, OPT_ARG_STAR),
	AST_APP_OPTION_ARG('z', OPT_POST, OPT_ARG_POST),
});

#define SET_MAX_LEN(option, arg, add) { \
	if (ast_test_flag(&flags, option) && !ast_strlen_zero(optargs[arg])) { \
		tmpptr = optargs[arg]; \
		if ((x = strlen(tmpptr)) > addlen) { \
			addlen = x + add; \
		} \
	} \
}

#define SET_VAR(option, arg, default, varname) { \
	if (ast_test_flag(&flags, option) && !ast_strlen_zero(optargs[arg])) { \
		varname = optargs[arg]; \
	} else { \
		varname = default; \
	} \
}


static int playdigits_exec(struct ast_channel *chan, const char *data)
{
#define DEFAULT_WAITMS 250
#define DEFAULT_BETWEEN "_"
#define DEFAULT_STAR "*"
#define DEFAULT_POUND "#"
#define DEFAULT_PRE ""
#define DEFAULT_POST ""
#define DEFAULT_PREFIX ""
#define DEFAULT_SUFFIX ""
	int d, dirlen, res = 0, addlen = 0, waitms = DEFAULT_WAITMS;
	char *tmp, *num, *orignum, *file;
	double sec;
	char *tmpptr, *between = "", *star = "", *pound = "", *pre = "", *post = "", *prefix = "", *suffix = "";
	struct ast_flags flags = {0};
	char *optargs[OPT_ARG_ARRAY_SIZE];

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(directory);
		AST_APP_ARG(number);
		AST_APP_ARG(waitsec);
		AST_APP_ARG(options);
	);

	tmp = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, tmp);

	if (ast_strlen_zero(args.directory)) {
		ast_log(LOG_WARNING, "%s requires a directory\n", app);
		return -1;
	}
	if (ast_strlen_zero(args.number)) {
		ast_log(LOG_WARNING, "%s requires a number\n", app);
		return -1;
	}
	if (!ast_strlen_zero(args.waitsec)) {
		if (sscanf(args.waitsec, "%30lg", &sec) != 1 || sec < 0) {
			ast_log(LOG_WARNING, "Invalid timeout provided: %s. Defaulting to %d ms\n", args.waitsec, DEFAULT_WAITMS);
			waitms = DEFAULT_WAITMS;
		} else {
			waitms = sec * 1000; /* sec to msec */
		}
	}
	addlen = 0;
	if (!ast_strlen_zero(args.options)) {
		int x;
		ast_app_parse_options(play_app_options, &flags, optargs, args.options);
		SET_MAX_LEN(OPT_PRE, OPT_ARG_PRE, 0);
		SET_MAX_LEN(OPT_POST, OPT_ARG_POST, 0);
		SET_MAX_LEN(OPT_BETWEEN, OPT_ARG_BETWEEN, 0);
		SET_MAX_LEN(OPT_STAR, OPT_ARG_STAR, 0);
		SET_MAX_LEN(OPT_POUND, OPT_ARG_POUND, 0);
		SET_MAX_LEN(OPT_PREFIX, OPT_ARG_PREFIX, 1); /* add 1 for the digit itself */
		SET_MAX_LEN(OPT_SUFFIX, OPT_ARG_SUFFIX, 1); /* add 1 for the digit itself */

		SET_VAR(OPT_PRE, OPT_ARG_PRE, DEFAULT_PRE, pre);
		SET_VAR(OPT_POST, OPT_ARG_POST, DEFAULT_POST, post);
		SET_VAR(OPT_BETWEEN, OPT_ARG_BETWEEN, DEFAULT_BETWEEN, between);
		SET_VAR(OPT_STAR, OPT_ARG_STAR, DEFAULT_STAR, star);
		SET_VAR(OPT_POUND, OPT_ARG_POUND, DEFAULT_POUND, pound);
		SET_VAR(OPT_PREFIX, OPT_ARG_PREFIX, DEFAULT_PREFIX, prefix);
		SET_VAR(OPT_SUFFIX, OPT_ARG_PREFIX, DEFAULT_SUFFIX, suffix);
	}

	num = filter_number(args.number, &d, 0);
	orignum = num;
	dirlen = strlen(args.directory);

	if (args.directory[dirlen - 1] == '/') { /* guaranteed to be at least 1 by here */
		args.directory[dirlen - 1] = '\0'; /* discard trailing / in directory */
	}

	dirlen += (addlen + 2); /* dir + '/' + file max len + null terminator */
	file = ast_malloc(dirlen);

	/* do not automatically answer the channel (seriously, do it explicitly if you want it) */
	ast_stopstream(chan);

	while (!res && !ast_strlen_zero(pre) && (tmpptr = strsep(&pre, "&"))) {
		snprintf(file, dirlen, "%s/%s", args.directory, tmpptr);
		res = ast_streamfile(chan, file, ast_channel_language(chan));
		if (!res) {
			res = ast_waitstream(chan, "");
			ast_stopstream(chan);
		}
	}

	while (*num && !res && d) {
		if (num[0] == '*') {
			snprintf(file, dirlen, "%s/%s", args.directory, star);
		} else if (num[0] == '#') {
			snprintf(file, dirlen, "%s/%s", args.directory, pound);
		} else {
			snprintf(file, dirlen, "%s/%s%c%s", args.directory, prefix, num[0], suffix);
		}
		if (ast_fileexists(file, NULL, NULL)) {
			res = ast_streamfile(chan, file, ast_channel_language(chan));
			if (!res) {
				res = ast_waitstream(chan, "");
				ast_stopstream(chan);
			}
		} else {
			ast_log(LOG_WARNING, "Unable to play digit '%c' in '%s' (missing file '%s')\n", num[0], orignum, file);
		}

		d--;
		num++;
		if (!res && d) {
			snprintf(file, dirlen, "%s/%s", args.directory, between);
			ast_debug(1, "Looking for between file '%s'\n", file);
			if (*between && ast_fileexists(file, NULL, NULL)) {
				res = ast_streamfile(chan, file, ast_channel_language(chan));
				if (!res) {
					res = ast_waitstream(chan, "");
					ast_stopstream(chan);
				}
			} else {
				res = ast_safe_sleep(chan, waitms);
			}
		}
	}

	while (!res && !ast_strlen_zero(post) && (tmpptr = strsep(&post, "&"))) {
		snprintf(file, dirlen, "%s/%s", args.directory, tmpptr);
		res = ast_streamfile(chan, file, ast_channel_language(chan));
		if (!res) {
			res = ast_waitstream(chan, "");
			ast_stopstream(chan);
		}
	}

	ast_free(file);
	ast_free(orignum);

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
	return ast_register_application_xml(app, playdigits_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Play Digits Application");
