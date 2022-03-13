/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2022, Naveen Albert <asterisk@phreaknet.org>
 *
 * See http://www.asterisk.org for more information about
 * the Asterisk project. Please do not directly contact
 * any of the maintainers of this project for assistance;
 * the project provides a web site, mailing lists and IRC
 * channels for your use.
 *
 */

/*! \file
 *
 * \brief Feature processing application
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*! \li \ref app_featureprocess.c uses the configuration file \ref app_featureprocess.conf
 * \addtogroup configuration_file Configuration Files
 */

/*!
 * \page app_featureprocess.conf app_featureprocess.conf
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/cli.h"
#include "asterisk/term.h"
#include "asterisk/config.h"
#include "asterisk/app.h"
#include "asterisk/utils.h"
#include "asterisk/strings.h"

/*** DOCUMENTATION
	<application name="FeatureProcess" language="en_US">
		<synopsis>
			Processes user-defined calling features before
			an incoming call to an endpoint
		</synopsis>
		<syntax>
			<parameter name="arg1" required="true" />
			<parameter name="arg2" multiple="true" />
		</syntax>
		<description>
			<para>This processes listed features corresponding to the profiles
			listed in the configuration file.</para>
			<para>Features are processed in the order provided to the application.</para>
			<para>Each specified feature is checked and if the corresponding condition is true, the feature will be "processed" according to the listed rules.</para>
		</description>
	</application>
	<configInfo name="app_featureprocess" language="en_US">
		<synopsis>Module to process calling features on incoming calls to endpoints</synopsis>
		<configFile name="app_featureprocess.conf">
			<configObject name="general">
				<synopsis>Options that apply globally to app_featureprocess</synopsis>
			</configObject>
			<configObject name="profile">
				<synopsis>Defined profiles for app_featureprocess to use with call verification.</synopsis>
				<configOption name="condition" default="1">
					<synopsis>Condition that must be true to process the feature.</synopsis>
					<description>
						<para>Dialplan condition that must be true to process the feature.</para>
						<para>May contain expressions, functions, and variables.</para>
						<para>If a condition is not specified, it is implicitly always true.</para>
					</description>
				</configOption>
				<configOption name="setvars">
					<synopsis>Variable assignments to make.</synopsis>
					<description>
						<para>Comma-separated variable assignments to make if condition is true.</para>
						<para>Format is the same as arguments to the MSet application.</para>
					</description>
				</configOption>
				<configOption name="gosublocation">
					<synopsis>Dialplan routine to execute if condition is true.</synopsis>
					<description>
						<para>Gosub subroutine to go to if condition is true.</para>
						<para>Subroutines are executed after variables have been set, but before a goto is processed, if applicable.</para>
						<para>Subroutines must end with a call to the <literal>Return</literal> application.</para>
					</description>
				</configOption>
				<configOption name="gosubargs">
					<synopsis>Arguments to be passed into a Gosub execution, if applicable.</synopsis>
					<description>
						<para>Comma-separated arguments to a Gosub execution of the routine specified by <literal>gosublocation</literal>.</para>
					</description>
				</configOption>
				<configOption name="gotolocation">
					<synopsis>Dialplan location to go to if condition is true.</synopsis>
					<description>
						<para>Dialplan location to go to if condition is true.</para>
						<para>Goto branching is always done last after other processing (e.g. variable assignments, if specified, will happen first).</para>
						<para>Takes the form similar to Goto() of [[context,]extension,]priority.</para>
						<para>May contain variables.</para>
					</description>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
 ***/

#define CONFIG_FILE "app_featureprocess.conf"

static char *app = "FeatureProcess";

#define MAX_YN_STRING		4

/*! \brief Data structure for feature */
struct feature_proc {
	ast_mutex_t lock;
	char name[AST_MAX_CONTEXT];			/*!< Name */
	int total;							/*!< Total number of processings */
	char condition[PATH_MAX];			/*!< Condition required to process */
	char gosublocation[AST_MAX_CONTEXT];/*!< Dialplan Gosub location */
	char gosubargs[PATH_MAX];			/*!< Dialplan Gosub args */
	char gotolocation[AST_MAX_CONTEXT];	/*!< Dialplan Goto location */
	char setvars[PATH_MAX];				/*!< Variable assignments */
	AST_LIST_ENTRY(feature_proc) entry;	/*!< Next Feature record */
};

static AST_RWLIST_HEAD_STATIC(features, feature_proc);

/*! \brief Allocate and initialize feature processing profile */
static struct feature_proc *alloc_profile(const char *fname)
{
	struct feature_proc *f;

	if (!(f = ast_calloc(1, sizeof(*f)))) {
		return NULL;
	}

	ast_mutex_init(&f->lock);
	ast_copy_string(f->name, fname, sizeof(f->name));

	/* initialize these only on allocation, so they're not reset during a reload */
	f->total = 0;

	return f;
}

#define LOAD_STR_PARAM(field, cond) \
if (!strcasecmp(param, #field)) { \
	if (!(cond)) { \
		ast_log(LOG_WARNING, "Invalid value for %s, ignoring: '%s'\n", param, val); \
		return; \
	} \
	ast_copy_string(f->field, val, sizeof(f->field)); \
	return; \
}

#define LOAD_INT_PARAM(field, cond) \
if (!strcasecmp(param, #field)) { \
	if (!(cond)) { \
		ast_log(LOG_WARNING, "Invalid value for %s, ignoring: '%s'\n", param, val); \
		return; \
	} \
	f->field = !strcasecmp(val, "yes") ? 1 : 0; \
	return; \
}

#define LOAD_FLOAT_PARAM(field, cond) \
if (!strcasecmp(param, #field)) { \
	if (!(cond)) { \
		ast_log(LOG_WARNING, "Invalid value for %s, ignoring: '%s'\n", param, val); \
		return; \
	} \
	if (sscanf(val, "%30f", &f->field) != 1) { \
		ast_log(LOG_WARNING, "Invalid floating point value for %s, ignoring: '%s'\n", param, val); \
	} \
	return; \
}

static int contains_whitespace(const char *str)
{
	char *s = (char*) str;
	while (*s) {
		if (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') {
			return 1;
		}
		s++;
	}
	return 0;
}

/*! \brief Set parameter in profile from configuration file */
static void profile_set_param(struct feature_proc *f, const char *param, const char *val, int linenum, int failunknown)
{
	LOAD_STR_PARAM(condition, val[0]);
	LOAD_STR_PARAM(gosublocation, val[0] && !contains_whitespace(val));
	LOAD_STR_PARAM(gosubargs, val[0] && !contains_whitespace(val));
	LOAD_STR_PARAM(gotolocation, val[0] && !contains_whitespace(val));
	LOAD_STR_PARAM(setvars, val[0]);
	if (failunknown) {
		if (linenum >= 0) {
			ast_log(LOG_WARNING, "Unknown keyword in profile '%s': %s at line %d of %s\n", f->name, param, linenum, CONFIG_FILE);
		} else {
			ast_log(LOG_WARNING, "Unknown keyword in profile '%s': %s\n", f->name, param);
		}
	}
}

/*! \brief Reload application module */
static int featureproc_reload(int reload)
{
	char *cat = NULL;
	struct feature_proc *f;
	struct ast_variable *var;
	struct ast_config *cfg;
	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };

	if (!(cfg = ast_config_load(CONFIG_FILE, config_flags))) {
		ast_debug(1, "No processing config file (%s), so no profiles loaded\n", CONFIG_FILE);
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Config file %s is in an invalid format. Aborting.\n", CONFIG_FILE);
		return 0;
	}

	AST_RWLIST_WRLOCK(&features);

	/* Reset Global Var Values (currently none) */

	/* General section */

	/* Remaining sections */
	while ((cat = ast_category_browse(cfg, cat))) {
		int new = 0;

		if (!strcasecmp(cat, "general")) {
			continue;
		}

		/* Look for an existing one */
		AST_LIST_TRAVERSE(&features, f, entry) {
			if (!strcasecmp(f->name, cat)) {
				break;
			}
		}

		ast_debug(1, "New profile: '%s'\n", cat);

		if (!f) {
			/* Make one then */
			f = alloc_profile(cat);
			new = 1;
		}

		/* Totally fail if we fail to find/create an entry */
		if (!f) {
			ast_log(LOG_WARNING, "Failed to create feature profile for '%s'\n", cat);
			continue;
		}

		if (!new) {
			ast_mutex_lock(&f->lock);
		}
		/* Re-initialize the defaults (none currently) */

		/* Search Config */
		var = ast_variable_browse(cfg, cat);
		while (var) {
			ast_debug(2, "Logging parameter %s with value %s from lineno %d\n", var->name, var->value, var->lineno);
			profile_set_param(f, var->name, var->value, var->lineno, 1);
			var = var->next;
		} /* End while(var) loop */

		if (!new) {
			ast_mutex_unlock(&f->lock);
		} else {
			AST_RWLIST_INSERT_HEAD(&features, f, entry);
		}
	}

	ast_config_destroy(cfg);
	AST_RWLIST_UNLOCK(&features);

	return 0;
}

static void assign_vars(struct ast_channel *chan, struct ast_str *strbuf, char *vars)
{
	char *var, *tmp = vars;

	ast_debug(1, "Processing variables: %s\n", tmp);

	while ((var = ast_strsep(&tmp, ',', AST_STRSEP_STRIP))) { /* strsep will cut on the next literal comma, and stuff could have commas inside... */
		char *varname, *varvalue;
		ast_debug(1, "Processing variable assignment: %s\n", var);
		varname = ast_strdupa(ast_strsep(&var, '=', AST_STRSEP_STRIP)); /* this should be fine, we're not going to have a huge number of vars... */
		varvalue = ast_strsep(&var, '=', AST_STRSEP_STRIP);
		if (!varname || !varvalue) {
			ast_log(LOG_WARNING, "Missing variable in assignment: %s = %s\n", varname ? varname : "(null)", varvalue ? varvalue : "(null)");
			continue;
		}
		ast_str_substitute_variables(&strbuf, 0, chan, varvalue); /* we don't actually know how big this will be, so use this instead of pbx_substitute_variables_helper */
		ast_debug(1, "Making variable assignment: %s = %s (%s)\n", varname, varvalue, ast_str_buffer(strbuf));
		pbx_builtin_setvar_helper(chan, varname, ast_str_buffer(strbuf));
		ast_str_reset(strbuf);
	}
}

#define FEATPROC_STRDUP(field) ast_copy_string(field, f->field, sizeof(field));

static int feature_process(struct ast_channel *chan, struct feature_proc *f, struct ast_str *strbuf)
{
#define BUGGY_AST_SUB 1

#if BUGGY_AST_SUB
	char substituted[1024];
#endif
	char name[AST_MAX_CONTEXT], condition[PATH_MAX], gosublocation[AST_MAX_CONTEXT], gosubargs[PATH_MAX], gotolocation[AST_MAX_CONTEXT], setvars[PATH_MAX];

	ast_mutex_lock(&f->lock);
	FEATPROC_STRDUP(name);
	FEATPROC_STRDUP(condition);
	FEATPROC_STRDUP(gosublocation);
	FEATPROC_STRDUP(gosubargs);
	FEATPROC_STRDUP(gotolocation);
	FEATPROC_STRDUP(setvars);
	ast_mutex_unlock(&f->lock);

	if (!ast_strlen_zero(condition)) {
#if BUGGY_AST_SUB
		pbx_substitute_variables_helper(chan, condition, substituted, sizeof(substituted) - 1);
		ast_debug(2, "Condition to check: %s (evaluates to %s)\n", condition, substituted);
		if (!pbx_checkcondition(substituted)) {
			ast_debug(1, "Not processing feature '%s'\n", name);
			return 0;
		}
#else
		ast_str_reset(strbuf);
		ast_str_substitute_variables(&strbuf, 0, chan, condition);
		ast_debug(2, "Condition to check: %s (evaluates to %s)\n", condition, ast_str_buffer(strbuf));
		if (!pbx_checkcondition(ast_str_buffer(strbuf))) {
			ast_debug(1, "Not processing feature '%s'\n", name);
			return 0;
		}
		ast_str_reset(strbuf);
#endif
	}
	ast_verb(4, "Processing feature '%s'\n", name);

	ast_mutex_lock(&f->lock);
	f->total++;
	ast_mutex_unlock(&f->lock);

	if (!ast_strlen_zero(setvars)) {
		/* variable assignments could have variables */
		ast_str_substitute_variables(&strbuf, 0, chan, setvars);
		assign_vars(chan, strbuf, ast_str_buffer(strbuf));
	}
	if (!ast_strlen_zero(gosublocation)) {
		/* location could have variables */
		ast_str_substitute_variables(&strbuf, 0, chan, gosublocation);
		ast_debug(3, "Feature '%s' triggered Gosub %s(%s)\n", name, gosublocation, gosubargs);
		ast_app_run_sub(NULL, chan, gosublocation, gosubargs, 0);
	}
	if (!ast_strlen_zero(gotolocation)) {
		/* location could have variables */
		ast_str_substitute_variables(&strbuf, 0, chan, gotolocation);
		if (!ast_parseable_goto(chan, ast_str_buffer(strbuf))) {
			ast_verb(3, "Feature '%s' triggered Goto (%s)\n", name, gotolocation);
		}
		return -1; /* if we hit a goto, that means end feature processing */
	}

	return 0;
}

static int featureproc_exec(struct ast_channel *chan, const char *data)
{
	char *argstr, *cur = NULL;
	struct ast_str *strbuf = NULL;

	argstr = ast_strdupa((char *) data);

	if (ast_strlen_zero(argstr)) {
		ast_log(LOG_WARNING, "%s requires an argument\n", app);
		return -1;
	}
	if (!(strbuf = ast_str_create(512))) {
		ast_log(LOG_ERROR, "Could not allocate memory for response.\n");
		return -1;
	}

	while ((cur = strsep(&argstr, ","))) {
		struct feature_proc *f;

		AST_RWLIST_RDLOCK(&features);
		AST_LIST_TRAVERSE(&features, f, entry) {
			if (!strcasecmp(f->name, cur)) {
				break;
			}
		}
		AST_RWLIST_UNLOCK(&features);
		if (!f) {
			ast_log(LOG_WARNING, "No profile found in configuration for processing feature '%s'\n", cur);
			continue;
		}
		if (feature_process(chan, f, strbuf)) {
			break;
		}
		ast_str_reset(strbuf);
	}

	ast_free(strbuf);

	return 0;
}

/*! \brief CLI command to list feature processing profiles */
static char *handle_show_profiles(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-20s %20s\n"
#define FORMAT2 "%-20s %20d\n"
	struct feature_proc *f;

	switch(cmd) {
	case CLI_INIT:
		e->command = "featureproc show profiles";
		e->usage =
			"Usage: featureproc show profiles\n"
			"       Lists all feature processing profiles.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	ast_cli(a->fd, FORMAT, "Name", "# Processings");
	ast_cli(a->fd, FORMAT, "--------------------", "-------------");
	AST_RWLIST_RDLOCK(&features);
	AST_LIST_TRAVERSE(&features, f, entry) {
		ast_cli(a->fd, FORMAT2, f->name, f->total);
	}
	AST_RWLIST_UNLOCK(&features);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

/*! \brief CLI command to dump verification profile */
static char *handle_show_profile(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-32s : %s\n"
#define FORMAT2 "%-32s : %f\n"
	struct feature_proc *f;
	int which = 0;
	char *ret = NULL;

	switch(cmd) {
	case CLI_INIT:
		e->command = "featureproc show profile";
		e->usage =
			"Usage: featureproc show profile <profile name>\n"
			"       Displays information about a feature processing profile.\n";
		return NULL;
	case CLI_GENERATE:
		if (a->pos != 3) {
			return NULL;
		}
		AST_LIST_TRAVERSE(&features, f, entry) {
			if (!strncasecmp(a->word, f->name, strlen(a->word)) && ++which > a->n) {
				ret = ast_strdup(f->name);
				break;
			}
		}

		return ret;
	}

	if (a->argc != 4) {
		return CLI_SHOWUSAGE;
	}

	AST_RWLIST_RDLOCK(&features);
	AST_LIST_TRAVERSE(&features, f, entry) {
		if (!strcasecmp(a->argv[3], f->name)) {
			ast_cli(a->fd, FORMAT, "Name", f->name);
			ast_cli(a->fd, FORMAT, "Condition", f->condition);
			ast_cli(a->fd, FORMAT, "Gosub Location", f->gosublocation);
			ast_cli(a->fd, FORMAT, "Gosub Arguments", f->gosubargs);
			ast_cli(a->fd, FORMAT, "Goto Location", f->gotolocation);
			ast_cli(a->fd, FORMAT, "Variable Assignments", f->setvars);
			break;
		}
	}
	AST_RWLIST_UNLOCK(&features);
	if (!f) {
		ast_cli(a->fd, "No such feature processing profile: '%s'\n", a->argv[3]);
	}

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

/*! \brief CLI command to reset verification stats */
static char *handle_reset_stats(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct feature_proc *f;

	switch(cmd) {
	case CLI_INIT:
		e->command = "featureproc reset stats";
		e->usage =
			"Usage: featureproc reset stats\n"
			"       Resets feature processing statistics.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	AST_RWLIST_RDLOCK(&features);
	AST_LIST_TRAVERSE(&features, f, entry) {
		f->total = 0;
	}
	AST_RWLIST_UNLOCK(&features);

	return CLI_SUCCESS;
}

static struct ast_cli_entry featureproc_cli[] = {
	AST_CLI_DEFINE(handle_show_profiles, "Display statistics about feature processing profiles"),
	AST_CLI_DEFINE(handle_show_profile, "Displays information about a feature processing profile"),
	AST_CLI_DEFINE(handle_reset_stats, "Resets feature processing statistics for all feature profiles"),
};

static int unload_module(void)
{
	struct feature_proc *f;

	ast_unregister_application(app);
	ast_cli_unregister_multiple(featureproc_cli, ARRAY_LEN(featureproc_cli));

	AST_RWLIST_WRLOCK(&features);
	while ((f = AST_RWLIST_REMOVE_HEAD(&features, entry))) {
		ast_free(f);
	}

	AST_RWLIST_UNLOCK(&features);

	return 0;
}

/*!
 * \brief Load the module
 *
 * Module loading including tests for configuration or dependencies.
 * This function can return AST_MODULE_LOAD_FAILURE, AST_MODULE_LOAD_DECLINE,
 * or AST_MODULE_LOAD_SUCCESS. If a dependency or environment variable fails
 * tests return AST_MODULE_LOAD_FAILURE. If the module can not load the
 * configuration file or other non-critical problem return
 * AST_MODULE_LOAD_DECLINE. On success return AST_MODULE_LOAD_SUCCESS.
 */
static int load_module(void)
{
	int res;

	if (featureproc_reload(0)) {
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_cli_register_multiple(featureproc_cli, ARRAY_LEN(featureproc_cli));

	res = ast_register_application_xml(app, featureproc_exec);

	return res;
}

static int reload(void)
{
	return featureproc_reload(1);
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Feature Processing Application",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.reload = reload,
);
