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
 * \brief Enunciate a telephone number
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
	<application name="SayTelephoneNumber" language="en_US">
		<synopsis>
			Enunciates a telephone number with the proper inflections and intonations
		</synopsis>
		<syntax>
			<parameter name="directory" required="true">
				<para>Directory containing the prompt set to be used.</para>
				<para>Do NOT terminate with a trailing slash.</para>
			</parameter>
			<parameter name="number" required="true">
				<para>The number to announce to the caller.</para>
				<para>Must be alphanumeric. Number may contain letters
				for the exchange name that will be used if a corresponding
				file exists.</para>
			</parameter>
			<parameter name="separationtime" required="false">
				<para>Pause duration, in seconds, between groups of
				numbers (such as between the area code, prefix, and
				station number).</para>
				<para>Default is 750 ms.</para>
			</parameter>
			<parameter name="options" required="no">
				<optionlist>
					<option name="m">
						<para>Enable triple inflection mode,
						and specifically use MCI inflections.
						Default is double inflection mode.</para>
					</option>
					<option name="n">
						<para>Suffix to use for files containing
						normal or down inflection.
						Default is nothing.</para>
					</option>
					<option name="p">
						<para>Filename (in same directory) to play before
						each digit.</para>
					</option>
					<option name="t">
						<para>Enable triple inflection mode.
						Default is double inflection mode.</para>
						<para>Most number announcements use double
						inflection.</para>
						<para>Use of the <literal>m</literal> option
						will override this option.</para>
					</option>
					<option name="u">
						<para>Suffix to use for files containing
						up inflection. Default is <literal>_</literal>
						(single underscore).</para>
					</option>
					<option name="v">
						<para>Suffix to use for files containing
						double-up inflection. Default is <literal>__</literal>
						(double underscore).</para>
						<para>Double up-inflection is only used in triple
						inflection mode (either regular or MCI).</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Dynamically enunciates a telephone number to the caller,
			emulating a standard number announcement drum machine.</para>
			<para>This application will properly enunciate a telephone
			number by alternating inflections on certain digits,
			to reflect the way that people actually say telephone numbers.</para>
			<para>In double-inflection mode, a down inflection is used
			on pre-pause (e.g. at the end of a number group) digits.</para>
			<para>In triple-inflection mode, an up inflection is used on
			pre-pause digits, except for the last number group (the station
			number). In this group, the 3rd to last digit receives an up
			inflection, and the very last digit (and only this digit) will use
			a down inflection.</para>
			<para>In all modes, a pause is added between number groups.</para>
			<para>Down inflection digits should be named as the digit only,
			not including the file extension.</para>
			<para>Up-inflection digits should be named as the digit
			followed by an underscore, not including the file extension.
			(This is the convention used by the Asterisk sounds library.)</para>
			<para>For double up-inflection digits (used only in triple inflection mode),
			suffix a double-underscore to the filename, not including the
			file extension.</para>
			<para>These suffixes may also be overridden using the appropriate
			options.</para>
			<para>By default, a pause is played between groups of digits. The
			duration of the pause can be configured. Additionally, if the file
			<literal>blank</literal> (or the configured name) exists in the
			provided directory, it will be used instead of a timed pause.</para>
			<para>Double or triple digits may also be provided, such
			as <literal>00</literal> or <literal>555</literal>. The file
			names <literal>thousand</literal> and <literal>hundred</literal>
			are also used if found and relevant.</para>
			<para>The application requires that vanilla numeric
			(up-inflection) digits exist. However, other files will
			be preferred if they would be a better match for enunciating
			particular digits, but the application will gracefully fall
			back if they do not exist. However, all files must exist
			for the proper inflections to be used.</para>
			<para>Currently only supports North American (NANPA)
			7 or 10-digit numbers.</para>
			<para>This application does not automatically answer the
			channel and should be preceded by <literal>Answer</literal>
			or <literal>Progress</literal> as appropriate.</para>
			<example title="Simple ANAC using built-in prompts">
			same => n,SayTelephoneNumber(digits,${CALLERID(num)})
			</example>
		</description>
		<see-also>
			<ref type="application">SayDigits</ref>
			<ref type="application">SayNumber</ref>
		</see-also>
	</application>
 ***/

static char *app = "SayTelephoneNumber";

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
		if ((allowalpha && isalnum(str[i])) || isdigit(str[i])) {
			new[j++] = str[i];
			if (length) {
				(*length)++;
			}
		}
	}
	return new;
}

enum say_option_flags {
	OPT_TRIPLE    = (1 << 0),
	OPT_MCI       = (1 << 1),
	OPT_NORMAL    = (1 << 2),
	OPT_UP        = (1 << 3),
	OPT_DOUBLEUP  = (1 << 4),
	OPT_BLANK     = (1 << 5),
	OPT_PREPLAY   = (1 << 6),
};

enum {
	OPT_ARG_NORMAL,
	OPT_ARG_UP,
	OPT_ARG_DOUBLEUP,
	OPT_ARG_BLANK,
	OPT_ARG_PREPLAY,
	/* Must be the last element */
	OPT_ARG_ARRAY_SIZE,
};

AST_APP_OPTIONS(say_app_options, {
	AST_APP_OPTION_ARG('b', OPT_BLANK, OPT_ARG_BLANK),
	AST_APP_OPTION('m', OPT_MCI),
	AST_APP_OPTION_ARG('n', OPT_NORMAL, OPT_ARG_NORMAL),
	AST_APP_OPTION_ARG('p', OPT_PREPLAY, OPT_ARG_PREPLAY),
	AST_APP_OPTION('t', OPT_TRIPLE),
	AST_APP_OPTION_ARG('u', OPT_UP, OPT_ARG_UP),
	AST_APP_OPTION_ARG('v', OPT_DOUBLEUP, OPT_ARG_DOUBLEUP),
});

static int saytelnum_exec(struct ast_channel *chan, const char *data)
{
#define DEFAULT_WAITMS 750
#define NORMAL_SUFFIX "" /* lowest pitch and last resort */
#define UP_SUFFIX "_" /* used for most digits */
#define DOUBLE_UP_SUFFIX "__" /* highest pitch */
#define BLANK "blank"
	int d, dirlen, res = 0, triple = 0, triplemci = 0, addlen = 0, waitms = DEFAULT_WAITMS;
	char *preplay = NULL;
	char *tmp, *num, *orignum, *file;
	double sec;
	char *normalsuffix = NORMAL_SUFFIX, *doubleupsuffix = DOUBLE_UP_SUFFIX, *blank = BLANK, *upsuffix = UP_SUFFIX;
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
		ast_log(LOG_WARNING, "SayTelephoneNumber requires a directory\n");
		return -1;
	}
	if (ast_strlen_zero(args.number)) {
		ast_log(LOG_WARNING, "SayTelephoneNumber requires a number\n");
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
	addlen = 8; /* length of thousand, the longest word */
	if (!ast_strlen_zero(args.options)) {
		int x;
		ast_app_parse_options(say_app_options, &flags, optargs, args.options);
		if (ast_test_flag(&flags, OPT_TRIPLE)) {
			triple = 1; /* use triple inflection mode instead of double inflection mode */
		}
		if (ast_test_flag(&flags, OPT_MCI)) {
			triplemci = 1; /* use triple inflection mode instead of double inflection mode */
		}
		if (ast_test_flag(&flags, OPT_BLANK) && !ast_strlen_zero(optargs[OPT_ARG_BLANK])) {
			blank = optargs[OPT_ARG_BLANK];
			if ((x = strlen(blank)) > addlen) {
				addlen = x;
			}
		}
		if (ast_test_flag(&flags, OPT_PREPLAY) && !ast_strlen_zero(optargs[OPT_ARG_PREPLAY])) {
			preplay = optargs[OPT_ARG_PREPLAY];
			if ((x = strlen(preplay)) > addlen) {
				addlen = x;
			}
		}
		/* these could be set to empty, so strlen zero is okay */
		if (ast_test_flag(&flags, OPT_NORMAL)) {
			normalsuffix = optargs[OPT_ARG_NORMAL];
			if ((x = strlen(normalsuffix)) >= addlen) { /* already a digit, so we have one less char than the buffer remaining */
				addlen = x + 1; /* the above comment is equally applicable to this line */
			}
		}
		if (ast_test_flag(&flags, OPT_UP)) {
			upsuffix = optargs[OPT_ARG_UP];
			if ((x = strlen(upsuffix)) >= addlen) { /* already a digit, so we have one less char than the buffer remaining */
				addlen = x + 1; /* the above comment is equally applicable to this line */
			}
		}
		if (ast_test_flag(&flags, OPT_DOUBLEUP)) {
			doubleupsuffix = optargs[OPT_ARG_DOUBLEUP];
			if ((x = strlen(doubleupsuffix)) >= addlen) { /* already a digit, so we have one less char than the buffer remaining */
				addlen = x + 1; /* the above comment is equally applicable to this line */
			}
		}
	}

	num = filter_number(args.number, &d, 1);
	orignum = num;
	dirlen = strlen(args.directory);

	if (args.directory[dirlen - 1] == '/') { /* guaranteed to be at least 1 by here */
		args.directory[dirlen - 1] = '\0'; /* discard trailing / in directory */
	}

	dirlen += (addlen + 2); /* dir + '/' + file (max len 8) + null terminator */
	file = ast_malloc(dirlen);

	/* do not automatically answer the channel (seriously, do it explicitly if you want it) */
	ast_stopstream(chan);

	while (*num && !res) { /* and d > 0, but that should be implicit */
		int n = 1, groupend = 0;
		char *prefinflection = NULL;

		if (preplay && *preplay) {
			snprintf(file, dirlen, "%s/%s", args.directory, preplay); /* thousand */
			if (ast_fileexists(file, NULL, NULL)) {
				res = ast_streamfile(chan, file, ast_channel_language(chan));
				if (!res) {
					res = ast_waitstream(chan, "");
					ast_stopstream(chan);
				}
				if (res) {
					goto next;
				}
			}
		}

		if (d == 3 && num[0] == '0' && num[1] == '0' && num[2] == '0') {
			snprintf(file, dirlen, "%s/%s", args.directory, "thousand"); /* thousand */
			if (ast_fileexists(file, NULL, NULL)) {
				n = 3;
				res = ast_streamfile(chan, file, ast_channel_language(chan));
				if (!res) {
					res = ast_waitstream(chan, "");
					ast_stopstream(chan);
				}
				goto next;
			}
		}
		if (d == 9 && num[0] == '0' && num[1] == '0') { /* e.g. "800" toll-free area code */
			snprintf(file, dirlen, "%s/%s", args.directory, "hundred"); /* hundred */
			if (ast_fileexists(file, NULL, NULL)) {
				n = 2;
				res = ast_streamfile(chan, file, ast_channel_language(chan));
				if (!res) {
					res = ast_waitstream(chan, "");
					ast_stopstream(chan);
				}
				goto next;
			}
		}
		if (d == 7 && isalpha(num[0]) && isalpha(num[1])) { /* exchange name for 2L+5N */
			snprintf(file, dirlen, "%s/%c%c", args.directory, num[0], num[1]);
			if (ast_fileexists(file, NULL, NULL)) {
				n = 2;
				res = ast_streamfile(chan, file, ast_channel_language(chan));
				if (!res) {
					res = ast_waitstream(chan, "");
					ast_stopstream(chan);
				}
				goto next;
			}
		}
		if ((d == 3 || d == 4 || d == 7 || d == 10 || d >= 13) && num[0] == num[1] && num[1] == num[2]) { /* triple-digits */
			snprintf(file, dirlen, "%s/%c%c%c", args.directory, num[0], num[1], num[2]);
			if (ast_fileexists(file, NULL, NULL)) {
				n = 3;
				res = ast_streamfile(chan, file, ast_channel_language(chan));
				if (!res) {
					res = ast_waitstream(chan, "");
					ast_stopstream(chan);
				}
				goto next;
			}
		}
		if (d >= 2 && d != 5 && d != 8 && num[0] == num[1] && (d != 3 || num[1] != num[2])) { /* double-digits */
			/* the last bit above is if we have something like 1555, we should prefer "one, five, double-five over one, double-five, five */
			snprintf(file, dirlen, "%s/%c%c", args.directory, num[0], num[1]);
			if (ast_fileexists(file, NULL, NULL)) {
				n = 2;
				res = ast_streamfile(chan, file, ast_channel_language(chan));
				if (!res) {
					res = ast_waitstream(chan, "");
					ast_stopstream(chan);
				}
				goto next;
			}
		}

		/*
		N = Normal
		U = Up
		V = Double Up
			d:					   1098-765-4321
			Double Inflection:		UUN-UUN-UUUN
			Triple Inflection:		VUN-VUN-VUUN
			MCI Triple Inflection:	UUV-UUV-UVUN
		*/

		/*! \todo This only supports NANPA numbers (basically of 7 or 10 digits). In the future, possibly allow user to somehow specify digits requiring different inflections */
		prefinflection = upsuffix;
		if (d == 1) {
			prefinflection = normalsuffix;
		} else if (triplemci && (d == 8 || d == 5 || d == 3)) { /* Triple inflection MCI mode */
			prefinflection = doubleupsuffix;
		} else if (triple) { /* Triple inflection */
			if (d == 10 || d == 7 || d == 4) {
				prefinflection = doubleupsuffix;
			} else if (d == 8 || d == 5) {
				prefinflection = normalsuffix;
			}
		} else if (d == 8 || d == 5) { /* double inflection */
			prefinflection = normalsuffix;
		}

		/* single digit */
		if (prefinflection != normalsuffix) { /* yes, we are doing a pointer comparison. We don't need strcmp */
			/* use different inflection on the right digits for more authentic sound, if possible */
			snprintf(file, dirlen, "%s/%c%s", args.directory, num[0], prefinflection);
			if (ast_fileexists(file, NULL, NULL)) {
				res = ast_streamfile(chan, file, ast_channel_language(chan));
				if (!res) {
					res = ast_waitstream(chan, "");
					ast_stopstream(chan);
				}
				goto next;
			}
		}
		/* last resort, fallback to normal (lowest) pitch */
		snprintf(file, dirlen, "%s/%c%s", args.directory, num[0], normalsuffix);
		if (ast_fileexists(file, NULL, NULL)) {
			res = ast_streamfile(chan, file, ast_channel_language(chan));
			if (!res) {
				res = ast_waitstream(chan, "");
				ast_stopstream(chan);
			}
			goto next;
		}
		ast_log(LOG_WARNING, "Unable to play digit '%c' in '%s' (missing file of last resort '%s')\n", num[0], orignum, file);
		/* we'll continue, instead of bailing out, but this won't be pretty... */
next:
		num += n;
		d -= n;
		groupend = (d == 7 || d == 4); /* don't pause after last digit (or the 3rd-to-last, for that matter, in triple inflection mode), but do pause inbetween */
		if (!res && groupend) { /* pause between area code, exchange, station number, etc. */
			snprintf(file, dirlen, "%s/%s", args.directory, blank);
			ast_debug(1, "Looking for blank file '%s'\n", file);
			if (*blank && ast_fileexists(file, NULL, NULL)) {
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
	return ast_register_application_xml(app, saytelnum_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Enunciate Telephone Number Application");
