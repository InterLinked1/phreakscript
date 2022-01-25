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
 * \brief Functions for managing channel-related info stored in the AstDB database
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
	<function name="DB_MINKEY" language="en_US">
		<synopsis>
			Retrieves the smallest numerical key within specified comma-separated families
		</synopsis>
		<syntax argsep="/">
			<parameter name="families" required="true" argsep=",">
				<para>AstDB families.</para>
			</parameter>
		</syntax>
		<description>
			<para>Returns the family/key of the smallest numerical key from a list of AstDB families.</para>
		</description>
		<see-also>
			<ref type="function">DB_MAXKEY</ref>
			<ref type="function">MIN</ref>
			<ref type="function">MAX</ref>
		</see-also>
	</function>
	<function name="DB_MAXKEY" language="en_US">
		<synopsis>
			Retrieves the largest numerical key within specified comma-separated families
		</synopsis>
		<syntax argsep="/">
			<parameter name="families" required="true" argsep=",">
				<para>AstDB families.</para>
			</parameter>
		</syntax>
		<description>
			<para>Returns the family/key of the largest numerical key from a list of AstDB families.</para>
		</description>
		<see-also>
			<ref type="function">DB_MINKEY</ref>
			<ref type="function">MIN</ref>
			<ref type="function">MAX</ref>
		</see-also>
	</function>
	<function name="DB_UNIQUE" language="en_US">
		<synopsis>
			Returns a unique DB key that can be used to store a value in AstDB.
		</synopsis>
		<syntax argsep="/">
			<parameter name="familyandkey" required="true">
				<para>The family/key to use as the base for the unique keyname.</para>
			</parameter>
		</syntax>
		<description>
			<para>Computes a unique DB key name by using <literal>familyandkey</literal> as
			base and adding a unique zero-padded suffix, beginning with 0.</para>
			<para>This can be used to input data into AstDB with a guaranteed new/unique key
			name that will sort later than any existing keys with the same base key name.</para>
			<para>If this function is read, it will return a unique key name (NOT including
			the family name). If this function is written to, it will store a value at the
			determined unique key name, reducing the chances of data being overwritten
			due to a collision.</para>
			<para>This function will search up to suffix 999, after which time
			the function will abort the search for a unique key name.</para>
		</description>
		<see-also>
			<ref type="function">DB_KEYS</ref>
			<ref type="function">DB_EXISTS</ref>
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

	if (prune == 2) { /* DB_CHANNEL_PRUNE_TIME */
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
			ast_log(LOG_WARNING, "%s: parallel family '%s' ends in a '/', truncated? Skipping...", cmd, parallel);
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

			if (prune != 2) { /* skip for DB_CHANNEL_PRUNE_TIME, because the values aren't channel names */
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
			}
			/* channel doesn't exist anymore: delete key and continue */
			if (prune == 2) { /* but only if it's staler than the required epoch time */
				int keyepoch = atoi(curkey);
				if (!(keyepoch < epochparse)) { /* if not older than threshold, don't delete */
					ast_debug(3, "%s: %d is newer than %d, not old enough to purge\n", cmd, keyepoch, epochparse);
					continue;
				}
				ast_debug(3, "%s: %d is older than %d, okay to purge\n", cmd, keyepoch, epochparse);
			} else {
				ast_debug(3, "%s: Channel %s doesn't exist, saying goodbye\n", cmd, channel);
			}
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

static int db_extreme_helper(char *parse, char *buf, size_t len, int newest)
{
	struct ast_db_entry *dbe, *orig_dbe;
	int winnerfound = 0;
	/* winner should be a float, not an int, to work properly with decimals, e.g. DB_UNIQUE */
	double winner; /* meh, a float might be sufficient, but the extra precision might be worth it */
	char *family;
	char *parsedup = ast_strdupa(parse);

	while ((family = strsep(&parsedup, ","))) {
		size_t parselen = strlen(family);
		ast_debug(1, "Traversing family '%s'\n", family);
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
			/* Find the current component */
			char *curkey = &dbe->key[parselen + 1], *slash;
			if (*curkey == '/') {
				curkey++;
			}
			/* Remove everything after the current component */
			if ((slash = strchr(curkey, '/'))) {
				*slash = '\0';
			}
			if (*curkey == '\0') {
				continue;
			}
			if (!winnerfound) {
				winnerfound = 1;
				winner = atof(curkey);
				ast_debug(1, "Winner is now %f (%s)\n", winner, curkey);
			} else {
				/* in reality, it'll probably be quicker to only keep track of the winning key,
					and then iterate through a 2nd time just once to copy the name of the key,
					as opposed to copying the key name each time there is a new winner */
				double x = atof(curkey);
				if (newest ? (x > winner) : (x < winner)) { /* do we want the newest or oldest key? */
					winner = x;
					ast_debug(1, "Winner is now %f (%s)\n", winner, curkey);
				}
			}
		}
		ast_db_freetree(orig_dbe);
	}
	if (!winnerfound) {
		return -1;
	}
	/* now we know the key we want, so go back and find it */
	while ((family = strsep(&parse, ","))) {
		int found;
		size_t parselen = strlen(family);
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
			/* Find the current component */
			char *curkey = &dbe->key[parselen + 1], *slash;
			if (*curkey == '/') {
				curkey++;
			}
			/* Remove everything after the current component */
			if ((slash = strchr(curkey, '/'))) {
				*slash = '\0';
			}
			if (atoi(curkey) == winner) {
				snprintf(buf, len, "%s/%s", family, curkey);
				found =  1;
				break;
			}
		}
		ast_db_freetree(orig_dbe);
		if (found) {
			break;
		}
	}
	return 0;
}

static int function_db_minkey(struct ast_channel *chan, const char *cmd, char *parse, char *buf, size_t len)
{
	return db_extreme_helper(parse, buf, len, 0);
}

static int function_db_maxkey(struct ast_channel *chan, const char *cmd, char *parse, char *buf, size_t len)
{
	return db_extreme_helper(parse, buf, len, 1);
}

static int db_unique_helper(char *family, int write, const char *value, char *buf, size_t len)
{
#define MAX_SUFFIX 999
	char *fullkey = NULL, *keybase;
	int fullkeysize, suffix = 0;
	char tmpbuf[1];

	size_t parselen = strlen(family);

	/* Remove leading and trailing slashes */
	while (family[0] == '/') {
		family++;
		parselen--;
	}
	while (family[parselen - 1] == '/') {
		family[--parselen] = '\0';
	}
	keybase = family + parselen + 1;
	while (keybase[0] != '/') {
		keybase--;
	}
	keybase[0] = '\0';
	keybase++;
	ast_debug(1, "%s / %s\n", family, keybase);

	fullkeysize = strlen(keybase) + 14; /* key + . + maxint(12) + null terminator */
	fullkey = ast_malloc(fullkeysize);

	while (1) {
		snprintf(fullkey, fullkeysize, "%s.%03d", keybase, suffix); /* zero pad the suffix, so it's guaranteed to sort in numerical order, rather than lexicographical order */
		if (ast_db_get(family, fullkey, buf ? buf : tmpbuf, 1)) { /* we don't actually care about the value, so only write up to 1 byte */
			break; /* if key not found, then we can use it */
		}
		if (++suffix > MAX_SUFFIX) { /* prevent runaway to oblivion */
			ast_log(LOG_WARNING, "Suffix exceeded %d, aborting\n", MAX_SUFFIX);
			break;
		}
	}

	if (buf) {
		buf[0] = '\0'; /* zero out the buffer so it's empty */
	}
	if (suffix <= MAX_SUFFIX) {
		if (write) {
			if (ast_db_put(family, fullkey, value)) {
				ast_log(LOG_WARNING, "DB_UNIQUE: Error writing value to database.\n");
			}
		} else {
			ast_copy_string(buf, fullkey, len); /* reading only, so write the key name into the buffer */
		}
	}
	ast_free(fullkey);
	return suffix > MAX_SUFFIX ? -1 : 0;
}

static int function_db_unique_read(struct ast_channel *chan, const char *cmd, char *parse, char *buf, size_t len)
{
	return db_unique_helper(parse, 0, NULL, buf, len);
}

static int function_db_unique_write(struct ast_channel *chan, const char *cmd, char *parse, const char *value)
{
	return db_unique_helper(parse, 1, value, NULL, 0);
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

static struct ast_custom_function db_minkey_function = {
	.name = "DB_MINKEY",
	.read = function_db_minkey,
};

static struct ast_custom_function db_maxkey_function = {
	.name = "DB_MAXKEY",
	.read = function_db_maxkey,
};

static struct ast_custom_function db_unique_function = {
	.name = "DB_UNIQUE",
	.read = function_db_unique_read,
	.write = function_db_unique_write,
};

static int unload_module(void)
{
	int res = 0;

	res |= ast_custom_function_unregister(&db_chan_get_function);
	res |= ast_custom_function_unregister(&db_chan_prune_function);
	res |= ast_custom_function_unregister(&db_chan_prune_time_function);
	res |= ast_custom_function_unregister(&db_minkey_function);
	res |= ast_custom_function_unregister(&db_maxkey_function);
	res |= ast_custom_function_unregister(&db_unique_function);

	return res;
}

static int load_module(void)
{
	int res = 0;

	res |= ast_custom_function_register(&db_chan_get_function);
	res |= ast_custom_function_register(&db_chan_prune_function);
	res |= ast_custom_function_register(&db_chan_prune_time_function);
	res |= ast_custom_function_register(&db_minkey_function);
	res |= ast_custom_function_register(&db_maxkey_function);
	res |= ast_custom_function_register(&db_unique_function);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "AstDB channel database mappings utility functions");
