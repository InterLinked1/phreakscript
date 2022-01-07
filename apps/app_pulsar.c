/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2006, Russ Price
 * Copyright (C) 2022, Naveen Albert
 *
 * Russ Price <kxt@fubegra.net>
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

/*
 * This moudle is a rewrite of Russ Price's app_rpsim (from 2006
 * for Asterisk 1.4) and pulsar.agi (with multi-protocol support).
 */

/*! \file
 *
 * \brief Simulated revertive pulse signaling
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

/*** DOCUMENTATION
	<application name="RevertivePulse" language="en_US">
		<synopsis>
			Simulates revertive pulsing for a 4-digit number.
		</synopsis>
		<syntax>
			<parameter name="digits" required="yes">
				<para>The 4-digit station number.</para>
			</parameter>
			<parameter name="switch" required="no">
				<para>The type of pulsing to use.</para>
				<para>Valid options are <literal>panel</literal>, <literal>1xb</literal>,
				and <literal>5xb</literal>.</para>
				<para>Default is <literal>panel</literal>.</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="b">
						<para>Adds 4 pulses to the second revertive pulse digit
						to simulate B-side exchange.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Mimics the revertive pulsing sounds characteristic of 
			Panel, Number 1 Crossbar, or Number 5 Crossbar pulsing.</para>
			<para>This application uses material sourced from Evan Doorbell
			tapes. The <literal>pulsar</literal> sounds directory should be
			placed directly into the main sounds directory.</para>
			<para>Sounds can be obtained from the original tarball
			at <literal>https://octothorpe.info/site/pulsar</literal>.</para>
			<para>This application does not automatically answer the channel
			and should be preceded by <literal>Progress</literal>
			or <literal>Answer</literal> as appropriate.</para>
		</description>
	</application>
 ***/

enum pulsar_option_flags {
	OPT_BSIDE = (1 << 0),
};

AST_APP_OPTIONS(pulsar_option_flags, {
	AST_APP_OPTION('b', OPT_BSIDE),
});

static const char *app = "RevertivePulse";

static int valid_rp_digits(char *digits)
{
	int i;
	for (i = 0; i < 4; i++) {
		if (!digits[i] || !isdigit(digits[i])) {
			return 0;
		}
	}
	return (!digits[i]); /* there shouldn't be anything left */
}

static void translate(int number, int b_side, int *data)
{
	if( number > 9999) {
		number %= 10000; /* should never happen */
	}
	data[0] = (number / 2000) + 1;
	data[1] = (number % 2000) / 500 + (b_side ? 4 : 0) + 1;
	data[2] = (number % 500) / 100 + 1;
	data[3] = (number % 100) / 10 + 1;
	data[4] = (number % 10) + 1;
}

#define BUFFER_SIZE 21 /* pulsar/panel/middleX is the longest possible string: 20 chars + null terminator = 21 */

static int pulsar_stream(struct ast_channel *chan, char *file)
{
	int res;

	res = ast_streamfile(chan, file, ast_channel_language(chan));
	if (!res) {
		res = ast_waitstream(chan, "");
		ast_stopstream(chan);
	} else {
		ast_log(LOG_WARNING, "Failed to play revertive pulse file '%s'\n", file);
	}
	return res;
}

static int pulsar_exec(struct ast_channel *chan, const char *data)
{
	char *argcopy = NULL, *protocol = "panel";
	struct ast_flags flags = {0};
	char buf[BUFFER_SIZE];
	int digit, res, bside = 0;
	int translated[5];

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(digits);
		AST_APP_ARG(type);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "%s requires an argument (variable)\n", app);
		return -1;
	}

	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, argcopy);

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(pulsar_option_flags, &flags, NULL, args.options);
		if (ast_test_flag(&flags, OPT_BSIDE)) {
			bside = 1;
		}
	}

	if (ast_strlen_zero(args.digits)) {
		ast_log(LOG_WARNING, "%s requires digits\n", app);
		return 0; /* RP isn't critical, it's just ear candy, so don't hang up */
	}
	if (!valid_rp_digits(args.digits)) {
		ast_log(LOG_WARNING, "Digits '%s' invalid for revertive pulsing protocol\n", args.digits);
		return 0;
	}
	if (!ast_strlen_zero(args.type)) {
		if (!strcasecmp(args.type, "1xb")) {
			protocol = "1xb";
		} else if (!strcasecmp(args.type, "5xb")) {
			protocol = "5xb";
		} else if (strcasecmp(args.type, "panel")) {
			ast_log(LOG_WARNING, "Invalid RP protocol: '%s'. Defaulting to panel.\n", args.type);
		}
	}
	translate(atoi(args.digits), bside, translated);
	ast_stopstream(chan);

	ast_debug(1, "Simulating revertive pulsing digits '%s' with '%s' protocol\n", args.digits, protocol);
	snprintf(buf, BUFFER_SIZE, "pulsar/%s/begin", protocol);
	if (!ast_fileexists(buf, NULL, NULL)) {
		ast_log(LOG_WARNING, "Revertive pulse audio file '%s' does not exist, aborting\n", buf);
		return 0; /* if this file does not exist, the other ones probably don't, so bail out now */
	}
	if (pulsar_stream(chan, buf)) {
		return -1;
	}
	for (digit = 0; digit < 5; digit++) {
		int i, midpulses;

		snprintf(buf, BUFFER_SIZE, "pulsar/%s/start%d", protocol, digit);
		res = pulsar_stream(chan, buf);
		if (!res && translated[digit] > 1) {
			snprintf(buf, BUFFER_SIZE, "pulsar/%s/first%d", protocol, digit);
			res = pulsar_stream(chan, buf);
		}
		if (res) {
			return -1;
		}
		midpulses = translated[digit] - 2;
		snprintf(buf, BUFFER_SIZE, "pulsar/%s/middle%d", protocol, digit);
		for (i = 0; i < midpulses; i++) {
			if (pulsar_stream(chan, buf)) {
				return -1;
			}
		}
		snprintf(buf, BUFFER_SIZE, "pulsar/%s/last%d", protocol, digit);
		if (pulsar_stream(chan, buf)) {
			return -1;
		}
	}
	snprintf(buf, BUFFER_SIZE, "pulsar/%s/end", protocol);
	res = pulsar_stream(chan, buf);

	return res;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, pulsar_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Simulated Revertive Pulsing");
