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
 * \brief Dialplan assertion application
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
	<application name="Assert" language="en_US">
		<synopsis>
			Asserts that an expression is true.
		</synopsis>
		<syntax>
			<parameter name="expression">
				<para>The expression to evaluate.</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="d">
						<para>Do not hang up if expression is false.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Evaluates <replaceable>expression</replaceable> and continues dialplan
			execution if the expression is true and ends the call with a warning
			if it is false (unless the <replaceable>d</replaceable> option is provided).</para>
			<para>This application can be used to verify functional correctness
			of dialplans (e.g. dialplan test cases), similar to the <replaceable>assert</replaceable>
			function in C. For instance, if a certain property is expected to always hold at some
			point in your dialplan, this application can be used to enforce that.</para>
		</description>
		<see-also>
			<ref type="application">RaiseException</ref>
		</see-also>
	</application>
 ***/

enum option_flags {
	OPT_NOHANGUP = (1 << 0),
};

AST_APP_OPTIONS(app_options, {
	AST_APP_OPTION('d', OPT_NOHANGUP),
});

static const char *app = "Assert";

static int assert_exec(struct ast_channel *chan, const char *data)
{
	char *argcopy = NULL;
	struct ast_flags flags = {0};
	int value = 0;

	AST_DECLARE_APP_ARGS(arglist,
		AST_APP_ARG(expression);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "%s requires an argument (variable)\n", app);
		return -1;
	}

	argcopy = ast_strdupa(data);

	AST_STANDARD_APP_ARGS(arglist, argcopy);

	if (!ast_strlen_zero(arglist.options)) {
		ast_app_parse_options(app_options, &flags, NULL, arglist.options);
	}

	if (ast_strlen_zero(arglist.expression)) {
		ast_log(LOG_WARNING, "%s requires an argument (variable)\n", app);
		return -1;
	}

	value = atoi(arglist.expression); /* already parsed, so it should already be an integer anyways */

	if (!value) {
		AST_DECLARE_APP_ARGS(extendata,
			AST_APP_ARG(expr);
			AST_APP_ARG(rest);
		);
		const char *exprparse = NULL;
		char *datacopy = NULL;
		char *context, *exten;
		int priority;

		/* so, what was the expression? Let's find out */
		struct ast_exten *e;
		struct pbx_find_info q = { .stacklen = 0 }; /* the rest is set in pbx_find_context */

		ast_channel_lock(chan);
		context = ast_strdupa(ast_channel_context(chan));
		exten = ast_strdupa(ast_channel_exten(chan));
		priority = ast_channel_priority(chan);
		ast_channel_unlock(chan);

		ast_rdlock_contexts();
		e = pbx_find_extension(chan, NULL, &q, context, exten, priority, NULL, "", E_MATCH);
		ast_unlock_contexts();
		if (!e) {
			ast_log(LOG_WARNING, "Couldn't find current execution location\n"); /* should never happen */
			return -1;
		}
		exprparse = ast_get_extension_app_data(e);
		datacopy = ast_strdupa(exprparse);
		AST_STANDARD_APP_ARGS(extendata, datacopy);

		ast_log(LOG_WARNING, "Assertion failed at %s,%s,%d: %s\n", context, exten, priority, extendata.expr);

		if (!ast_test_flag(&flags, OPT_NOHANGUP)) {
			return -1;
		}
	}

	return 0;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, assert_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Dialplan assertion test application");
