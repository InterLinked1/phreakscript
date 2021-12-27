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
 * \brief Translate telephone number into tech/peer
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
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
#include "asterisk/conversions.h"
#include "asterisk/callerid.h"

/*** DOCUMENTATION
	<function name="NUM2DEVICE" language="en_US">
		<synopsis>
			Translates a telephone number into a dialable device
		</synopsis>
		<syntax>
			<parameter name="number" required="yes">
				<para>Number to use. Number must be the Caller ID
				defined in the relevant channel driver config file.</para>
			</parameter>
			<parameter name="index" required="no">
				<para>Index (starting with 1) of match to return, if
				multiple are found. Default is 1 (first match).</para>
			</parameter>
			<parameter name="options">
				<para>If no options are specified, all registered channel
				technologies will be queried. If options are specified, only
				the indicated channel technologies will be searched.</para>
				<para>Channel technologies are searched in alphabetic order.</para>
				<optionlist>
					<option name="d">
						<para>Check DAHDI.</para>
					</option>
					<option name="i">
						<para>Check IAX2.</para>
					</option>
					<option name="p">
						<para>Check PJSIP.</para>
					</option>
					<option name="s">
						<para>Check SIP. Note that SIP is deprecated,
						and this option will eventually be removed.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Translates a telephone number into a tech/device that can be used with <literal>Dial</literal>.</para>
			<para>This function requires no additional configuration to use. However, it is highly recommend
			that you configure and use hints instead of this function (see the <literal>HINT</literal> function for usage).</para>
			<example title="Dial 5551212">
			same => n,Dial(${NUM2DEVICE(5551212)})
			</example>
			<example title="Dial extension">
			exten => _X!,1,Dial(${NUM2DEVICE(${EXTEN})})
			</example>
		</description>
		<see-also>
			<ref type="function">HINT</ref>
		</see-also>
	</function>
 ***/

static int number_to_device(char *buffer, size_t buflen, const char *number, int index, int *currindex,
	const char *tech, const char *module, const char *cfgfile,
	const char *clidfield, const char *clidonlyfield, const char *addfilter)
{
	struct ast_config *cfg;
	struct ast_category *category = NULL;
	struct ast_flags config_flags = { CONFIG_FLAG_WITHCOMMENTS | CONFIG_FLAG_NOCACHE };
	char *filter;
	int filterlen, res = -1;

	if (!ast_module_check(module)) {
		ast_debug(1, "Module %s is not loaded, skipping\n", module);
		return -1; /* if module isn't loaded, how can it be relevant? */
	}
	if (!(cfg = ast_config_load2(cfgfile, "func_numpeer", config_flags))) {
		ast_debug(1, "Config file not found: %s\n", cfgfile);
		return -1;
	}
	if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_debug(1, "Config file has invalid format: %s\n", cfgfile);
		/* do not destroy config, that will crash */
		return -1;
	}

	filterlen = strlen(clidfield);
	if (clidonlyfield) {
		int strlen2 = strlen(clidonlyfield);
		if (strlen2 > filterlen) {
			filterlen = strlen2; /* ensures big enough for either */
		}
	}
	if (addfilter) {
		filterlen += strlen(addfilter) + 1; /* +1 for leading = that needs to be added */
	}
	filterlen += strlen(number);
	filterlen += 2; /* '=' for first regex, null terminator */

	filter = ast_malloc(filterlen);
	/* it seems to work correctly even if the filter "regex" doesn't account for the caller ID name */
	if (addfilter) {
		snprintf(filter, filterlen, "%s=%s,%s", clidfield, number, addfilter);
	} else {
		snprintf(filter, filterlen, "%s=%s", clidfield, number);
	}

	ast_debug(1, "Searching '%s' for categories with filter '%s' (filter len %d)\n", cfgfile, filter, filterlen);

	while ((category = ast_category_browse_filtered(cfg, NULL, category, filter))) {
		const char *catname, *callerid;
		char *name, *location;

		callerid = ast_variable_find(category, clidfield);

		if (ast_callerid_parse((char*) callerid, &name, &location)) {
			ast_debug(1, "Failed to parse '%s' as valid caller ID\n", callerid);
			continue;
		}
		catname = ast_category_get_name(category);
		if (!strcasecmp(location, number)) { /* this might be our guy */
			ast_debug(1, "Caller ID match %d/%d: category %s has caller ID %s\n", ++(*currindex), index, catname, location);
			/* but is it actually our guy? */
			if (*currindex == index) {
				/* yup, you're the one I want... */
				/*! \todo this should be the right format for Dial in general, but DAHDI may have additional requirements? Group/Number? */
				snprintf(buffer, buflen, "%s/%s", tech, catname);
				res = 0;
				break;
			}
		} else {
			ast_debug(1, "Caller ID non-match, skipping: category %s has caller ID %s\n", catname, location);
		}
	}

	ast_free(filter);
	ast_config_destroy(cfg);

	return res;
}

enum num_opts {
	OPT_DAHDI = (1 << 1),
	OPT_IAX2 = (1 << 2),
	OPT_PJSIP = (1 << 3),
	OPT_SIP = (1 << 4),
};

AST_APP_OPTIONS(num_opts, {
	AST_APP_OPTION('d', OPT_DAHDI),
	AST_APP_OPTION('i', OPT_IAX2),
	AST_APP_OPTION('p', OPT_PJSIP),
	AST_APP_OPTION('s', OPT_SIP),
});

static int acf_numpeer_exec(struct ast_channel *chan, const char *cmd,
			 char *parse, char *buffer, size_t buflen)
{
	struct ast_flags flags = {0};
	int index = 1, currindex = 0;
	int dahdi = 1, iax2 = 1, pjsip = 1, sip = 1;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(number);
		AST_APP_ARG(index);
		AST_APP_ARG(options);
	);

	AST_STANDARD_APP_ARGS(args, parse);

	if (ast_strlen_zero(args.number)) {
		ast_log(LOG_WARNING, "%s requires a number\n", cmd);
		return -1;
	}
	if (!ast_strlen_zero(args.index) && (ast_str_to_int(args.index, &index) || index < 1)) {
		ast_log(LOG_WARNING, "Invalid index: %s\n", args.index);
		return -1;
	}
	if (!ast_strlen_zero(args.options)) {
		dahdi = iax2 = pjsip = sip = 0;
		ast_app_parse_options(num_opts, &flags, NULL, args.options);
		if (ast_test_flag(&flags, OPT_DAHDI)) {
			dahdi = 1;
		}
		if (ast_test_flag(&flags, OPT_IAX2)) {
			iax2 = 1;
		}
		if (ast_test_flag(&flags, OPT_PJSIP)) {
			pjsip = 1;
		}
		if (ast_test_flag(&flags, OPT_SIP)) {
			sip = 1;
		}
	}

	if (dahdi && !number_to_device(buffer, buflen, args.number, index, &currindex, "DAHDI", "chan_dahdi", "chan_dahdi.conf", "callerid", "cid_number", NULL)) {
		return 0;
	}
	if (iax2 && !number_to_device(buffer, buflen, args.number, index, &currindex, "IAX2", "chan_iax2", "iax.conf", "callerid", NULL, NULL)) {
		return 0;
	}
	if (pjsip && !number_to_device(buffer, buflen, args.number, index, &currindex, "PJSIP", "chan_pjsip", "pjsip.conf", "callerid", NULL, "type=endpoint")) {
		return 0;
	}
	if (sip && !number_to_device(buffer, buflen, args.number, index, &currindex, "SIP", "chan_sip", "sip.conf", "callerid", NULL, NULL)) {
		return 0;
	}

	return -1;
}

static struct ast_custom_function acf_numpeer = {
	.name = "NUM2DEVICE",
	.read = acf_numpeer_exec,
};

static int unload_module(void)
{
	ast_custom_function_unregister(&acf_numpeer);

	return 0;
}

static int load_module(void)
{
	return ast_custom_function_register(&acf_numpeer);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Number to device peer function");
