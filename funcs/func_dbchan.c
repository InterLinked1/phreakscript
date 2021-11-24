/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2021, Naveen Albert <asterisk@phreaknet.org>
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
 * \brief Functions for managing channels with the AstDB database
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
#include "asterisk/astdb.h"

/*** DOCUMENTATION
	<function name="DB_CHANNEL" language="en_US">
		<synopsis>
			Return the name of the oldest alive channel in an AstDB family
			where the values correspond to channels.
		</synopsis>
		<syntax argsep="/">
			<parameter name="families" required="true" />
		</syntax>
		<description>
			<para>Returns the key corresponding to the first channel that is still alive in a DB
			family or comma-separated list of DB families of keys corresponding to sequentially
			ordered IDs and values corresponding to channel names or unique IDs. Channels that no
			longer exist will be automatically purged if they are older than the first match. If no
			match is found or the DB family does not exist, an empty string is returned.</para>
			<para>This function is designed to facilitate easy search operations when channels are
			stored in AstDB and the oldest existing channel needs to be found.</para>
		</description>
		<see-also>
			<ref type="function">CHANNEL_EXISTS</ref>
			<ref type="function">DB_KEYS</ref>
		</see-also>
	</function>
	<function name="DB_CHANNEL_PRUNE" language="en_US">
		<synopsis>
			Deletes all entries in a family or families whose values are channels that
			no longer exist.
		</synopsis>
		<syntax argsep="/">
			<parameter name="families" required="true" argsep=",">
				<para>AstDB families. Individual families can be pipe-separated to
				delete an entry with the same key in the second family if an entry
				in the first family is deleted (synchronized delete).</para>
			</parameter>
		</syntax>
		<description>
			<para>Iterates through a DB family or comma-separated list of DB families and deletes any
			key-value pairs where the value is no longer a valid channel name or unique ID. Returns
			the number of key/value pairs that were deleted from the database.</para>
		</description>
		<see-also>
			<ref type="function">CHANNEL_EXISTS</ref>
			<ref type="function">DB_KEYS</ref>
			<ref type="function">DB_DELETE</ref>
			<ref type="function">DB_CHANNEL_PRUNE_TIME</ref>
		</see-also>
	</function>
	<function name="DB_CHANNEL_PRUNE_TIME" language="en_US">
		<synopsis>
			Deletes all entries in a DB family or families whose keys are epoch times older
			than the specified epoch. Note that unlike the other DB_CHANNEL functions, this
			requires that keys follow a certain format (i.e. that they correspond to an epoch timestamp).
		</synopsis>
		<syntax argsep="/">
			<parameter name="epoch" required="true" />
			<parameter name="families" required="true" argsep=",">
				<para>AstDB families. Individual families can be pipe-separated to
				delete an entry with the same key in the second family if an entry
				in the first family is deleted (synchronized delete).</para>
			</parameter>
		</syntax>
		<description>
			<para>Iterates through a DB family or comma-separated list of DB families and deletes any
			key-value pairs where the key is an epoch timestamp older than a specified epoch.</para>
			<para>This function should NOT be used if keys do not correspond to epochs.</para>
		</description>
		<see-also>
			<ref type="function">CHANNEL_EXISTS</ref>
			<ref type="function">DB_KEYS</ref>
			<ref type="function">DB_DELETE</ref>
			<ref type="function">DB_CHANNEL_PRUNE</ref>
		</see-also>
	</function>
 ***/

static int db_chan_helper(struct ast_channel *chan, const char *cmd, char *parse, char *buf, size_t len, int prune)
{
	struct ast_db_entry *dbe, *orig_dbe;
	char *family;
	const char *last = "";
	int pruned = 0;
	int epochparse = 0, found = 0;

	ast_debug(4, "Something at level 4\n");
	if (prune == 2) {
		char *epochthreshold = strsep(&parse, ",");
		epochparse = atoi(epochthreshold);
		if (!epochparse) {
			ast_log(LOG_WARNING, "Invalid epoch threshold: %s\n", epochthreshold);
			return -1;
		}
		ast_debug(4, "Pruning entries with keys < %d\n", epochparse);
	}

	while ((family = strsep(&parse, ","))) {
		size_t parselen = strlen(family);
		char *parallel = family;
		family = strsep(&parallel, "|");
		ast_debug(3, "%s: original family: %s, parallel family: %s\n", cmd, family, parallel ? parallel : "(null)");
		if (parallel && !strcmp(family, parallel)) {
			ast_log(LOG_WARNING, "%s: original family '%s' is same as parallel family '%s', skipping\n", cmd, family, parallel);
			continue;
		}
		if (family[strlen(family) - 1] == '/') {
			ast_log(LOG_WARNING, "%s: family '%s' ends in a '/', truncated? Skipping...", cmd, family);
			continue;
		}
		if (parallel && parallel[strlen(parallel) - 1] == '/') {
			ast_log(LOG_WARNING, "%s: paralell family '%s' ends in a '/', truncated? Skipping...", cmd, parallel);
			continue;
		}
		/* Remove leading and trailing slashes */
		while (family[0] == '/') {
			family++;
			parselen--;
		}
		while (family[parselen - 1] == '/') {
			family[--parselen] = '\0';
		}

		/* Nothing within the database at that prefix? */
		if (!(orig_dbe = dbe = ast_db_gettree(family, NULL))) {
			continue;
		}

		for (; dbe; dbe = dbe->next) {
#define BUFFER_SIZE 256
			struct ast_channel *chan_found = NULL;
			char channel[BUFFER_SIZE];
			/* Find the current component */
			char *curkey = &dbe->key[parselen + 1], *slash;
			if (*curkey == '/') {
				curkey++;
			}
			/* Remove everything after the current component */
			if ((slash = strchr(curkey, '/'))) {
				*slash = '\0';
			}

			/* Skip duplicates */
			if (!strcasecmp(last, curkey)) {
				continue;
			}
			last = curkey;

			if (ast_db_get(family, curkey, channel, BUFFER_SIZE)) {
				ast_log(LOG_WARNING, "%s: Couldn't find %s/%s in database\n", cmd, family, curkey);
				continue;
			}

			chan_found = ast_channel_get_by_name(channel);
			if (chan_found) {
				ast_channel_unref(chan_found);
				/* main difference between DB_CHANNEL and DB_CHANNEL_PRUNE is latter always checks all keys.
					Former stops if/when we find a match. */
				if (prune) {
					continue;
				}
				ast_copy_string(buf, curkey, len);
				found = 1;
				break;
			}
			/* channel doesn't exist anymore: delete key and continue */
			if (prune == 2) { /* but only if it's staler than the required epoch time */
				int keyepoch = atoi(curkey);
				if (!(keyepoch < epochparse)) { /* if not older than threshold, don't delete */
					ast_debug(4, "%s: %d is newer than %d, not old enough to purge\n", cmd, keyepoch, epochparse);
					continue;
				}
				ast_debug(4, "%s: %d is older than %d, okay to purge\n", cmd, keyepoch, epochparse);
			}
			ast_debug(3, "%s: Channel %s doesn't exist, saying goodbye\n", cmd, channel);
			pruned++;
			if (ast_db_del(family, curkey)) {
				ast_log(LOG_WARNING, "%s: %s/%s could not be deleted from the database\n", cmd, family, curkey);
			} else if (parallel) {
				ast_debug(3, "%s: Also saying goodbye to %s/%s, if it exists\n", cmd, parallel, curkey);
				if (!ast_db_get(parallel, curkey, buf, len - 1)) {
					if (ast_db_del(parallel, curkey)) {
						ast_debug(1, "%s/%s could not be deleted from the database\n", parallel, curkey);
					}
				}
			}
		}
		ast_db_freetree(orig_dbe);
		if (found) {
			break;
		}
	}
	if (prune) { /* DB_CHANNEL_PRUNE returns # of channels pruned. */
		snprintf(buf, len, "%d", pruned);
	}
	return 0;
}

static int function_db_chan_get(struct ast_channel *chan, const char *cmd, char *parse, char *buf, size_t len)
{
	return db_chan_helper(chan, cmd, parse, buf, len, 0);
}

static int function_db_chan_prune(struct ast_channel *chan, const char *cmd, char *parse, char *buf, size_t len)
{
	return db_chan_helper(chan, cmd, parse, buf, len, 1);
}

static int function_db_chan_prune_time(struct ast_channel *chan, const char *cmd, char *parse, char *buf, size_t len)
{
	return db_chan_helper(chan, cmd, parse, buf, len, 2);
}

static struct ast_custom_function db_chan_get_function = {
	.name = "DB_CHANNEL",
	.read = function_db_chan_get,
};

static struct ast_custom_function db_chan_prune_function = {
	.name = "DB_CHANNEL_PRUNE",
	.read = function_db_chan_prune,
};

static struct ast_custom_function db_chan_prune_time_function = {
	.name = "DB_CHANNEL_PRUNE_TIME",
	.read = function_db_chan_prune_time,
};

static int unload_module(void)
{
	int res = 0;

	res |= ast_custom_function_unregister(&db_chan_get_function);
	res |= ast_custom_function_unregister(&db_chan_prune_function);
	res |= ast_custom_function_unregister(&db_chan_prune_time_function);

	return res;
}

static int load_module(void)
{
	int res = 0;

	res |= ast_custom_function_register(&db_chan_get_function);
	res |= ast_custom_function_register(&db_chan_prune_function);
	res |= ast_custom_function_register(&db_chan_prune_time_function);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "AstDB channel database mappings utility functions");
