/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2021, Naveen Albert
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
 * \brief Dialplan extension evaluation function
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup functions
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/utils.h"
#include "asterisk/app.h"

/*** DOCUMENTATION
	<function name="EVAL_EXTEN" language="en_US">
		<synopsis>
			Evaluates the contents of a dialplan extension and returns it as a string.
		</synopsis>
		<syntax>
			<parameter name="context" />
			<parameter name="extensions" />
			<parameter name="priority" required="true" />
		</syntax>
		<description>
			<para>The EVAL_EXTEN function looks up a dialplan entry by context,extension,priority,
			evaluates the contents of a Return statement to resolve any variable or function
			references, and returns the result as a string.</para>
			<para>You can use this function to create simple user-defined lookup tables or
			user-defined functions.</para>
			<example title="Custom dialplan functions">
			[call-types]
			exten => _1NNN,1,Return(internal)
			exten => _NXXNXXXXXX,1,Return(external)

			[udf]
			exten => calleridlen,1,Return(${LEN(${CALLERID(num)})})

			[default]
			exten => _X!,1,Verbose(Call type ${EVAL_EXTEN(call-types,${EXTEN},1)} - ${EVAL_EXTEN(udf,calleridlen,1)})
			</example>
			<para>Any variables in the evaluated data will be resolved in the context of
			that extension. For example, <literal>${EXTEN}</literal> would refer to the
			EVAL_EXTEN extension, not the extension in the context invoking the function.
			This behavior is similar to other applications, e.g. <literal>Gosub</literal>.</para>
			<example title="Choosing which prompt to use">
			same => n,Read(input,${EVAL_EXTEN(prompts,${CALLERID(num)},1)})

			[prompts]
			exten => _X!,1,Return(default)
			exten => _20X,1,Return(welcome)
			exten => _2XX,1,Return(${DB(promptsettings/${EXTEN})})
			exten => _3XX,1,Return(${ODBC_MYFUNC(${EXTEN})})
			</example>
			<para>Extensions on which EVAL_EXTEN is invoked are not different from other
			extensions. However, the application at that extension is not executed.
			Only the application data is parsed and evaluated.</para>
			<para>A limitation of this function is that the application at the specified
			extension isn't actually executed, and thus unlike a Gosub, you can't pass
			arguments in the EVAL_EXTEN function.</para>
		</description>
		<see-also>
			<ref type="function">EVAL</ref>
		</see-also>
	</function>
 ***/

static int get_extension_data(char *name, int namesize, struct ast_channel *c,
	const char *context, const char *exten, int priority)
{
	struct ast_exten *e;
	struct pbx_find_info q = { .stacklen = 0 }; /* the rest is set in pbx_find_context */
	ast_rdlock_contexts();
	e = pbx_find_extension(c, NULL, &q, context, exten, priority, NULL, "", E_MATCH);
	ast_unlock_contexts();
	if (e) {
		if (name) {
			const char *tmp = ast_get_extension_app_data(e);
			if (tmp) {
				ast_copy_string(name, tmp, namesize);
			}
		}
		return 0;
	}
	return -1;
}

static int eval_exten_read(struct ast_channel *chan, const char *cmd, char *data, char *buf, size_t len)
{
	char *exten, *pri, *context, *parse;
	int ipri;
	const char *realcontext, *realexten;
	int realpriority;
	char tmpbuf[len];

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "The EVAL_EXTEN function requires an extension\n");
		return -1;
	}

	parse = ast_strdupa(data);
	/* Split context,exten,pri */
	context = strsep(&parse, ",");
	exten = strsep(&parse, ",");
	pri = strsep(&parse, ",");
	if (!exten) {
		/* Only a priority in this one */
		pri = context;
		exten = NULL;
		context = NULL;
	} else if (!pri) {
		/* Only an extension and priority in this one */
		pri = exten;
		exten = context;
		context = NULL;
	}
	if (sscanf(pri, "%30d", &ipri) != 1) {
		ipri = ast_findlabel_extension(chan, context ? context : ast_channel_context(chan),
			exten ? exten : ast_channel_exten(chan), pri,
			S_COR(ast_channel_caller(chan)->id.number.valid, ast_channel_caller(chan)->id.number.str, NULL));
		if (ipri < 1) {
			ast_log(LOG_WARNING, "Priority '%s' must be a number > 0, or valid label\n", pri);
			return -1;
		}
	}

	ast_channel_lock(chan);
	realcontext = ast_strdupa(ast_channel_context(chan));
	realexten = ast_strdupa(ast_channel_exten(chan));
	realpriority = ast_channel_priority(chan);
	ast_channel_unlock(chan);
	if (ast_strlen_zero(exten)) {
		exten = ast_strdupa(realexten);
	}
	if (ast_strlen_zero(context)) {
		context = ast_strdupa(realcontext);
	}

	if (get_extension_data(tmpbuf, len, chan, context, exten, ipri)) {
		return -1; /* EVAL_EXTEN failed */
	}
	/* Substitute variables now, using the location of the evaluated extension */
	/* strdupa required or we'll just overwrite what we read when we set these */
	ast_channel_lock(chan);
	ast_channel_context_set(chan, context);
	ast_channel_exten_set(chan, exten);
	ast_channel_priority_set(chan, ipri);
	ast_channel_unlock(chan);

	pbx_substitute_variables_helper(chan, tmpbuf, buf, len);

	ast_channel_lock(chan);
	ast_channel_context_set(chan, realcontext);
	ast_channel_exten_set(chan, realexten);
	ast_channel_priority_set(chan, realpriority);
	ast_channel_unlock(chan);

	return 0;
}

static struct ast_custom_function eval_exten_function = {
	.name = "EVAL_EXTEN",
	.read = eval_exten_read,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&eval_exten_function);
}

static int load_module(void)
{
	return ast_custom_function_register(&eval_exten_function);
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Extension evaluation function");
