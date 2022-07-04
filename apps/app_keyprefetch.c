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
 * \brief IAX2 public key prefetch application
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<depend>res_crypto</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <sys/stat.h>   /* stat(2) */
#include <curl/curl.h>

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/channel.h"
#include "asterisk/app.h"
#include "asterisk/module.h"
#include "asterisk/conversions.h"
#include "asterisk/crypto.h"
#include "asterisk/paths.h"
#include "asterisk/config.h"

/*** DOCUMENTATION
	<application name="KeyPrefetch" language="en_US">
		<synopsis>
			Obtains or updates a public key to accept for incoming IAX2 calls
		</synopsis>
		<syntax>
			<parameter name="cat" required="true">
				<para>Category in <literal>iax.conf</literal> which should accept downloaded inkeys.</para>
			</parameter>
			<parameter name="keyname" required="true">
				<para>The name of the public key as it should be saved to the file system.</para>
				<para>Do not include the extension (e.g. omit <literal>.pub</literal>).</para>
			</parameter>
			<parameter name="url" required="true">
				<para>The HTTP (or HTTPS) endpoint to query for the public key. The HTTP request must
				return a valid RSA public key.</para>
			</parameter>
			<parameter name="maxage" required="true">
				<para>The maximum age of a key, in seconds, before it will be downloaded again,
				even if it already exists.</para>
				<para>If a key with the same name already exists, it will not be downloaded again,
				unless it is older than this parameter.</para>
				<para>Default is 86400 (24 hours).</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="d">
						<para>Do not reload <literal>chan_iax2</literal> and <literal>res_crypto</literal>
						if a new key is installed. These modules need to be reloaded before any added
						or modified keys will be accepted for incoming calls.</para>
					</option>
					<option name="n">
						<para>Do not throw a warning if a public key request is a 404 Not Found.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>The IAX2 channel driver allows users to specify a list of names of RSA public keys
			(or <literal>inkeys</literal>) to accept for authentication on incoming calls.</para>
			<para>This application allows public keys to be automatically downloaded and updated during
			dialplan execution. It will also add keys to an IAX2 section config for you.</para>
			<para>This application sets the following variable:</para>
			<variablelist>
				<variable name="KEYPREFETCHSTATUS">
					<value name="NEW">
						New key downloaded.
					</value>
					<value name="UPDATED">
						Existing key has been updated.
					</value>
					<value name="UNMODIFIED">
						Key already exists and was not modified.
					</value>
					<value name="FAILURE">
						Failure occured.
					</value>
				</variable>
			</variablelist>
		</description>
	</application>
 ***/

enum fetch_option_flags {
	OPT_NORELOAD = (1 << 0),
	OPT_NO404WARN = (1 << 1),
};

AST_APP_OPTIONS(fetch_app_options, {
	AST_APP_OPTION('d', OPT_NORELOAD),
	AST_APP_OPTION('n', OPT_NO404WARN),
});

static const char *app = "KeyPrefetch";

/*! \todo remove url_is_vulnerable, if/once refactored */

/* Copied from func_curl.c. */
static int url_is_vulnerable(const char *url)
{
	if (strpbrk(url, "\r\n")) {
		return 1;
	}

	return 0;
}

static int fetch_exec(struct ast_channel *chan, const char *data)
{
	char *argcopy = NULL;
	struct ast_flags flags = {0};
	int maxage = 86400;
	int reload = 1, newkey = 1, needreload = 0;
	struct ast_key *inkey;
	char *filepath;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(cat);
		AST_APP_ARG(keyname);
		AST_APP_ARG(url);
		AST_APP_ARG(maxage);
		AST_APP_ARG(options);
	);

	argcopy = ast_strdupa(data);

	AST_STANDARD_APP_ARGS(args, argcopy);

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(fetch_app_options, &flags, NULL, args.options);
	}

	if (ast_strlen_zero(args.keyname)) {
		ast_log(LOG_WARNING, "%s requires a key name\n", app);
		return -1;
	}
	if (ast_strlen_zero(args.cat)) {
		ast_log(LOG_WARNING, "%s requires a category\n", app);
		return -1;
	}
	if (ast_strlen_zero(args.url)) {
		ast_log(LOG_WARNING, "%s requires a URL\n", app);
		return -1;
	}
	if (!ast_strlen_zero(args.maxage) && (ast_str_to_int(args.maxage, &maxage) || maxage < 0)) {
		ast_log(LOG_WARNING, "Invalid max age: %s\n", args.maxage);
		return -1;
	}
	if (ast_test_flag(&flags, OPT_NORELOAD)) {
		reload = 0;
	}

	/* calculate the filename */
	if (ast_asprintf(&filepath, "%s/%s.pub", ast_config_AST_KEY_DIR, args.keyname) < 0) {
		ast_log(LOG_WARNING, "Failed to calculate file path\n");
		pbx_builtin_setvar_helper(chan, "KEYPREFETCHSTATUS", "FAILURE");
		return -1;
	}
	ast_debug(1, "Key name is '%s', key path is '%s'\n", args.keyname, filepath);

	/* first, see if a key with this name exists already. if it's new enough, bail out early. */
	inkey = ast_key_get(args.keyname, AST_KEY_PUBLIC);

	if (inkey) {
		struct stat s;
		int created, modified, now, keytime, keymax;
		unsigned int size;

		newkey = 0; /* key with the same name already existed... so we'll assume it's already in the list of inkeys allowed */
		if (stat(filepath, &s)) {
			ast_debug(1, "Inkey '%s' currently loaded, but not found on file system. Redownloading.\n", args.keyname); /* could happen if somebody deleted the key, but hasn't reloaded yet */
			inkey = NULL;
			/* don't set newkey = 1, because we assume that the key was already written to the config before */
		}
		if (inkey) {
			created = (int) s.st_ctime;
			modified = (int) s.st_mtime;
			size = (unsigned int) s.st_size;
			ast_debug(1, "Key '%s' already exists, created %d, modified %d, size %u\n", args.keyname, created, modified, size);
			now = (int) time(NULL);
			keytime = (created > modified ? created : modified);
			keymax = now - maxage; /* minimum time the key must be if we're to let it be */
			if (now < keytime) {
				ast_debug(1, "How can the key be modified in the future (%d)?\n", keytime);
			} else if (keytime < keymax) {
				ast_debug(1, "Key is older (%d) than maxage %d, marked for replacement\n", now - keytime, maxage);
				inkey = NULL;
				/* don't set newkey = 1, because we assume that the key was already written to the config before */
			} else {
				pbx_builtin_setvar_helper(chan, "KEYPREFETCHSTATUS", "UNMODIFIED");
			}
		}
	}
	if (!inkey) {
		struct stat s;
		unsigned int size;
		FILE *public_key_file;
		long http_code;
		CURL **curl;
		char *tmp_filename;
		const char *template_name = "pkeyXXXXXX";
		char curl_errbuf[CURL_ERROR_SIZE + 1];
		int fd;
		int failure = 0;

		ast_debug(1, "Planning to curl '%s' for public key\n", args.url);
		needreload = 1;

		if (url_is_vulnerable(args.url)) {
			ast_log(LOG_ERROR, "URL '%s' is vulnerable to HTTP injection attacks. Aborting CURL() call.\n", args.url);
			goto done;
		}

		curl = curl_easy_init();
		if (!curl) {
			ast_log(LOG_WARNING, "Failed to initialize curl\n");
			goto done;
		}

		fd = ast_file_fdtemp(ast_config_AST_KEY_DIR, &tmp_filename, template_name);
		if (fd == -1) {
			ast_log(LOG_ERROR, "Failed to get temporary file descriptor for CURL\n");
			goto done;
		}
		public_key_file = fdopen(fd, "wb");

		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15);
		curl_easy_setopt(curl, CURLOPT_USERAGENT, AST_CURL_USER_AGENT);
		curl_easy_setopt(curl, CURLOPT_URL, args.url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, public_key_file);
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);

		ast_autoservice_start(chan);

		if (curl_easy_perform(curl)) {
			ast_log(LOG_ERROR, "%s\n", curl_errbuf);
			curl_easy_cleanup(curl);
			fclose(public_key_file);
			remove(tmp_filename);
			ast_free(tmp_filename);
			pbx_builtin_setvar_helper(chan, "KEYPREFETCHSTATUS", "FAILURE");
			goto done;
		}

		ast_autoservice_stop(chan);

		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		curl_easy_cleanup(curl);
		fclose(public_key_file);

		pbx_builtin_setvar_helper(chan, "KEYPREFETCHSTATUS", "UPDATED");
		if (http_code / 100 != 2) {
			if (http_code == 404 && ast_test_flag(&flags, OPT_NO404WARN)) {
				ast_debug(1, "Failed to retrieve URL '%s': code %ld\n", args.url, http_code);
			} else {
				ast_log(LOG_ERROR, "Failed to retrieve URL '%s': code %ld\n", args.url, http_code);
			}
			failure = 1;
		} else if (stat(tmp_filename, &s)) {
			ast_log(LOG_WARNING, "Inkey temp file '%s' not found on file system?\n", tmp_filename);
			failure = 1;
		} else if ((size = (unsigned int) s.st_size) == 0) {
			ast_log(LOG_WARNING, "File '%s' is %d bytes, aborting\n", tmp_filename, size);
			failure = 1;
		} else if (rename(tmp_filename, filepath)) {
			ast_log(LOG_ERROR, "Failed to rename temporary file %s to %s after CURL\n", tmp_filename, filepath);
			failure = 1;
		}
		if (failure) {
			remove(tmp_filename);
			newkey = 0;
			needreload = 0;
			pbx_builtin_setvar_helper(chan, "KEYPREFETCHSTATUS", "FAILURE");
		}
		ast_free(tmp_filename);
	}
	if (newkey) { /* add the new key we just got to the list of inkeys allowed */
		struct ast_config *cfg;
		struct ast_category *category = NULL;
		struct ast_flags config_flags = { CONFIG_FLAG_WITHCOMMENTS | CONFIG_FLAG_NOCACHE };
		char *sfn = "iax.conf", *dfn = "iax.conf";
		const char *cat = args.cat;
		int foundcat = 0;

		ast_debug(1, "Got a new key, '%s', adding to inkeys\n", args.keyname);
		pbx_builtin_setvar_helper(chan, "KEYPREFETCHSTATUS", "NEW");

		if (!(cfg = ast_config_load2(sfn, "app_keyprefetch", config_flags))) {
			ast_log(LOG_WARNING, "Config file not found: %s\n", sfn);
			goto done;
		} else if (cfg == CONFIG_STATUS_FILEINVALID) {
			ast_log(LOG_WARNING, "Config file has invalid format: %s\n", sfn);
			goto cfgcleanup;
		}

		while ((category = ast_category_browse_filtered(cfg, cat, category, NULL))) {
			int newlen;
			const char *currentinkeys = ast_variable_find(category, "inkeys");

			if (!currentinkeys) {
				struct ast_variable *v;
				ast_debug(1, "Found category '%s', but no inkeys variable, creating one now\n", cat);
				if (!(v = ast_variable_new("inkeys", "", "iax.conf"))) {
					ast_log(LOG_WARNING, "Failed to create 'inkeys' variable for category '%s'\n", cat);
					goto cfgcleanup;
				}
				ast_variable_append(category, v);
				ast_include_rename(cfg, sfn, dfn); /* change the include references from dfn to sfn, so things match up */
				if (ast_config_text_file_save2(dfn, cfg, "app_keyprefetch", 0)) {
					ast_log(LOG_WARNING, "Failed to save config\n");
				}
			}

			currentinkeys = ast_variable_find(category, "inkeys");
			if (!currentinkeys) {
				ast_log(LOG_WARNING, "Okay, who's playing games with us now?\n"); /* shouldn't ever happen */
				goto cfgcleanup;
			}

			newlen = strlen(currentinkeys) + strlen(args.keyname) + 2; /* 1 for : separator (except for first one), strlen of keyname, 1 for null terminator */

			if (currentinkeys) {
				char newinkeys[newlen]; /* use stack, not heap */
				if (!ast_strlen_zero(currentinkeys)) {
					snprintf(newinkeys, newlen, "%s:%s", currentinkeys, args.keyname);
				} else {
					snprintf(newinkeys, newlen, "%s", args.keyname);
				}
				ast_debug(1, "Writing out '%s' for inkeys in [%s] now\n", newinkeys, cat);
				if (ast_variable_update(category, "inkeys", newinkeys, NULL, 0)) {
					ast_log(LOG_WARNING, "Failed to update inkeys\n"); /* at this point, it should exist, so it shouldn't fail */
					goto cfgcleanup;
				}
			}
			foundcat = 1;
		}

		if (!foundcat) {
			ast_log(LOG_WARNING, "Could not find category '%s' in IAX2 configuration\n", cat);
			goto cfgcleanup;
		}

		ast_include_rename(cfg, sfn, dfn); /* change the include references from dfn to sfn, so things match up */
		if (ast_config_text_file_save2(dfn, cfg, "app_keyprefetch", 0)) {
			ast_log(LOG_WARNING, "Failed to save config\n");
		}
cfgcleanup:
		ast_config_destroy(cfg);
	}

done:

	ast_free(filepath);

	if (reload && needreload) {
		if (ast_module_reload("chan_iax2") != AST_MODULE_RELOAD_SUCCESS || ast_module_reload("res_crypto") != AST_MODULE_RELOAD_SUCCESS) {
			ast_log(LOG_WARNING, "Failed to reload chan_iax2 and/or res_crypto\n");
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
	return ast_register_application_xml(app, fetch_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Key prefetch application for IAX2");
