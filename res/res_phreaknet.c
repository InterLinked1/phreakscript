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
 * \brief PhreakNet resource module
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 */

/*** MODULEINFO
	<depend>res_crypto</depend>
	<depend>curl</depend>
	<depend>pbx_config</depend>
	<use type="module">app_verify</use>
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
#include "asterisk/cli.h"
#include "asterisk/cdr.h"
#include "asterisk/acl.h"
#include "asterisk/causes.h"
#include "asterisk/ast_version.h"

/*** DOCUMENTATION
	<configInfo name="res_phreaknet" language="en_US">
		<synopsis>PhreakNet enhancement module</synopsis>
		<configFile name="res_phreaknet.conf">
			<configObject name="general">
				<configOption name="hostname" default="yes">
					<synopsis>PhreakNet hostname</synopsis>
					<description>
						<para>Public PhreakNet hostname of this machine, used for key rotation.</para>
					</description>
				</configOption>
				<configOption name="autokeyfetch" default="yes">
					<synopsis>Whether to automatically fetch RSA public keys periodically.</synopsis>
					<description>
						<para>Because new keypairs can be created at any point, and existing keys may also be rotated at any point, this feature
						ensures your system has the latest public keys for all other nodes. It is more efficient and accurate than relying on
						the <literal>KeyPrefetch</literal> application to download keys ad hoc, and also ensures that you have the public keys
						for nodes that have not attempted to call you before, ensuring that even the first call can use RSA authentication
						instead of being forced to fall back to MD5 authentication.</para>
					</description>
				</configOption>
				<configOption name="autokeyrotate" default="yes">
					<synopsis>Whether to automatically rotate the local PhreakNet RSA keypair once per day.</synopsis>
					<description>
						<para>Asterisk currently only supports 1024-bit RSA keys, which are widely considered insecure by modern computational attack standards.</para>
						<para>1024-bit keys can currently be cracked in about a year. A good security measure therefore is to ensure that keys are rotated often.
						Enabling this feature secures your system against current and future attack vectors on 1024-bit keys.</para>
					</description>
				</configOption>
				<configOption name="requesttoken" default="no">
					<synopsis>Whether to automatically request a token on outgoing PhreakNet calls.</synopsis>
					<description>
						<para>If the public IP address used for outgoing calls does not match your incoming IP address, you will need to request a token
						so that call verification succeeds.</para>
					</description>
				</configOption>
				<configOption name="fallbackwarning" default="yes">
					<synopsis>Provide audible warning tone if a call initially attempted using RSA authentication is forced to fallback to MD5 authentication</synopsis>
					<description>
						<para>If enabled, provides a brief burst of 440 Hz tone to indicate an authentication fallback.</para>
					</description>
				</configOption>
				<configOption name="blacklistthreshold" default="2.1">
					<synopsis>Blacklisting threshold for lookup requests.</synopsis>
					<description>
						<para>A value between 0 and 3 that indicates the threshold required to blacklist calls with a given score.</para>
					</description>
				</configOption>
				<configOption name="requirekeytoload" default="yes">
					<synopsis>Specifies whether this module should decline to load if API key is missing.</synopsis>
					<description>
						<para>If you need to load this module without an API key, you must disable this setting.</para>
					</description>
				</configOption>
			</configObject>
			<configObject name="intervals">
				<configOption name="keyfetch_interval" default="1800">
					<synopsis>How often (in seconds) all RSA public keys will be downloaded.</synopsis>
					<description>
						<para>Asterisk will fetch all RSA public keys periodically according to this frequency.</para>
						<para>Generally, a couple times per hour should be sufficient. A smaller setting will most likely unnecessarily increase
						API requests with no benefit, and a larger setting may result in stale keys being cached for a longer time than desired,
						potentially resulting in an increased result of failed RSA authenticated incoming calls to your node.</para>
					</description>
				</configOption>
				<configOption name="keyrotate_hour" default="4">
					<synopsis>The hour of the day (from 0-23), in system time, to rotate the local PhreakNet RSA keypair.</synopsis>
					<description>
						<para>Asterisk will attempt to rotate your keypair once per day, if <literal>autokeyrotate</literal> is enabled.</para>
						<para>You can control the hour of the day during which this rotation happens.
						You should set this to the hour of the day when the system has the fewest outgoing calls, so that other nodes will have more time
						to fetch your new public key before any calls are attempted to them. Hence, during the middle of the night is generally a good time.</para>
						<para>Note that the time specified is relative to the system time, not necessarily your local time or UTC, etc.</para>
						<para>The rotation will generally happen near the beginning of the hour specified.</para>
					</description>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
	<application name="PhreakNetDial" language="en_US">
		<synopsis>
			Dial a PhreakNet number
		</synopsis>
		<syntax>
			<parameter name="number">
				<para>PhreakNet number to call.</para>
			</parameter>
			<parameter name="trunkflags">
				<optionlist>
					<option name="m">
						<para>Support multifrequency (MF) signalling.</para>
					</option>
					<option name="s">
						<para>Support singlefrequency (SF) signalling.</para>
					</option>
				</optionlist>
			</parameter>
			<parameter name="authflags">
				<para>If this parameter is not specified, the default is to support both MD5 and RSA authentication.</para>
				<optionlist>
					<option name="m">
						<para>Support MD5 authentication.</para>
					</option>
					<option name="r">
						<para>Support RSA authentication.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Places a PhreakNet call.</para>
			<para>This application automatically handles lookup requests, out-verification, in-band signalling setup, and authentication fallbacks and retries.</para>
			<example title="Call 5551212">
			same => n,PhreakNetDial(5551212)
			</example>
		</description>
		<see-also>
			<ref type="function">PHREAKNET</ref>
		</see-also>
	</application>
	<function name="PHREAKNET" language="en_US">
		<synopsis>
			PhreakNet lookup and CDR function
		</synopsis>
		<syntax>
			<parameter name="property" required="true">
				<para>Must be one of the following:</para>
				<enumlist>
					<enum name="cdr">
						<para>W/O. Enable PhreakNet CDR on the channel</para>
					</enum>
					<enum name="lookup">
						<para>R/O. Check if a PhreakNet number exists</para>
					</enum>
					<enum name="lookup">
						<para>R/O. Perform a lookup for a PhreakNet number</para>
					</enum>
				</enumlist>
			</parameter>
			<parameter name="args" required="true">
				<para>Argument to the above property.</para>
			</parameter>
		</syntax>
		<description>
			<para>Usage varies depending on the property chosen.</para>
			<example title="Enable PhreakNet CDR on a channel">
			same => n,Set(PHREAKNET(CDR)=${EXTEN})
			</example>
			<example title="Check whether 555-1212 is a number that exists">
			same => n,Set(exists=${PHREAKNET(exists,5551212)})
			</example>
		</description>
		<see-also>
			<ref type="application">PhreakNetDial</ref>
		</see-also>
	</function>
 ***/

#define MODULE_NAME "res_phreaknet"
#define CONFIG_FILE "res_phreaknet.conf"
#define IAX_CONFIG_FILE "iax.conf"

/*! \brief Default IAX2 bindport is 4569 */
#define IAX_DEFAULT_BINDPORT 4569

#define IAX_CATEGORY_NAME "phreaknet"

/*! \brief Name of PhreakNet RSA keypair files */
#define MY_KEYPAIR_NAME "phreaknetrsa"

/*! \brief All API keys are 48 characters long */
#define INTERLINKED_API_KEYLEN 48

/*! \brief Default periodic task execution interval is twice per hour */
#define DEFAULT_KEYFETCH_INTERVAL 1800

/*! \brief Default rotation hour is around 4 a.m. system time */
#define DEFAULT_KEYROTATE_HOUR 4

/*! \brief Default blacklist threshold is 2.1 */
#define DEFAULT_BLACKLIST_THRESHOLD 2.1

struct {
	unsigned int autokeyfetch:1;
	unsigned int autokeyrotate:1;
	unsigned int requesttoken:1;
	unsigned int fallbackwarning:1;
	unsigned int requirekeytoload:1;
	/* Not set from config file */
	unsigned int autovonsupport:1;
} module_flags;

static int keyfetch_interval;
static int keyrotate_hour;
static float blacklist_threshold;

static char interlinked_api_key[INTERLINKED_API_KEYLEN + 1];
static char mainphreaknetdisa[8];

static char hostname_override[256] = "";

static pthread_t phreaknet_thread = AST_PTHREADT_NULL;
static ast_mutex_t refreshlock;
static ast_cond_t refresh_condition;
static int module_unloading = 0;
static int can_suspend = 0;

static int iax_bindport;

static int load_time;
static int reload_time;

struct {
	unsigned int keys_created;
	unsigned int keys_updated;
	unsigned int outgoing_calls;
	unsigned int current_outgoing_calls;
} phreaknet_stats;

ast_mutex_t stat_lock;

struct phreaknet_cdr_channel {
	char *channel;
	char ani[16];
	char caller[16];
	char callee[16];
	int startsecs;
	char starttime[20];
	char type[8]; /* direct, station, person, collect */
	AST_LIST_ENTRY(phreaknet_cdr_channel) entry; /*!< Next channel */
};

static AST_RWLIST_HEAD_STATIC(cdr_channels, phreaknet_cdr_channel);

/*! \note from test_res_prometheus.c */
/*! \todo replace with ast_curl_str_write_callback if/when merged */
static size_t curl_write_string_callback(char *rawdata, size_t size, size_t nmemb, void *userdata)
{
	struct ast_str **buffer = userdata;

	/* ast_str_append will return number of bytes written this round */
	return ast_str_append(buffer, 0, "%.*s", (int) (size * nmemb), rawdata);
}

static struct ast_str *curl_get(const char *url, const char *data)
{
	CURL **curl;
	CURLcode res;
	struct ast_str *str;
	long int http_code;
	char curl_errbuf[CURL_ERROR_SIZE + 1] = "";

	str = ast_str_create(512);
	if (!str) {
		return NULL;
	}

	curl = curl_easy_init();
	if (!curl) {
		ast_free(str);
		return NULL;
	}

	curl_easy_setopt(curl, CURLOPT_USERAGENT, AST_CURL_USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_string_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);

	ast_debug(6, "cURL URL: %s\n", url);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		if (*curl_errbuf) {
			ast_log(LOG_WARNING, "%s\n", curl_errbuf);
		}
		ast_log(LOG_WARNING, "Failed to curl URL '%s': %s\n", url, curl_easy_strerror(res));
		curl_easy_cleanup(curl);
		ast_free(str);
		return NULL;
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	curl_easy_cleanup(curl);

	ast_debug(3, "Response: %s\n", ast_str_buffer(str));
	if (http_code / 100 != 2) {
		ast_log(LOG_ERROR, "Failed to retrieve URL '%s': HTTP response code %ld\n", url, http_code);
		ast_free(str);
		return NULL;
	}

	return str;
}

static struct ast_str *curl_post(const char *url, const char *data)
{
	CURL **curl;
	CURLcode res;
	struct ast_str *str;
	long int http_code;
	char curl_errbuf[CURL_ERROR_SIZE + 1] = "";

	str = ast_str_create(512);
	if (!str) {
		return NULL;
	}

	curl = curl_easy_init();
	if (!curl) {
		ast_free(str);
		return NULL;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, -1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, AST_CURL_USER_AGENT);

	ast_debug(6, "cURL URL: %s\n", url);
	res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		if (*curl_errbuf) {
			ast_log(LOG_WARNING, "%s\n", curl_errbuf);
		}
		ast_log(LOG_WARNING, "Failed to curl URL '%s': %s\n", url, curl_easy_strerror(res));
		curl_easy_cleanup(curl);
		ast_free(str);
		return NULL;
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	curl_easy_cleanup(curl);

	ast_debug(3, "Response: %s\n", ast_str_buffer(str));
	if (http_code / 100 != 2) {
		ast_log(LOG_ERROR, "Failed to retrieve URL '%s': HTTP response code %ld\n", url, http_code);
		ast_free(str);
		return NULL;
	}

	return str;
}

#define ltrim(s) while (isspace(*s)) s++;

#define rtrim(s) { \
	if (s) { \
		char *back = s + strlen(s); \
		while (back != s && isspace(*--back)); \
		if (*s) { \
			*(back + 1) = '\0'; \
		} \
	} \
}

#define DATE_FORMAT "%Y-%m-%d %T"

static void cdr_channel_free(struct phreaknet_cdr_channel *channel)
{
	ast_free(channel->channel);
	ast_free(channel);
}

static void cdr_channel_cleanup(void)
{
	int removed = 0;
	struct phreaknet_cdr_channel *channel;

	/* In theory, the list should always be empty at this point. */
	AST_RWLIST_WRLOCK(&cdr_channels);
	while ((channel = AST_RWLIST_REMOVE_HEAD(&cdr_channels, entry))) {
		cdr_channel_free(channel);
		removed++;
	}

	AST_RWLIST_UNLOCK(&cdr_channels);
	if (removed) {
		/* Should never happen, since active CDRs will prevent this module from unloading. */
		ast_log(LOG_ERROR, "PhreakNet CDR forcibly terminated for %d channel%s\n", removed, ESS(removed));
	}
}

static struct phreaknet_cdr_channel *cdr_channel_applicable(const char *name, int remove)
{
	struct phreaknet_cdr_channel *channel, *channel2;
	int removed = 0;

	AST_RWLIST_WRLOCK(&cdr_channels);
	AST_RWLIST_TRAVERSE_SAFE_BEGIN(&cdr_channels, channel, entry) {
		if (!strcmp(channel->channel, name)) {
			if (remove) {
				phreaknet_stats.current_outgoing_calls--; /* Protected by the list WRLOCK */
				AST_RWLIST_REMOVE_CURRENT(entry);
				removed++;
			}
			break;
		}
	}
	AST_RWLIST_TRAVERSE_SAFE_END;

	if (remove) {
		if (removed) {
			ast_debug(5, "Removed %s from CDR list\n", name);
		} /* else, not necessarily an error, maybe wasn't a PhreakNet channel. */
		/* In theory, this could all go in the above if, but this is fine */
		AST_RWLIST_TRAVERSE(&cdr_channels, channel2, entry) {
			break;
		}
		if (channel2 && !ast_active_channels()) {
			/* This is a userland bug, not a module bug.
			 * If CDR is disabled on a channel that it's been PhreakNet-enabled on previously,
			 * before our callback is called for it, then we'll never get called to remove it
			 * from the list. If we happen to catch this, make some noise so user can fix the dialplan.
			 * This is also why the "phreaknet cdr cleanup" command exists. It's a hack, but has a purpose.
			 */
			ast_log(LOG_WARNING, "No channels left globally, but PhreakNet CDR channels remaining? Dialplan bug!\n");
			/* In the case where we hit 0 channels, we know we can just remove everything. */
			AST_RWLIST_TRAVERSE_SAFE_BEGIN(&cdr_channels, channel2, entry) {
				phreaknet_stats.current_outgoing_calls--; /* Protected by the list WRLOCK */
				AST_RWLIST_REMOVE_CURRENT(entry);
				removed++;
			}
			AST_RWLIST_TRAVERSE_SAFE_END;
			/* channel2 should now be NULL, so we can also go ahead and suspend */
		}
		if (!channel2) { /* List is empty now. */
			/* Suspend the CDR backend. This way we can unload res_phreaknet if there are any CDRs present on the entire system,
			 * as long as they are not PhreakNet CDRs, since we don't care about other stuff anyways.
			 * We set a flag here rather than calling ast_cdr_backend_suspend directly,
			 * because post_cdr in cdr.c gets a RDLOCK on the CDR callback list and then calls the CDR callback.
			 * Calling ast_cdr_backend_suspend will try to WRLOCK that list, which results in deadlock.
			 * So let the periodic thread handle this instead.
			 */
			can_suspend = 1;
		}
	}
	AST_RWLIST_UNLOCK(&cdr_channels);
	return channel;
}

#define ENSURE_STRLEN_NONZERO(s) \
	if (ast_strlen_zero(s)) { \
		ast_log(LOG_WARNING, "%s is empty\n", #s); \
		return -1; \
	}

static int cdr_channel_push(const char *name, const char *ani, const char *caller, const char *callee, const char *type)
{
	char *channelname;
	struct phreaknet_cdr_channel *channel, *channel2;
	struct timeval when = ast_tvnow();
	struct ast_tm tm;

	ENSURE_STRLEN_NONZERO(ani);
	ENSURE_STRLEN_NONZERO(caller);
	ENSURE_STRLEN_NONZERO(callee);

	if (cdr_channel_applicable(name, 0)) {
		ast_log(LOG_WARNING, "PhreakNet CDR already enabled for %s\n", name);
		return -1;
	}

	channelname = ast_strdup(name);
	if (!channelname) {
		return -1;
	}

	channel = ast_calloc(1, sizeof(*channel));
	if (!channel) {
		return -1;
	}

	channel->channel = channelname;
	ast_copy_string(channel->ani, ani, sizeof(channel->ani));
	ast_copy_string(channel->caller, caller, sizeof(channel->caller));
	ast_copy_string(channel->callee, callee, sizeof(channel->callee));
	ast_copy_string(channel->type, type, sizeof(channel->type));

	/* Save the start time in case CDR(start) is screwed up */
	channel->startsecs = time(NULL); /* Save EPOCH time */
	ast_localtime(&when, &tm, NULL);
	ast_strftime(channel->starttime, sizeof(channel->starttime), "%Y-%m-%d %H:%M:%S", &tm); /* Save string timestamp */

	AST_RWLIST_WRLOCK(&cdr_channels);
	can_suspend = 0;
	/* Unsuspend the module if this is the first active CDR now. */
	AST_RWLIST_TRAVERSE(&cdr_channels, channel2, entry) {
		break;
	}
	if (!channel2) {
		/* The list was empty, which means this will be our first CDR for the moment. Unsuspend CDR processing. */
		if (ast_cdr_backend_unsuspend(MODULE_NAME)) {
			ast_log(LOG_WARNING, "Failed to unsuspend CDR processing\n");
		} else {
			ast_debug(3, "Unsuspended CDR processing\n");
		}
	}
	AST_RWLIST_INSERT_HEAD(&cdr_channels, channel, entry);
	phreaknet_stats.current_outgoing_calls += 1; /* Protected by the list WRLOCK */
	AST_RWLIST_UNLOCK(&cdr_channels);

	ast_mutex_lock(&stat_lock);
	phreaknet_stats.outgoing_calls += 1;
	ast_mutex_unlock(&stat_lock);

	return 0;
}

static int cdr_handler(struct ast_cdr *cdr)
{
	char url[256];
	struct phreaknet_cdr_channel *cdr_channel;
	char *channel, *dstchannel;
	int duration, billsec;
	struct timeval *start, *answer, *end;
	char startstr[20], answerstr[20], endstr[20];
	struct ast_tm tm;
	struct ast_str *str;
	char *type;

	channel = cdr->channel;
	dstchannel = cdr->dstchannel;
	duration = cdr->duration;
	billsec = cdr->billsec;
	start = &cdr->start;
	answer = &cdr->answer;
	end = &cdr->end;

	ast_debug(2, "CDR %s -> %s: %d (%d)\n", channel, dstchannel, duration, billsec);
	cdr_channel = cdr_channel_applicable(channel, 1);
	if (!cdr_channel) {
		ast_debug(4, "Not a PhreakNet CDR on %s\n", channel);
		return 0; /* Not a PhreakNet CDR, skip */
	}

	ast_localtime(start, &tm, NULL); /* Use the local time zone here, the server will use UTC. */
	ast_strftime(startstr, sizeof(startstr), DATE_FORMAT, &tm);
	ast_localtime(end, &tm, NULL); /* Use the local time zone here, the server will use UTC. */
	ast_strftime(endstr, sizeof(endstr), DATE_FORMAT, &tm);

	if (ast_tvzero(*answer)) {
		ast_verb(3, "PhreakNet Call not answered (%s => %s), skipping ticket %s => %s\n", cdr_channel->caller, cdr_channel->callee, startstr, endstr);
		goto cleanup;
	}

	if (!billsec && answer->tv_sec) {
		int oldduration = duration;
		/* XXX May or may not be necessary anymore. Edge case where called party hangs up first... */
		/* Manually fix the duration. Unfortunately, this sets billsec=duration, which is wrong. */
		duration = duration ? duration : time(NULL) - cdr_channel->startsecs;
		ast_log(LOG_WARNING, "Call was answered, but billsec is 0? Adjusting duration from %d to %d\n", oldduration, duration);
	}

	ast_localtime(answer, &tm, NULL); /* Use the local time zone here, the server will use UTC. */
	ast_strftime(answerstr, sizeof(answerstr), DATE_FORMAT, &tm);

	type = S_OR(cdr_channel->type, "direct");

	if (!ast_strlen_zero(interlinked_api_key)) {
		char start_encoded[48];
		if (ast_uri_encode(startstr, start_encoded, sizeof(start_encoded), ast_uri_http) == NULL) {
			ast_log(LOG_WARNING, "Failed to encode timestamp %s\n", startstr);
			goto cleanup;
		}
		snprintf(url, sizeof(url), "https://api.phreaknet.org/v1/billing?key=%s&ani=%s&caller=%s&callee=%s&duration=%d&type=%s&start=%s",
			interlinked_api_key, cdr_channel->ani, cdr_channel->caller, cdr_channel->callee, duration, type, start_encoded);
		str = curl_get(url, NULL);
		if (str) {
			ast_free(str);
		} else {
			ast_log(LOG_WARNING, "Failed to ticket call\n");
		}
	} else {
		ast_log(LOG_WARNING, "No InterLinked API key loaded, skipping ticket\n");
	}

	ast_verb(3, "PhreakNet Call Ticketed (%s => %s) == %s => %s (%d:%02d)\n", cdr_channel->caller, cdr_channel->callee, startstr, endstr, billsec / 60, billsec % 60);

cleanup:
	cdr_channel_free(cdr_channel);
	return 0;
}

static char *file_to_string(const char *path, int maxlen)
{
	FILE *fp;
	char c;
	/* Since we know an upper bound on the (small) file size, just malloc all at once up front, rather than using realloc or an ast_str */
	char *m, *s = ast_malloc(maxlen);

	if (!s) {
		return NULL;
	}

	m = s; /* Keep a copy of the original malloc'd pointer, for ast_free */

	fp = fopen(path, "r");
	if (!fp) {
		ast_log(LOG_WARNING, "Failed to open %s\n", path);
		return NULL;
	}

	while ((c = fgetc(fp)) != EOF) {
		if (--maxlen <= 1) { /* Leave room for null terminator */
			ast_free(m);
			ast_log(LOG_WARNING, "Buffer overflow, aborting file read of %s\n", path);
			return NULL;
		}
		*s++ = c;
	}

	fclose(fp);
	*s++ = '\0';
	return m; /* Return start, not end */
}

static int file_equals_string(const char *path, const char *str)
{
	FILE *fp;
	char c;
	const char *s = str;
	int equal = 1;

	fp = fopen(path, "r");
	if (!fp) {
		ast_log(LOG_WARNING, "Failed to open %s\n", path);
		return 0;
	}

	while ((c = fgetc(fp)) != EOF) {
		if (*s++ != c) {
			equal = 0;
			break;
		}
	}

	fclose(fp);
	return equal;
}

static int save_system_output(char *buf, size_t len, const char *data)
{
	FILE *ptr;
	char plbuff[255];

	buf[0] = '\0';

	ptr = popen(data, "r");
	if (!ptr) {
		ast_log(LOG_WARNING, "Failed to execute shell command '%s'\n", data);
		return -1;
	}
	while (fgets(plbuff, sizeof(plbuff), ptr)) {
		strncat(buf, plbuff, len - strlen(buf) - 1);
	}
	pclose(ptr);
	return 0;
}

static int ip_is_valid(const char *ip)
{
	struct ast_ha *ha;

	if (!(ha = ast_calloc(1, sizeof(*ha)))) {
		return -1;
	}

	/* If we can't parse it as an IP, then it's no good. */
	if (!ast_sockaddr_parse(&ha->addr, ip, PARSE_PORT_FORBID)) {
		ast_log(LOG_WARNING, "Invalid IP address: %s\n", ip);
		ast_free_ha(ha);
		return -1;
	}

	ast_free_ha(ha);
	return 0;
}

static int get_fqdn(char *buf, size_t len)
{
	char ip[256];
	char hostname[256];
	char cmd[324];
	char *tmp;

	/* Don't use getaddrinfo because that won't always work for every system. */
	/* XXX This is really a total hack, we should find a way to do this "natively", if there is one...
	 * PhreakScript does ensure that dig gets installed as a pre-req, so, there's that...
	 */

	/* Get our public IP */
	if (save_system_output(ip, sizeof(ip), "dig +short myip.opendns.com @resolver4.opendns.com")) {
		return -1;
	}

	tmp = ip; /* We can't call rtrim on a char array directly (the address of ip will always evaluate as true), do it on a pointer */
	rtrim(tmp); /* Trim trailing LF */
	if (ip_is_valid(ip)) {
		return -1;
	}

	/* Get our FQDN */
	snprintf(cmd, sizeof(cmd), "dig +short -x %s", ip); /* Be very, very careful! */
	if (save_system_output(hostname, sizeof(hostname), cmd)) {
		return -1;
	}
	tmp = hostname;
	rtrim(tmp); /* Trim trailing LF */

	/* We seem to get a trailing '.' as well, trim that. */
	tmp = strrchr(hostname, '.');
	if (!tmp) {
		ast_log(LOG_WARNING, "FQDN appears to be local: %s\n", hostname);
		return -1;
	}
	if (*(tmp + 1) == '\0') {
		*tmp = '\0';
	}

	ast_debug(2, "Public IP: %s, FQDN: %s\n", ip, hostname);
	ast_copy_string(buf, hostname, len);
	return 0;
}

/*! \brief Update RSA public keys */
static int update_rsa_pubkeys(void)
{
	char *chunks, *chunk;
	struct ast_str *keystr;
	char url[256];
	char key_name[256];
	char key_file[261]; /* + .pub */
	struct ast_key *inkey;
	int fd;
	FILE *fp;
	char template[64] = "phreaknetkeyXXXXXX";
	char *tmp_filename;
	struct ast_category *category = NULL;
	struct ast_config *cfg = NULL;
	char *sfn = IAX_CONFIG_FILE, *dfn = IAX_CONFIG_FILE;
	struct ast_flags config_flags = { CONFIG_FLAG_WITHCOMMENTS | CONFIG_FLAG_NOCACHE };
	const char *currentinkeys = NULL;
	struct ast_str *keylist = NULL;
	int keys_updated = 0, keys_created = 0;
	char *port;

	if (ast_strlen_zero(interlinked_api_key)) {
		ast_log(LOG_WARNING, "No InterLinked API key is loaded, not fetching public keys\n");
		return -1;
	}

	ast_debug(1, "Synchronizing RSA public keys\n");

	snprintf(url, sizeof(url), "https://api.phreaknet.org/v1/rsa?key=%s&fetchall&version=%s", interlinked_api_key, ast_get_version());
	keystr = curl_get(url, NULL);
	if (!keystr) {
		return -1;
	}

	chunks = ast_str_buffer(keystr);

	while ((chunk = strsep(&chunks, "#"))) {
		char *key = strchr(chunk, '-'); /* -----BEGIN PUBLIC KEY----- */
		if (!key) {
			rtrim(key);
			if (!ast_strlen_zero(key)) { /* There will be some trailing whitespace at the end */
				ast_log(LOG_WARNING, "Chunk contains no key: %s\n", chunk);
			}
			continue;
		}
		if (key == chunk) { /* Chunk began with a '-' */
			ast_log(LOG_WARNING, "Chunk is missing hostname header: %s\n", chunk);
			continue;
		}

		ast_debug(5, "Processing key header: %s\n", chunk);
		*(key - 2) = '\0'; /* CR LF termination, so terminate at the CR instead of just at the LF. */

		port = strchr(chunk, ':');
		if (port) {
			*port = '.'; /* If the hostname has a port, change it to a '.'. Don't strip, since multiple servers could have same hostname but different port. */
		}

		rtrim(key); /* Trim any trailing CR LF */
		ast_debug(6, "Public key for '%s' is '%s'\n", chunk, key);

		/* Create the public key file, in a temp location first */
		fd = ast_file_fdtemp("/tmp", &tmp_filename, template);
		if (fd < 0) {
			ast_log(LOG_ERROR, "Failed to get temporary file descriptor\n");
			continue;
		}

		fp = fdopen(fd, "wb");
		if (!fp) {
			ast_log(LOG_WARNING, "Failed to create temp file\n");
			continue;
		}
		fprintf(fp, "%s", key);
		fclose(fp);

		/* Check if we already have a key for this host */
		snprintf(key_name, sizeof(key_name), "phreaknet-rsa-%s", chunk);
		snprintf(key_file, sizeof(key_file), "%s/%s.pub", ast_config_AST_KEY_DIR, key_name);
		inkey = ast_key_get(key_name, AST_KEY_PUBLIC);
		if (inkey) {
			if (!access(key_file, F_OK)) {
				if (file_equals_string(key_file, key)) {
					ast_debug(3, "Key %s has not changed\n", key_file);
					unlink(tmp_filename);
					goto cleanup;
				}
				if (unlink(key_file)) {
					ast_log(LOG_WARNING, "Failed to delete file %s: %s\n", key_file, strerror(errno));
					unlink(tmp_filename);
					goto cleanup;
				}
				ast_debug(3, "Deleted existing public key file %s\n", key_file);
			} else {
				/* Probably the actual files were deleted, but the keys are still loaded in memory.
				 * They might still be referenced in iax.conf as well. */
				ast_log(LOG_WARNING, "Strange, key %s is loaded, but file %s does not exist?\n", key_name, key_file);
			}
		}

		/* Create key file using public key material. We cannot simply use rename here as that
		 * /tmp and /var may be on seperate file systems. With systemd tmp_filename is needs to be relative to systemd
		 * private tmp directory */
		if ((fd = open(key_file, O_WRONLY | O_TRUNC | O_CREAT, AST_FILE_MODE)) < 0) {
			ast_log(LOG_WARNING, "Unable to open %s in write-only mode\n", key_file);
			unlink(tmp_filename);
			goto cleanup;
		}
		fp = fdopen(fd, "wb");
		if (!fp) {
			ast_log(LOG_WARNING, "Failed to create key file %s\n", key_file);
			unlink(tmp_filename);
			goto cleanup;
		}
		fprintf(fp, "%s", key);
		fclose(fp);
		unlink(tmp_filename);

		/* XXX Future improvement would be to delete public keys that no longer exist (and remove from chan_iax2 config) */

		ast_debug(2, "Successfully %s public key file %s\n", inkey ? "updated" : "created", key_name);
		if (inkey) {
			keys_updated++;
		} else {
			keys_created++;
			/* If this is a new key, we also need to add it to chan_iax2's config */
			if (!cfg) {
				/* Don't open the config unless we need to.
				 * But once we open the config, keep the handle open until we're all done. */
				if (!(cfg = ast_config_load2(sfn, MODULE_NAME, config_flags))) {
					ast_log(LOG_WARNING, "Config file not found: %s\n", sfn);
					goto cleanup;
				} else if (cfg == CONFIG_STATUS_FILEINVALID) {
					ast_log(LOG_WARNING, "Config file has invalid format: %s\n", sfn);
					cfg = NULL;
					goto cleanup;
				}

				keylist = ast_str_create(64);
				if (!keylist) {
					continue;
				}

				/* Find phreaknet inkeys */
				currentinkeys = ast_variable_retrieve(cfg, IAX_CATEGORY_NAME, "inkeys");
				if (currentinkeys) {
					ast_str_append(&keylist, 0, "%s", currentinkeys);
				} else {
					ast_log(LOG_WARNING, "Could not find category '%s' in IAX2 configuration\n", IAX_CATEGORY_NAME);
				}
			}
			if (keylist) {
				if (!strstr(ast_str_buffer(keylist), key_name)) {
					ast_str_append(&keylist, 0, "%s%s", ast_str_strlen(keylist) ? ":" : "", key_name);
				} else {
					/* If it's already listed in iax.conf as an inkey but we didn't have it for some reason previously, don't reappend it. */
					ast_debug(1, "Huh, previously had key '%s' in inkeys but not on disk\n", key_name);
				}
			}
		}
cleanup:
		ast_free(tmp_filename);
	}

	if (cfg) {
		/* Update iax.conf config */
		ast_debug(3, "New key list: %s\n", ast_str_buffer(keylist));
		category = ast_category_get(cfg, IAX_CATEGORY_NAME, NULL);
		if (category) {
			if (currentinkeys) {
				ast_debug(3, "Already have an inkeys list, updating it\n");
				if (ast_variable_update(category, "inkeys", ast_str_buffer(keylist), NULL, 0)) {
					ast_log(LOG_WARNING, "Failed to update inkeys\n"); /* at this point, it should exist, so it shouldn't fail */
				}
			} else {
				const char *newinkeys;
				struct ast_variable *v;

				ast_debug(3, "Found category '%s', but no inkeys variable, creating one now\n", IAX_CATEGORY_NAME);
				if (!(v = ast_variable_new("inkeys", ast_str_buffer(keylist), IAX_CONFIG_FILE))) {
					ast_log(LOG_WARNING, "Failed to create 'inkeys' variable for category '%s'\n", IAX_CATEGORY_NAME);
				} else {
					ast_variable_append(category, v);
				}
				newinkeys = ast_variable_find(category, "inkeys");
				if (strcmp(newinkeys, ast_str_buffer(keylist))) {
					ast_log(LOG_WARNING, "Failed to save new inkeys list, expected '%s' but have '%s'\n", ast_str_buffer(keylist), newinkeys);
				}
			}
			ast_include_rename(cfg, sfn, dfn); /* change the include references from dfn to sfn, so things match up */
			if (ast_config_text_file_save2(dfn, cfg, MODULE_NAME, 0)) {
				ast_log(LOG_WARNING, "Failed to save config\n");
			}
		} else {
			ast_log(LOG_WARNING, "No %s category?\n", IAX_CATEGORY_NAME);
		}
		ast_config_destroy(cfg);
		ast_free(keylist);
	}

	ast_free(keystr);

	/* Reload anything that needs to be reloaded */
	if ((keys_updated || keys_created) && ast_module_reload("res_crypto") != AST_MODULE_RELOAD_SUCCESS) {
		ast_log(LOG_WARNING, "Failed to reload res_crypto\n");
	} else if (keys_created && ast_module_reload("chan_iax2") != AST_MODULE_RELOAD_SUCCESS) {
		ast_log(LOG_WARNING, "Failed to reload chan_iax2\n");
	}

	ast_mutex_lock(&stat_lock);
	phreaknet_stats.keys_updated += keys_updated;
	phreaknet_stats.keys_created += keys_created;
	ast_mutex_unlock(&stat_lock);
	return 0;
}

static int iax_purge_keys(void)
{
	struct ast_category *category = NULL;
	struct ast_config *cfg = NULL;
	char *sfn = IAX_CONFIG_FILE, *dfn = IAX_CONFIG_FILE;
	struct ast_flags config_flags = { CONFIG_FLAG_WITHCOMMENTS | CONFIG_FLAG_NOCACHE };

	if (!(cfg = ast_config_load2(sfn, MODULE_NAME, config_flags))) {
		ast_log(LOG_WARNING, "Config file not found: %s\n", sfn);
		return -1;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_WARNING, "Config file has invalid format: %s\n", sfn);
		return -1;
	}

	/* Find phreaknet inkeys */
	while ((category = ast_category_browse_filtered(cfg, IAX_CATEGORY_NAME, category, NULL))) {
		const char *currentinkeys = ast_variable_find(category, "inkeys");
		if (currentinkeys) {
			if (ast_variable_update(category, "inkeys", "", NULL, 0)) {
				ast_log(LOG_WARNING, "Failed to update inkeys\n"); /* at this point, it should exist, so it shouldn't fail */
			}
			ast_include_rename(cfg, sfn, dfn); /* change the include references from dfn to sfn, so things match up */
			if (ast_config_text_file_save2(dfn, cfg, MODULE_NAME, 0)) {
				ast_log(LOG_WARNING, "Failed to save config\n");
			}
		}
		break;
	}
	ast_config_destroy(cfg);

	if (ast_module_reload("res_crypto") != AST_MODULE_RELOAD_SUCCESS) {
		ast_log(LOG_WARNING, "Failed to reload res_crypto\n");
	} else if (ast_module_reload("chan_iax2") != AST_MODULE_RELOAD_SUCCESS) {
		ast_log(LOG_WARNING, "Failed to reload chan_iax2\n");
	}
	return 0;
}

#define MY_KEYPAIR_TEMPFILE_PREFIX "/tmp/" MY_KEYPAIR_NAME

/*! \brief Generate a new or rotated PhreakNet public key pair and upload it to the server */
static int gen_keypair(int rotate)
{
	char fqdn[256];
	char postdata[892]; /* More than 2*256, since the fqdn and key pair itself are each about 256-280 */
	char privkeyfile[256];
	char pubkeyfile[256];
	char uploadkey[512];
	char *pubkey;
	struct ast_str *str;
	const char *clli;
	struct stat st;
	char *const argv[] = { "astgenkey", "-q", "-n", MY_KEYPAIR_TEMPFILE_PREFIX, NULL };

	if (ast_strlen_zero(interlinked_api_key)) {
		/* Can't successfully upload to server with no API key */
		ast_log(LOG_WARNING, "No InterLinked API key found, aborting keypair generation\n");
		return -1;
	}

	/* If we want to rotate our keys, we better have a keypair to begin with. */
	if (rotate) {
		if (!ast_key_get(MY_KEYPAIR_NAME, AST_KEY_PUBLIC) || !ast_key_get(MY_KEYPAIR_NAME, AST_KEY_PRIVATE)) {
			ast_log(LOG_WARNING, "Can't rotate key %s, because it doesn't currently exist?\n", MY_KEYPAIR_NAME);
			return -1;
		}
	}
	snprintf(privkeyfile, sizeof(privkeyfile), "%s/%s.key", ast_config_AST_KEY_DIR, MY_KEYPAIR_NAME);
	snprintf(pubkeyfile, sizeof(pubkeyfile), "%s/%s.pub", ast_config_AST_KEY_DIR, MY_KEYPAIR_NAME);
	if (!access(pubkeyfile, F_OK)) {
		if (!rotate) {
			ast_log(LOG_WARNING, "Key file %s already exists, aborting keypair generation\n", pubkeyfile);
			return -1;
		}
		/* If these keys already exist, make sure the files (or at least the public key) are writable by this user. If not, this will fail. */
		if (access(pubkeyfile, W_OK)) {
			/* Silly user, make the keys writable and try again. */
			ast_log(LOG_WARNING, "File %s exists but is not writable, aborting keypair generation\n", pubkeyfile);
			return -1;
		}
	}

	/* If we can't determine our FQDN, bail now, before we do any rotation. */
	if (!ast_strlen_zero(hostname_override)) {
		ast_copy_string(fqdn, hostname_override, sizeof(fqdn));
	} else if (get_fqdn(fqdn, sizeof(fqdn) - 8)) {
		ast_log(LOG_WARNING, "Failed to determine FQDN, aborting keypair %s\n", rotate ? "rotation" : "generation");
		return -1;
	}

	/* We need to tack :port on to the hostname if our bindport isn't the default */
	if (iax_bindport != IAX_DEFAULT_BINDPORT) {
		char bindport[6];
		snprintf(bindport, sizeof(bindport), ":%d", iax_bindport);
		strncat(fqdn, bindport, sizeof(fqdn) - 2);
		fqdn[sizeof(fqdn) - 1] = '\0';
	}

	/* Generate the new keypair, and wait for it to finish. */
	ast_debug(3, "Executing: astgenkey -q -n %s\n", MY_KEYPAIR_TEMPFILE_PREFIX);
	if (ast_safe_execvp(0, "astgenkey", argv) == -1) {
		ast_log(LOG_WARNING, "Failed to execute astgenkey: %s\n", strerror(errno));
		return -1;
	} else if (stat(MY_KEYPAIR_TEMPFILE_PREFIX ".key", &st)) {
		ast_log(LOG_WARNING, "Failed to stat %s: %s\n", MY_KEYPAIR_TEMPFILE_PREFIX ".key", strerror(errno));
		return -1;
	}

	/* astgenkey will create files in the current working directory, not ast_config_AST_KEY_DIR.
	 * We could chdir after we fork but before we execvp, but let's just rename the file from what it is to what it should be. */
	if (rename(MY_KEYPAIR_TEMPFILE_PREFIX ".key", privkeyfile)) {
		ast_log(LOG_ERROR, "Failed to rename %s to %s: %s\n", MY_KEYPAIR_TEMPFILE_PREFIX ".key", privkeyfile, strerror(errno));
		return -1;
	} else if (rename(MY_KEYPAIR_TEMPFILE_PREFIX ".pub", pubkeyfile)) {
		ast_log(LOG_ERROR, "Failed to rename %s to %s: %s\n", MY_KEYPAIR_TEMPFILE_PREFIX ".pub", pubkeyfile, strerror(errno));
		return -1;
	}

	/* Read the public key into a string */
	pubkey = file_to_string(pubkeyfile, 300); /* Based on the actual length of 1024-bit public keys (~276), this should be more than enough. */
	if (!pubkey) {
		ast_log(LOG_WARNING, "Failed to read public key at %s\n", pubkeyfile);
		return -1;
	}

	rtrim(pubkey); /* Trim any trailing whitespace */
	ast_debug(6, "Trimmed public key: %s\n", pubkey);
	if (ast_uri_encode(pubkey, uploadkey, sizeof(uploadkey), ast_uri_http) == NULL) {
		ast_log(LOG_WARNING, "Failed to encode public key %s\n", pubkeyfile);
		return -1;
	}

	ast_free(pubkey);
	ast_debug(3, "New public key: %s\n", uploadkey);

	/* Upload the key. */

	/* Central keypair server now supports just using FQDN to update, so clli is optional, but recommended */
	clli = pbx_builtin_getvar_helper(NULL, "phreaknetclli");
	if (!clli) {
		clli = pbx_builtin_getvar_helper(NULL, "clli");
	}
	if (!clli) {
		ast_log(LOG_WARNING, "Could not autodetect switch CLLI, are your [globals] set?\n");
	}

	snprintf(postdata, sizeof(postdata), "key=%s&hostname=%s&clli=%s&rsakey=%s", interlinked_api_key, fqdn, S_OR(clli, ""), uploadkey);
	str = curl_post("https://api.phreaknet.org/v1/rsa", postdata);
	if (!str) {
		ast_log(LOG_WARNING, "Failed to upload RSA public key to server\n");
		return -1;
	}
	ast_free(str); /* Response not needed */

	if (ast_module_reload("res_crypto") != AST_MODULE_RELOAD_SUCCESS) {
		ast_log(LOG_WARNING, "Failed to reload res_crypto\n");
		return -1;
	}
	/* We can't check if the key is loaded until we actually reload res_crypto, so do this at the very end. */
	if (!ast_key_get(MY_KEYPAIR_NAME, AST_KEY_PUBLIC) || !ast_key_get(MY_KEYPAIR_NAME, AST_KEY_PRIVATE)) {
		ast_log(LOG_WARNING, "Created key %s, but unable find it?\n", MY_KEYPAIR_NAME);
		return -1;
	}
	ast_verb(3, "Successfully rotated PhreakNet keypair\n");
	return 0;
}

/*! \brief Directory iterator callback */
static int on_keyfile(const char *dir_name, const char *filename, void *obj)
{
	char fullpath[PATH_MAX];
	int *fdptr = obj;
	int fd = *fdptr;

	ast_debug(4, "Found file: %s\n", filename);
	if (strncmp("phreaknet-rsa-", filename, strlen("phreaknet-rsa-"))) { /* I trust gcc will optimize this */
		ast_debug(3, "%s does not begin with desired prefix\n", filename);
		return 0; /* Not a PhreakNet key. */
	}
	if (!strstr(filename, ".pub")) {
		ast_debug(3, "%s is not a public key file\n", filename);
		return 0; /* Not a public key. */
	}

	snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_name, filename);
	if (unlink(fullpath)) {
		ast_log(LOG_WARNING, "Failed to delete file %s: %s\n", fullpath, strerror(errno));
	} else {
		ast_cli(fd, "Deleted %s\n", fullpath);
	}

	return 0;
}

static char *handle_show_settings(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "phreaknet show settings";
		e->usage =
			"Usage: phreaknet show settings\n"
			"       Show module settings.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

#define CLI_FMT_S "%-34s %s\n"
#define CLI_FMT_D "%-34s %d\n"
#define CLI_FMT_F "%-34s %1.2f\n"
	ast_cli(a->fd, CLI_FMT_S, "InterLinked API key loaded", strlen(interlinked_api_key) == INTERLINKED_API_KEYLEN ? "Yes" : "No");
	ast_cli(a->fd, CLI_FMT_S, "Main PhreakNet DISA", S_OR(mainphreaknetdisa, "Not Loaded"));
	ast_cli(a->fd, CLI_FMT_D, "IAX2 bindport", iax_bindport);
	ast_cli(a->fd, CLI_FMT_S, "Autofetch public keys", AST_CLI_YESNO(module_flags.autokeyfetch));
	ast_cli(a->fd, CLI_FMT_D, "Keyfetch frequency (s)", keyfetch_interval);
	ast_cli(a->fd, CLI_FMT_S, "Autorotate keypair", AST_CLI_YESNO(module_flags.autokeyrotate));
	ast_cli(a->fd, CLI_FMT_D, "Autorotate hour of day", keyrotate_hour);
	ast_cli(a->fd, CLI_FMT_S, "Request Token (outgoing calls)", AST_CLI_YESNO(module_flags.requesttoken));
	ast_cli(a->fd, CLI_FMT_S, "Fallback Warning (outgoing calls)", AST_CLI_YESNO(module_flags.fallbackwarning));
	ast_cli(a->fd, CLI_FMT_S, "Require Key To Load", AST_CLI_YESNO(module_flags.requirekeytoload));
	ast_cli(a->fd, CLI_FMT_F, "Blacklist threshold", blacklist_threshold);
	ast_cli(a->fd, CLI_FMT_S, "AUTOVON/MLPP support", AST_CLI_YESNO(module_flags.autovonsupport));
#undef CLI_FMT_S
#undef CLI_FMT_D
#undef CLI_FMT_F
	return CLI_SUCCESS;
}

static char *handle_show_stats(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	int now = time(NULL);

	switch (cmd) {
	case CLI_INIT:
		e->command = "phreaknet show stats";
		e->usage =
			"Usage: phreaknet show stats\n"
			"       Show module stats.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	ast_cli(a->fd, "%u cumulative outgoing call%s.\n", phreaknet_stats.outgoing_calls, ESS(phreaknet_stats.outgoing_calls));
	ast_cli(a->fd, "%u current outgoing call%s.\n", phreaknet_stats.current_outgoing_calls, ESS(phreaknet_stats.current_outgoing_calls));
	ast_cli(a->fd, "%u RSA key creation%s.\n", phreaknet_stats.keys_created, ESS(phreaknet_stats.keys_created));
	ast_cli(a->fd, "%u RSA key update%s.\n", phreaknet_stats.keys_updated, ESS(phreaknet_stats.keys_updated));

	ast_cli(a->fd, "Module loaded %d seconds ago\n", now - load_time);
	if (reload_time) {
		ast_cli(a->fd, "Module last reloaded %d seconds ago\n", now - reload_time);
	}
	return CLI_SUCCESS;
}

static char *handle_cdr_show(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	int count = 0;
	int now = time(NULL);
	struct phreaknet_cdr_channel *channel;

	switch (cmd) {
	case CLI_INIT:
		e->command = "phreaknet cdr show";
		e->usage =
			"Usage: phreaknet cdr show\n"
			"       Show outgoing call CDR stats.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	AST_RWLIST_RDLOCK(&cdr_channels);
	AST_RWLIST_TRAVERSE(&cdr_channels, channel, entry) {
		if (!count) {
			/* Print heading only if there are calls */
			ast_cli(a->fd, "%-20s %8s %15s %15s %10s %7s %s\n", "Start Time", "Duration", "ANI", "Caller", "Destination", "Type", "Channel");
		}
		ast_cli(a->fd, "%-20s %8d %15s %15s %10s %7s %s\n", channel->starttime, now - channel->startsecs,
			channel->ani, channel->caller, channel->callee, S_OR(channel->type, "direct"), channel->channel);
		count++;
	}
	AST_RWLIST_UNLOCK(&cdr_channels);

	ast_cli(a->fd, "%d active outgoing PhreakNet call%s\n", count, ESS(count));
	return CLI_SUCCESS;
}

static char *handle_cdr_cleanup(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct phreaknet_cdr_channel *channel;
	struct ast_channel *ochan;
	int removed = 0;

	switch (cmd) {
	case CLI_INIT:
		e->command = "phreaknet cdr cleanup";
		e->usage =
			"Usage: phreaknet cdr cleanup\n"
			"       Reconciles PhreakNet CDRs against Asterisk channels.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	AST_RWLIST_WRLOCK(&cdr_channels);
	AST_RWLIST_TRAVERSE_SAFE_BEGIN(&cdr_channels, channel, entry) {
		ochan = ast_channel_get_by_name(channel->channel);
		if (ochan) {
			ast_channel_unref(ochan);
			continue;
		}
		phreaknet_stats.current_outgoing_calls--; /* Protected by the list WRLOCK */
		AST_RWLIST_REMOVE_CURRENT(entry);
		removed++;
	}
	AST_RWLIST_TRAVERSE_SAFE_END;
	AST_RWLIST_UNLOCK(&cdr_channels);

	ast_cli(a->fd, "Removed %d stale CDR record%s\n", removed, ESS(removed));
	return CLI_SUCCESS;
}

static char *handle_create_keypair(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "phreaknet keypair create";
		e->usage =
			"Usage: phreaknet keypair create\n"
			"       Create a new PhreakNet RSA public keypair.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	return gen_keypair(0) ? CLI_FAILURE : CLI_SUCCESS;
}

static char *handle_rotate_keypair(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "phreaknet keypair rotate";
		e->usage =
			"Usage: phreaknet keypair rotate\n"
			"       Rotate an existing PhreakNet RSA public keypair.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	return gen_keypair(1) ? CLI_FAILURE : CLI_SUCCESS;
}

static char *handle_fetch_keys(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "phreaknet keys fetch";
		e->usage =
			"Usage: phreaknet keys fetch\n"
			"       Fetch all PhreakNet RSA public keys.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	return update_rsa_pubkeys() ? CLI_FAILURE : CLI_SUCCESS;
}

static char *handle_purge_keys(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	int fd = a->fd;

	switch (cmd) {
	case CLI_INIT:
		e->command = "phreaknet keys purge";
		e->usage =
			"Usage: phreaknet keys purge\n"
			"       Delete all foreign PhreakNet RSA public keys.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	ast_file_read_dir(ast_config_AST_KEY_DIR, on_keyfile, &fd);
	iax_purge_keys();
	return CLI_SUCCESS;
}

static struct ast_cli_entry phreaknet_cli[] = {
	AST_CLI_DEFINE(handle_show_settings, "Display module settings"),
	AST_CLI_DEFINE(handle_show_stats, "Display status of registrations"),
	AST_CLI_DEFINE(handle_cdr_show, "Display status of outgoing PhreakNet calls"),
	AST_CLI_DEFINE(handle_cdr_cleanup, "Reconciles active PhreakNet CDRs with channels list"),
	AST_CLI_DEFINE(handle_create_keypair, "Create a new PhreakNet RSA public key pair"),
	AST_CLI_DEFINE(handle_rotate_keypair, "Rotate existing PhreakNet RSA public key pair"),
	AST_CLI_DEFINE(handle_fetch_keys, "Fetch all PhreakNet RSA public keys"),
	AST_CLI_DEFINE(handle_purge_keys, "Purge all foreign PhreakNet RSA public keys"),
};

static int find_bindport(int reload)
{
	struct ast_config *cfg;
	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };
	const char *varval;

	iax_bindport = IAX_DEFAULT_BINDPORT; /* Default bindport */
	if (!(cfg = ast_config_load(IAX_CONFIG_FILE, config_flags))) {
		ast_debug(1, "Config file %s not found\n", IAX_CONFIG_FILE);
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		ast_debug(1, "Config file %s unchanged, skipping\n", CONFIG_FILE);
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Config file %s is in an invalid format. Aborting.\n", CONFIG_FILE);
		return -1;
	}

	if ((varval = ast_variable_retrieve(cfg, "general", "bindport")) && !ast_strlen_zero(varval)) {
		int tmp;
		if (!ast_str_to_int(varval, &tmp)) {
			ast_log(LOG_WARNING, "Invalid IAX2 bindport %s\n", varval);
		} else {
			iax_bindport = tmp;
			ast_config_destroy(cfg);
			return 0;
		}
	}
	ast_config_destroy(cfg);
	return -1;
}

#define load_global_var(var, varname, force) __load_global_var(var, sizeof(var), varname, force)

static int __load_global_var(char *buf, size_t len, const char *varname, int force)
{
	if (force || ast_strlen_zero(buf)) {
		const char *globalvarval;
		globalvarval = pbx_builtin_getvar_helper(NULL, varname);
		if (globalvarval) {
			ast_copy_string(buf, globalvarval, len);
			if (!force && !ast_strlen_zero(buf)) {
				ast_debug(1, "Variable '%s' previously empty but now loaded\n", varname);
			}
			return !ast_strlen_zero(buf) ? 0 : -1;
		} else {
			ast_log(LOG_WARNING, "Unable to load %s global variable\n", varname);
			return -1;
		}
	}
	return 0;
}

static int load_global_vars(int force)
{
	int res = 0;

	res |= load_global_var(interlinked_api_key, "interlinkedkey", force);
	res |= load_global_var(mainphreaknetdisa, "mainphreaknetdisa", force);

	module_flags.autovonsupport = ast_exists_extension(NULL, "autovonpreempted-callee", "0", 1, NULL);

	return res;
}

/*! \brief Single thread to periodically do things */
static void *phreaknet_periodic(void *varg)
{
	unsigned int last_autokeyrotate = 0;
	unsigned int iterations = 0;

	if (!ast_fully_booted) {
		while (!ast_fully_booted) {
			usleep(500000); /* Don't start tasks immediately as soon as module is loaded if Asterisk is still starting. */
		}

		/* Finish loading settings */
		find_bindport(0); /* Determine what IAX2 bindport we're using. */
		/* Shouldn't be needed now, since we explicitly require pbx_config to load before we do: */
		load_global_vars(0); /* Global variables may not have been loaded when the module loaded (possibly due to our load order priority), if during startup. */
	}

	/* Okay, *now* we're really ready to roll. */

	for (;;) {
		struct timeval now = ast_tvnow();
		struct timespec ts = {0,};
		struct ast_tm tm;

		/* Wake up, and see if there are any tasks that need to be executed. */
		ast_localtime(&now, &tm, NULL);

		AST_RWLIST_WRLOCK(&cdr_channels); /* Use a WRLOCK for the integrity of can_suspend */
		if (can_suspend) {
			if (ast_cdr_backend_suspend(MODULE_NAME)) {
				ast_log(LOG_WARNING, "Failed to suspend CDR processing\n");
			} else {
				ast_debug(3, "Suspended CDR processing\n");
			}
			can_suspend = 0;
		}
		AST_RWLIST_UNLOCK(&cdr_channels);

		/* If we're going to rotate our keypair, do it before we fetch them all, so we get our own back. Don't autorotate more than once in the target hour. */
		if (module_flags.autokeyrotate && tm.tm_hour == keyrotate_hour && (!last_autokeyrotate || last_autokeyrotate < iterations - 120)) {
			gen_keypair(ast_key_get(MY_KEYPAIR_NAME, AST_KEY_PUBLIC) ? 1 : 0);
			last_autokeyrotate = iterations;
		}

		/* Fetch all the public keys. */
		if (module_flags.autokeyfetch && iterations % keyfetch_interval == 0) {
			update_rsa_pubkeys();
		}

		/* Wait a minute. */
		ast_mutex_lock(&refreshlock);
		ts.tv_sec = (now.tv_sec + 60) + 1;
		ast_cond_timedwait(&refresh_condition, &refreshlock, &ts);
		ast_mutex_unlock(&refreshlock);

		if (module_unloading) {
			break;
		}
		iterations++;
	}

	return NULL;
}

static int load_config(int reload)
{
	const char *cat = NULL;
	struct ast_config *cfg;
	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };
	struct ast_variable *var;
	int tmp;
	int decline = 0;

	/* Set (or reset) defaults. */
	hostname_override[0] = '\0';;
	memset(&module_flags, 0, sizeof(module_flags));
	module_flags.autokeyfetch = 1;
	module_flags.autokeyrotate = 1;
	module_flags.fallbackwarning = 1;
	module_flags.requirekeytoload = 1;
	keyfetch_interval = DEFAULT_KEYFETCH_INTERVAL;
	keyrotate_hour = DEFAULT_KEYROTATE_HOUR;
	blacklist_threshold = DEFAULT_BLACKLIST_THRESHOLD;

	find_bindport(reload); /* Determine what IAX2 bindport we're using. */

	if (load_global_vars(1)) {
		decline = module_flags.requirekeytoload && ast_strlen_zero(interlinked_api_key) ? -1 : 0;
	}

	if (!(cfg = ast_config_load(CONFIG_FILE, config_flags))) {
		ast_debug(1, "Config file %s not found\n", CONFIG_FILE);
		if (decline) {
			ast_log(LOG_WARNING, "InterLinked API key is missing, declining to load module\n");
		}
		return decline;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		ast_debug(1, "Config file %s unchanged, skipping\n", CONFIG_FILE);
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Config file %s is in an invalid format. Aborting.\n", CONFIG_FILE);
		return -1;
	}

	while ((cat = ast_category_browse(cfg, cat))) {
		if (!strcasecmp(cat, "general")) {
			var = ast_variable_browse(cfg, cat);
			while (var) {
				if (!strcasecmp(var->name, "hostname")) {
					ast_copy_string(hostname_override, var->value, sizeof(hostname_override));
				} else if (!strcasecmp(var->name, "autokeyfetch")) {
					module_flags.autokeyfetch = ast_true(var->value) ? 1 : 0;
				} else if (!strcasecmp(var->name, "autokeyrotate")) {
					module_flags.autokeyrotate = ast_true(var->value) ? 1 : 0;
				} else if (!strcasecmp(var->name, "requesttoken")) {
					module_flags.requesttoken = ast_true(var->value) ? 1 : 0;
				} else if (!strcasecmp(var->name, "fallbackwarning")) {
					module_flags.fallbackwarning = ast_true(var->value) ? 1 : 0;
				} else if (!strcasecmp(var->name, "requirekeytoload")) {
					module_flags.requirekeytoload = ast_true(var->value) ? 1 : 0;
					decline = module_flags.requirekeytoload && ast_strlen_zero(interlinked_api_key) ? -1 : 0;
				} else if (!strcasecmp(var->name, "blacklistthreshold")) {
					if (sscanf(var->value, "%f", &blacklist_threshold) != 1) {
						ast_log(LOG_WARNING, "Invalid blacklist threshold: %s\n", var->value);
						blacklist_threshold = DEFAULT_BLACKLIST_THRESHOLD;
					} else if (blacklist_threshold < 0 || blacklist_threshold > 3) {
						ast_log(LOG_WARNING, "Invalid blacklist threshold: %s\n", var->value);
						blacklist_threshold = DEFAULT_BLACKLIST_THRESHOLD;
					}
				} else {
					ast_log(LOG_WARNING, "Unknown setting at line %d: '%s'\n", var->lineno, var->name);
				}
				var = var->next;
			}
		} else if (!strcasecmp(cat, "intervals")) {
			var = ast_variable_browse(cfg, cat);
			while (var) {
				if (!strcasecmp(var->name, "keyfetch")) {
					if (ast_str_to_int(var->value, &tmp)) {
						ast_log(LOG_WARNING, "Invalid key fetch frequency, defaulting to %d\n", keyfetch_interval);
					} else {
						keyfetch_interval = tmp;
					}
				} else if (!strcasecmp(var->name, "keyrotate_hour")) {
					if (ast_str_to_int(var->value, &tmp) || tmp < 0 || tmp > 23) {
						ast_log(LOG_WARNING, "Invalid 24-hour spec, defaulting to %d\n", DEFAULT_KEYROTATE_HOUR);
					} else {
						keyfetch_interval = tmp;
					}
				} else {
					ast_log(LOG_WARNING, "Unknown setting at line %d: '%s'\n", var->lineno, var->name);
				}
				var = var->next;
			}
		} else {
			ast_log(LOG_WARNING, "Line %d: Invalid config section: %s\n", var->lineno, var->name);
			continue;
		}
	}

	ast_config_destroy(cfg);

	if (decline) {
		/* There is currently no dynamic memory allocated by loading the config, but watch out in the future. */
		/* By default, this module will decline to load for non-PhreakNet systems. */
		ast_log(LOG_WARNING, "InterLinked API key is missing, declining to load module\n");
		return -1;
	}

	if (reload) {
		ast_cond_signal(&refresh_condition);
	}
	return 0;
}

/*! \brief Hack to determine what hangup handlers exist on a channel. */
/*! \todo There should be an API in pbx_hangup_handler.c to do this? */
static int cli_exec(char *cmd, char *buf, size_t len)
{
	FILE *tf;
	char *realbuf = buf;

	tf = tmpfile();
	if (!tf) {
		return -1;
	}

	ast_cli_command(fileno(tf), cmd);
	rewind(tf);

	*buf = '\0';
	while (fgets(buf, len, tf)) {
		int bytes = strlen(buf);
		buf += bytes;
		len -= bytes;
	}
	fclose(tf);

	if (ast_strlen_zero(realbuf)) {
		return -1;
	}

	ast_debug(5, "Command '%s' returned '%s'\n", cmd, realbuf);
	return 0;
}

static int pop_legacy_handler(struct ast_channel *chan)
{
	char cmd[256];
	char output[512];
	char *tmp, *next, *handler_name;

	snprintf(cmd, sizeof(cmd), "core show hanguphandlers %s", ast_channel_name(chan));
	if (cli_exec(cmd, output, sizeof(output))) {
		ast_log(LOG_WARNING, "Failed to determine hangup handlers\n");
		return -1;
	}
	tmp = strstr(output, ast_channel_name(chan));
	if (!tmp) {
		/* There aren't any hangup handlers on this channel. */
		ast_debug(4, "No hangup handlers currently on channel %s\n", ast_channel_name(chan));
		return 0;
	}
	ltrim(tmp);

	handler_name = strstr(tmp, "phreaknet-ticket");
	if (!handler_name) {
		ast_debug(2, "Legacy ticketing hangup handler not detected\n");
		return 0;
	}

	/* Only pop if it was the most recently added one. */
	next = strchr(tmp, '\n');
	if (next && next < handler_name) {
		ast_log(LOG_WARNING, "Legacy hangup handler detected, but was not most recently pushed, so can't pop. Fix your dialplan!\n");
		return -1;
	}

	/* It was the most recently pushed. */
	ast_debug(2, "Legacy hangup handler detected on channel %s, popping it\n", ast_channel_name(chan));
	ast_pbx_hangup_handler_pop(chan);
	return 0;
}

static struct ast_str *phreaknet_lookup_full(const char *number, const char *flags, const char *clid, int ani2, const char *cnam, const char *cvs, const char *ss)
{
	static const char *version = NULL;
	static const char *version_num = NULL;
	char url[324]; /* min to make gcc happy */

	/* Fetch the first time only */
	if (!version) {
		version = ast_get_version();
	}
	if (!version_num) {
		version_num = ast_get_version_num();
	}

	snprintf(url, sizeof(url), "https://api.phreaknet.org/v1/?key=%s&asterisk=%s&asteriskv=%s"
		"&number=%s&clid=%s&ani2=%d&cnam=%s&cvs=%s&nodevia=%s&flags=%s&threshold=%f&ss=%s%s",
		interlinked_api_key, version, version_num,
		number, clid, ani2, cnam, cvs, mainphreaknetdisa, flags, blacklist_threshold, ss, module_flags.requesttoken ? "&request_key" : "");

	return curl_get(url, NULL);
}

static int safe_encoded_string(const char *restrict s, char *restrict buf, size_t len)
{
	ast_assert(s != NULL);
	while (*s) {
		if (isalnum(*s)) {
			*buf++ = *s;
			len--;
		} else if (isspace(*s)) {
			*buf++ = '+'; /* Replace spaces with + */
			len--;
		}
		s++;
		if (len <= 1) {
			return -1;
		}
	}
	*buf = '\0';
	return 0;
}

static struct ast_str *phreaknet_lookup(struct ast_channel *chan, const char *number, const char *flags)
{
	char *clid, *cnam;
	char cvs[3];
	char ss[2];
	char filtered_cnam[128] = "";
	int ani2;
	const char *clidverif;
	const char *ssvarval;

	ani2 = ast_channel_caller(chan)->ani2;
	clid = ast_channel_caller(chan)->id.number.str;
	cnam = ast_channel_caller(chan)->id.name.str;

	ast_channel_lock(chan);
	clidverif = pbx_builtin_getvar_helper(chan, "clidverif");
	ast_copy_string(cvs, S_OR(clidverif, ""), sizeof(cvs));
	ssvarval = pbx_builtin_getvar_helper(chan, "ssverstat");
	ast_copy_string(ss, S_OR(ssvarval, ""), sizeof(ss));
	ast_channel_unlock(chan);

	if (!ast_strlen_zero(cnam) && safe_encoded_string(cnam, filtered_cnam, sizeof(filtered_cnam))) {
		ast_log(LOG_ERROR, "Failed to encode CNAM string '%s'\n", cnam);
		return NULL;
	}

	return phreaknet_lookup_full(number, flags, clid, ani2, filtered_cnam, cvs, ss);
}

#define lookup_number(chan, number, flags, buf, len) lookup_number_token(chan, number, flags, buf, len, NULL, 0)

static int lookup_number_token(struct ast_channel *chan, const char *number, const char *flags, char *buf, size_t len, char *doptbuf, size_t doptlen)
{
	char *tilde, *result, *dialopts;
	struct ast_str *lookup = phreaknet_lookup(chan, S_OR(number, ""), flags);

	if (!lookup) {
		return -1;
	}

	tilde = strchr(ast_str_buffer(lookup), '~');
	result = ast_str_buffer(lookup);
	if (tilde) {
		/* There's a token prepended to the lookup result */
		*tilde++ = '\0';
		ast_copy_string(buf, tilde, len); /* Number is after the tilde. */
		ast_func_write(chan, "IAXVAR(vertoken)", result);
		result = tilde;
	}

	if (doptbuf) {
		dialopts = strchr(result, '^');
		if (dialopts) {
			*dialopts++ = '\0';
			ast_copy_string(doptbuf, dialopts, doptlen);
		} else {
			*doptbuf = '\0'; /* initialize, in case there aren't any */
		}
	} /* else, if no separate buffer for dialopts, put it all in the same buffer. */

	ast_copy_string(buf, result, len);
	ast_free(lookup);
	return 0;
}

static int number_exists(const char *number)
{
	struct ast_str *str;
	char url[256];
	int i;

	snprintf(url, sizeof(url), "https://api.phreaknet.org/v1/exists?key=%s&number=%s", interlinked_api_key, number);
	str = curl_get(url, NULL);
	if (!str) {
		return 0;
	}
	i = atoi(S_OR(ast_str_buffer(str), ""));
	ast_free(str);
	return i;
}

static int phreaknet_read(struct ast_channel *chan, const char *cmd, char *data, char *buf, size_t len)
{
	if (!chan) {
		ast_log(LOG_WARNING, "No channel was provided to %s function.\n", cmd);
		return -1;
	}
	if (ast_begins_with(data, "lookup")) {
		char *number = strchr(data, ',');
		if (number) {
			*number++ = '\0';
		}
		if (!ast_channel_caller(chan)->id.number.str) {
			const char *maindisa;
			/* If there's no Caller ID (such as when querying from the CLI), this will fail.
			 * Use a generic Caller ID assigned to this node to get a valid response. */
			ast_channel_lock(chan);
			/* It's a global variable, so don't even bother providing a channel: */
			maindisa = pbx_builtin_getvar_helper(NULL, "mainphreaknetdisa");
			if (maindisa) {
				ast_channel_caller(chan)->id.number.str = ast_strdup(maindisa);
				if (ast_channel_caller(chan)->id.number.str) {
					ast_channel_caller(chan)->id.number.valid = 1;
				}
			}
			ast_channel_unlock(chan);
		}
		return lookup_number(chan, number, "", buf, len);
	} else if (ast_begins_with(data, "exists")) {
		const char *number = strchr(data, ',');
		if (!number) {
			ast_log(LOG_ERROR, "Missing number\n");
			return -1;
		}
		number++;
		if (ast_strlen_zero(number)) {
			ast_log(LOG_ERROR, "Missing number\n");
			return -1;
		}
		ast_copy_string(buf, number_exists(number) ? "1" : "0", len);
	} else if (!strcasecmp(data, "CDR")) {
		/* PhreakNet CDR already enabled on this channel? */
		ast_copy_string(buf, cdr_channel_applicable(ast_channel_name(chan), 0) ? "1" : "0", len);
	}
	return 0;
}

static int phreaknet_write(struct ast_channel *chan, const char *cmd, char *data, const char *value)
{
	if (!chan) {
		return -1;
	}

	if (!strcasecmp(data, "CDR")) {
		const char *ani, *caller, *callee, *type;
		AST_DECLARE_APP_ARGS(args,
			AST_APP_ARG(callee);
			AST_APP_ARG(caller);
			AST_APP_ARG(ani);
			AST_APP_ARG(type);
		);
		AST_STANDARD_APP_ARGS(args, ast_strdupa(S_OR(value, "")));
		callee = S_OR(args.callee, ast_channel_exten(chan));
		caller = S_OR(args.caller, ast_channel_caller(chan)->id.number.str);
		ani = S_OR(args.ani, ast_channel_caller(chan)->ani.number.str);
		ani = S_OR(ani, caller); /* Have ANI fall back to Caller ID */
		type = S_OR(args.type, "");
		/* Ensure that the dialplan hangup handler isn't also being utilized, or we'll double ticket. */
		if (pop_legacy_handler(chan)) {
			/* Don't continue or we'll double ticket. */
			return -1;
		}
		/* Enable CDR on the channel itself.
		 * Otherwise, if CDR is disabled by default, we'll never actually have the callback execute. */
		ast_func_write(chan, "CDR_PROP(disable)", "0");
		if (cdr_channel_push(ast_channel_name(chan), ani, caller, callee, type)) {
			ast_log(LOG_WARNING, "Failed to enable PhreakNet CDR on %s\n", ast_channel_name(chan));
			return -1;
		}
		/* Dialplan should also ensure this hasn't also been set */
		/* BUGBUG CDRs seem to get "stuck"? */
	} else {
		ast_log(LOG_WARNING, "Unknown setting: %s\n", data);
		return -1;
	}

	return 0;
}

static struct ast_custom_function phreaknet_function = {
	.name = "PHREAKNET",
	.read = phreaknet_read,
	.write = phreaknet_write,
};

enum net_flags {
	OPT_MF = (1 << 0),
	OPT_SF = (1 << 1),
};

AST_APP_OPTIONS(net_flags, {
	AST_APP_OPTION('m', OPT_MF),
	AST_APP_OPTION('s', OPT_SF),
});

enum auth_flags {
	OPT_AUTH_MD5 = (1 << 0),
	OPT_AUTH_RSA = (1 << 1),
};

AST_APP_OPTIONS(auth_flags, {
	AST_APP_OPTION('m', OPT_AUTH_MD5),
	AST_APP_OPTION('r', OPT_AUTH_RSA),
});

static const char *dial_app = "PhreakNetDial";

static int outverify(struct ast_channel *chan, const char *lookup)
{
	char buf[266];
	int proceed;
	const char *varval;
	char *cnam = ast_channel_caller(chan)->id.name.str;

	if (!ast_strlen_zero(cnam)) {
		if (*cnam == '"') {
			ast_log(LOG_WARNING, "Caller ID Name '%s' is malformed (nested in quotes)\n", cnam);
			return -1;
		} else if (strchr(cnam, '<')) {
			ast_log(LOG_WARNING, "Caller ID Name '%s' is malformed (contains '<')\n", cnam);
			return -1;
		}
	}

	snprintf(buf, sizeof(buf), "phreaknet,%s", lookup);
	if (ast_pbx_exec_application(chan, "OutVerify", buf)) {
		return -1;
	}

	ast_channel_lock(chan);
	varval = pbx_builtin_getvar_helper(chan, "OUTVERIFYSTATUS");
	/* Protect against channel attacks, bad lookups, local IP attacks, etc. Bail on bad lookup. */
	proceed = !strcmp(varval, "PROCEED");
	ast_channel_unlock(chan);

	if (!proceed) {
		ast_log(LOG_WARNING, "Lookup failed validation: %s\n", lookup);
	}

	return proceed ? 0 : 1;
}

static int call_failed(struct ast_channel *chan)
{
	const char *varval, *varval2;
	int code = -1;

	ast_channel_lock(chan);
	/* These *should* always exist... */
	varval = pbx_builtin_getvar_helper(chan, "DIALSTATUS");
	varval2 = pbx_builtin_getvar_helper(chan, "HANGUPCAUSE"); /* Not always set */
	ast_debug(3, "Dial disposition: %s (%s)\n", varval, S_OR(varval2, ""));
	if (varval) {
		if (!strcmp(varval, "CONGESTION") || !strcmp(varval, "CHANUNAVAIL")) {
			ast_channel_unlock(chan);
			return -1;
		}
	}
	if (varval2) {
		code = atoi(varval2);
	}
	ast_channel_unlock(chan);

	/* 0, 3, 20, 27, 50 */
	if (code == 0 || code == AST_CAUSE_NO_ROUTE_DESTINATION || code == AST_CAUSE_SUBSCRIBER_ABSENT ||
		code == AST_CAUSE_DESTINATION_OUT_OF_ORDER || code == AST_CAUSE_FACILITY_NOT_SUBSCRIBED) {
		return -1;
	}
	return 0;
}

static int dial_exec(struct ast_channel *chan, const char *data)
{
	char lookup[256];
	char dialargs[614]; /* min for gcc to be happy with snprintf */
	char dialopts[84];
	struct ast_flags netflags = {0};
	struct ast_flags authflags = {0};
	char *argcopy;
	char *secret, *host;
	char autovonchan[256];
	const char *varval;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(number);
		AST_APP_ARG(trunkflags);
		AST_APP_ARG(destcontext);
		AST_APP_ARG(authoptions);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "%s requires arguments\n", dial_app);
		return -1;
	}

	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, argcopy);

	/* Empty number is okay (permanent signal) */
	if (!ast_strlen_zero(args.trunkflags)) {
		/* Not actually used, but ast_app_parse_options will verify invalid options weren't passed in */
		ast_app_parse_options(net_flags, &netflags, NULL, args.trunkflags);
	}
	if (!ast_strlen_zero(args.authoptions)) {
		ast_app_parse_options(auth_flags, &authflags, NULL, args.authoptions);
	} else {
		/* Default, if not specified, is allow both. */
		ast_set_flag(&authflags, OPT_AUTH_MD5);
		ast_set_flag(&authflags, OPT_AUTH_RSA);
	}
	if (lookup_number_token(chan, args.number, args.trunkflags, lookup, sizeof(lookup), dialopts, sizeof(dialopts))) {
		ast_log(LOG_ERROR, "Lookup failed for %s\n", S_OR(args.number, ""));
		pbx_builtin_setvar_helper(chan, "DIALSTATUS", "CHANUNAVAIL");
		return 0;
	}

	ast_debug(3, "Lookup(%s) = %s, dialopts: %s\n", S_OR(args.number, ""), lookup, dialopts);

	if (outverify(chan, lookup)) {
		return -1;
	}

	/* Format is IAX2/user:secret@host:port/exten[@context],,dialflags */
	host = strchr(lookup, '@');
	if (!host) {
		ast_log(LOG_WARNING, "No host?\n");
		return -1;
	}
	secret = strchr(lookup, ':'); /* Technically, this is the start of the secret, not just the secret */
	if (secret && secret > host) {
		/* first : was after the first @, so there is no secret */
		secret = NULL;
	}

	*host++ = '\0';

	if (module_flags.autovonsupport) {
		ast_channel_lock(chan);
		varval = pbx_builtin_getvar_helper(chan, "autovonchan");
		if (!ast_strlen_zero(varval)) {
			snprintf(autovonchan, sizeof(autovonchan), "F(%s)", varval);
		} else {
			autovonchan[0] = '\0';
		}
		ast_channel_unlock(chan);
	}

	/* Only try authenticated calls if we have a secret. */
	if (secret) {
		if (!ast_test_flag(&authflags, OPT_AUTH_RSA) && !ast_test_flag(&authflags, OPT_AUTH_MD5)) {
			/* Shouldn't happen, since we don't allow setting both of these off. */
			ast_log(LOG_WARNING, "Call requires authentication, but neither MD5 nor RSA available?\n");
			pbx_builtin_setvar_helper(chan, "DIALSTATUS", "CHANUNAVAIL");
			return 0;
		}
		/* Enforce encryption if there's a secret */
		ast_func_write(chan, "CHANNEL(secure_bridge_signaling)", "1");
		ast_func_write(chan, "CHANNEL(secure_bridge_media)", "1");
		if (ast_test_flag(&authflags, OPT_AUTH_RSA)) {
			/* Both are allowed. Try a combined dial first. Prefer RSA and fall back to MD5. */
			snprintf(dialargs, sizeof(dialargs), "%s:[phreaknetrsa]@%s%s%s,,g%s%s",
				lookup, host,
				!ast_strlen_zero(args.destcontext) ? "@" : "",
				S_OR(args.destcontext, ""),
				module_flags.autovonsupport ? autovonchan : "",
				S_OR(dialopts, "")
			);
			ast_debug(1, "%s: RSA Dial(%s)\n", ast_channel_name(chan), dialargs);
			if (ast_pbx_exec_application(chan, "Dial", dialargs)) {
				return -1;
			}
			if (!call_failed(chan)) {
				return 0;
			}
			/* If combined failed, this probably isn't going to succeed at all... */
			if (!ast_test_flag(&authflags, OPT_AUTH_MD5)) {
				ast_log(LOG_WARNING, "Call to %s failed using RSA attempt, aborting\n", S_OR(args.number, ""));
				return 0;
			}
			ast_log(LOG_WARNING, "Call to %s failed using RSA attempt, falling back to MD5\n", S_OR(args.number, ""));
		}
		if (!ast_test_flag(&authflags, OPT_AUTH_MD5)) {
			/* Shouldn't be reachable. */
			ast_log(LOG_WARNING, "No authentication methods available?\n");
			return -1;
		}
		/* Fall back to MD5 */
		if (module_flags.fallbackwarning) {
			/* Audible warning */
			int res = ast_tonepair(chan, 440, 0, 250, 0);
			if (!res) {
				res = ast_tonepair(chan, 0, 0, 250, 0);
			}
			if (!res) {
				res = ast_tonepair(chan, 440, 0, 250, 0);
			}
			if (res) {
				return -1;
			}
		}
	} else {
		ast_verb(4, "Outgoing call to %s is insecure (no secret available)\n", args.number);
	}

	/* MD5 or no authentication */
	*(host - 1) = '@'; /* Undo the string split. */
	snprintf(dialargs, sizeof(dialargs), "%s%s%s,,g%s%s",
		lookup,
		!ast_strlen_zero(args.destcontext) ? "@" : "",
		S_OR(args.destcontext, ""),
		module_flags.autovonsupport ? autovonchan : "",
		S_OR(dialopts, "")
	);
	ast_debug(1, "%s: %s Dial(%s)\n", ast_channel_name(chan), secret ? "MD5" : "Unauthenticated", dialargs);
	if (ast_pbx_exec_application(chan, "Dial", dialargs)) {
		return -1;
	}
	if (!call_failed(chan)) {
		return 0;
	}
	if (ast_test_flag(&authflags, OPT_AUTH_MD5)) {
		ast_log(LOG_WARNING, "Call to %s failed using MD5 attempt, aborting\n", S_OR(args.number, ""));
	}
	return 0;
}

static int reload_module(void)
{
	reload_time = time(NULL);
	return load_config(1);
}

static int load_module(void)
{
	int res = 0;

	load_time = time(NULL);

	ast_mutex_init(&stat_lock);
	memset(&phreaknet_stats, 0, sizeof(phreaknet_stats));

	if (load_config(0)) {
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_mutex_init(&refreshlock);
	ast_cond_init(&refresh_condition, NULL);

	if ((res = ast_cdr_register(MODULE_NAME, ast_module_info->description, cdr_handler))) {
		ast_log(LOG_ERROR, "Unable to register CDR handler\n");
		return AST_MODULE_LOAD_DECLINE;
	}

	/* Suspend until actually needed. */
	if (ast_cdr_backend_suspend(MODULE_NAME)) {
		ast_log(LOG_WARNING, "Failed to suspend CDR processing\n");
	}

	if (ast_pthread_create_background(&phreaknet_thread, NULL, phreaknet_periodic, NULL) < 0) {
		ast_log(LOG_ERROR, "Unable to start periodic thread\n");
		ast_cdr_unregister(MODULE_NAME);
		return AST_MODULE_LOAD_DECLINE;
	}

	res |= ast_register_application_xml(dial_app, dial_exec);
	res |= ast_custom_function_register(&phreaknet_function);
	ast_cli_register_multiple(phreaknet_cli, ARRAY_LEN(phreaknet_cli));
	if (res) {
		ast_cdr_unregister(MODULE_NAME);
	}
	return res;
}

static int unload_module(void)
{
	/* If we have active PhreakNet CDRs, we cannot unload. */
	if (phreaknet_stats.current_outgoing_calls) {
		ast_log(LOG_WARNING, "Can't unload %s while there are active outgoing PhreakNet calls\n", MODULE_NAME);
		return -1;
	}
	/* In theory, ast_cdr_unregister should always succeed at this point since our CDR backend should be suspended.
	 * Unless the last call cleared within the last minute and periodic_thread hasn't had time to suspend it yet. */
	if (ast_cdr_unregister(MODULE_NAME)) {
		return -1;
	}

	ast_cli_unregister_multiple(phreaknet_cli, ARRAY_LEN(phreaknet_cli));
	ast_custom_function_unregister(&phreaknet_function);
	ast_unregister_application(dial_app);

	ast_mutex_lock(&refreshlock);
	module_unloading = 1;
	ast_cond_signal(&refresh_condition);
	ast_mutex_unlock(&refreshlock);

	pthread_join(phreaknet_thread, NULL);
	cdr_channel_cleanup();
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "PhreakNet Enhancements",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.reload = reload_module,
	.load_pri = AST_MODPRI_CDR_DRIVER,
	.requires = "cdr,pbx_config",
);
