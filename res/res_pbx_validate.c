/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2022, 2024, Naveen Albert
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
 * \brief Dialplan application syntax and usage validation
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/paths.h"	/* use ast_config_AST_CONFIG_DIR */
#include "asterisk/file.h" /* use ast_file_read_dir */
#include "asterisk/cli.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/app.h" /* use AST_DECLARE_APP_ARGS */

/* This module contains code that was originally intended to be merged into pbx.c,
 * which is why some stuff is duplicated from pbx.c, and why callbacks
 * are defined for dialplan applications in this file, which is obviously not ideal.
 * However, the majority of the logic in this module does not need to be in the PBX core anyways. */

/* From pbx.c */
struct dialplan_counters {
	int total_items;
	int total_context;
	int total_exten;
	int total_prio;
	int context_existence;
	int extension_existence;
};

/* from pbx.c */
static char *complete_show_dialplan_context(const char *line, const char *word, int pos, int state)
{
	struct ast_context *c = NULL;
	char *ret = NULL;
	int which = 0;
	int wordlen;

	ast_rdlock_contexts();

	wordlen = strlen(word);

	/* walk through all contexts and return the n-th match */
	while ( (c = ast_walk_contexts(c)) ) {
		if (!strncasecmp(word, ast_get_context_name(c), wordlen) && ++which > state) {
			ret = ast_strdup(ast_get_context_name(c));
			break;
		}
	}

	ast_unlock_contexts();

	return ret;
}

/* basically a stripped down version of pbx_parse_location, from pbx.c */
static int pbx_parseable_location(char **label, char **context, char **exten, char **pri, int *ipri)
{
	*context = *exten = *pri = NULL;
	/* Split context,exten,pri */
	*context = strsep(label, ",");
	*exten = strsep(label, ",");
	*pri = strsep(label, ",");
	if (!*exten) {
		/* Only a priority in this one */
		*pri = *context;
		*exten = NULL;
		*context = NULL;
	} else if (!*pri) {
		/* Only an extension and priority in this one */
		*pri = *exten;
		*exten = *context;
		*context = NULL;
	}
	*ipri = atoi(*pri);
	return 0;
}

static int context_includes(struct ast_context *c, const char *parent, const char *child)
{
	if (!c) {
		c = ast_context_find(parent);
	}
	if (!c) {
		/* well, if parent doesn't exist, how can the child be included in it? */
		return 0;
	}
	if (!strcmp(ast_get_context_name(c), parent)) {
		/* found the context of the hint app_queue is using. Now, see
		 * if that context includes the one that just changed state */
		struct ast_include *inc = NULL;

		while ((inc = (struct ast_include*) ast_walk_context_includes(c, inc))) {
			const char *includename = ast_get_include_name(inc);
			if (!strcasecmp(child, includename)) {
				return 1;
			}
			/* recurse on this context, for nested includes. The
			 * PBX extension parser will prevent infinite recursion. */
			if (context_includes(NULL, includename, child)) {
				return 1;
			}
		}
	}
	return 0;
}

static int including_context_contains_extension(const char *context, char *exten, char *pri, int ipri)
{
	struct ast_context *c = NULL;

	/* walk all contexts ... we assume we're already locked here. */
	while ( (c = ast_walk_contexts(c)) ) {
		if (context_includes(c, ast_get_context_name(c), context)) { /* okay, so directly or indirectly, this context DOES include the child */
			/* now the real question, does it contain the extension we care about? */
			if (ipri && ast_exists_extension(NULL, ast_get_context_name(c), exten, ipri, NULL)) {
				ast_debug(3, "Context '%s' is included by context '%s' which contains extension '%s'\n", context, ast_get_context_name(c), exten);
				return 1;
			} else if (ast_findlabel_extension(NULL, ast_get_context_name(c), exten, pri, NULL) > 0) {
				ast_debug(3, "Context '%s' is included by context '%s' which contains extension '%s'\n", context, ast_get_context_name(c), exten);
				return 1;
			} else {
				ast_debug(3, "Context '%s' is included by context '%s', which doesn't contain extension '%s'\n", context, ast_get_context_name(c), exten);
			}
		}
	}

	return 0;
}

static int dialplan_location_exists(char *context, struct ast_context *c, char *exten, struct ast_exten *e, char *pri, int ipri, int searchforparent)
{
	if (!context) {
		context = (char*) ast_get_context_name(c); /* assume current context */
	}
	if (!exten) {
		exten = (char*) ast_get_extension_name(e); /* assume current extension */
	}
	ast_debug(8, "Scrutinizing %s:%d %s: %s,%s,%s\n", ast_get_extension_registrar_file(e), ast_get_extension_registrar_line(e),
		ast_get_extension_app(e), context, exten, pri);
	/* Caller ID is ignored here since it's not processed by this function anyways */
	if (ipri && ast_exists_extension(NULL, context, exten, ipri, NULL)) {
		return 1;
	} else if (ast_findlabel_extension(NULL, context, exten, pri, NULL) > 0) {
		return 1;
	}

	/* There is a slight edge case here. Say context A includes context B. A contains extension C, and B does not.
	 * If B does a Goto to C, will it succeed? If it was because B was included in A, yes. If it was because
	 * we were directly in B, then no, it will fail. For the purposes of doing dialplan validation, we should
	 * ignore cases where B is included in A, because this could be perfectly valid at runtime. */
	if (searchforparent && including_context_contains_extension(context, exten, pri, ipri)) {
		return 1;
	}

	return 0;
}

static int __crawl_dialplan(const char *context, const char *exten, const char *appname, struct dialplan_counters *dpc, const struct ast_include *rinclude, int includecount, const char *includes[], int *crawlcounter, int (*crawl_callback)(struct ast_context *c, struct ast_exten *e))
{
	struct ast_context *c = NULL;
	int res = 0;

	ast_rdlock_contexts();

	/* walk all contexts ... */
	while ( (c = ast_walk_contexts(c)) ) {
		int idx;
		struct ast_exten *e;

		if (context && strcmp(ast_get_context_name(c), context)) {
			continue;	/* skip this one, name doesn't match */
		}
		dpc->context_existence = 1;
		ast_rdlock_context(c);
		if (!exten) {
			dpc->total_context++;
		}

		/* walk extensions ... */
		e = NULL;
		while ( (e = ast_walk_context_extensions(c, e)) ) {
			struct ast_exten *p;
			int countexten = 0;

			if (exten && !ast_extension_match(ast_get_extension_name(e), exten)) {
				continue;	/* skip, extension match failed */
			}

			dpc->extension_existence = 1;

			/* unlike show_dialplan_helper, we only count priorities, etc. if we execute the callback */
			if (!appname || !strcmp(appname, ast_get_extension_app(e))) {
				int lres;
				dpc->total_prio++;
				countexten = 1;
				lres = crawl_callback(c, e);
				if (lres == -1) {
					(*crawlcounter)++;
				}
				res |= lres;
			}

			/* walk next extension peers */
			p = e;	/* skip the first one, we already got it */
			while ( (p = ast_walk_extension_priorities(e, p)) ) {
				if (!appname || !strcmp(appname, ast_get_extension_app(p))) {
					int lres;
					dpc->total_prio++;
					countexten = 1;
					lres = crawl_callback(c, p);
					if (lres == -1) {
						(*crawlcounter)++;
					}
					res |= lres;
				}
			}
			if (countexten) {
				dpc->total_exten++;
			}
		}

		for (idx = 0; idx < ast_context_includes_count(c); idx++) {
			const struct ast_include *i = ast_context_includes_get(c, idx);
			if (exten) {
				/* Check all includes for the requested extension */
				if (includecount >= AST_PBX_MAX_STACK) {
					ast_log(LOG_WARNING, "Maximum include depth exceeded!\n");
				} else {
					int dupe = 0;
					int x;
					for (x = 0; x < includecount; x++) {
						if (!strcasecmp(includes[x], ast_get_include_name(i))) {
							dupe++;
							break;
						}
					}
					if (!dupe) {
						includes[includecount] = ast_get_include_name(i);
						__crawl_dialplan(ast_get_include_name(i), exten, appname, dpc, i, includecount + 1, includes, crawlcounter, crawl_callback);
					} else {
						ast_log(LOG_WARNING, "Avoiding circular include of %s within %s\n", ast_get_include_name(i), context);
					}
				}
			}
		}
		ast_unlock_context(c);
	}
	ast_unlock_contexts();

	return res;
}

/*!
 * \brief Traverses part of or the entire dialplan and executes a callback function for every priority
 *
 * \param context Specific dialplan context or NULL for all contexts
 * \param exten Specific dialplan extension to crawl or NULL for all extensions
 * \param appname Optional, application name required to match for callback execution (NULL for any application)
 * \param[out] counters
 * \param[out] crawlcounter Pointer to a counter which will be incremented each time the callback function returns -1.
 * \param crawl_callback Callback function to execute for each traversed priority encountered
 *
 * \note This function does not skip processing of hints. Callback functions should check for hint and ignore if not relevant.
 *
 * \retval 0 if callback function always returned 0 for every callback execution
 * \return non-zero OR'ed return value from non-zero callback returns
 */
static int __attribute__ ((nonnull (4, 5, 6))) ast_crawl_dialplan(const char *context, const char *exten, const char *appname, struct dialplan_counters *dpc, int *crawlcounter, int (*crawl_callback)(struct ast_context *c, struct ast_exten *e))
{
	const char *incstack[AST_PBX_MAX_STACK];
	return __crawl_dialplan(context, exten, appname, dpc, NULL, 0, incstack, crawlcounter, crawl_callback);
}

/* These callbacks were never added to the core,
 * so commented out unless they are added at some point. */
#ifdef CORE_CALLBACKS_EXIST
static int ast_dialplan_validate_callback(struct ast_context *c, struct ast_exten *e)
{
	struct ast_app *app;
	const char *appname;

	if (ast_get_extension_priority(e) == PRIORITY_HINT) {
		/* if there were a callback to verify hints, that could be done at some point */
		return 1; /* not a problem */
	}

	appname = ast_get_extension_app(e);
	app = pbx_findapp(appname);
	if (!app) {
		ast_log(LOG_WARNING, "%s:%d: Could not find application %s\n", ast_get_extension_registrar_file(e),
			ast_get_extension_registrar_line(e), appname);
		return -1; /* this is a problem, so return -1, not 1 */
	}
	if (!app->validate) {
		return 1; /* no registered validation function (not a problem) */
	}
	return app->validate(c, e); /* return value will be non-positive */
}
#else
static int pbx_builtin_goto_validate(struct ast_context *c, struct ast_exten *e)
{
	char *context, *exten, *pri;
	char *data, *sep;
	int ipri, res = 0;
	const char *app = ast_get_extension_app(e);

	data = ast_strdupa(ast_get_extension_app_data(e));
	if (ast_strlen_zero(data)) {
		return -1; /* there should always be data */
	}
	if (!strcasecmp(app, "Goto")) {
		if (strchr(data, '$')) {
			return 0; /* if location contains a variable, there's no real way to know at "compile" time, gotta hope for the best at runtime... */
		}
		pbx_parseable_location(&data, &context, &exten, &pri, &ipri);
		res = dialplan_location_exists(context, c, exten, e, pri, ipri, 1) ? 0 : -1;
	} else if (!strcasecmp(app, "GotoIf") || !strcasecmp(app, "GotoIfTime")) {
		if (strchr(data, '$')) {
			/* in theory, could parse the condition and each branch separately, but we'd need to find the right ? and :
				This is more difficult than actual GotoIf/GosubIf because data isn't variable-substituted at all in advance,
				nor can we substitute it. */
			return 0;
		}
		data = strchr(data, '?');
		if (!data) {
			return -1; /* this shouldn't happen, so *something* is wrong... */
		}
		data++;
		/* because we already bailed out early if a $ was detected, it is guaranteed that the first : will be the separator */
		sep = strchr(data, ':');
		if (sep) {
			sep[0] = '\0';
			sep++;
			if (*sep) { /* make sure this branch contains data */
				pbx_parseable_location(&sep, &context, &exten, &pri, &ipri);
				if (!dialplan_location_exists(context, c, exten, e, pri, ipri, 1)) {
					res = -1;
					goto gotovalidated;
				}
			}
		}
		if (!*data) { /* make sure this branch contains data */
			return 0; /* if location contains a variable, there's no real way to know at "compile" time, gotta hope for the best at runtime... */
		}
		pbx_parseable_location(&data, &context, &exten, &pri, &ipri);
		res = dialplan_location_exists(context, c, exten, e, pri, ipri, 1) ? 0 : -1;
	} else { /* should never happen */
		ast_log(LOG_WARNING, "Invalid validation function for %s\n", app);
		return 1;
	}
gotovalidated:
	if (res) {
		ast_log(LOG_WARNING, "%s:%d: %s(%s) will fail (branch to nonexistent location)\n", ast_get_extension_registrar_file(e),
		ast_get_extension_registrar_line(e), ast_get_extension_app(e), (char*) ast_get_extension_app_data(e));
	}
	return res;
}

static int validate_audio_app_call(struct ast_context *c, struct ast_exten *e)
{
	char *data, *cur, *audiodata;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(audio);
		AST_APP_ARG(rest);
	);

	data = ast_strdupa(ast_get_extension_app_data(e));
	AST_STANDARD_APP_ARGS(args, data);

	if (ast_strlen_zero(args.audio)) { /* this is NEVER valid */
		ast_log(LOG_WARNING, "%s:%d: %s(%s) missing mandatory argument\n", ast_get_extension_registrar_file(e),
		ast_get_extension_registrar_line(e), ast_get_extension_app(e), (char*) ast_get_extension_app_data(e));
		return -1;
	}
	audiodata = args.audio;
	/* by delaying this check until here, we will be able to parse more things since succeeding parameters with variables are discarded first */
	if (strchr(audiodata, '$')) {
		return 0; /* ignore audio args with variables, hope for the best at runtime... */
	}

	while ((cur = strsep(&audiodata, "&"))) {
		if (!strncmp(cur, "/tmp/", 5)) {
			continue; /* ignore tmp files, for obvious reasons... */
		}
		if (ast_fileexists(cur, NULL, NULL)) {
			continue;
		}
		ast_log(LOG_WARNING, "%s:%d: %s(%s) will fail (audio file '%s' does not exist)\n", ast_get_extension_registrar_file(e),
			ast_get_extension_registrar_line(e), ast_get_extension_app(e), (char*) ast_get_extension_app_data(e), cur);
		return -1;
	}
	return 0;
}

static int validate_read_call(struct ast_context *c, struct ast_exten *e)
{
	char *data, *cur, *audiodata;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(audio);
		AST_APP_ARG(audio2);
		AST_APP_ARG(digits);
		AST_APP_ARG(opts);
		AST_APP_ARG(rest);
	);

	data = ast_strdupa(ast_get_extension_app_data(e));

	AST_STANDARD_APP_ARGS(args, data);

	if (ast_strlen_zero(args.audio)) { /* arg1 is not audio for Read/ReadExten, but regardless it IS a mandatory arg so this is NEVER valid */
		ast_log(LOG_WARNING, "%s:%d: %s(%s) missing mandatory argument\n", ast_get_extension_registrar_file(e),
		ast_get_extension_registrar_line(e), ast_get_extension_app(e), (char*) ast_get_extension_app_data(e));
		return 0;
	}

	audiodata = args.audio2;
	if (ast_strlen_zero(audiodata) || strchr(audiodata, '$') || (args.audio && strchr(args.audio, '$'))) { /* if previous args contained vars, that could mess up our primitive parsing */
		return 0; /* ignore audio args with variables, hope for the best at runtime... */
	}

	while ((cur = strsep(&audiodata, "&"))) {
		if (!strncmp(cur, "/tmp/", 5)) {
			continue; /* ignore tmp files, for obvious reasons... */
		}
		if (ast_fileexists(cur, NULL, NULL)) {
			continue;
		}
		if (args.opts && strchr(args.opts, 'i')) {
			continue; /* ignore indications tone names */
		}
		ast_log(LOG_WARNING, "%s:%d: %s(%s) will fail (audio file '%s' does not exist)\n", ast_get_extension_registrar_file(e),
			ast_get_extension_registrar_line(e), ast_get_extension_app(e), (char*) ast_get_extension_app_data(e), cur);
	}
	return 0;
}

static int gosub_validate(struct ast_context *c, struct ast_exten *e)
{
	/* this is similar to pbx_builtin_goto_validate, but there is a wee bit more going on */
	char *context, *exten, *pri;
	char *data, *sep;
	int ipri, res = 0;
	const char *app = ast_get_extension_app(e);

	data = ast_strdupa(ast_get_extension_app_data(e));
	if (ast_strlen_zero(data)) {
		return -1;
	}
	if (!strcasecmp(app, "Gosub")) {
		if (strchr(data, '$')) {
			return 0; /* if location contains a variable, there's no real way to know at "compile" time, gotta hope for the best at runtime... */
		}
		sep = strchr(data, '('); /* start of arguments */
		if (sep) {
			sep[0] = '\0'; /* discard Gosub arguments */
		}
		pbx_parseable_location(&data, &context, &exten, &pri, &ipri);
		res = dialplan_location_exists(context, c, exten, e, pri, ipri, 1) ? 0 : -1;
	} else if (!strcasecmp(app, "GosubIf")) {
		if (strchr(data, '$')) {
			return 0; /* in theory, could parse the condition and each branch separately, but we'd need to find the right ? and : */
		}
		data = strchr(data, '?');
		if (!data) {
			return -1; /* this shouldn't happen, so *something* is wrong... */
		}
		data++;
		/* because we already bailed out early if a $ was detected, it is guaranteed that the first : will be the separator */
		sep = strchr(data, ':');
		if (sep) {
			sep[0] = '\0';
			sep++;
			if (*sep) { /* make sure this branch contains data */
				char *argstart = strchr(data, '(');
				if (argstart) {
					argstart[0] = '\0'; /* discard Gosub arguments */
				}
				pbx_parseable_location(&sep, &context, &exten, &pri, &ipri);
				if (!dialplan_location_exists(context, c, exten, e, pri, ipri, 1)) {
					res = -1;
					goto gosubvalidated;
				}
			}
		}
		if (!*data) { /* make sure this branch contains data */
			return 0; /* if location contains a variable, there's no real way to know at "compile" time, gotta hope for the best at runtime... */
		}
		sep = strchr(data, '('); /* start of arguments */
		if (sep) {
			sep[0] = '\0'; /* discard Gosub arguments */
		}
		pbx_parseable_location(&data, &context, &exten, &pri, &ipri);
		res = dialplan_location_exists(context, c, exten, e, pri, ipri, 1) ? 0 : -1;
	} else {
		ast_log(LOG_WARNING, "Invalid validation function for %s\n", app);
		return 1;
	}
gosubvalidated:
	if (res) {
		ast_log(LOG_WARNING, "%s:%d: %s(%s) will fail (branch to nonexistent location)\n", ast_get_extension_registrar_file(e),
		ast_get_extension_registrar_line(e), ast_get_extension_app(e), (char*) ast_get_extension_app_data(e));
	}
	return res;
}

static int dialplan_validate_callback(struct ast_context *c, struct ast_exten *e)
{
	const char *appname;

	if (ast_get_extension_priority(e) == PRIORITY_HINT) {
		/* if there were a callback to verify hints, that could be done at some point */
		return 1; /* not a problem */
	}

	appname = ast_get_extension_app(e);

#define VALIDATE_CALLBACK(app, validate_cb) \
	if (!strcasecmp(appname, app)) { \
		return validate_cb(c, e); \
	}

	VALIDATE_CALLBACK("Goto", pbx_builtin_goto_validate);
	VALIDATE_CALLBACK("GotoIf", pbx_builtin_goto_validate);
	VALIDATE_CALLBACK("GotoIfTime", pbx_builtin_goto_validate);
	VALIDATE_CALLBACK("BackGround", validate_audio_app_call);
	VALIDATE_CALLBACK("Playback", validate_audio_app_call);
	VALIDATE_CALLBACK("ControlPlayback", validate_audio_app_call);
	VALIDATE_CALLBACK("TalkDetect", validate_audio_app_call);
	VALIDATE_CALLBACK("Read", validate_read_call);
	VALIDATE_CALLBACK("ReadExten", validate_read_call);
	VALIDATE_CALLBACK("Gosub", gosub_validate);
	VALIDATE_CALLBACK("GosubIf", gosub_validate);

#undef VALIDATE_CALLBACK
	return 0; /* No callback */
}
#endif

static char *handle_dialplan_validate_exten(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	char *exten = NULL, *context = NULL;
	/* Variables used for different counters */
	struct dialplan_counters counters;
	int crawlcounter = 0;

	switch (cmd) {
	case CLI_INIT:
		e->command = "dialplan validate exten";
		e->usage =
			"Usage: dialplan validate exten [[exten@]context]\n"
			"       Analyzes dialplan for syntax and usage problems\n";
		return NULL;
	case CLI_GENERATE:
		if (a->argc == 4) {
			return complete_show_dialplan_context(a->line, a->word, a->pos, a->n);
		}
		return NULL;
	}

	if (a->argc != 3 && a->argc != 4) {
		return CLI_SHOWUSAGE;
	}

	memset(&counters, 0, sizeof(counters));

	/* we obtained [exten@]context? if yes, split them ... */
	if (a->argc == 4) {
		if (strchr(a->argv[3], '@')) {	/* split into exten & context */
			context = ast_strdupa(a->argv[3]);
			exten = strsep(&context, "@");
			/* change empty strings to NULL */
			if (ast_strlen_zero(exten)) {
				exten = NULL;
				}
		} else { /* no '@' char, only context given */
			context = ast_strdupa(a->argv[3]);
		}
		if (ast_strlen_zero(context)) {
			context = NULL;
		}
	}
	/* else Show complete dial plan, context and exten are NULL */
	ast_crawl_dialplan(context, exten, NULL, &counters, &crawlcounter, dialplan_validate_callback);

	/* check for input failure and throw some error messages */
	if (context && !counters.context_existence) {
		ast_cli(a->fd, "There is no existence of '%s' context\n", context);
		return CLI_FAILURE;
	}

	if (exten && !counters.extension_existence) {
		if (context) {
			ast_cli(a->fd, "There is no existence of %s@%s extension\n",
				exten, context);
		} else {
			ast_cli(a->fd,
				"There is no existence of '%s' extension in all contexts\n",
				exten);
		}
		return CLI_FAILURE;
	}

	ast_cli(a->fd,"-= %d %s (%d %s) in %d %s. =-\n",
		counters.total_exten, counters.total_exten == 1 ? "extension" : "extensions",
		counters.total_prio, counters.total_prio == 1 ? "priority" : "priorities",
		counters.total_context, counters.total_context == 1 ? "context" : "contexts");
	ast_cli(a->fd, "%d dialplan application call%s failed to validate\n", crawlcounter, crawlcounter != 1 ? "s" : "");

	return CLI_SUCCESS;
}

struct context_name {
	char context[AST_MAX_CONTEXT];
	AST_LIST_ENTRY(context_name) entry;
};

AST_RWLIST_HEAD(context_list, context_name);

static void destroy_context_list(struct context_list *list)
{
	struct context_name *name;
	/* This should only be true when freeing the original line (unloading the module), not when freeing copies of it. */
	AST_RWLIST_WRLOCK(list);
	while ((name = AST_RWLIST_REMOVE_HEAD(list, entry))) {
		ast_free(name);
	}
	AST_RWLIST_UNLOCK(list);
	ast_free(list);
}

static int build_context_list(struct context_list *list, const char *context)
{
	struct ast_context *c = NULL;
	int count = 0;
	int res = 0;

	ast_rdlock_contexts();

	/* walk all contexts ... */
	while ( (c = ast_walk_contexts(c)) ) {
		struct context_name *name;

		if (!ast_strlen_zero(context) && strcasecmp(context, ast_get_context_name(c))) {
			continue;
		}

		name = ast_calloc(1, sizeof(*name));
		if (!name) {
			ast_log(LOG_WARNING, "Memory allocation failed\n");
			continue;
		}

		ast_rdlock_context(c);
		ast_copy_string(name->context, ast_get_context_name(c), sizeof(name->context));
		ast_unlock_context(c);

		AST_RWLIST_INSERT_TAIL(list, name, entry);
		count++;
	}
	ast_unlock_contexts();

	ast_debug(1, "Added %d context%s to list\n", count, ESS(count));

	return res;
}

static inline int in_context_list(struct context_list *list, const char *s)
{
	int in_list = 0;
	struct context_name *name;

	AST_LIST_TRAVERSE(list, name, entry) {
		if (!strcasecmp(s, name->context)) {
			in_list = 1;
			break;
		}
	}

	return in_list;
}

static int add_config_contexts(struct context_list *list, const char *config)
{
	struct ast_config *cfg;
	char *cat = NULL;
	struct ast_flags config_flags = { 0 };
	struct ast_variable *var;

	if (!(cfg = ast_config_load(config, config_flags))) {
		return 0; /* Doesn't exist */
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		/* This could happen if we try to load a config file that should only be included in another (primary) config file. Just skip it. */
		ast_debug(1, "Config file %s is in an invalid format.\n", config);
		return 0;
	}

	while ((cat = ast_category_browse(cfg, cat))) {
		const char *context_type = NULL;
		if (!strcmp(config, "res_alarmsystem.conf")) {
			struct ast_category *category = ast_category_get(cfg, cat, NULL);
			context_type = ast_variable_find(category, "type");
		}
		var = ast_variable_browse(cfg, cat);
		while (var) {
			if (strstr(var->name, "context")) {
				/* Add to our list, if it's not already in it. */
				if (!in_context_list(list, var->value)) {
					struct context_name *name = ast_calloc(1, sizeof(*name));
					if (!name) {
						ast_log(LOG_WARNING, "Memory allocation failed\n");
						continue;
					}
					ast_copy_string(name->context, var->value, sizeof(name->context));
					ast_debug(1, "First discovered reference to context '%s' in %s\n", name->context, config);
					AST_RWLIST_INSERT_TAIL(list, name, entry);
				}
			} else if ((context_type && !strcmp(context_type, "contexts")) || !strcmp(var->name, "channel") || (!strcmp(config, "queues.conf") && !strcasecmp(var->name, "member"))) {
				/* e.g. calendar.conf for notification destination channels.
				 * e.g. queues.conf for member destination channels.
				 */
				const char *context = var->value;
				context = strchr(context, '@');
				if (!ast_strlen_zero(context) && !ast_strlen_zero(++context)) {
					char *tmp;
					struct context_name *name = ast_calloc(1, sizeof(*name));
					if (!name) {
						ast_log(LOG_WARNING, "Memory allocation failed\n");
						continue;
					}
					ast_copy_string(name->context, context, sizeof(name->context));
					tmp = name->context;
					tmp = strchr(tmp, ',');
					if (tmp) {
						*tmp = '\0';
					}
					tmp = name->context;
					tmp = strchr(tmp, '/');
					if (tmp) {
						*tmp = '\0';
					}
					if (!in_context_list(list, name->context)) {
						ast_debug(1, "First discovered reference to context '%s' in %s\n", name->context, config);
						AST_RWLIST_INSERT_TAIL(list, name, entry);
					} else {
						ast_free(name);
					}
				}
			}
			var = var->next;
		} /* End while(var) loop */
	}

	ast_config_destroy(cfg);
	return 0;
}

/*! \brief Dialplan apps that could reference a context name in a way that could cause branching there */
#define RELEVANT_APP(app) (!strncasecmp(app, "Goto", 4) || !strncasecmp(app, "Gosub", 5) || !strcasecmp(app, "Originate") || !strcasecmp(app, "ChannelRedirect") || !strcasecmp(app, "Set") || !strcasecmp(app, "MSet") || !strcasecmp(app, "Dial") || !strcasecmp(app, "InbandDial") || !strcasecmp(app, "System") || !strncasecmp(app, "Return", 6) || !strncasecmp(app, "Exec", 4) || !strcasecmp(app, "DISA") || !strcasecmp(app, "DialTone"))

/*! \brief Directory iterator callback */
static int on_file(const char *dir_name, const char *filename, void *obj)
{
	int len;
	struct context_list *configcontexts = obj;

	/* If it doesn't end in .conf, skip it. */
	len = strlen(filename);
	if (strcmp(filename + len - 5, ".conf")) {
		ast_debug(2, "Skipping %s\n", filename);
		return 0;
	}
	/* Skip stuff that will obviously not be relevant. */
	if (!strcmp(filename, "extensions.conf")) {
		return 0; /* Don't process the dialplan again, duh. */
	}
	/* Channel driver config files don't necessarily begin with "chan" so we can't rely on that.
	 * queues.conf, res_parking.conf, etc. are also relevant.
	 * Ideally we would skip files that are included in other files.
	 */
	if (!strcmp(filename, "ast_debug_tools.conf") || !strcmp(filename, "dbsep.conf") || !strcmp(filename, "rtp_custom.conf")) {
		return 0;
	}
	/* dahdi-channels.conf a stupid stub, ignore it */
	if (!strcmp(filename, "dahdi-channels.conf")) {
		return 0;
	}
	return add_config_contexts(configcontexts, filename);
}

static int search_for_references(struct context_list *list, const char *context)
{
	struct context_name *name;
	int total_unref = 0;
	struct context_list *configcontexts = ast_calloc(1, sizeof(*list));

	/* Add any contexts referenced in other Asterisk configs to a separate linked list. We do this up front and only once for obvious performance reasons. */
	ast_file_read_dir(ast_config_AST_CONFIG_DIR, on_file, configcontexts);

	AST_LIST_TRAVERSE(list, name, entry) {
		/* Walk all contexts and try to find a reference to this. No need to process includes specially. */
		int idx, found_ref = 0;
		struct ast_context *c = NULL;

		if (!strcmp(name->context, "parkedcalls")) {
			continue; /* Created by res_parking, ignore. */
		} else if (!strncmp(name->context, "__", 2)) {
			continue; /* Internally used by modules (e.g. __func_periodic_hook_context__, __TEST_MESSAGE_CONTEXT__), ignore. */
		}

		/* The main outer loop can take a non-instant amount of time to execute on large dialplans,
		 * so reacquire the lock every loop so that other things in Asterisk have a chance to as well.
		 * Also, DO NOT add ANY debug messages in this loop, that will slow everything down way too much.
		 */
		ast_rdlock_contexts();
		while (!found_ref && (c = ast_walk_contexts(c)) ) {
			struct ast_exten *e = NULL;
			ast_rdlock_context(c);

			/* Check includes before extensions, because it's more likely to be there. */
			for (idx = 0; idx < ast_context_includes_count(c); idx++) {
				const struct ast_include *i = ast_context_includes_get(c, idx);
				if (!strcasecmp(name->context, ast_get_include_name(i))) {
					found_ref = 1; /* It's not referenced from a dialplan application, but it is included. */
				}
			}

			while (!found_ref && (e = ast_walk_context_extensions(c, e)) ) {
				struct ast_exten *p;
				const char *data;

				if (RELEVANT_APP(ast_get_extension_app(e)) || strstr(ast_get_extension_app_data(e), "EVAL_EXTEN")) {
					data = ast_get_extension_app_data(e);
					if (strstr(data, name->context)) {
						found_ref = 1;
					}
				}

				/* walk next extension peers */
				p = e;	/* skip the first one, we already got it */
				while (!found_ref && (p = ast_walk_extension_priorities(e, p)) ) {
					if (RELEVANT_APP(ast_get_extension_app(p)) || strstr(ast_get_extension_app_data(p), "EVAL_EXTEN")) {
						data = ast_get_extension_app_data(p);
						if (strstr(data, name->context)) {
							found_ref = 1;
						}
					}
				}
			}

			ast_unlock_context(c);
		}
		ast_unlock_contexts();

		if (!found_ref) {
			/* Finally, check if it was referenced in any (non-dialplan) configs. */
			struct context_name *cname;
			AST_LIST_TRAVERSE(configcontexts, cname, entry) {
				if (!strcasecmp(cname->context, name->context)) {
					found_ref = 1;
					break;
				}
			}
		}

		/* Done searching (potentially all) contexts for a reference to this context name. */
		if (!found_ref) {
			ast_log(LOG_WARNING, "Context '%s' does not seem to be referenced anywhere, unused?\n", name->context);
			total_unref++;
		} else if (context) {
			ast_debug(1, "Context '%s' is referenced\n", name->context); /* Debug okay because there will only be 1. */
		}
	}

	destroy_context_list(configcontexts);

	if (total_unref) {
		ast_log(LOG_WARNING, "%d context%s not referenced from the dialplan and potentially unused\n", total_unref, ESS(total_unref));
		return 1;
	}

	return 0;
}

static char *handle_dialplan_validate_contexts(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	const char *context;
	struct context_list *list = ast_calloc(1, sizeof(*list));

	switch (cmd) {
	case CLI_INIT:
		e->command = "dialplan validate context";
		e->usage =
			"Usage: dialplan validate context [context]\n"
			"       Analyzes dialplan for unreferenced contexts and bad branches\n";
		return NULL;
	case CLI_GENERATE:
		if (a->argc == 4) {
			return complete_show_dialplan_context(a->line, a->word, a->pos, a->n);
		}
		return NULL;
	}

	if (a->argc != 3 && a->argc != 4) {
		return CLI_SHOWUSAGE;
	}

	context = a->argc == 4 ? a->argv[3] : NULL;

	/* No need to hold any list locks, nobody else knows about this. */
	if (build_context_list(list, context)) {
		return CLI_FAILURE;
	}

	if (!search_for_references(list, context)) {
		ast_cli(a->fd, "No branching issues found\n");
	}

	destroy_context_list(list);
	return CLI_SUCCESS;
}

static char *handle_dialplan_validate_all(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "dialplan validate all";
		e->usage =
			"Usage: dialplan validate all\n"
			"       Analyzes entire dialplan for all potential issues\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	/* Validate everything */
	handle_dialplan_validate_contexts(e, cmd, a);
	handle_dialplan_validate_exten(e, cmd, a);
	return CLI_SUCCESS;
}

static struct ast_cli_entry validate_cli[] = {
	AST_CLI_DEFINE(handle_dialplan_validate_all, "Analyzes entire dialplan for all potential issues"),
	AST_CLI_DEFINE(handle_dialplan_validate_exten, "Analyzes dialplan for application usage issues by extension"), /* Application syntax issues */
	AST_CLI_DEFINE(handle_dialplan_validate_contexts, "Analyzes dialplan for unreferenced contexts and bad branches"), /* Branching issues */
};

static int unload_module(void)
{
	ast_cli_unregister_multiple(validate_cli, ARRAY_LEN(validate_cli));
	return 0;
}

static int load_module(void)
{
	ast_cli_register_multiple(validate_cli, ARRAY_LEN(validate_cli));
	return 0;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Dialplan Analysis and Validation");
