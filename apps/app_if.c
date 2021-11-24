/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright 2021, Naveen Albert <asterisk@phreaknet.org>
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
 * \brief If Branch Implementation
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/channel.h"

/*** DOCUMENTATION
	<application name="If" language="en_US">
		<synopsis>
			Start an if branch.
		</synopsis>
		<syntax>
			<parameter name="expr" required="true" />
		</syntax>
		<description>
			<para>Start an If branch.  Execution will continue inside the branch
			if expr is true.</para>
		</description>
		<see-also>
			<ref type="application">EndIf</ref>
			<ref type="application">ExitIf</ref>
		</see-also>
	</application>
	<application name="EndIf" language="en_US">
		<synopsis>
			End an if branch.
		</synopsis>
		<syntax />
		<description>
			<para>Ends the branch begun by the preceding <literal>If()</literal> application.</para>
		</description>
		<see-also>
			<ref type="application">If</ref>
			<ref type="application">ExitIf</ref>
		</see-also>
	</application>
	<application name="ExitIf" language="en_US">
		<synopsis>
			End an If branch.
		</synopsis>
		<syntax />
		<description>
			<para>Exits an <literal>If()</literal> branch, whether or not it has completed.</para>
		</description>
		<see-also>
			<ref type="application">If</ref>
			<ref type="application">EndIf</ref>
		</see-also>
	</application>
 ***/

static char *start_app = "If";
static char *stop_app = "EndIf";
static char *exit_app = "ExitIf";

#define VAR_SIZE 64


static const char *get_index(struct ast_channel *chan, const char *prefix, int idx) {
	char varname[VAR_SIZE];

	snprintf(varname, VAR_SIZE, "%s_%d", prefix, idx);
	return pbx_builtin_getvar_helper(chan, varname);
}

static struct ast_exten *find_matching_priority(struct ast_context *c, const char *exten, int priority, const char *callerid)
{
	struct ast_exten *e;
	struct ast_context *c2;
	int idx;

	for (e=ast_walk_context_extensions(c, NULL); e; e=ast_walk_context_extensions(c, e)) {
		if (ast_extension_match(ast_get_extension_name(e), exten)) {
			int needmatch = ast_get_extension_matchcid(e);
			if ((needmatch && ast_extension_match(ast_get_extension_cidmatch(e), callerid)) ||
				(!needmatch)) {
				/* This is the matching extension we want */
				struct ast_exten *p;
				for (p=ast_walk_extension_priorities(e, NULL); p; p=ast_walk_extension_priorities(e, p)) {
					if (priority != ast_get_extension_priority(p))
						continue;
					return p;
				}
			}
		}
	}

	/* No match; run through includes */
	for (idx = 0; idx < ast_context_includes_count(c); idx++) {
		const struct ast_include *i = ast_context_includes_get(c, idx);

		for (c2=ast_walk_contexts(NULL); c2; c2=ast_walk_contexts(c2)) {
			if (!strcmp(ast_get_context_name(c2), ast_get_include_name(i))) {
				e = find_matching_priority(c2, exten, priority, callerid);
				if (e)
					return e;
			}
		}
	}
	return NULL;
}

static int find_matching_endif(struct ast_channel *chan)
{
	struct ast_context *c;
	int res = -1;

	if (ast_rdlock_contexts()) {
		ast_log(LOG_ERROR, "Failed to lock contexts list\n");
		return -1;
	}

	for (c=ast_walk_contexts(NULL); c; c=ast_walk_contexts(c)) {
		struct ast_exten *e;

		if (!ast_rdlock_context(c)) {
			if (!strcmp(ast_get_context_name(c), ast_channel_context(chan))) {
				/* This is the matching context we want */
				int cur_priority = ast_channel_priority(chan) + 1, level=1;

				for (e = find_matching_priority(c, ast_channel_exten(chan), cur_priority,
					S_COR(ast_channel_caller(chan)->id.number.valid, ast_channel_caller(chan)->id.number.str, NULL));
					e;
					e = find_matching_priority(c, ast_channel_exten(chan), ++cur_priority,
						S_COR(ast_channel_caller(chan)->id.number.valid, ast_channel_caller(chan)->id.number.str, NULL))) {
					if (!strcasecmp(ast_get_extension_app(e), "IF")) {
						level++;
					} else if (!strcasecmp(ast_get_extension_app(e), "ENDIF")) {
						level--;
					}

					if (level == 0) {
						res = cur_priority;
						break;
					}
				}
			}
			ast_unlock_context(c);
			if (res > 0) {
				break;
			}
		}
	}
	ast_unlock_contexts();
	return res;
}

static int _if_exec(struct ast_channel *chan, const char *data, int end)
{
	int res=0;
	const char *if_pri = NULL;
	char *my_name = NULL;
	const char *condition = NULL, *label = NULL;
	char varname[VAR_SIZE], end_varname[VAR_SIZE + 4];
	const char *prefix = "IF";
	size_t size=0;
	int used_index_i = -1, x=0;
	char used_index[VAR_SIZE] = "0", new_index[VAR_SIZE] = "0";

	if (!chan) {
		return -1;
	}

	for (x=0;;x++) {
		if (get_index(chan, prefix, x)) {
			used_index_i = x;
		} else
			break;
	}

	snprintf(used_index, VAR_SIZE, "%d", used_index_i);
	snprintf(new_index, VAR_SIZE, "%d", used_index_i + 1);

	if (!end)
		condition = ast_strdupa(data);

	size = strlen(ast_channel_context(chan)) + strlen(ast_channel_exten(chan)) + 32;
	my_name = ast_alloca(size);
	memset(my_name, 0, size);
	snprintf(my_name, size, "%s_%s_%d", ast_channel_context(chan), ast_channel_exten(chan), ast_channel_priority(chan));

	ast_channel_lock(chan);
	if (end) {
		label = used_index;
	} else if (!(label = pbx_builtin_getvar_helper(chan, my_name))) {
		label = new_index;
		pbx_builtin_setvar_helper(chan, my_name, label);
	}
	snprintf(varname, VAR_SIZE + 4, "%s_%s", prefix, label);
	if ((if_pri = pbx_builtin_getvar_helper(chan, varname)) && !end) {
		if_pri = ast_strdupa(if_pri);
		snprintf(end_varname,VAR_SIZE + 4,"END_%s",varname);
	}
	ast_channel_unlock(chan);

	if ((!end && !pbx_checkcondition(condition)) || (end > 0)) {
		/* Condition Met (clean up helper vars) */
		const char *goto_str;
		pbx_builtin_setvar_helper(chan, varname, NULL);
		pbx_builtin_setvar_helper(chan, my_name, NULL);
		snprintf(end_varname,VAR_SIZE + 4,"END_%s",varname);
		ast_channel_lock(chan);
		if ((goto_str = pbx_builtin_getvar_helper(chan, end_varname))) {
			ast_parseable_goto(chan, goto_str);
			pbx_builtin_setvar_helper(chan, end_varname, NULL);
		} else {
			int pri = find_matching_endif(chan);
			if (pri > 0) {
				ast_verb(3, "Jumping to priority %d\n", pri);
				ast_channel_priority_set(chan, pri);
			} else if (end == 2) { /* Condition added because of end > 0 instead of end == 2 */
				ast_log(LOG_WARNING, "Couldn't find matching EndIf? (If at %s@%s priority %d)\n", ast_channel_context(chan), ast_channel_exten(chan), ast_channel_priority(chan));
			}
		}
		ast_channel_unlock(chan);
		return res;
	}

	if (!end && !if_pri) {
		char *goto_str;
		size = strlen(ast_channel_context(chan)) + strlen(ast_channel_exten(chan)) + 32;
		goto_str = ast_alloca(size);
		memset(goto_str, 0, size);
		snprintf(goto_str, size, "%s,%s,%d", ast_channel_context(chan), ast_channel_exten(chan), ast_channel_priority(chan));
		pbx_builtin_setvar_helper(chan, varname, goto_str);
	} else if (end && if_pri) {
		/* END of branch */
		snprintf(end_varname, VAR_SIZE + 4, "END_%s", varname);
		if (! pbx_builtin_getvar_helper(chan, end_varname)) {
			char *goto_str;
			size = strlen(ast_channel_context(chan)) + strlen(ast_channel_exten(chan)) + 32;
			goto_str = ast_alloca(size);
			memset(goto_str, 0, size);
			snprintf(goto_str, size, "%s,%s,%d", ast_channel_context(chan), ast_channel_exten(chan), ast_channel_priority(chan)+1);
			pbx_builtin_setvar_helper(chan, end_varname, goto_str);
		}
		ast_parseable_goto(chan, if_pri);
	}

	return res;
}

static int if_start_exec(struct ast_channel *chan, const char *data) {
	return _if_exec(chan, data, 0);
}

static int if_end_exec(struct ast_channel *chan, const char *data) {
	return _if_exec(chan, data, 1);
}

static int if_exit_exec(struct ast_channel *chan, const char *data) {
	return _if_exec(chan, data, 2);
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(start_app);
	res |= ast_unregister_application(stop_app);
	res |= ast_unregister_application(exit_app);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_register_application_xml(start_app, if_start_exec);
	res |= ast_register_application_xml(stop_app, if_end_exec);
	res |= ast_register_application_xml(exit_app, if_exit_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "If Branch and Conditional Execution");
