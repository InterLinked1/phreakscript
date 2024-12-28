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
 * \brief Pre-dial helpers
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <regex.h>

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/utils.h"
#include "asterisk/app.h"

/*** DOCUMENTATION
	<application name="PreDial" language="en_US">
		<synopsis>
			Send pre-dial headers to an endpoint in a pre-dial handler
		</synopsis>
		<syntax>
			<parameter name="options">
				<optionlist>
					<option name="a">
						<para>Auto answer the call.</para>
					</option>
					<option name="c">
						<para>Specify a distinctive ringing cadence or ringtone to be used.</para>
						<para>The numeric 1-indexed cadence number should be provided as an argument.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>This application will, in a pre-dial handler, set up a call on a specific endpoint
			by sending the necessary IP headers for certain functionality that depends on them.</para>
			<para>This can be helpful in abstracting away vendor-specific implementation details
			from your call processing. Because a pre-dial handler executes on each device that is dialed,
			this application will be called uniquely for each device that is dialed and sent each device
			the correct headers, regardless of what headers may be sent to other endpoints.</para>
			<para>This application may be called technology-agnostically. Unsupported technologies
			are silently ignored.</para>
		</description>
		<see-also>
			<ref type="function">USER_AGENT</ref>
		</see-also>
	</application>
	<function name="USER_AGENT" language="en_US">
		<synopsis>
			Retrieve the user agent of the device associated with a channel
		</synopsis>
		<syntax>
			<parameter name="device" required="no">
				<para>The full tech/device to query for user agent.</para>
				<para>Non-IP technologies (e.g. DAHDI) are not supported.</para>
				<para>If not provided, the device associated with the current
				channel will be used.</para>
			</parameter>
		</syntax>
		<description>
			<para>This function retrieves the user agent of an endpoint.</para>
			<para>Unlike other channel functions, this may be used
			technology-agnostically.</para>
		</description>
		<see-also>
			<ref type="application">PreDial</ref>
		</see-also>
	</function>
 ***/

static char *app = "PreDial";

/*! \brief Unlike CHANNEL(useragent), this works even when the channel isn't fully "up" yet */
static int get_user_agent(struct ast_channel *chan, char *buf, size_t len, char *device)
{
	char tmp[282];
	char workspace[256];
	char device_name[256];
	char *endpoint;
	const char *tech;

	if (!chan) {
		ast_log(LOG_ERROR, "Missing channel\n");
		return -1;
	}

	if (!ast_strlen_zero(device)) { /* we were given a device to use */
		endpoint = strchr(device, '/');
		if (!endpoint) {
			ast_log(LOG_WARNING, "Invalid tech/device: %s\n", device);
			return -1;
		}
	} else { /* Current channel */
		ast_channel_get_device_name(chan, device_name, sizeof(device_name));
		if (ast_strlen_zero(device_name)) {
			ast_log(LOG_WARNING, "Unable to determine device name\n");
			return -1;
		}

		endpoint = strchr(device_name, '/');
		if (!endpoint) {
			ast_log(LOG_WARNING, "Unable to determine device name: %s\n", device_name);
			return -1;
		}
	}

	endpoint++; /* Eat the slash so we just have the name without the tech */

	if (!ast_channel_tech(chan)) { /* This could be NULL, like if evaluating a function from the CLI */
		ast_log(LOG_ERROR, "Channel has no tech?\n");
		return -1;
	}
	tech = ast_channel_tech(chan)->type;
	/* Can't use CHANNEL(useragent) in predial unfortunately... only works when channel is really there */
	if (!strcasecmp(tech, "PJSIP")) {
		snprintf(tmp, sizeof(tmp), "PJSIP_AOR(%s,contact)", endpoint);
		if (ast_func_read(chan, tmp, workspace, sizeof(workspace))) {
			ast_log(LOG_ERROR, "Failed to get contact for %s\n", endpoint);
			return -1;
		}
		if (ast_strlen_zero(workspace)) {
			ast_log(LOG_WARNING, "No AOR found for %s\n", endpoint);
			return -1;
		}
		ast_debug(3, "Contact for endpoint %s is %s\n", endpoint, workspace);
		/* If multiple contacts are present, then there's no real way to know which one to use.
		 * Just yell at the user that there should only be 1 contact! */
		if (strchr(workspace, ',')) {
			ast_log(LOG_WARNING, "Multiple contacts detected for endpoint '%s': %s\n", endpoint, workspace);
			/* This will probably fail now, but go ahead and fail anyways */
		}
		snprintf(tmp, sizeof(tmp), "PJSIP_CONTACT(%s,user_agent)", workspace);
		if (ast_func_read(chan, tmp, buf, len)) {
			ast_log(LOG_ERROR, "Failed to get user agent using contact %s\n", workspace);
			return -1;
		}
		ast_debug(1, "User agent of PJSIP/%s is '%s'\n", endpoint, buf);
	} else if (!strcasecmp(tech, "SIP")) {
		snprintf(tmp, sizeof(tmp), "SIPPEER(%s,useragent)", endpoint);
		if (ast_func_read(chan, tmp, buf, len)) {
			ast_log(LOG_ERROR, "Failed to get user agent for %s\n", endpoint);
			return -1;
		}
		ast_debug(1, "User agent of SIP/%s is '%s'\n", endpoint, buf);
	} else {
		ast_log(LOG_WARNING, "Unsupported channel technology: %s\n", ast_channel_tech(chan)->type);
		return -1;
	}
	return 0;
}

/*! \brief Somewhat arbitrary, but broken down by manufacturer / vendor / things that respond differently. */
enum user_agent_type {
	UA_UNKNOWN = 0,
	/* General vendors */
	UA_GRANDSTREAM,
	UA_POLYCOM,
	UA_POLYCOM_OBI,
	UA_CISCO,
	UA_LINKSYS,
	UA_PANASONIC,
	UA_DIGIUM,
	UA_MITEL,
	UA_YEALINK,
	/* Softphones and other specific things */
	UA_MICROSIP,
	UA_TSIP,
	UA_OTHER,
};

static int regex_match(char *str, char *regex)
{
	regex_t regexbuf;
	int errcode;
	int success = 0;

	if ((errcode = regcomp(&regexbuf, regex, REG_EXTENDED | REG_NOSUB))) {
		char errbuf[256];
		regerror(errcode, &regexbuf, errbuf, sizeof(errbuf));
		ast_log(LOG_WARNING, "Malformed input (%s): %s\n", regex, errbuf);
	} else if (!regexec(&regexbuf, str, 0, NULL, 0)) {
		success = 1;
	}

	regfree(&regexbuf);

	return success;
}

static enum user_agent_type parse_user_agent(struct ast_channel *chan)
{
	char user_agent[256];

	get_user_agent(chan, user_agent, sizeof(user_agent), NULL);

	if (regex_match(user_agent, "Grandstream")) {
		return UA_GRANDSTREAM;
	} else if (regex_match(user_agent, "Polycom")) {
		return UA_POLYCOM;
	} else if (regex_match(user_agent, "OBIHAI")) {
		return UA_POLYCOM_OBI;
	} else if (regex_match(user_agent, "Linksys")) {
		return UA_LINKSYS;
	} else if (regex_match(user_agent, "Cisco")) {
		return UA_CISCO;
	} else if (regex_match(user_agent, "Digium")) {
		return UA_DIGIUM;
	} else if (regex_match(user_agent, "Panasonic")) {
		return UA_PANASONIC;
	} else if (regex_match(user_agent, "Mitel")) {
		return UA_MITEL;
	} else if (regex_match(user_agent, "Yealink")) {
		return UA_YEALINK;
	} else if (regex_match(user_agent, "MicroSIP")) {
		return UA_MICROSIP;
	} else if (regex_match(user_agent, "tSIP")) {
		return UA_CISCO;
	}

	return UA_OTHER; /* Dunno what it is */
}

enum {
	OPT_CADENCE = (1 << 0),
	OPT_AUTOANSWER = (1 << 1),
};

enum {
	OPT_ARG_CADENCE,
	/* note: this entry _MUST_ be the last one in the enum */
	OPT_ARG_ARRAY_SIZE,
};

AST_APP_OPTIONS(app_opts, {
	AST_APP_OPTION('a', OPT_AUTOANSWER),
	AST_APP_OPTION_ARG('c', OPT_CADENCE, OPT_ARG_CADENCE),
});

static int send_header(struct ast_channel *chan, const char *header, const char *value)
{
	char buf[256];

	ast_debug(2, "Header(%s): %s\n", header, value);

	if (!strcasecmp(ast_channel_tech(chan)->type, "PJSIP")) {
		snprintf(buf, sizeof(buf), "PJSIP_HEADER(add,%s)", header);
		return ast_func_write(chan, buf, value);
	} else if (!strcasecmp(ast_channel_tech(chan)->type, "SIP")) {
		struct ast_app *app;
		app = pbx_findapp("SIPAddHeader");
		if (!app) {
			ast_log(LOG_WARNING, "%s application not registered\n", "SIPAddHeader");
			return -1;
		}
		snprintf(buf, sizeof(buf), "%s:%s", header, value);
		return pbx_exec(chan, app, buf);
	}

	ast_log(LOG_WARNING, "Unsupported channel technology: %s\n", ast_channel_tech(chan)->type);
	return -1;
}

static int predial_exec(struct ast_channel *chan, const char *data)
{
	/* Don't try to retrieve the user agent unless we need to. Apart from performance, that could throw warnings. */
	enum user_agent_type ua = 0;
	struct ast_flags flags;
	char *opt_args[OPT_ARG_ARRAY_SIZE];
	char *parse;
	char headerval[256];
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "Missing arguments\n");
		return -1;
	} else if (!chan) {
		ast_log(LOG_WARNING, "No channel?\n");
		return -1;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(app_opts, &flags, opt_args, args.options);
	}

	/* If in the future we do anything with DAHDI, check here first so we can return early. */

	if (!strcasecmp(ast_channel_tech(chan)->type, "DAHDI")) {
		/* No IP headers for DAHDI */
		return 0;
	}

	ua = parse_user_agent(chan);
	if (!ua) {
		ast_log(LOG_WARNING, "Unable to parse user agent\n");
		return -1;
	}
	ast_debug(2, "User agent is type %d\n", ua);

	if (ast_test_flag(&flags, OPT_CADENCE) && !ast_strlen_zero(opt_args[OPT_ARG_CADENCE])) {
		/* Send custom header for custom ring cadence */
		/* Applies only to PJSIP, etc. Not applicable to DAHDI, since cadence gets specified in dial string. */
		int cadence = atoi(opt_args[OPT_ARG_CADENCE]);
		if (cadence <= 0) {
			ast_log(LOG_WARNING, "Invalid cadence number: %s\n", opt_args[OPT_ARG_CADENCE]);
		} else if (ua > 0) {
			switch (ua) {
			case UA_GRANDSTREAM:
				snprintf(headerval, sizeof(headerval), "<http://127.0.0.1>;info=ring%d", cadence);
				break;
			case UA_POLYCOM:
			case UA_POLYCOM_OBI:
				snprintf(headerval, sizeof(headerval), "http://127.0.0.1/Bellcore-dr%d", cadence);
				break;
			case UA_YEALINK:
				snprintf(headerval, sizeof(headerval), "<>;info=Ring%d", cadence); /* Yealink doesn't have a defined text, it's configurable, so assume the text is Ring1, Ring2, etc. */
				break;
			case UA_CISCO:
			case UA_LINKSYS:
			default: /* Blindly fall back to this and hope for the best */
				snprintf(headerval, sizeof(headerval), "<http://127.0.0.1>;info=Bellcore-r%d", cadence);
				break;
			}
			/* Send the header */
			send_header(chan, "Alert-Info", headerval);
		}
	}

	/* Auto-Answer (IP phones/softphones only: N/A to Analog Telephone Adapters) */
	/*! \todo add support for auto-answer after X seconds. */
	if (ast_test_flag(&flags, OPT_AUTOANSWER)) {
		if (ua > 0) {
			switch (ua) {
			case UA_POLYCOM:
			case UA_POLYCOM_OBI:
				send_header(chan, "Alert-Info", "Auto Answer");
				break;
			case UA_PANASONIC:
				send_header(chan, "Alert-Info", "Intercom");
				break;
			case UA_DIGIUM:
				send_header(chan, "Alert-Info", "ring-answer");
				break;
			case UA_MITEL:
				send_header(chan, "Call-Info", "<sip:broadworks.net>;answer-after=0");
				break;
			case UA_MICROSIP:
				send_header(chan, "Call-Info", "Auto Answer");
				break;
			case UA_YEALINK:
				send_header(chan, "Call-Info", "answer-after=0");
				break;
			default:
				ast_log(LOG_WARNING, "Don't know how to send auto-answer to user agent type %d\n", ua);
				break;
			}
		}
	}

	return 0;
}

static int useragent_read(struct ast_channel *chan, const char *cmd, char *data, char *buffer, size_t buflen)
{
	return get_user_agent(chan, buffer, buflen, data);
}

static struct ast_custom_function useragent_function = {
	.name = "USER_AGENT",
	.read = useragent_read,
};

static int unload_module(void)
{
	int res = 0;

	res |= ast_custom_function_unregister(&useragent_function);
	res |= ast_unregister_application(app);

	return res;
}

static int load_module(void)
{
	int res = 0;

	res |= ast_custom_function_register(&useragent_function);
	res |= ast_register_application_xml(app, predial_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Predial helpers");
