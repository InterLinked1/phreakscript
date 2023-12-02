/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2022, Naveen Albert
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
 * \brief Device Digit Map Generation
 *
 * \details This module can be used to generate the digit maps used
 *          by most common SIP devices, such as ATAs, gateways, and
 *          IP phones.
 *          It can also be used to assist in troubleshooting or
 *          debugging dialplan pattern matching.
 *
 * \note The generated digit map should be compatible with most systems.
 *       For Grandstream, you will need to surround with braces { }
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/cli.h"
#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/manager.h"

/*** DOCUMENTATION
	<manager name="GenerateDigitMap" language="en_US" module="res_xmpp">
		<synopsis>
			Generate a digit map for a context.
		</synopsis>
		<syntax>
			<xi:include xpointer="xpointer(/docs/manager[@name='Login']/syntax/parameter[@name='ActionID'])" />
			<parameter name="Context" required="true">
				<para>Name of dialplan context for which to generate a digit map.</para>
			</parameter>
		</syntax>
		<description>
			<para>Generates the digit map for the specified dialplan context.</para>
			<para>Digit maps are used by many SIP devices when digits are collected locally,
			and the digit map should correspond to the dial plan allowed by a device's context.</para>
		</description>
	</manager>
***/

#define BUF_SIZE 2048 /* Grandstream devices only support a digit map of max length 2048 */

#define buf_append(buf, len, fmt, ...) { \
	bytes = snprintf(buf, len, fmt, ## __VA_ARGS__); \
	if (bytes >= len) { \
		res = -1; \
		break; \
	} \
	buf += bytes; \
	len -= bytes; \
}

struct map_geninfo {
	int includecount;
};

/*! \retval -1 on failure, number of bytes written on success */
static int generate_digit_map(const char *prefix, const char *rootcontext, const char *context, struct map_geninfo *geninfo, const char *includes[], char *buf, size_t len, int timeoutsuffix)
{
	int idx;
	struct ast_context *c;
	struct ast_exten *e = NULL;
	int i, res = 0;
	int bytes;
	char *start = buf;

	if (!prefix) {
		prefix = "";
	}

	c = ast_context_find(context);
	if (!c) {
		ast_log(LOG_WARNING, "No such context: %s\n", context);
		return -1;
	}

	includes[geninfo->includecount] = context;
	ast_debug(2, "Crawling context #%d: %s for extensions (current prefix: %s)\n", geninfo->includecount, context, !ast_strlen_zero(prefix) ? prefix : "none");
	geninfo->includecount += 1;

	ast_rdlock_context(c);
	while ((e = ast_walk_context_extensions(c, e))) {
		char firstchar[2] = "";
		int ignorepat = 0;
		int needtimeout = 0;
		int complex_exten = 0;
		const char *startbuf, *name = ast_get_extension_name(e);
		/* XXX use macro for this */
		if (!strcmp(name, "a") || !strcmp(name, "i") || !strcmp(name, "s") || !strcmp(name, "t")) {
			continue; /* Skip special dialplan extensions */
		}

		/* Skip if it's not priority 1, e.g. a hint or something else */
		if (ast_get_extension_priority(e) != 1) {
			ast_debug(3, "Skipping %s,%s,%d\n", context, name, ast_get_extension_priority(e));
			continue;
		}

		if (*name == '_') {
			name++; /* It's a pattern */
		}

		ast_debug(3, "Processing extension: %s\n", ast_get_extension_name(e));

		firstchar[0] = !ast_strlen_zero(prefix) ? *prefix : *name; /* If we already have a prefix, use that instead as that's the first true dialed digit, to which ignorepat will apply */

		/* Check if there's an ignorepat in any of the contexts above us in the hierarchy, and obviously including the current context. */
		for (i = 0; !ignorepat && i < geninfo->includecount; i++) {
			ast_debug(5, "Checking exten %c in %s (#%d) for ignorepat\n", firstchar[0], includes[i], i);
			/* Also check existing include contexts we've processed on the way here. */
			ignorepat = ast_ignore_pattern(includes[i], firstchar);
			if (ignorepat) {
				ast_debug(4, "ignorepat match for %c in context %s\n", firstchar[0], includes[i]);
			}
		}

		buf_append(buf, len, "|");
		startbuf = buf;
		if (prefix) {
			buf_append(buf, len, "%s", prefix);
		}

		/* Process the pattern, one character at a time. */
		while (*name) {
			/* If there's already a prefix, insert comma for 2nd dial tone now. Otherwise the prefix code is the first digit below and we do so afterwards. */
			if (!ast_strlen_zero(prefix) && ignorepat) {
				/* If there's an ignorepat for this prefix, insert a comma for second dial tone */
				/* Note that Grandstream does not currently support 2nd dial tones, but this won't cause any issues otherwise. */
				buf_append(buf, len, ",");
				ignorepat = 0;
			}

			/* Digit maps don't understand N or Z, so expand them */
			if (*name == 'N') {
				buf_append(buf, len, "[2-9]");
				complex_exten = 1;
			} else if (*name == 'Z') {
				buf_append(buf, len, "[1-9]");
				complex_exten = 1;
			} else if (*name == 'X') {
				buf_append(buf, len, "x"); /* Digit maps do recognize 'x', but they use lowercase x, not uppercase X */
				complex_exten = 1;
			} else if (*name == '!') {
#if 0
				/* S0 is not support by Grandstream. So we should just ignore ! characters. */
				buf_append(buf, len, "S0"); /* Translate ! into immediate match */
#endif
			} else {
				/* Process [] ranges as an entire unit */
				if (*name == '[') {
					char range_first = 0;
					const char *initial_range_start, *range_start, *range_end = strchr(name, ']');

					initial_range_start = range_start = name;
					complex_exten = 1;
					if (!range_end) {
						ast_log(LOG_WARNING, "Dialplan is invalid: unterminated range: %s\n", ast_get_extension_name(e));
						res = -1;
						break;
					} else if (*range_start == '-') {
						ast_log(LOG_WARNING, "Dialplan is invalid: range has no start: %s\n", ast_get_extension_name(e));
						res = -1;
						break;
					}

					name = range_end; /* Once we exit the loop, pick up at the end of the [] range */
					ast_debug(4, "Processing section: %.*s\n", (int) (range_end + 1 - range_start), range_start); /* Print out contents of [] */

					while (range_start < range_end) {
						if (*range_start == '-') {
							/* This does contain a range. We may need to expand it to form a valid digit map. */
							range_first = *(range_start - 1); /* This will always succeed, because we ensure the initial range_start doesn't start with a - */
						}
#ifdef EXTRA_DEBUG
						ast_debug(5, "initial_range_start: %p (%c), range_start: %p (%c), range_end: %p\n",
							initial_range_start, *initial_range_start, range_start, *range_start, range_end);
#endif
						/* We check 2 bounds here: if we started more than 2 ago or we're ending more than 2 from now, then there's more than just 1 range specified, and we need to expand. */
						if (range_first && (initial_range_start < (range_start - 2) || range_end > (range_start + 2))) {
							/* Grandstream digit maps (and possibly others) don't like combinations, e.g. [02-9] is not valid, either do 0 and 2-9 separately or do [023456789] */
							/* If the [] is of format [2-9], then leave it alone. If it's any larger than that, we need to expand it or it won't be valid for a digit map. */
							char range_last = *(range_start + 1); /* End of the range is whatever the next character is. */
							/* Expand the range so it forms a valid digit map. */
							ast_debug(4, "Range from %c to %c must be expanded for %s\n", range_first, range_last, ast_get_extension_name(e));
							range_first++; /* Add 1, since we already wrote the first character in the range the previous iteration of the loop. */
							while (range_first <= range_last) { /* Write out the entire range */
								buf_append(buf, len, "%c", range_first);
								range_first++;
							}
							range_first = 0;
							range_start++; /* Skip ahead */
						} else {
#ifdef EXTRA_DEBUG
							ast_debug(3, "Appending %c\n", *range_start);
#endif
							buf_append(buf, len, "%c", *range_start);
							range_first = 0; /* It's just a single range in the [], nothing special */
						}
						range_start++;
					}
					buf_append(buf, len, "%c", ']'); /* End the range */
					ast_assert(!range_first);
				} else {
					/* Copy literally, including the . character. */
#ifdef EXTRA_DEBUG
					ast_debug(3, "XXXXXX Appending %c\n", *name);
#endif
					buf_append(buf, len, "%c", *name);
				}
			}
			if (ast_strlen_zero(prefix) && ignorepat) {
				/* If there's an ignorepat for this prefix, insert a comma for second dial tone */
				buf_append(buf, len, ",");
				ignorepat = 0;
			}
			name++;
		}
#define VALID_DIGIT(x) (isdigit(x) || (x >= 'A' && x <= 'D') || x == '*' || x == '#')
#define VALID_DIGITMAP_DIGIT(x) (VALID_DIGIT(x) || x == 'x')
		/* If this is a prefix of another valid extension, certain devices (e.g. Polycom) require the digit T be suffixed. */
		if (timeoutsuffix && !needtimeout) {
			/* This first check only works for literal extensions, not patterns. */
			if (!complex_exten && ast_matchmore_extension(NULL, ast_get_context_name(c), ast_get_extension_name(e), 1, NULL)) {
				needtimeout = 1;
			} else if (complex_exten) { /* Assume it's a pattenr */
				char sample_exten[AST_MAX_EXTENSION + 1]; /* Should ensure bounds check is okay */
				char *dst = sample_exten;
				int in_pattern = 0;
				char start_digit = 0, end_digit = 0;
				const char *src = startbuf;
				/* If we have a pattern like _[2-9] and the pattern, say, _[2-4]XXX
				 * then the first pattern is a prefix of the second one (at least when the first digit is 2-4).
				 * Ideally, we would split the first pattern into _[2-4]T and _[5-9],
				 * but it would be difficult to do this efficiently, so we compromise
				 * by allowing for some false positives for timeouts (since false negatives are never acceptable,
				 * as that would preclude being able to dial certain extensions).
				 * To do this, we need to instantiate the pattern and see if it could match anything further.
				 * Admittedly, the way we do this here is not 100% accurate, more probalistic, but should work in most real world use cases: */
				while (*src) {
					if (*src == '[') {
						if (in_pattern++) {
							ast_log(LOG_WARNING, "Was already in a pattern match?\n");
						}
					} else if (*src == ']') {
						if (in_pattern--) {
							if (!start_digit || !end_digit) {
								ast_log(LOG_WARNING, "Pattern missing start or end?\n");
							} else {
								*dst++ = start_digit;
							}
						} else {
							ast_log(LOG_WARNING, "Wasn't already in a pattern match?\n");
						}
					} else {
						if (in_pattern) {
							if (!start_digit) {
								if (VALID_DIGIT(*src)) {
									start_digit = *src;
								} else {
									ast_log(LOG_WARNING, "Invalid digit prior to - character: %c\n", *src);
								}
							} else {
								/* Even if the range is not continuous, this is fine for a hueristic approach: */
								end_digit = *src;
							}
						} else {
							if (*src != ',') {
								if (!VALID_DIGITMAP_DIGIT(*src)) {
									ast_log(LOG_WARNING, "Invalid digit outside of pattern match: %c\n", *src);
								}
								*dst++ = *src;
							}
						}
					}
					src++;
				}
				*dst = '\0';
				/* Now, instantiate any instances of X, N, or Z, arbitrarily. */
				dst = sample_exten;
				while (*dst) {
					int min = 0; /* Max is 9 for all */
					if (isalpha(*dst)) {
						switch (*dst) {
							case 'N':
								min = 2;
								break;
							case 'Z':
								min = 1;
								break;
							case 'X':
								break;
							default:
								break;
						}
						*dst = min + '0'; /* Replace pattern digit with something concrete */
					}
					dst++;
				}
				ast_debug(3, "Instantiated pattern '%s' as %s@%s\n", startbuf, sample_exten, rootcontext);
				/* It could match in a context that includes the current one, so use the root context for comparison.
				 * When doing this, we need to include the prefix. */
				if (ast_matchmore_extension(NULL, rootcontext, sample_exten, 1, NULL)) {
					needtimeout = 1;
				}
			}
		}

		if (needtimeout) {
			ast_debug(2, "Extension %s %s prefixes other valid extension(s)\n", ast_get_extension_name(e), complex_exten ? "probably" : "definitely");
			buf_append(buf, len, "%c", 'T');
		}
		ast_debug(3, "%p: Added to digit map: %s\n", startbuf, startbuf);
	}

	if (res && len <= 0) {
		ast_log(LOG_WARNING, "No space left in digit map buffer\n"); /* Truncation occured */
	}

	/* Skip if res */
	for (idx = 0; res >= 0 && idx < ast_context_includes_count(c); idx++) {
		const struct ast_include *i = ast_context_includes_get(c, idx);
		if (i) {
			/* Check all includes for extension patterns */
			if (geninfo->includecount >= AST_PBX_MAX_STACK) {
				ast_log(LOG_WARNING, "Maximum include depth exceeded!\n");
			} else {
				int dupe = 0;
				int x;
				char *tmp, *tmp2 = "", *includename = ast_strdup(ast_get_include_name(i)); /* Don't use strdupa in a loop */

				if (!includename) {
					continue;
				}
				/* Only care about the include context name, not other arguments to the include */
				/* XXX We also need to do this in the pbx validate review */
				tmp = strchr(includename, '|');
				if (tmp) {
					*tmp++ = '\0';
					tmp2 = strchr(tmp, '|');
					if (tmp2) {
						*tmp2++ = '\0';
						ast_debug(3, "Found an include prefix: %s\n", tmp2);
					}
				}
				tmp = strchr(includename, ',');
				if (tmp) {
					*tmp = '\0';
				}

				if (ast_strlen_zero(includename)) {
					ast_log(LOG_WARNING, "Empty include context\n");
					ast_free(includename);
					continue;
				}

				for (x = 0; x < geninfo->includecount; x++) {
					if (!strcasecmp(includes[x], includename)) {
						dupe++;
						break;
					}
				}
				if (!dupe) {
					/* XXX We should use a public API to retrieve the prefix */
					char newprefix[32];
					snprintf(newprefix, sizeof(newprefix), "%s%s", prefix, tmp2); /* prefix is non NULL so no need for S_OR */

					res = generate_digit_map(newprefix, rootcontext, includename, geninfo, includes, buf, len, timeoutsuffix);

					if (res > 0) {
						buf += res;
						len -= res;
					}
				} else {
					ast_log(LOG_WARNING, "Avoiding circular include of %s within %s\n", ast_get_include_name(i), context);
				}
				ast_free(includename);
				if (!dupe && res < 0) {
					break; /* Break after ast_free, instead of in the !dupe block */
				}
			}
		}
	}
	ast_unlock_context(c);

	/* Remove from the stack, the pointer will no longer be valid and it's not in the hierarchy any longer so it shouldn't be there. */
	geninfo->includecount -= 1;
	includes[geninfo->includecount] = NULL;

	return res < 0 ? res : (buf - start);
}

static int generate_digit_map_all(int fd, const char *context_name, int timeoutsuffix)
{
	int res = 0;
	const char *includes[AST_PBX_MAX_STACK];
	char buf[BUF_SIZE];
	struct map_geninfo geninfo = {
		.includecount = 0,
	};

	res = generate_digit_map(NULL, context_name, context_name, &geninfo, includes, buf, sizeof(buf), timeoutsuffix);

	if (res <= 0) {
		return res;
	}

	/* XXX Would be nice if we could condense the direct translation of dialplan => digit map into a condensed form, e.g. 22|23|24 -> 2[2-4] */

	/* Print */
	ast_debug(1, "Generated digit map length: %d\n", res);
	ast_cli(fd, "%s\n", buf + 1); /* Skip leading | */
	return 0;
}

static char *handle_dialplan_generate_digitmap(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "dialplan generate digitmap";
		e->usage =
			"Usage: dialplan generate digitmap <context> [T]\n"
			"       Generate device digit maps for a dialplan context\n"
			"       The argument T will suffix a T for any extensions that prefix other valid extensions.\n";
		return NULL;
	case CLI_GENERATE:
#if 0
		/* XXX Waiting for res_pbx_validate merges to happen. */
		return ast_complete_dialplan_contexts(a->line, a->word, a->pos, a->n);
#else
		return NULL;
#endif
	}

	if (a->argc != 4 && a->argc != 5) {
		return CLI_SHOWUSAGE;
	}

	return generate_digit_map_all(a->fd, a->argv[3], a->argc == 5) ? CLI_FAILURE : CLI_SUCCESS;
}

static int manager_digitmap(struct mansession *s, const struct message *m)
{
	int res = 0;
	int timeoutsuffix;
	const char *includes[AST_PBX_MAX_STACK];
	char buf[BUF_SIZE];
	struct map_geninfo geninfo = {
		.includecount = 0,
	};
	const char *id = astman_get_header(m, "ActionID");
	const char *context = astman_get_header(m, "Context");
	const char *timeoutsuffix_s = astman_get_header(m, "TimeoutSuffix");

	if (ast_strlen_zero(context)) {
		astman_send_error(s, m, "No context specified");
		return 0;
	}

	timeoutsuffix = !ast_strlen_zero(timeoutsuffix_s) && ast_true(timeoutsuffix_s);

	res = generate_digit_map(NULL, context, context, &geninfo, includes, buf, sizeof(buf), timeoutsuffix);
	if (res <= 0) {
		astman_send_error(s, m, "Could not generate digit map for requested context");
		return 0;
	}

	ast_debug(1, "Generated digit map length: %d: '%s'\n", res, buf + 1);
	astman_append(s, "Response: Success\r\n");
	if (!ast_strlen_zero(id)) {
		astman_append(s, "ActionID: %s\r\n", id);
	}
	astman_append(s, "DigitMap: %s\r\n\r\n", buf + 1); /* Skip leading | */
	return 0;
}

static struct ast_cli_entry generate_cli[] = {
	AST_CLI_DEFINE(handle_dialplan_generate_digitmap, "Generate device digit maps from the dialplan"),
};

static int unload_module(void)
{
	ast_manager_unregister("GenerateDigitMap");
	ast_cli_unregister_multiple(generate_cli, ARRAY_LEN(generate_cli));
	return 0;
}

static int load_module(void)
{
	int res = 0;
	res |= ast_cli_register_multiple(generate_cli, ARRAY_LEN(generate_cli));
	res |= ast_manager_register_xml("GenerateDigitMap", EVENT_FLAG_CONFIG | EVENT_FLAG_REPORTING, manager_digitmap);
	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Device Digit Map Generation");
