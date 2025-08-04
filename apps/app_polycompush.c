/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2025, Naveen Albert <asterisk@phreaknet.org>
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
 * \brief Polycom push notifications
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 */

/*** MODULEINFO
	<depend>res_curl</depend>
	<depend>curl</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <curl/curl.h>

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/utils.h"
#include "asterisk/app.h"

/*** DOCUMENTATION
	<application name="PolycomPush" language="en_US">
		<synopsis>
			Send push notification to Polycom IP phone(s)
		</synopsis>
		<syntax>
			<parameter name="device" required="yes" argsep="&amp;">
				<para>A list of devices to which the push notification should be sent.</para>
			</parameter>
			<parameter name="username" required="yes">
				<para>Push notification username.</para>
			</parameter>
			<parameter name="password" required="yes">
				<para>Push notification password.</para>
			</parameter>
			<parameter name="options" required="no">
				<optionlist>
					<option name="s">
						<para>Use HTTPS instead of HTTP for notify request(s).</para>
					</option>
				</optionlist>
			</parameter>
			<parameter name="message" required="yes">
				<para>Notification to push to device(s).</para>
			</parameter>
		</syntax>
		<description>
			<para>This application simplifies the process of delivering push notifications to Polycom IP phones.</para>
			<para>Provide the devices corresponding to the Polycom IP phones to which notifications should be sent,
			and this application will look up their current IP addresses and deliver the notification using
			the appropriate HTTP/HTTPS request.</para>
		</description>
	</application>
 ***/

static char *app = "PolycomPush";

static int get_ip(char *buf, size_t len, const char *device)
{
	char tmp[282];
	char workspace[256];
	char *endpoint;

	endpoint = strchr(device, '/');
	if (!endpoint) {
		ast_log(LOG_WARNING, "Invalid tech/device: %s\n", device);
		return -1;
	}

	endpoint++; /* Eat the slash so we just have the name without the tech */

	if (ast_begins_with(device, "PJSIP/")) {
		snprintf(tmp, sizeof(tmp), "PJSIP_AOR(%s,contact)", endpoint);
		if (ast_func_read(NULL, tmp, workspace, sizeof(workspace))) {
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
		snprintf(tmp, sizeof(tmp), "PJSIP_CONTACT(%s,via_addr)", workspace);
		if (ast_func_read(NULL, tmp, buf, len)) {
			ast_log(LOG_ERROR, "Failed to get IP address using contact %s\n", workspace);
			return -1;
		}
		ast_debug(3, "IP address of PJSIP/%s is '%s'\n", endpoint, buf);
	} else if (ast_begins_with(device, "SIP/")) {
		snprintf(tmp, sizeof(tmp), "SIPPEER(%s,via_addr)", endpoint);
		if (ast_func_read(NULL, tmp, buf, len)) {
			ast_log(LOG_ERROR, "Failed to get IP address for %s\n", endpoint);
			return -1;
		}
		ast_debug(3, "IP address of SIP/%s is '%s'\n", endpoint, buf);
	} else {
		ast_log(LOG_WARNING, "Unsupported channel technology: %s\n", device);
		return -1;
	}
	return 0;
}

enum {
	OPT_HTTPS = (1 << 0),
};

AST_APP_OPTIONS(app_opts, {
	AST_APP_OPTION('s', OPT_HTTPS),
});

static size_t curl_write_string_callback(char *rawdata, size_t size, size_t nmemb, void *userdata)
{
	struct ast_str **buffer = userdata;
	return ast_str_append(buffer, 0, "%.*s", (int) (size * nmemb), rawdata);
}

struct push_notify_target {
	const char *device;
	pthread_t thread;
	/* References to common data */
	const char *userpwd;
	const char *message;
	unsigned int https:1;
	AST_RWLIST_ENTRY(push_notify_target) entry;
	char data[];
};

AST_LIST_HEAD_NOLOCK(push_notify, push_notify_target);

static int notify(const char *device, int https, const char *userpwd, const char *message)
{
	char curl_errbuf[CURL_ERROR_SIZE + 1];
	struct curl_slist *headers;
	long http_code;
	CURL *curl;
	struct ast_str *str;
	char ip[128];
	char url[sizeof(ip) + 13];

	if (get_ip(ip, sizeof(ip), device)) {
		return -1;
	}

	curl = curl_easy_init();
	if (!curl) {
		return -1;
	}

	str = ast_str_create(512);
	if (!str) {
		curl_easy_cleanup(curl);
		return -1;
	}

	curl_errbuf[CURL_ERROR_SIZE] = '\0';
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);

	snprintf(url, sizeof(url), "http%s://%s/push", https ? "s" : "", ip);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	ast_debug(2, "Making request to %s, with payload: %s\n", url, message);

	headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/xhtml+xml");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	curl_easy_setopt(curl, CURLOPT_USERAGENT, AST_CURL_USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_string_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message);
	curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_DIGEST); /* Only digest auth is accepted */
	if (https) {
		/* The phones use self-signed certs */
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	}

	if (curl_easy_perform(curl)) {
		ast_log(LOG_WARNING, "%s\n", curl_errbuf);
		curl_easy_cleanup(curl);
		ast_free(str);
		return -1;
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	ast_verb(4, "%-36s [%ld] %s\n", device, http_code, ast_str_buffer(str));
	curl_easy_cleanup(curl);
	ast_free(str);
	return http_code != 200;
}

static void *notifier(void *varg)
{
	struct push_notify_target *p = varg;
	if (notify(p->device, p->https, p->userpwd, p->message)) {
		ast_log(LOG_WARNING, "Failed to push notification to %s\n", p->device);
	}
	return NULL;
}

static int predial_exec(struct ast_channel *chan, const char *data)
{
	struct ast_flags flags;
	char *parse;
	char *device, *devices;
	struct push_notify n;
	struct push_notify_target *p;
	char userpwd[128];
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(devices);
		AST_APP_ARG(username);
		AST_APP_ARG(password);
		AST_APP_ARG(options);
		AST_APP_ARG(message);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "Missing arguments\n");
		return -1;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	if (ast_strlen_zero(args.devices)) {
		ast_log(LOG_WARNING, "Missing devices\n");
		return -1;
	} else if (ast_strlen_zero(args.username) || ast_strlen_zero(args.password)) {
		ast_log(LOG_WARNING, "Must specify username and password\n");
		return -1;
	} else if (ast_strlen_zero(args.message)) {
		ast_log(LOG_WARNING, "Message is empty\n");
		return -1;
	}

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(app_opts, &flags, NULL, args.options);
	}

	snprintf(userpwd, sizeof(userpwd), "%s:%s", args.username, args.password);

	memset(&n, 0, sizeof(n));

	devices = args.devices;
	while ((device = strsep(&devices, "&"))) {
		/* Make the cURL requests in parallel, since they can be slow (1-2 seconds per endpoint) */
		p = ast_calloc(1, sizeof(*p) + strlen(device) + 1);
		if (!p) {
			continue;
		}
		strcpy(p->data, device);
		p->device = p->data;
		p->userpwd = userpwd;
		p->https = ast_test_flag(&flags, OPT_HTTPS);
		p->message = args.message;
		ast_debug(2, "Launching thread to handle %s\n", device);
		if (ast_pthread_create(&p->thread, NULL, notifier, p)) {
			ast_free(p);
			continue;
		}
		AST_LIST_INSERT_TAIL(&n, p, entry);
	}
	/* Wait for all the notifications to finish and clean up */
	ast_debug(2, "Waiting for threads to clean up\n");
	while ((p = AST_LIST_REMOVE_HEAD(&n, entry))) {
		pthread_join(p->thread, NULL);
		ast_free(p);
	}

	return 0;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, predial_exec);
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "Polycom Push Notifications",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.requires = "res_curl",
);
