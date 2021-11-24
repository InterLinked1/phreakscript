/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2021, Digium, Inc.
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
 * \brief Arbitrary channel access dialplan function
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup functions
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <regex.h>
#include <ctype.h>

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/bridge.h"
#include "asterisk/pbx.h"
#include "asterisk/utils.h"
#include "asterisk/app.h"
#include "asterisk/indications.h"
#include "asterisk/stringfields.h"

/*** DOCUMENTATION
	<function name="OTHER_CHANNEL" language="en_US">
		<synopsis>
			Gets or sets variables on any arbitrary channel that exists.
		</synopsis>
		<syntax>
			<parameter name="var" required="true">
				<para>Variable name</para>
			</parameter>
			<parameter name="channel" required="true">
				<para>The complete channel name: <literal>SIP/12-abcd1234</literal>.</para>
			</parameter>
		</syntax>
		<description>
			<para>Allows access to any existing channel if it exists. This is a potentially
			dangerous function if not used carefully, and the MASTER_CHANNEL and SHARED functions
			should be used instead if possible.</para>
		</description>
	</function>
 ***/

static int func_ochan_read(struct ast_channel *chan, const char *function,
			     char *data, struct ast_str **buf, ssize_t len)
{
	struct ast_channel *ochan;
	char *output = ast_alloca(4 + strlen(data));

	AST_DECLARE_APP_ARGS(args,
			     AST_APP_ARG(var);
			     AST_APP_ARG(channel);
	);
	AST_STANDARD_APP_ARGS(args, data);

	if (!args.channel) {
		ast_log(LOG_WARNING, "No channel was provided to %s function.\n", function);
		return -1;
	}
	ochan = ast_channel_get_by_name(args.channel);
	if (!ochan) {
		ast_log(LOG_ERROR, "Channel '%s' not found!  Variable '%s' will be blank.\n", args.channel, args.var);
		return -1;
	}
	if (!args.var) {
		ast_log(LOG_WARNING, "No variable name was provided to %s function.\n", function);
		return -1;
	}

	sprintf(output, "${%s}", data); /* SAFE */
	ast_str_substitute_variables(buf, len, ochan, output);
	if (ochan) {
		ast_channel_unref(ochan);
	}
	return 0;
}

static int func_ochan_write(struct ast_channel *chan, const char *function,
			      char *data, const char *value)
{
	struct ast_channel *ochan;

	AST_DECLARE_APP_ARGS(args,
			     AST_APP_ARG(var);
			     AST_APP_ARG(channel);
	);
	AST_STANDARD_APP_ARGS(args, data);

	if (!args.channel) {
		ast_log(LOG_WARNING, "No channel was provided to %s function.\n", function);
		return -1;
	}
	if (!(ochan = ast_channel_get_by_name(args.channel))) {
		ast_log(LOG_WARNING, "Channel '%s' not found!  Variable '%s' will be blank.\n", args.channel, args.var);
		return -1;
	}
	if (!args.var) {
		ast_log(LOG_WARNING, "No variable name was provided to %s function.\n", function);
		return -1;
	}

	pbx_builtin_setvar_helper(ochan, data, value);
	if (ochan) {
		ast_channel_unref(ochan);
	}
	return 0;
}

static struct ast_custom_function ochan_function = {
	.name = "OTHER_CHANNEL",
	.read2 = func_ochan_read,
	.write = func_ochan_write,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&ochan_function);
}

static int load_module(void)
{
	return ast_custom_function_register(&ochan_function);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Arbitrary channel access dialplan function");
