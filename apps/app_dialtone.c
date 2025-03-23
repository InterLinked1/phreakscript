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
 * \brief Customer dial pulse receiver (CDPR)
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
#include "asterisk/app.h"
#include "asterisk/module.h"
#include "asterisk/indications.h"

/*** DOCUMENTATION
	<application name="DialTone" language="en_US">
		<synopsis>
			Reads a telephone number from a user digit by digit, terminating dialing against a digit map.
		</synopsis>
		<syntax>
			<parameter name="variable" required="true">
				<para>The input digits will be stored in the given <replaceable>variable</replaceable>
				name.</para>
			</parameter>
			<parameter name="context" required="true">
				<para>Context to use to as the digit map for this dial tone. The digit map
				context is a dialplan context with extensions (including pattern matches)
				that should return a non-zero value to conclude dialing on a match. Returning
				0 will continue dialing.</para>
			</parameter>
			<parameter name="filenames" argsep="&amp;">
				<argument name="filename" required="true">
					<para>file(s) to play before reading first digit or tone with option i</para>
				</argument>
				<argument name="filename2" multiple="true" />
			</parameter>
			<parameter name="filenames2" argsep="&amp;">
				<argument name="filename" required="true">
					<para>file(s) to play before reading subsequent digits or tone with option i</para>
				</argument>
				<argument name="filename2" multiple="true" />
			</parameter>
			<parameter name="maxdigits">
				<para>Maximum acceptable number of digits. Stops reading after
				<replaceable>maxdigits</replaceable> have been entered (without
				requiring the user to press the <literal>#</literal> key).</para>
				<para>Defaults to <literal>0</literal> - no limit - wait for the
				user press the <literal>#</literal> key. Any value below
				<literal>0</literal> means the same. Max accepted value is
				<literal>255</literal>. This overrides the digit map, and will terminate
				dialing even if there is no match in the digit map context.</para>
			</parameter>
			<parameter name="timeout">
				<para>The number of seconds to wait for a digit response. If greater
				than <literal>0</literal>, that value will override the default timeout.
				Can be floating point.</para>
			</parameter>
			<parameter name="leading">
				<para>Leading digits that should be used to initialize the number dialed.
				Useful for second dial tones or when additional digits need to be
				received and use the same digit map.</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="d">
						<para>Echo back digits to caller as they are entered, as DTMF.</para>
					</option>
					<option name="i">
						<para>to play <literal>filename</literal> as an indication tone from your
						<filename>indications.conf</filename>.</para>
					</option>
					<option name="m">
						<para>Echo back digits to caller as they are entered, as MF.</para>
					</option>
					<option name="p">
						<para>Parse digit map by performing variable substitution.</para>
					</option>
					<option name="r">
						<para>Reevaluate the variable substitution of
						<replaceable>filename2</replaceable> each digit
						(for all subsequent digits).</para>
					</option>
					<option name="t">
						<para>Terminate dialing using the <literal>#</literal> key.
						By default, the <literal>#</literal> key is not treated specially
						and will not terminate input.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Reads a telephone number from a user into the
			given <replaceable>variable</replaceable>. Dialing
			concludes once an extension match is found in <literal>context</literal>
			that returns a positive number.</para>
			<para>If <literal>context</literal> returns a negative number, the absolute
			value will be used to wait in silence for an additional digit. For example,
			if the digit map returns -5, the application will wait 5 seconds in silence
			for an additional digit and complete if it does not receive any.</para>
			<para>This application does not automatically answer the channel and should
			be preceded by <literal>Progress</literal> or <literal>Answer</literal>.</para>
			<example title="Simulated city dial tone">
			same => n,DialTone(num,my-digit-map,custom/dialtone,custom/dialsounds/sound${RAND(1,6)},32,,,pr)
			</example>
			<example title="Precise dial tone with 5 second interdigit timeout">
			same => n,DialTone(num,my-digit-map,dial,,32,5,,ip)
			</example>
			<example title="Terminate dialing with # key">
			same => n,DialTone(num,my-digit-map,dial,,32,10,,ipt)
			</example>
		</description>
		<see-also>
			<ref type="application">DISA</ref>
			<ref type="application">Read</ref>
			<ref type="application">ReadExten</ref>
		</see-also>
	</application>
 ***/

#define BUFFER_LEN 256

enum dialtone_option_flags {
	OPT_INDICATION = (1 << 0),
	OPT_REEVALUATE = (1 << 1),
	OPT_TERMINATE =  (1 << 2),
	OPT_PARSE =      (1 << 3),
	OPT_ECHO_DTMF =  (1 << 4),
	OPT_ECHO_MF =    (1 << 5),
};

AST_APP_OPTIONS(dialtone_app_options, {
	AST_APP_OPTION('d', OPT_ECHO_DTMF),
	AST_APP_OPTION('i', OPT_INDICATION),
	AST_APP_OPTION('m', OPT_ECHO_MF),
	AST_APP_OPTION('p', OPT_PARSE),
	AST_APP_OPTION('r', OPT_REEVALUATE),
	AST_APP_OPTION('t', OPT_TERMINATE),
});

static char *app = "DialTone";

static int digit_map_match(struct ast_channel *chan, char *context, char *digits, int parse)
{
	char tmpbuf[BUFFER_LEN];
	int pri = 1, res;

	if (ast_get_extension_data(tmpbuf, BUFFER_LEN, chan, context, digits, pri)) {
		ast_debug(1, "Digit map result: could not find extension %s in context %s\n", digits, context);
		return 0; /* if extension DOESN'T exist, then definitely not a match */
	}
	if (parse) {
		char buf[BUFFER_LEN];

		pbx_substitute_variables_helper_full_location(chan, (chan) ? ast_channel_varshead(chan) : NULL, tmpbuf, buf, BUFFER_LEN, NULL, context, digits, pri);

		res = atoi(buf);
		ast_debug(1, "Substituted %s -> %s -> %d\n", tmpbuf, buf, res);
	} else {
		res = atoi(tmpbuf);
	}
	/* if extension does exist, it MAY be a match */
	ast_debug(1, "Digit map result: %d\n", res);
	return res;
}

static int dialtone_exec(struct ast_channel *chan, const char *data)
{
	int res = 0;
	char tmp[256] = "";
	int maxdigits = 255;
	int to = 0, x = 0;
	double tosec;
	char *argcopy = NULL;
	struct ast_tone_zone_sound *ts = NULL;
	struct ast_flags flags = {0};
	int done = 0;
	char *tmpdigit, *subsequentaudio = NULL;
	char *terminator = "";
	int parse = 0;
	int echodtmf = 0, echomf = 0;

	AST_DECLARE_APP_ARGS(arglist,
		AST_APP_ARG(variable);
		AST_APP_ARG(context);
		AST_APP_ARG(filename);
		AST_APP_ARG(filename2);
		AST_APP_ARG(maxdigits);
		AST_APP_ARG(timeout);
		AST_APP_ARG(leading);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "DialTone requires an argument (variable)\n");
		return -1;
	}

	argcopy = ast_strdupa(data);

	AST_STANDARD_APP_ARGS(arglist, argcopy);

	if (ast_strlen_zero(arglist.variable)) {
		ast_log(LOG_WARNING, "Invalid! Usage: DialTone(variable,context[,initialfilename][,subsequentfilename][,timeout][,options])\n\n");
		return -1;
	}
	if (!ast_strlen_zero(arglist.context) && !ast_context_find(arglist.context)) {
		ast_log(LOG_ERROR, "Dialplan context '%s' does not exist\n", arglist.context);
		return -1;
	}
	if (!ast_strlen_zero(arglist.maxdigits)) {
		maxdigits = atoi(arglist.maxdigits);
		if ((maxdigits < 1) || (maxdigits > 255)) {
			maxdigits = 255;
		} else {
			ast_verb(5, "Accepting a maximum of %d digits.\n", maxdigits);
		}
	}
	if (!ast_strlen_zero(arglist.options)) {
		ast_app_parse_options(dialtone_app_options, &flags, NULL, arglist.options);
	}

	if (!ast_strlen_zero(arglist.timeout)) {
		tosec = atof(arglist.timeout);
		if (tosec <= 0) {
			to = 0;
		} else {
			to = tosec * 1000.0;
		}
	}
	if (ast_strlen_zero(arglist.filename)) {
		arglist.filename = NULL;
	}
	if (!ast_strlen_zero(arglist.filename2)) {
		subsequentaudio = arglist.filename2;
	}
	if (!ast_strlen_zero(arglist.leading)) {
		ast_copy_string(tmp, arglist.leading, BUFFER_LEN);
		x = strlen(arglist.leading);
	}
	if (ast_test_flag(&flags, OPT_TERMINATE)) {
		terminator = "#";
	}
	if (ast_test_flag(&flags, OPT_PARSE)) {
		parse = 1;
	}
	if (ast_test_flag(&flags, OPT_ECHO_DTMF)) {
		echodtmf = 1;
	}
	if (ast_test_flag(&flags, OPT_ECHO_MF)) {
		echomf = 1;
	}
	if (ast_test_flag(&flags, OPT_REEVALUATE)) {
		const char *context = NULL, *exten = NULL;
		int ipri;
		char tmpbuf[BUFFER_LEN];
		/* this is capable of parsing the exten data properly */
		AST_DECLARE_APP_ARGS(extendata,
			AST_APP_ARG(variable);
			AST_APP_ARG(context);
			AST_APP_ARG(filename);
			AST_APP_ARG(filename2);
			AST_APP_ARG(maxdigits);
			AST_APP_ARG(timeout);
			AST_APP_ARG(leading);
			AST_APP_ARG(options);
		);

		ast_channel_lock(chan);
		context = ast_channel_context(chan);
		exten = ast_strdupa(ast_channel_exten(chan));
		ipri = ast_channel_priority(chan);
		ast_channel_unlock(chan);

		if (ast_get_extension_data(tmpbuf, BUFFER_LEN, chan, context, exten, ipri)) {
			ast_log(LOG_WARNING, "Cannot reevaluate audio: %s\n", arglist.filename2);
			return -1;
		}

		AST_STANDARD_APP_ARGS(extendata, tmpbuf);

		subsequentaudio = ast_strdupa(extendata.filename2); /* this is what we wanted */
	}
	if (!to && ast_strlen_zero(arglist.filename)) {
		ast_log(LOG_WARNING, "No timeout provided, but no initial filename specified. This is unlikely to be the desired behavior!\n");
	}
	if (!to && ast_strlen_zero(subsequentaudio)) {
		ast_log(LOG_WARNING, "No timeout provided, but no subsequent filename(s) specified. This is unlikely to be the desired behavior!\n");
	}
	ast_stopstream(chan);
	if (x && digit_map_match(chan, arglist.context, tmp, parse) > 0) { /* leading digits were passed in, so check the digit map before doing anything */
		done = 1;
	}
	if (!done) {
		int timeoutoverride = 0;
		do {
			if (ts) {
				ts = ast_tone_zone_sound_unref(ts); /* unref first ts, ref second ts (etc.) */
				/* note, ts will be null here, there is no need to manually set it to NULL */
			}
			if (!ts && ast_test_flag(&flags, OPT_INDICATION)) {
				if ((!x && !ast_strlen_zero(arglist.filename)) || (x && !ast_strlen_zero(subsequentaudio))) {
					ts = ast_get_indication_tone(ast_channel_zone(chan), x ? subsequentaudio : arglist.filename);
				}
			}
			if (ts && ((!x && !ast_strlen_zero(arglist.filename)) || (x && !ast_strlen_zero(subsequentaudio)))) { /* read using indications */
				res = ast_playtones_start(chan, 0, ts->data, 0);
				res = ast_waitfordigit(chan, to ? to : (ast_channel_pbx(chan) ? ast_channel_pbx(chan)->rtimeoutms : 6000));
				ast_playtones_stop(chan);
				if (res < 1) {
					tmp[x] = '\0';
					done = 1;
				} else {
					tmp[x++] = res;
					if (strchr(terminator, tmp[x-1])) {
						ast_debug(1, "Received terminator, ending digit collection\n");
						tmp[x-1] = '\0';
						done = 1;
					}
				}
			} else if (timeoutoverride < 0) { /* wait in silence and see if there's another digit */
				tmpdigit = tmp + x;
				/* if digit map returned digit < 0, that is the timeout, e.g. -5 means 5 second timeout to wait for further digits, and otherwise complete */
				res = ast_app_getdata_terminator(chan, "", tmpdigit, 1, timeoutoverride * -1000, terminator);
				if (res != AST_GETDATA_COMPLETE) {
					ast_debug(1, "Received terminator, ending digit collection\n");
					done = 1;
				}
				x++;
			} else { /* read using a file */
				char buf[BUFFER_LEN];
				char *audio = subsequentaudio;
				if (ast_test_flag(&flags, OPT_REEVALUATE)) {
					pbx_substitute_variables_helper(chan, subsequentaudio, buf, BUFFER_LEN);
					audio = buf;
				}
				tmpdigit = tmp + x;
				/* default timeout is NO timeout for audio files. But 0 does not mean 0 timeout, so get really close (1ms) */
				res = ast_app_getdata_terminator(chan, x ? audio : arglist.filename, tmpdigit, 1, to ? to : 1, terminator);
				if (res != AST_GETDATA_COMPLETE) {
					ast_debug(1, "Received terminator, ending digit collection\n");
					done = 1;
				}
				x++;
			}
			if (tmp[x - 1]) {
				tmpdigit = tmp + x - 1;
				ast_verb(4, "User dialed digit '%s'\n", tmpdigit);
			}
			if (!done && (echodtmf || echomf)) { /* if no digit dialed, don't echo */
				if (echodtmf) {
					ast_debug(1, "Echoing '%s' as DTMF\n", tmp + x - 1);
					res = ast_dtmf_stream(chan, NULL, tmp + x - 1, 250, 100);
				} else if (echomf) {
					ast_debug(1, "Echoing '%s' as MF\n", tmp + x - 1);
					res = ast_mf_stream(chan, NULL, NULL, tmp + x - 1, 250, 65, 120, 60, 0);
				}
				if (res < 0) {
					ast_debug(1, "Channel went away during digit echoing\n");
					return -1;
				}
			}
		} while (!done && ((timeoutoverride = digit_map_match(chan, arglist.context, tmp, parse)) <= 0) && x < maxdigits);
	}

	if (!ast_strlen_zero(tmp)) {
		ast_verb(3, "User dialed '%s'\n", tmp);
	} else {
		ast_verb(3, "User dialed nothing.\n");
	}

	pbx_builtin_setvar_helper(chan, arglist.variable, tmp);

	if (ts) {
		ts = ast_tone_zone_sound_unref(ts);
	}

	return res < 0 ? -1 : 0;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, dialtone_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Advanced dial tone application");
