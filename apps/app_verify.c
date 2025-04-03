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
 */

/*! \file
 *
 * \brief Call verification application
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*! \li \ref app_verify.c uses the configuration file \ref verify.conf
 * \addtogroup configuration_file Configuration Files
 */

/*!
 * \page verify.conf verify.conf
 * \verbinclude verify.conf.sample
 */

/*** MODULEINFO
	<depend>curl</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <curl/curl.h>
#include <regex.h>

#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/cli.h"
#include "asterisk/term.h"
#include "asterisk/config.h"
#include "asterisk/app.h"
#include "asterisk/conversions.h"
#include "asterisk/utils.h"
#include "asterisk/acl.h"
#include "asterisk/enum.h"

/*** DOCUMENTATION
	<application name="Verify" language="en_US">
		<synopsis>
			Verifies an incoming call
		</synopsis>
		<syntax>
			<parameter name="profile" required="yes">
				<para>The profile in <literal>verify.conf</literal>
				to use for verification.</para>
			</parameter>
			<parameter name="number" required="no">
				<para>Number against which to verify. Typically,
				this should be the contents of the CALLERID(num)
				variable, and this is the default if unspecified.</para>
			</parameter>
		</syntax>
		<description>
			<para>This application verifies an incoming call and stores the result
			in a dialplan variable. Verification is performed in conjunction with
			pre-specified parameters and allows spoofed or fradulent calls in a
			peer-to-peer trunking system to be screened out.</para>
			<para>This application may generally only be used with IAX2 channels,
			except for simple verify methods regex and pattern.</para>
			<para>STIR/SHAKEN processing is only supported on SIP/PJSIP channels.</para>
		</description>
		<see-also>
			<ref type="application">OutVerify</ref>
		</see-also>
	</application>
	<application name="OutVerify" language="en_US">
		<synopsis>
			Sets up an outgoing call for downstream verification
		</synopsis>
		<syntax>
			<parameter name="profile" required="yes">
				<para>The profile in <literal>verify.conf</literal>
				to use.</para>
			</parameter>
			<parameter name="lookup" required="no">
				<para>IAX2 lookup to analyze for potential
				malicious dialplan injection, to avoid call fraud.</para>
			</parameter>
		</syntax>
		<description>
			<para>This application sets up a call for outgoing verification.
			As part of this process, any remote variables needed for successful
			downstream verification are set or obtained, and the call is analyzed
			in accordance with rules in <literal>verify.conf</literal> to help
			prevent certain risky or malicious calls from being completed.</para>
			<para>This application may only be used with IAX2 channels.</para>
			<para>This application sets the following variable:</para>
			<variablelist>
				<variable name="OUTVERIFYSTATUS">
					<value name="PROCEED">
						Call is okay to proceed.
					</value>
					<value name="FAILURE">
						Failed to successfully out verify the call.
					</value>
					<value name="MALICIOUS">
						The destination corresponds to a potentially malicious
						destination, such as a private IP address.
					</value>
					<value name="QUARANTINEDISA">
						This is a thru call which should not be allowed to
						complete the outgoing call.
					</value>
					<value name="QUARANTINEPSTN">
						This is a PSTN call which should not be allowed to
						complete the outgoing call.
					</value>
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">Verify</ref>
		</see-also>
	</application>
	<configInfo name="app_verify" language="en_US">
		<synopsis>Module to verify incoming and outgoing calls</synopsis>
		<configFile name="verify.conf">
			<configObject name="general">
				<synopsis>Options that apply globally to app_verify</synopsis>
				<configOption name="curltimeout">
					<synopsis>The number of seconds curl has to retrieve a resource before timing out.</synopsis>
				</configOption>
			</configObject>
			<configObject name="profile">
				<synopsis>Defined profiles for app_verify to use with call verification.</synopsis>
				<configOption name="verifymethod" default="reverse">
					<synopsis>Method to use for verification.</synopsis>
					<description>
						<para>Can be <literal>reverse</literal>, <literal>direct</literal>, <literal>regex</literal>, or <literal>pattern</literal>.</para>
						<enumlist>
							<enum name="reverse"><para>
								Manually compute verification using reverse lookups. Only supported with IAX2.
							</para></enum>
							<enum name="direct"><para>
								Query an authoritative central server via HTTP for the result. Only supported with IAX2.
							</para></enum>
							<enum name="regex"><para>
								Perform no verification but simply validate against the number format in a specified regular expression.
							</para></enum>
							<enum name="pattern"><para>
								Evaluate the calling number as an extension in the specified dialplan context.
							</para></enum>
						</enumlist>
					</description>
				</configOption>
				<configOption name="requestmethod" default="curl">
					<synopsis>Method to use for making verification requests.</synopsis>
					<description>
						<para>Can be <literal>curl</literal> for HTTP lookups or "enum" for ENUM lookups. Must be <literal>curl</literal> if <literal>verifymethod</literal> is <literal>direct</literal>. Does not apply when <literal>verifymethod</literal> is set to <literal>regex</literal>.</para>
					</description>
				</configOption>
				<configOption name="verifyrequest">
					<synopsis>Request to make for verification requests.</synopsis>
					<description>
						<para>URL for "curl" lookups or arguments to ENUMLOOKUP function for "enum" lookups. Parameter for number should be replaced with <literal>${VERIFYARG1}</literal> for substitution during verification. Dialplan variables may be used.</para>
					</description>
				</configOption>
				<configOption name="verifycontext">
					<synopsis>Context to use for verification.</synopsis>
					<description>
						<para>Dialplan context containing patterns to match calling numbers. Only applies when <literal>verifymethod</literal> is <literal>pattern</literal>. Extensions should return the verification code at priority 1. Variables and functions may be used. The format of the dialplan context is the same as with using <literal>EVAL_EXTEN</literal> in the dialplan.</para>
					</description>
				</configOption>
				<configOption name="local_var">
					<synopsis>Name of variable in which to store the verification result.</synopsis>
					<description>
						<para>Name of variable in which to store the verification result. To make the variable persist through Dial, prefix with double underscore.</para>
					</description>
				</configOption>
				<configOption name="stirshaken_var">
					<synopsis>Name of variable in which to store the STIR/SHAKEN disposition.</synopsis>
					<description>
						<para>Name of variable in which to store the STIR/SHAKEN disposition. To make the variable persist through Dial, prefix with double underscore.</para>
					</description>
				</configOption>
				<configOption name="remote_stirshaken_var">
					<synopsis>Name of remote variable in which the STIR/SHAKEN disposition can be found.</synopsis>
					<description>
						<para>Name of remote variable in which the STIR/SHAKEN disposition can be found.</para>
					</description>
				</configOption>
				<configOption name="via_number">
					<synopsis>Number on this node for reverse identification.</synopsis>
					<description>
						<para>Number on this node which can be used against us in a reverse lookup for positive identification. A lookup for this number in the telephony domain must resolve to this node. Any such number will suffice.</para>
					</description>
				</configOption>
				<configOption name="remote_var">
					<synopsis>Name of remote variable containing upstream verification result.</synopsis>
					<description>
						<para>Name of remote variable (probably an IAXVAR) in which the verification result is attested by the upstream node. If extendtrust is <literal>yes</literal> and verification succeeds, the contents of this variable will be used to populate <literal>local_var</literal>.  Must be universal across a telephony domain.</para>
					</description>
				</configOption>
				<configOption name="via_remote_var">
					<synopsis>Name of remote variable containing upstream node identifier.</synopsis>
					<description>
						<para>Name of remote variable (probably an IAXVAR) in which the node identifier for the upstream node, if present, may be found. Needed for tandem-through verification (see <literal>extendtrust</literal> option). Must be universal across a telephony domain.</para>
					</description>
				</configOption>
				<configOption name="setinvars">
					<synopsis>Variable assignments to make after verification of an incoming call has completed.</synopsis>
					<description>
						<para>Variable assignments to make after verification of an incoming call has completed. This will apply to all calls, successful or not. This can allow for loading other remote variables into local variables (e.g. storing IAXVARs in channel variables). It can also allow for variable parameter operations, if you know that certain fields will need to be manipulated. The format here is the same as MSet's, but assignments will be done left to right, so later operations can depend on earlier operations completing. Values containing commas MUST be surrounded in single quotes.</para>
					</description>
				</configOption>
				<configOption name="setoutvars">
					<synopsis>Opposite of setinvars: variable assignments made after OutVerify has completed.</synopsis>
				</configOption>
				<configOption name="sanitychecks" default="yes">
					<synopsis>Whether or not to perform basic sanity checks.</synopsis>
					<description>
						<para>Whether or not to perform basic sanity checks on the alleged calling number for "reverse" verification scenarios. This should be used if there is any possibility of invalid numbers resolving to *anything*. If it is guaranteed that invalid numbers will always return an empty lookup, this can be disabled. attested verified.</para>
					</description>
				</configOption>
				<configOption name="extendtrust" default="yes">
					<synopsis>Whether or not to allow thru calls to be verified by verifying the upstream node.</synopsis>
					<description>
						<para>Whether or not to allow thru calls (i.e. not originating on the upstream node) to be verified by instead verifying the node which is passing the call off. This must be set to <literal>yes</literal> if calls can pass through multiple nodes between originating and termination.</para>
					</description>
				</configOption>
				<configOption name="allowdisathru" default="yes">
					<synopsis>Whether or not to allow thru-dialing out of the node.</synopsis>
				</configOption>
				<configOption name="allowpstnthru" default="yes">
					<synopsis>Whether or not to allow PSTN calls to leave the node.</synopsis>
				</configOption>
				<configOption name="allowtoken" default="no">
					<synopsis>Whether or not to allow token verification.</synopsis>
					<description>
						<para>Only supported for "reverse" verification. "direct" must encompass tokens in the verify request. Useful for scenarios where the outgoing IP of a node does not match the incoming IP of a node. This applies to both allowing tokens for incoming calls and requesting tokens for outgoing calls. Request must return the same value as via_remote_var for success.</para>
					</description>
				</configOption>
				<configOption name="validatetokenrequest">
					<synopsis>Request to make for token verification. Only supported for "reverse" verifications. Must be an HTTP endpoint. Dialplan variables may be used.</synopsis>
				</configOption>
				<configOption name="token_remote_var">
					<synopsis>Name of remote variable (probably an IAXVAR) in which a verification token may be stored. Must be universal across a telephony domain.</synopsis>
				</configOption>
				<configOption name="exceptioncontext">
					<synopsis>Name of dialplan context containing exceptions.</synopsis>
					<description>
						<para>Name of dialplan context containing extension pattern matches for numbers which may not be verifiable using reverse IP address lookups. The extension must return a comma-separated list of IP addresses which the specified numbers can validate against, at priority 1 for the extension. Only applies with <literal>verifymethod</literal> set to <literal>reverse</literal>, not <literal>direct</literal>. Avoid using this option if possible, and only use as a last resort.</para>
					</description>
				</configOption>
				<configOption name="successregex" regex="yes">
					<synopsis>Regular expression to determine if a verification was successful or not.</synopsis>
					<description>
						<para>This is effective only with <literal>direct</literal>, <literal>regex</literal>, or <literal>pattern</literal> <literal>verifymethod</literal> methods. This does not apply to <literal>reverse</literal>.</para>
						<para>For the "direct" and "pattern" methods, this is used against the verification code to determine if it constitutes success. For the "regex" method, this is used against the calling number to determine if it should be considered a valid number.</para>
					</description>
				</configOption>
				<configOption name="blacklist_endpoint">
					<synopsis>Request to make for blacklist check.</synopsis>
					<description>
						<para>Must be an HTTP endpoint. Dialplan variables may be used.</para>
						<para>The endpoint should return a floating point score, where a higher score corresponds to a less trustworthy call.</para>
					</description>
				</configOption>
				<configOption name="blacklist_threshold" default="1">
					<synopsis>Blacklist threshold above which calls are rejected</synopsis>
					<description>
						<para>Floating point score above which incoming calls should be completely rejected. Only applies if <literal>blacklist_endpoint</literal> is set.</para>
					</description>
				</configOption>
				<configOption name="blacklist_failopen" default="no">
					<synopsis>Allow calls that cannot successfully be queried using the blacklist API</synopsis>
					<description>
						<para>Whether or not to fail open to rejecting calls that fail to query (as opposed to failing safe). Set to <literal>yes</literal> to allow any calls that fail to query against the blacklist API (this may present a security risk).</para>
					</description>
				</configOption>
				<configOption name="flagprivateip" default="yes">
					<synopsis>Whether or not to flag calls to private IP addresses as malicious destinations.</synopsis>
					<description>
						<para>When set to "yes", flags private (Class A, B, or C), APIPA, or localhost addresses, as malicious. <literal>OUTVERIFYSTATUS</literal> will be set to MALICIOUS if a private/local IP address is detected. Only IPv4 is supported currently.</para>
					</description>
				</configOption>
				<configOption name="outregex" regex="yes">
					<synopsis>Regular expression to use to validate lookups, if provided to the OutVerify application.</synopsis>
				</configOption>
				<configOption name="threshold" default="0">
					<synopsis>Maximum number of unsuccessfully verified calls to accept before subsequent calls are dropped upon arrival.</synopsis>
					<description>
						<para>Default is 0, e.g. any call that fails to verify will be dropped. In reality, you may want to set this to a more conservative value to allow for some legitimate accident calls to get through. The greater this value, the more vulnerable the node is to a spam attack. This option is only effective if failgroup is specified, since the group is used to keep track of concurrent calls.</para>
					</description>
				</configOption>
				<configOption name="failgroup">
					<synopsis>Group category to which to assign calls that fail verification.</synopsis>
					<description>
						<para>The group will be the name of this verification section, e.g. same as Set(GROUP(failgroup)=example).</para>
					</description>
				</configOption>
				<configOption name="failureaction" default="nothing">
					<synopsis>Action to take when a call fails verification.</synopsis>
					<description>
						<para>Note that if the failure threshold specified in the "threshold" option is reached, any subsequent calls will be immediately dropped to prevent a spam attack, regardless of this setting. Valid options are <literal>nothing</literal>, which will take no action, <literal>hangup</literal> which will end the call, <literal>playback</literal> which will play a recording and then hang up, and <literal>redirect</literal> which will redirect the call to another dialplan extension.</para>
					</description>
				</configOption>
				<configOption name="failurefile">
					<synopsis>File or ampersand-separated files to play when verification fails.</synopsis>
					<description>
						<para>Effective when <literal>failureaction</literal> is set to <literal>playback</literal>. As with Playback, do not specify the file extension(s).</para>
					</description>
				</configOption>
				<configOption name="region">
					<synopsis>Region identifier (e.g. 2 characters) to use for tagging PSTN calls that enter and leave this node.</synopsis>
					<description>
						<para>Tagging format is "CALLERID(name) via clli region PSTN".</para>
					</description>
				</configOption>
				<configOption name="clli">
					<synopsis>String node identifier (e.g. CLLI) to use for tagging calls that leave this node.</synopsis>
					<description>
						<para>Tagging format is "CALLERID(name) via clli" for normal thru calls and "CALLERID(name) via clli PSTN" for PSTN calls.</para>
					</description>
				</configOption>
				<configOption name="code_good">
					<synopsis>Codes to assign to calls we attempt to verify.</synopsis>
					<description>
						<para>These codes MUST be the same across a telephony domain. For multiple networks, these codes should be unique (e.g. prefix with a network-specific number).</para>
					</description>
				</configOption>
				<configOption name="code_fail">
					<synopsis>Code to assign to local_var for unsuccessfully verified calls.</synopsis>
				</configOption>
				<configOption name="code_requestfail">
					<synopsis>Code to assign to local_var for calls that could not be verified (e.g. request failure).</synopsis>
				</configOption>
				<configOption name="code_spoof">
					<synopsis>Code to assign to local_var for spoofed calls.</synopsis>
				</configOption>
				<configOption name="loglevel">
					<synopsis>Name of log level at which to log incoming calls (e.g. WARNING, custom, etc.)</synopsis>
				</configOption>
				<configOption name="logmsg">
					<synopsis>Format of message to log. May contain variables which will be evaluated/substituted at log time.</synopsis>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
 ***/

static char *app = "Verify";
static char *app2 = "OutVerify";

#define MAX_YN_STRING		4

/*! \brief Data structure for verify */
struct call_verify {
	ast_mutex_t lock;
	char name[AST_MAX_CONTEXT];				/*!< Name - Verify section */
	unsigned int in;						/*!< Total number of incoming calls attempted to verify under this profile */
	unsigned int insuccess;					/*!< Total number of incoming calls successfully passed verification */
	unsigned int out;						/*!< Total number of outgoing calls attempted to out verify under this profile */
	unsigned int outsuccess;				/*!< Total number of outgoing calls out-verified under this profile */
	unsigned int total_blacklisted;			/*!< Total number of incoming calls that have been rejected due to blacklisting */
	char verifymethod[PATH_MAX];			/*!< Algorithm to use for verification: direct or reverse */
	char requestmethod[PATH_MAX];			/*!< Request method: curl or enum */
	char verifyrequest[PATH_MAX];			/*!< Request URL or ENUM lookup */
	char verifycontext[AST_MAX_CONTEXT];	/*!< Dialplan context for verification */
	char validatetokenrequest[PATH_MAX];	/*!< HTTP lookup for token verification */
	char obtaintokenrequest[PATH_MAX];		/*!< HTTP lookup for obtaining a verification token */
	char local_var[AST_MAX_CONTEXT];		/*!< Variable in which to store verification status */
	char stirshaken_var[AST_MAX_CONTEXT];	/*!< Variable in which to store STIR/SHAKEN disposition */
	char remote_stirshaken_var[AST_MAX_CONTEXT]; /*!< Variable in which remote STIR/SHAKEN disposition arrives */
	char via_number[AST_MAX_CONTEXT]; 		/*!< Number on this node, used for downstream identification */
	char remote_var[AST_MAX_CONTEXT];		/*!< Variable in which verification status arrives to us */
	char via_remote_var[AST_MAX_CONTEXT];	/*!< Variable in which upstream node identification arrives */
	char token_remote_var[AST_MAX_CONTEXT];	/*!< Variable in which token may arrive */
	char setinvars[PATH_MAX];				/*!< Variables to set on incoming call */
	char setoutvars[PATH_MAX];				/*!< Variables to set on outgoing call */
	char successregex[PATH_MAX];			/*!< Regex to use for direct verification to determine success */
	char blacklist_endpoint[PATH_MAX];		/*!< Blacklist endpoint */
	float blacklist_threshold;				/*!< Blacklist threshold */
	char outregex[PATH_MAX];				/*!< Regex to use to verify outgoing URIs */
	char exceptioncontext[PATH_MAX];		/*!< Action to take upon failure */
	char failureaction[PATH_MAX];			/*!< Action to take upon failure */
	char failurefile[PATH_MAX];				/*!< File for failure playback */
	char failurelocation[PATH_MAX];			/*!< Dialplan location for failure */
	char region[AST_MAX_CONTEXT];			/*!< Short region identifier for outgoing identification */
	char clli[PATH_MAX];					/*!< CLLI for outgoing identification */
	char failgroup[PATH_MAX];				/*!< Category of group to which to assign failed calls */
	char code_good[PATH_MAX];				/*!< Result for good calls */
	char code_fail[PATH_MAX];				/*!< Result for failed calls */
	char code_requestfail[PATH_MAX];		/*!< Result for failed verification request */
	char code_spoof[PATH_MAX];				/*!< Result for spoofed calls */
	int threshold;							/*!< Threshold at which to reject calls that failed to verify */
	char loglevel[AST_MAX_CONTEXT];			/*!< Log level */
	char logmsg[PATH_MAX];					/*!< Log message */
	AST_LIST_ENTRY(call_verify) entry;		/*!< Next Verify record */
	/* Flags */
	unsigned int sanitychecks:1;			/*!< Whether or not to do sanity checks on the alleged calling number */
	unsigned int extendtrust:1;				/*!< Whether to verify through/via calls */
	unsigned int allowdisathru:1;			/*!< Whether to allow DISA thru calls */
	unsigned int allowpstnthru:1;			/*!< Whether to allow PSTN thru calls */
	unsigned int allowtoken:1;				/*!< Whether to allow verification tokens */
	unsigned int flagprivateip:1;			/*!< Whether to flag private IP addresses as malicious */
	unsigned int blacklist_failopen:1;		/*!< Allow blacklist to fail open */
};

#define DEFAULT_CURL_TIMEOUT 8
#define SUB_BUFLEN 1024

static int curltimeout = DEFAULT_CURL_TIMEOUT;		/*!< Curl Timeout */

static AST_RWLIST_HEAD_STATIC(verifys, call_verify);

unsigned int stir_shaken_stats[2][6];
ast_mutex_t ss_lock;

/*! \brief Allocate and initialize verify profile */
static struct call_verify *alloc_profile(const char *vname)
{
	struct call_verify *v;

	if (!(v = ast_calloc(1, sizeof(*v)))) {
		return NULL;
	}

	ast_mutex_init(&v->lock);
	ast_copy_string(v->name, vname, sizeof(v->name));

	/* initialize these only on allocation, so they're not reset during a reload */
	v->in = 0;
	v->insuccess = 0;
	v->out = 0;
	v->outsuccess = 0;
	v->total_blacklisted = 0;

	return v;
}

#define VERIFY_LOAD_STR_PARAM(field, cond) \
if (!strcasecmp(param, #field)) { \
	if (!(cond)) { \
		ast_log(LOG_WARNING, "Invalid value for %s, ignoring: '%s'\n", param, val); \
		return; \
	} \
	ast_copy_string(v->field, val, sizeof(v->field)); \
	return; \
}

#define VERIFY_LOAD_INT_PARAM(field, cond) \
if (!strcasecmp(param, #field)) { \
	if (!(cond)) { \
		ast_log(LOG_WARNING, "Invalid value for %s, ignoring: '%s'\n", param, val); \
		return; \
	} \
	v->field = !strcasecmp(val, "yes") ? 1 : 0; \
	return; \
}

#define VERIFY_LOAD_FLOAT_PARAM(field, cond) \
if (!strcasecmp(param, #field)) { \
	if (!(cond)) { \
		ast_log(LOG_WARNING, "Invalid value for %s, ignoring: '%s'\n", param, val); \
		return; \
	} \
	if (sscanf(val, "%30f", &v->field) != 1) { \
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
static void profile_set_param(struct call_verify *v, const char *param, const char *val, int linenum, int failunknown)
{
	VERIFY_LOAD_STR_PARAM(verifymethod, !strcasecmp(val, "direct") || !strcasecmp(val, "reverse") || !strcasecmp(val, "regex") || !strcasecmp(val, "pattern"));
	VERIFY_LOAD_STR_PARAM(requestmethod, !strcasecmp(val, "curl") || !strcasecmp(val, "enum"));
	VERIFY_LOAD_STR_PARAM(verifyrequest, val[0]);
	VERIFY_LOAD_STR_PARAM(verifycontext, val[0]);
	VERIFY_LOAD_STR_PARAM(validatetokenrequest, val[0]);
	VERIFY_LOAD_STR_PARAM(obtaintokenrequest, val[0]);
	VERIFY_LOAD_STR_PARAM(local_var, val[0] && !contains_whitespace(val)); /* could cause bad things to happen if we try setting a var name with spaces */
	VERIFY_LOAD_STR_PARAM(stirshaken_var, val[0] && !contains_whitespace(val)); /* could cause bad things to happen if we try setting a var name with spaces */
	VERIFY_LOAD_STR_PARAM(remote_stirshaken_var, val[0] && !contains_whitespace(val)); /* could cause bad things to happen if we try setting a var name with spaces */
	VERIFY_LOAD_STR_PARAM(via_number, val[0] && !contains_whitespace(val));
	VERIFY_LOAD_STR_PARAM(remote_var, val[0] && !contains_whitespace(val));
	VERIFY_LOAD_STR_PARAM(via_remote_var, val[0] && !contains_whitespace(val));
	VERIFY_LOAD_STR_PARAM(token_remote_var, val[0] && !contains_whitespace(val));
	VERIFY_LOAD_STR_PARAM(setinvars, val[0] && !contains_whitespace(val));
	VERIFY_LOAD_STR_PARAM(setoutvars, val[0] && !contains_whitespace(val));
	VERIFY_LOAD_INT_PARAM(sanitychecks, !strcasecmp(val, "yes") || !strcasecmp(val, "no"));
	VERIFY_LOAD_INT_PARAM(extendtrust, !strcasecmp(val, "yes") || !strcasecmp(val, "no"));
	VERIFY_LOAD_INT_PARAM(allowtoken, !strcasecmp(val, "yes") || !strcasecmp(val, "no"));
	VERIFY_LOAD_INT_PARAM(flagprivateip, !strcasecmp(val, "yes") || !strcasecmp(val, "no"));
	VERIFY_LOAD_STR_PARAM(successregex, val[0]);
	VERIFY_LOAD_STR_PARAM(blacklist_endpoint, val[0]);
	VERIFY_LOAD_FLOAT_PARAM(blacklist_threshold, val[0]);
	VERIFY_LOAD_INT_PARAM(blacklist_failopen, !strcasecmp(val, "yes") || !strcasecmp(val, "no"));
	VERIFY_LOAD_STR_PARAM(outregex, val[0]);
	VERIFY_LOAD_STR_PARAM(exceptioncontext, val[0]);
	VERIFY_LOAD_STR_PARAM(failureaction, !strcasecmp(val, "nothing") || !strcasecmp(val, "hangup") || !strcasecmp(val, "playback") || !strcasecmp(val, "redirect"));
	VERIFY_LOAD_STR_PARAM(failurefile, val[0]);
	VERIFY_LOAD_STR_PARAM(failurelocation, val[0]);
	VERIFY_LOAD_STR_PARAM(region, val[0] && !contains_whitespace(val));
	VERIFY_LOAD_STR_PARAM(clli, val[0] && !contains_whitespace(val));
	VERIFY_LOAD_STR_PARAM(failgroup, val[0] && !contains_whitespace(val));
	VERIFY_LOAD_STR_PARAM(code_good, val[0]);
	VERIFY_LOAD_STR_PARAM(code_fail, val[0]);
	VERIFY_LOAD_STR_PARAM(code_requestfail, val[0]);
	VERIFY_LOAD_STR_PARAM(code_spoof, val[0]);
	VERIFY_LOAD_STR_PARAM(loglevel, val[0] && !contains_whitespace(val));
	VERIFY_LOAD_STR_PARAM(logmsg, val[0]);

	if (!strcasecmp(param, "threshold")) { /* this is parsing an actual number, not turning yes or no into a 1 or 0 */
		if (ast_str_to_int(val, &v->threshold) || v->threshold < 0) {
			ast_log(LOG_WARNING, "Invalid %s: '%s'\n", param, val);
			v->threshold = 0;
		}
		return;
	}
	if (failunknown) {
		if (linenum >= 0) {
			ast_log(LOG_WARNING, "Unknown keyword in profile '%s': %s at line %d of verify.conf\n", v->name, param, linenum);
		} else {
			ast_log(LOG_WARNING, "Unknown keyword in profile '%s': %s\n", v->name, param);
		}
	}
}

/*! \brief Reload verify application module */
static int reload_verify(int reload)
{
	const char *tempstr;
	char *cat = NULL;
	struct call_verify *v;
	struct ast_variable *var;
	struct ast_config *cfg;
	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };

	if (!(cfg = ast_config_load("verify.conf", config_flags))) {
		ast_debug(1, "No verify config file (verify.conf), so no verification profiles loaded\n");
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Config file verify.conf is in an invalid format. Aborting.\n");
		return 0;
	}

	AST_RWLIST_WRLOCK(&verifys);

	/* Reset Global Var Values */
	curltimeout = DEFAULT_CURL_TIMEOUT;

	/* General section */
	if (!ast_strlen_zero(tempstr = ast_variable_retrieve(cfg, "general", "curltimeout"))) {
		if (ast_str_to_int(tempstr, &curltimeout)) {
			ast_log(LOG_WARNING, "Invalid curl timeout: %s\n", tempstr);
			curltimeout = DEFAULT_CURL_TIMEOUT;
		}
	}

	/* Remaining sections */
	while ((cat = ast_category_browse(cfg, cat))) {
		int new = 0;

		if (!strcasecmp(cat, "general")) {
			continue;
		}

		/* Look for an existing one */
		AST_LIST_TRAVERSE(&verifys, v, entry) {
			if (!strcasecmp(v->name, cat)) {
				break;
			}
		}

		if (!v) {
			/* Make one then */
			v = alloc_profile(cat);
			new = 1;
		}

		/* Totally fail if we fail to find/create an entry */
		if (!v) {
			ast_log(LOG_WARNING, "Failed to create verify profile for '%s'\n", cat);
			continue;
		}

		if (!new) {
			ast_mutex_lock(&v->lock);
		}
		/* Re-initialize the defaults */
		v->extendtrust = 1;
		v->allowdisathru = 1;
		v->allowpstnthru = 1;
		v->allowtoken = 0;
		v->threshold = 0;
		v->sanitychecks = 0;
		v->flagprivateip = 1;
		v->blacklist_threshold = 1;
		v->blacklist_failopen = 0;
		ast_copy_string(v->verifymethod, "reverse", sizeof(v->verifymethod));
		ast_copy_string(v->requestmethod, "curl", sizeof(v->requestmethod));
		/* Search Config */
		var = ast_variable_browse(cfg, cat);
		while (var) {
			profile_set_param(v, var->name, var->value, var->lineno, 1);
			var = var->next;
		} /* End while(var) loop */

		if (!new) {
			ast_mutex_unlock(&v->lock);
		} else {
			AST_RWLIST_INSERT_HEAD(&verifys, v, entry);
		}
	}

	ast_config_destroy(cfg);

	AST_RWLIST_UNLOCK(&verifys);

	return 0;
}

/* Copied from func_curl.c */
static int url_is_vulnerable(const char *url)
{
	if (strpbrk(url, "\r\n")) {
		return 1;
	}

	return 0;
}

/* from test_res_prometheus.c */
static size_t curl_write_string_callback(char *rawdata, size_t size, size_t nmemb, void *userdata)
{
	struct ast_str **buffer = userdata;

	/* ast_str_append will return number of bytes written this round */
	return ast_str_append(buffer, 0, "%.*s", (int) (size * nmemb), rawdata);
}

/* from app_verbose.c */
static int verify_log(struct ast_channel *chan, char *level, char *msg)
{
	int lnum;
	char extension[AST_MAX_EXTENSION + 5], context[AST_MAX_EXTENSION + 2];

	if (!strcasecmp(level, "ERROR")) {
		lnum = __LOG_ERROR;
	} else if (!strcasecmp(level, "WARNING")) {
		lnum = __LOG_WARNING;
	} else if (!strcasecmp(level, "NOTICE")) {
		lnum = __LOG_NOTICE;
	} else if (!strcasecmp(level, "DEBUG")) {
		lnum = __LOG_DEBUG;
	} else if (!strcasecmp(level, "VERBOSE")) {
		lnum = __LOG_VERBOSE;
	} else if (!strcasecmp(level, "DTMF")) {
		lnum = __LOG_DTMF;
	} else {
		lnum = ast_logger_get_dynamic_level(level);
	}

	if (lnum < 0) {
		ast_log(LOG_ERROR, "Unknown log level: '%s'\n", level);
		return -1;
	}

	snprintf(context, sizeof(context), "@ %s", ast_channel_context(chan));
	snprintf(extension, sizeof(extension), "Ext. %s", ast_channel_exten(chan));

	ast_log(lnum, extension, ast_channel_priority(chan), context, "%s\n", msg);
	return 0;

	return 0;
}

static int verify_curl(struct ast_channel *chan, struct ast_str *buf, char *url)
{
	CURL **curl;
	char curl_errbuf[CURL_ERROR_SIZE + 1];

	ast_debug(1, "Planning to curl '%s'\n", url);

	if (url_is_vulnerable(url)) {
		ast_log(LOG_ERROR, "URL '%s' is vulnerable to HTTP injection attacks. Aborting CURL() call.\n", url);
		return -1;
	}

	if (!buf) {
		ast_log(LOG_WARNING, "Failed to allocate buffer\n");
		return -1;
	}

	curl = curl_easy_init();
	if (!curl) {
		return -1;
	}

	curl_easy_setopt(curl, CURLOPT_TIMEOUT, curltimeout);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, AST_CURL_USER_AGENT);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_string_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);

	ast_autoservice_start(chan);

	if (curl_easy_perform(curl)) {
		ast_autoservice_stop(chan);
		ast_log(LOG_ERROR, "%s\n", curl_errbuf);
		ast_log(LOG_ERROR, "Failed to curl URL '%s'\n", url);
		curl_easy_cleanup(curl);
		return -1;
	}

	curl_easy_cleanup(curl);
	ast_autoservice_stop(chan);

	if (!ast_str_buffer(buf)) {
		return -1;
	}
	if (ast_str_strlen(buf) < 1) { /* didn't get anything back? */
		return -1;
	}

	return 0;
}

static void verify_set_var(struct ast_channel *chan, char *vname, char *vvalue)
{
	if (!vname || !(*vname)) {
		ast_log(LOG_WARNING, "No variable specified for storing result, discarding: %s\n", vvalue ? vvalue : "(null)");
		return;
	}
	if (!vvalue || !(*vvalue)) {
		ast_log(LOG_WARNING, "No value specified to use as result for '%s' variable\n", vname ? vname : "(null)");
		return;
	}
	pbx_builtin_setvar_helper(chan, vname, vvalue);
}

static int resolve_verify_request(struct ast_channel *chan, char *url, struct ast_str *strbuf, int curl, char *varg1)
{
	char substituted[SUB_BUFLEN];

	if (varg1) {
		pbx_builtin_setvar_helper(chan, "VERIFYARG1", varg1);
	}
	pbx_substitute_variables_helper(chan, url, substituted, sizeof(substituted) - 1);
	if (varg1) {
		pbx_builtin_setvar_helper(chan, "VERIFYARG1", "");
	}
	if (curl) {
		if (verify_curl(chan, strbuf, substituted)) {
			ast_debug(1, "curl failed\n");
			return -1;
		}
		ast_debug(1, "curl result is: %s\n", ast_str_buffer(strbuf));
	} else { /* ENUM */
		int res;
		AST_DECLARE_APP_ARGS(args,
			AST_APP_ARG(number);
			AST_APP_ARG(tech);
			AST_APP_ARG(options);
			AST_APP_ARG(record);
			AST_APP_ARG(zone);
		);
		char tech[80];
		char dest[256] = "", tmp[2] = "", num[AST_MAX_EXTENSION] = "";
		char *s, *p;
		unsigned int record = 1;

		ast_debug(1, "ENUMLOOKUP: '%s'\n", substituted);

		AST_STANDARD_APP_ARGS(args, substituted);

		ast_copy_string(tech,args.tech && !ast_strlen_zero(args.tech) ? args.tech : "sip", sizeof(tech));

		if (!args.zone) {
			args.zone = "e164.arpa";
		}
		if (!args.options) {
			args.options = "";
		}
		if (args.record) {
			record = atoi(args.record) ? atoi(args.record) : record;
		}

		/* strip any '-' signs from number */
		for (s = p = args.number; *s; s++) {
			if (*s != '-') {
				snprintf(tmp, sizeof(tmp), "%c", *s);
				strncat(num, tmp, sizeof(num) - strlen(num) - 1);
			}
		}
		res = ast_get_enum(chan, num, dest, sizeof(dest), tech, sizeof(tech), args.zone, args.options, record, NULL);
		if (res < 0) {
			return -1;
		} else if (!res) {
			ast_log(LOG_WARNING, "ENUM lookup failed for ast_get_enum(%s,%s,%s,%s,%u)\n", num, dest, args.zone, args.options, record);
			return -1;
		}
		ast_debug(1, "ENUM lookup: %s\n", dest);
		p = strchr(dest, ':');
		if (p && strcasecmp(tech, "ALL") && !strchr(args.options, 'u')) {
			ast_str_set(&strbuf, 0, "%s", p + 1);
		} else {
			ast_str_set(&strbuf, 0, "%s", dest);
		}
	}
	return 0;
}

/* simplified and modified version of parse_dial_string from chan_iax2.c */
static void parse_iax2_dial_string(char *data, struct ast_str *strbuf)
{
	char *exten = NULL, *username = NULL, *secret = NULL, *context = NULL, *peer = NULL, *port = NULL;

	strsep(&data, "/"); /* eat the IAX2/ */
	peer = strsep(&data, "/");
	exten = strsep(&data, "/");

	if (exten) {
		data = exten;
		exten = strsep(&data, "@");
		context = data;
	}
	if (peer && strchr(peer, '@')) {
		data = peer;
		username = strsep(&data, "@");
		peer = data;
	}
	if (username) {
		data = username;
		username = strsep(&data, ":");
		secret = strsep(&data, ":");
	}

	data = peer;
	peer = strsep(&data, ":");
	port = data;
	ast_debug(1, "Peer: %s, Port: %s, Username: %s, Secret: %s, Context: %s, Exten: %s\n", peer, port, username, secret, context, exten); /* no need for outkey */
	ast_str_set(&strbuf, 0, "%s", peer); /* piggyback of this ast_str, cause why not? */
}

/* based on https://www.binarytides.com/hostname-to-ip-address-c-sockets-linux/ */
/* not ideal, but ast_get_ip seems to have issues under the hood... */
static int hostname_to_ip(struct ast_channel *chan, char *hostname , char *ip)
{
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_in *h;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	/* DNS lookups could potentially take a while, so autoservice the channel just in case. */
	ast_autoservice_start(chan);

	if ((rv = getaddrinfo(hostname, "4569", &hints, &servinfo)) != 0) {
		ast_log(LOG_WARNING, "getaddrinfo: %s\n", gai_strerror(rv));
		ast_autoservice_stop(chan);
		return 1;
	}

	/* loop through all the results and connect to the first we can */
	for (p = servinfo; p != NULL; p = p->ai_next) {
		h = (struct sockaddr_in *) p->ai_addr;
		strcpy(ip, ast_inet_ntoa(h->sin_addr));
	}

	freeaddrinfo(servinfo);
	ast_debug(1, "Hostname '%s' resolved to IP address '%s'\n", hostname, ip);

	ast_autoservice_stop(chan);

	return 0;
}

static int is_private_ipv4(char *ip)
{
	int oc1, oc2, oc3, oc4;
	char *full, *remainder;
	full = ip;
	oc1 = atoi(strsep(&full, "."));
	oc2 = atoi(strsep(&full, "."));
	oc3 = atoi(strsep(&full, "."));
	oc4 = atoi(strsep(&full, "."));
	remainder = strsep(&full, ".");
	if (remainder) {
		return -1;
	}
	if (oc1 == 10) {
		return 1; /* Class A */
	}
	if (oc1 == 172 && (oc2 >= 16 && oc2 <= 31)) {
		return 2; /* Class B */
	}
	if (oc1 == 192 && oc2 == 168) {
		return 3; /* Class C */
	}
	if (oc1 == 127 && oc2 == 0 && oc3 == 0 && oc4 == 1) {
		return 4; /* localhost */
	}
	if (oc1 == 169 && oc2 == 254) {
		return 4; /* APIPA */
	}
	if (oc4 == 0 || oc4 == 255) {
		return 5; /* broadcast or unicast */
	}
	return 0;
}

static void assign_vars(struct ast_channel *chan, struct ast_str *strbuf, char *vars)
{
	char *var, *tmp = vars;

	ast_debug(1, "Processing variables: %s\n", tmp);

	while ((var = ast_strsep(&tmp, ',', AST_STRSEP_STRIP))) { /* strsep will cut on the next literal comma, and stuff could have commas inside... */
		char *newvar, *oldvar;
		ast_debug(1, "Processing variable assignment: %s\n", var);
		newvar = ast_strsep(&var, '=', AST_STRSEP_STRIP);
		oldvar = ast_strsep(&var, '=', AST_STRSEP_STRIP);
		if (!newvar || !oldvar) {
			ast_log(LOG_WARNING, "Missing variable in assignment: %s=%s\n", newvar, oldvar);
			continue;
		}
		ast_str_substitute_variables(&strbuf, 0, chan, oldvar); /* we don't actually know how big this will be, so use this instead of pbx_substitute_variables_helper */
		ast_debug(1, "Making variable assignment: %s = %s (%s)\n", newvar, oldvar, ast_str_buffer(strbuf));
		pbx_builtin_setvar_helper(chan, newvar, ast_str_buffer(strbuf));
		ast_str_reset(strbuf);
	}
}

static int v_blacklisted(char *name)
{
	struct call_verify *v = NULL;

	AST_RWLIST_RDLOCK(&verifys);
	AST_LIST_TRAVERSE(&verifys, v, entry) {
		if (!strcasecmp(v->name, name)) {
			break;
		}
	}
	AST_RWLIST_UNLOCK(&verifys);

	if (!v) {
		ast_log(LOG_WARNING, "Verification profile '%s' unexpectedly disappeared\n", name);
		return -1;
	}

	ast_mutex_lock(&v->lock);
	v->total_blacklisted++;
	ast_mutex_unlock(&v->lock);

	return 0;
}

static int v_success(char *name, int in)
{
	struct call_verify *v = NULL;

	AST_RWLIST_RDLOCK(&verifys);
	AST_LIST_TRAVERSE(&verifys, v, entry) {
		if (!strcasecmp(v->name, name)) {
			break;
		}
	}
	AST_RWLIST_UNLOCK(&verifys);

	if (!v) {
		ast_log(LOG_WARNING, "Verification profile '%s' unexpectedly disappeared\n", name);
		return -1;
	}

	ast_mutex_lock(&v->lock);
	in ? v->insuccess++ : v->outsuccess++;
	ast_mutex_unlock(&v->lock);

	return 0;
}

static const char *stir_shaken_name(char c)
{
	switch (c) {
	case 'A':
		return "Full Attestation";
	case 'B':
		return "Partial Attestation";
	case 'C':
		return "Gateway Attestation";
	case 'D':
		return "No Validation";
	case 'E':
		return "Empty (No parameter)";
	case 'F':
		return "Failed Validation";
	default:
		break;
	}
	ast_assert(0);
	return "";
}

static char ss_handle_verstat(const char *verstat)
{
	ast_assert(!ast_strlen_zero(verstat));
	/* Treat TN-Validation-Passed as 'C' since Passed on its own doesn't convey a particular attestation,
	 * it just means the validation was done successfully. So, if that's all the info we have, assume the worst. */
	if (!strcmp(verstat, "TN-Validation-Passed-A")) {
		return 'A';
	} else if (!strcmp(verstat, "TN-Validation-Passed-B")) {
		return 'B';
	} else if (!strcmp(verstat, "TN-Validation-Passed-C") || !strcmp(verstat, "TN-Validation-Passed")) {
		return 'C';
	} else if (!strcmp(verstat, "No-TN-Validation")) {
		return 'D';
	} else if (!strcmp(verstat, "TN-Validation-Failed")) {
		return 'F';
	} else {
		ast_log(LOG_WARNING, "Unexpected verstat param value: '%s'\n", verstat);
		return 0;
	}
}

static char parse_hdr_for_verstat(const char *hdr_name, char *hdr, char **verstat)
{
	char *chunk;
	while ((chunk = strsep(&hdr, ";"))) {
		if (!strncmp(chunk, "verstat=", 8)) {
			char *end;
			chunk += 8;
			if (ast_strlen_zero(chunk)) {
				ast_log(LOG_WARNING, "verstat parameter is empty?\n");
				break;
			}
			/* Don't care about anything after this parameter, so stop as soon as we find a symbol, e.g. @, ;, >, etc. */
			end = chunk;
			while (*end) {
				if (!isalpha(*end) && *end != '-') {
					*end = '\0';
					break;
				}
				end++;
			}
			ast_debug(3, "%s header contains verstat parameter: %s\n", hdr_name, chunk);
			*verstat = chunk;
			return ss_handle_verstat(chunk);
		}
	}
	return 0;
}

static void stir_shaken_stat_inc(int type, char c)
{
	ast_assert(type == 0 || type == 1);
	ast_mutex_lock(&ss_lock);
	switch (c) {
	case 'A':
	case 'B':
	case 'C':
	case 'D':
	case 'E':
	case 'F':
		stir_shaken_stats[type][c - 'A'] += 1;
		break;
	default:
		if (isprint(c)) {
			ast_log(LOG_WARNING, "Unexpected STIR/SHAKEN rating: %c\n", c);
		} else if (c) {
			ast_log(LOG_WARNING, "Unexpected STIR/SHAKEN rating: %d\n", c);
		}
	}
	ast_mutex_unlock(&ss_lock);
}

static char parse_pai_header(struct ast_channel *chan, const char *headername)
{
	int res;
	char headerfunc[48];
	char headerval[512] = "";
	char *verstat = NULL; /* At the moment, not used anymore */

	if (!strcmp(ast_channel_tech(chan)->type, "PJSIP")) {
		snprintf(headerfunc, sizeof(headerfunc), "PJSIP_HEADER(read,%s,1)", headername);
	} else { /* SIP */
		snprintf(headerfunc, sizeof(headerfunc), "SIP_HEADER(%s,1)", headername);
	}
	res = ast_func_read(chan, headerfunc, headerval, sizeof(headerval));
	if (res || ast_strlen_zero(headerval)) {
		return 0;
	}
	return parse_hdr_for_verstat(headername, headerval, &verstat);
}

static void parse_stir_shaken(struct ast_channel *chan, const char *stirshaken_var)
{
	char attestation_hdr[128] = "";
	int res;
	char ss_verstat;

	/* STIR/SHAKEN references (including carrier implementations) consulted, in no particular order:
	 * https://www.carrierx.com/documentation/how-it-works/stir-shaken#introduction
	 * https://help.webex.com/en-us/article/8j6te9/Spam-or-fraud-call-indication-in-Webex-Calling
	 * https://www.metaswitch.com/knowledge-center/reference/what-is-stir/shaken
	 * https://www.vitelity.com/frequently-asked-questions-about-stir-shaken/
	 * https://doc.didww.com/services/did/stir-shaken.html
	 * https://support.telnyx.com/en/articles/5402969-stir-shaken-with-telnyx
	 * https://transnexus.com/whitepapers/shaken-vs/
	 * https://www.plivo.com/docs/sip-trunking/concepts/stir-shaken
	 */

	if (strcmp(ast_channel_tech(chan)->type, "PJSIP") && strcmp(ast_channel_tech(chan)->type, "SIP")) {
		ast_log(LOG_WARNING, "STIR/SHAKEN only supported on PJSIP channels\n");
		return;
	}

#define VALID_ATTESTATION(c) (c == 'A' || c == 'B' || c == 'C')

	/* Check P-Attestation-Indicator header.
	 * If it has an attestation value, we can use that directly. */
	if (!strcmp(ast_channel_tech(chan)->type, "PJSIP")) {
		res = ast_func_read(chan, "PJSIP_HEADER(read,P-Attestation-Indicator,1)", attestation_hdr, sizeof(attestation_hdr));
	} else { /* SIP */
		res = ast_func_read(chan, "SIP_HEADER(P-Attestation-Indicator,1)", attestation_hdr, sizeof(attestation_hdr));
	}
	if (!res && !ast_strlen_zero(attestation_hdr)) {
		ss_verstat = attestation_hdr[0];
		if (VALID_ATTESTATION(ss_verstat)) {
			goto done;
		}
		ast_log(LOG_WARNING, "Invalid P-Attestation-Indicator header value '%s'\n", attestation_hdr);
	}

	/* Check the P-Asserted-Identity header, then the From header if it's not present there */
	ss_verstat = parse_pai_header(chan, "P-Asserted-Identity");
	if (!ss_verstat) {
		ss_verstat = parse_pai_header(chan, "From");
	}

done:
	if (ss_verstat) {
		char ss_result[2];
		ast_verb(4, "STIR/SHAKEN attestation rating is '%c' (%s)\n", ss_verstat, stir_shaken_name(ss_verstat));
		snprintf(ss_result, sizeof(ss_result), "%c", ss_verstat);
		pbx_builtin_setvar_helper(chan, stirshaken_var, ss_result);
		stir_shaken_stat_inc(0, ss_verstat);
	} else {
		ast_debug(2, "No STIR/SHAKEN information available\n");
		pbx_builtin_setvar_helper(chan, stirshaken_var, "E");
		stir_shaken_stat_inc(0, 'E');
	}
}

#define VERIFY_STRDUP(field) ast_copy_string(field, v->field, sizeof(field));

static int verify_exec(struct ast_channel *chan, const char *data)
{
	struct call_verify *v;
	char *vresult, *argstr, *callerid;
	struct ast_str *strbuf = NULL;
	int blacklisted = 0, success = 0;

	int curl, method, extendtrust, allowtoken, sanitychecks, threshold, blacklist_failopen;
	char name[AST_MAX_CONTEXT], verifyrequest[PATH_MAX], verifycontext[AST_MAX_CONTEXT], local_var[AST_MAX_CONTEXT], stirshaken_var[AST_MAX_CONTEXT], remote_stirshaken_var[AST_MAX_CONTEXT], remote_var[AST_MAX_CONTEXT], via_remote_var[AST_MAX_CONTEXT], token_remote_var[AST_MAX_CONTEXT], validatetokenrequest[PATH_MAX], code_good[PATH_MAX], code_fail[PATH_MAX], code_spoof[PATH_MAX], exceptioncontext[PATH_MAX], setinvars[PATH_MAX], failgroup[PATH_MAX], failureaction[PATH_MAX], failurefile[PATH_MAX], failurelocation[PATH_MAX], successregex[PATH_MAX], blacklist_endpoint[PATH_MAX], loglevel[AST_MAX_CONTEXT], logmsg[PATH_MAX];
	float blacklist_threshold;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(profile);
		AST_APP_ARG(against);
	);

	argstr = ast_strdupa((char *) data);

	AST_STANDARD_APP_ARGS(args, argstr);

	if (ast_strlen_zero(args.profile)) {
		ast_log(LOG_WARNING, "%s requires an argument (verify profile)\n", app);
		return -1;
	}

	AST_RWLIST_RDLOCK(&verifys);
	AST_LIST_TRAVERSE(&verifys, v, entry) {
		if (!strcasecmp(v->name, args.profile)) {
			break;
		}
	}
	AST_RWLIST_UNLOCK(&verifys);

	/* No realtime support, but if it were added in the future, check that here */

	if (!v) {
		ast_log(LOG_WARNING, "Verification profile '%s' requested, but not found in the configuration.\n", data);
		return -1;
	}

	ast_mutex_lock(&v->lock);
	v->in++;
	if (!strcasecmp(v->verifymethod, "direct")) {
		method = 2;
	} else if (!strcasecmp(v->verifymethod, "regex")) {
		method = 3;
	} else if (!strcasecmp(v->verifymethod, "pattern")) {
		method = 4;
	} else {
		method = 1; /* reverse */
	}
	curl = !strcasecmp(v->requestmethod, "curl") ? 1 : 0;
	extendtrust = v->extendtrust;
	allowtoken = v->allowtoken;
	sanitychecks = v->sanitychecks;
	threshold = v->threshold;
	blacklist_threshold = v->blacklist_threshold;
	blacklist_failopen = v->blacklist_failopen;
	VERIFY_STRDUP(name);
	VERIFY_STRDUP(verifyrequest);
	VERIFY_STRDUP(verifycontext);
	VERIFY_STRDUP(via_remote_var);
	VERIFY_STRDUP(local_var);
	VERIFY_STRDUP(stirshaken_var);
	VERIFY_STRDUP(remote_stirshaken_var);
	VERIFY_STRDUP(remote_var);
	VERIFY_STRDUP(token_remote_var);
	VERIFY_STRDUP(validatetokenrequest);
	VERIFY_STRDUP(exceptioncontext);
	VERIFY_STRDUP(setinvars);
	VERIFY_STRDUP(failgroup);
	VERIFY_STRDUP(failureaction);
	VERIFY_STRDUP(failurefile);
	VERIFY_STRDUP(failurelocation);
	VERIFY_STRDUP(code_good);
	VERIFY_STRDUP(code_fail);
	VERIFY_STRDUP(code_spoof);
	VERIFY_STRDUP(successregex);
	VERIFY_STRDUP(blacklist_endpoint);
	VERIFY_STRDUP(loglevel);
	VERIFY_STRDUP(logmsg);
	ast_mutex_unlock(&v->lock);

	if (!chan || (strcmp(ast_channel_tech(chan)->type, "IAX2") && method <= 2)) {
		ast_log(LOG_ERROR, "Unsupported channel technology: %s (%s only supports IAX2)\n", ast_channel_tech(chan)->type, app);
		return -1;
	}

	if (!(strbuf = ast_str_create(512))) {
		ast_log(LOG_ERROR, "Could not allocate memory for response.\n");
		return -1;
	}

	ast_channel_lock(chan);
	if (ast_strlen_zero(args.against)) {
		callerid = ast_channel_caller(chan)->id.number.str;
	} else {
		callerid = args.against;
	}
	ast_debug(1, "Verifying call against number '%s'\n", callerid);
	ast_channel_unlock(chan);

	/* Analyze STIR/SHAKEN result, if applicable */
	if (!ast_strlen_zero(stirshaken_var)) {
		parse_stir_shaken(chan, stirshaken_var);
	} else if (!ast_strlen_zero(remote_stirshaken_var)) {
		/* If remote result available, use that purely to update the statistics. */
		char result[3];
		int size = strlen(remote_stirshaken_var) + 4;
		char iaxvartmp[size]; /* ${} + null terminator */
		snprintf(iaxvartmp, size, "${%s}", remote_stirshaken_var);
		result[0] = '\0';
		pbx_substitute_variables_helper(chan, iaxvartmp, result, sizeof(result));
		if (strlen(result) > 1) {
			ast_log(LOG_WARNING, "Received invalid STIR/SHAKEN disposition from upstream: %s\n", result);
		} else {
			char ss_verstat = result[0];
			stir_shaken_stat_inc(1, ss_verstat);
			/* Don't automatically set the local var to the remote var, setinvars will do this, if requested. */
		}
	} else {
		ast_debug(3, "No STIR/SHAKEN analysis requested\n");
	}

	if (method == 2) {
		if (!curl) {
			ast_log(LOG_WARNING, "Request method %s is incompatible with verification method %s\n", "direct", "enum");
			return -1;
		}
		if (!resolve_verify_request(chan, verifyrequest, strbuf, curl, callerid)) {
			vresult = ast_str_buffer(strbuf);
			if (*code_good && !strncmp(code_good, vresult, strlen(code_good))) { /* if the result starts with the code_good value, then it's a success. There could be other stuff afterwards. */
				success = 1;
			}
			/* if both are specified, we should do both */
			if (*successregex) {
#define BUFLEN2 256
				char buf[BUFLEN2];
				int errcode;
				regex_t regexbuf;

				success = 0;

				if ((errcode = regcomp(&regexbuf, successregex, REG_EXTENDED | REG_NOSUB))) {
					regerror(errcode, &regexbuf, buf, BUFLEN2);
					ast_log(LOG_WARNING, "Malformed input %s(%s): %s\n", "REGEX", successregex, buf);
				} else if (!regexec(&regexbuf, vresult, 0, NULL, 0)) {
					success = 1;
				} else {
					ast_debug(1, "Code '%s' does not match with regex '%s'\n", vresult, successregex);
				}
				regfree(&regexbuf);
			}
			ast_verb(3, "Verification result for %s (%s) is '%s'\n", callerid, name, vresult ? vresult : "(null)");
			verify_set_var(chan, local_var, vresult);
		}
	} else if (method == 3) { /* regex */
		char buf[BUFLEN2];
		int errcode;
		regex_t regexbuf;

		if (!*successregex) {
			ast_log(LOG_WARNING, "Verify method is 'regex', but 'successregex' not set!\n");
			return -1;
		}

		if ((errcode = regcomp(&regexbuf, successregex, REG_EXTENDED | REG_NOSUB))) {
			regerror(errcode, &regexbuf, buf, BUFLEN2);
			ast_log(LOG_WARNING, "Malformed input %s(%s): %s\n", "REGEX", successregex, buf);
		} else if (!regexec(&regexbuf, callerid, 0, NULL, 0)) {
			verify_set_var(chan, local_var, code_good);
			ast_verb(3, "Verification result for %s (%s) is '%s' (SUCCESS)\n", callerid, name, *code_good ? code_good : "(null)");
			success = 1;
		} else {
			verify_set_var(chan, local_var, code_fail);
			ast_verb(3, "Verification result for %s (%s) is '%s' (FAILURE)\n", callerid, name, *code_fail ? code_fail : "(null)");
		}
		regfree(&regexbuf);
	} else if (method == 4) { /* dialplan pattern */
		char tmpbuf[BUFLEN2]; /* in reality, we only need to store a 1 or a 0 */
		if (!*verifycontext) {
			ast_log(LOG_WARNING, "Verify method is 'pattern', but 'verifycontext' not set!\n");
			return -1;
		}
		if (ast_get_extension_data(tmpbuf, BUFLEN2, chan, verifycontext, callerid, 1)) {
			ast_log(LOG_WARNING, "Failed to find extension match for %s in context %s. Autofallthrough to bad call.\n", callerid, verifycontext);
			/* it's probably a safe bet that if we couldn't find a pattern, it's a bad call. At least, fail safe to flagging as bad. */
			verify_set_var(chan, local_var, code_fail);
			ast_verb(3, "Verification result for %s (%s) is '%s' (FAILURE)\n", callerid, name, *code_fail ? code_fail : "(null)");
		} else {
			verify_set_var(chan, local_var, tmpbuf);
			do {
				char buf[BUFLEN2];
				int errcode;
				regex_t regexbuf;

				if (!*successregex) {
					ast_log(LOG_WARNING, "Verify method is 'regex', but 'successregex' not set!\n");
					return -1;
				}

				if ((errcode = regcomp(&regexbuf, successregex, REG_EXTENDED | REG_NOSUB))) {
					regerror(errcode, &regexbuf, buf, BUFLEN2);
					ast_log(LOG_WARNING, "Malformed input %s(%s): %s\n", "REGEX", successregex, buf);
				} else if (!regexec(&regexbuf, tmpbuf, 0, NULL, 0)) {
					ast_verb(3, "Verification result for %s (%s) is '%s' (SUCCESS)\n", callerid, name, *tmpbuf ? tmpbuf : "(null)");
					success = 1;
				} else {
					ast_verb(3, "Verification result for %s (%s) is '%s' (FAILURE)\n", callerid, name, *tmpbuf ? tmpbuf : "(null)");
				}
				regfree(&regexbuf);
			} while (0);
		}
	} else { /* reverse */
		char remote_result[64] = { 0 };
		char via[64] = { 0 };
		char peerip[50]; /* more than the max IP address size */
		char *dialstring, *peer;
		char ip[50];
		int viaverify = 0;

		pbx_substitute_variables_helper(chan, "${CHANNEL(peerip)}", peerip, sizeof(peerip));

		ast_channel_lock(chan);
		if (*via_remote_var) {
			int size = strlen(via_remote_var) + 4;
			char iaxvartmp[size]; /* ${} + null terminator */
			snprintf(iaxvartmp, size, "${%s}", via_remote_var);
			pbx_substitute_variables_helper(chan, iaxvartmp, via, sizeof(via)); /* pbx_builtin_getvar_helper doesn't work for IAXVAR, or functions in general */
		}
		if (!*remote_var) {
			ast_log(LOG_WARNING, "Missing variable in configuration: %s\n", "remote_var");
		} else {
			int size = strlen(remote_var) + 4;
			char iaxvartmp[size]; /* ${} + null terminator */
			snprintf(iaxvartmp, size, "${%s}", remote_var);
			pbx_substitute_variables_helper(chan, iaxvartmp, remote_result, sizeof(remote_result)); /* pbx_builtin_getvar_helper doesn't work for IAXVAR, or functions in general */
		}
		if (*remote_result) { /* call tandemed through an upstream node, so we need to verify the upstream node */
			if (extendtrust) {
				viaverify = 1;
				ast_debug(1, "Reverse lookup effective against %s instead of %s, to accept result '%s'\n", via, callerid, remote_result);
			} else {
				ast_log(LOG_WARNING, "Call may require extended trust to verify successfully\n");
			}
		} /* call allegedly originated from the node from which we received this call. Use reverse lookups to confirm. */
		ast_channel_unlock(chan);

		if (allowtoken && *token_remote_var) {
			char *token;
			ast_channel_lock(chan);
			token = (char*) pbx_builtin_getvar_helper(chan, token_remote_var);
			ast_channel_unlock(chan);
			if (ast_strlen_zero(token)) {
				ast_debug(1, "Token verification allowed, but no token present, skipping\n");
			} else if (!*validatetokenrequest) {
				ast_log(LOG_WARNING, "Token '%s' is present, but no way to verify it\n", token); /* only applies to reverse verification. Direct must handle tokens on its own. */
			} else if (!*via) {
				ast_log(LOG_WARNING, "Token '%s' is present, but missing via_remote_var in configuration\n", token);
			} else {
				if (resolve_verify_request(chan, validatetokenrequest, strbuf, curl, NULL)) { /* this number is probably not valid on anything, anywhere */
					ast_log(LOG_WARNING, "Token verification failed, proceeding with regular verification\n");
				}
				if (!strcmp(via, ast_str_buffer(strbuf))) { /* token must return the same value that via_remote_var contains */
					if (viaverify) {
						ast_debug(1, "Successfully verified node via token, trusting its assignment of '%s'\n", remote_result);
					} else {
						ast_debug(1, "Successfully verified node via token\n");
					}
					vresult = viaverify ? remote_result : code_good;
					success = 1;
					goto success;
				} else {
					ast_debug(1, "Token resolution '%s' not equal to via '%s'\n", ast_str_buffer(strbuf), via);
				}
				ast_str_reset(strbuf);
			}
		}
		if (*token_remote_var) {
			pbx_builtin_setvar_helper(chan, token_remote_var, ""); /* clear this variable regardless, so it doesn't get reused accidentally on the way out */
		}

		ast_debug(1, "Verifying call against '%s' (%s)\n", viaverify ? via : callerid, viaverify ? "indirect" : "direct");
		if (resolve_verify_request(chan, verifyrequest, strbuf, curl, viaverify ? via : callerid)) {
			goto fail;
		}
		dialstring = ast_strdupa(ast_str_buffer(strbuf));
		ast_str_reset(strbuf);

		if (sanitychecks) {
			char *nonumber, *invalidnumber;
			/* Check that the calling number is not simply blatantly stupid */
			if (resolve_verify_request(chan, verifyrequest, strbuf, curl, "")) {
				goto fail;
			}
			nonumber = ast_strdupa(ast_str_buffer(strbuf)); /* strdupa required since we'll overwrite strbuf */
			ast_str_reset(strbuf);

			if (resolve_verify_request(chan, verifyrequest, strbuf, curl, "0123456789876543210")) { /* this number is probably not valid on anything, anywhere */
				goto fail;
			}
			invalidnumber = ast_strdupa(ast_str_buffer(strbuf));
			ast_str_reset(strbuf);
			/* Check if the URI is something it shouldn't be */
			if (!strcmp(dialstring, nonumber) || !strcmp(dialstring, invalidnumber) || ast_strlen_zero(dialstring)) {
				ast_debug(1, "URI '%s' matched with a reverse lookup that it shouldn't, auto-failing verification\n", dialstring);
				verify_set_var(chan, local_var, code_fail);
				goto done;
			}
		}

		ast_debug(1, "Reverse lookup on caller: %s\n", dialstring);
		if (!curl) {
			ast_debug(1, "ENUM lookup is '%s'\n", dialstring);
			if (*dialstring && dialstring[4] != ':') {
				ast_log(LOG_WARNING, "Unexpected character '%c' in IAX URI: %s\n", dialstring[4], dialstring);
			}
			dialstring[4] = '/'; /* change the : in ENUM lookup to an /, so it's the same as Dial syntax, so we can parse the same way */
		}
		if (!*dialstring) {
			goto fail; /* call came from a number we can't call back, so abort now */
		}
		parse_iax2_dial_string(dialstring, strbuf); /* this is a destructive operation on dialstring, but we don't need it anymore so no need for ast_strdupa */
		peer = ast_strdupa(ast_str_buffer(strbuf));
		ast_debug(1, "Calling host is '%s'\n", peer);
		ast_str_reset(strbuf);

		if (hostname_to_ip(chan, peer, ip)) { /* yes, this works with both hostnames and IP addresses, so just try to resolve everything */
			goto fail;
		}
		ast_debug(1, "%s resolved to %s, next, comparing it with %s\n", peer, ip, peerip);

		if (!strcmp(peerip, ip)) {
			if (viaverify) {
				ast_debug(1, "Successfully verified upstream node, trusting its assignment of '%s'\n", remote_result);
			} else {
				ast_debug(1, "Successfully verified upstream node\n");
			}
			vresult = viaverify ? remote_result : code_good;
			success = 1;
		} else if (!strcmp(peerip, "127.0.0.1")) {
			if (viaverify) {
				ast_debug(1, "Upstream node is loopback, trusting its assignment of '%s'\n", remote_result);
			} else {
				ast_debug(1, "Upstream node is loopback\n");
			}
			vresult = viaverify ? remote_result : code_good;
			success = 1;
		} else {
			vresult = viaverify ? code_spoof : code_fail;
		}
		if (!success && *exceptioncontext && !ast_strlen_zero(exceptioncontext)) { /* check if there are any exceptions specified in the extension context */
#define BUFLEN 64
			char tmpbuf[BUFLEN]; /* in reality, we only need to store a 1 or a 0 */
			char *host, *tmp;
			if (ast_get_extension_data(tmpbuf, BUFLEN, chan, exceptioncontext, viaverify ? via : callerid, 1)) {
				ast_debug(1, "Failed to find extension match for %s in context %s\n", viaverify ? via : callerid, exceptioncontext);
			} else {
				/* no variable substitution, since that's not necessary */
				tmp = tmpbuf; /* we need a char pointer, not a char array */
				while ((host = strsep(&tmp, ","))) { /* is this host defined as an exception? */
					ast_debug(1, "Comparing actual peer '%s' against exception '%s'\n", peerip, host);
					if (!strcmp(peerip, host)) {
						ast_debug(1, "Verified '%s' using manual exception for %s\n", viaverify ? via : callerid, peerip);
						vresult = viaverify ? remote_result : code_good;
						success = 1;
						goto success;
					}
				}
			}
			goto fail;
		}
		if (!success) {
			goto fail;
		}
success: /* only as a branch, if we fall through to here, that doesn't necessarily mean success */
		verify_set_var(chan, local_var, vresult);
		ast_verb(3, "Verification result for %s (%s) is '%s' (SUCCESS)\n", callerid, name, vresult ? vresult : "(null)");
		goto done;
fail:
		verify_set_var(chan, local_var, viaverify ? code_spoof : code_fail);
		ast_verb(3, "Verification result for %s (%s) is '%s' (FAILURE)\n", callerid, name, viaverify ? (*code_spoof ? code_spoof : "(null)") : (*code_fail ? code_fail : "(null)"));
	}

done:
	if (success) { /* technically, might fail if it's gone? So we need to iterate again and check... */
		if (v_success(name, 1)) {
			return -1;
		}
	}

	if (*setinvars) {
		assign_vars(chan, strbuf, setinvars);
	}

	/* process custom logging. Caller could hang up before the announcement plays. So if instructed to log, do it now. Plus, use strbuf while we've still got it. */
	if (*loglevel) {
		if (!*logmsg) {
			ast_log(LOG_WARNING, "No log message format specified for log level '%s'\n", loglevel);
		} else {
			ast_str_substitute_variables(&strbuf, 0, chan, logmsg); /* we don't actually know how big this will be, so use this instead of pbx_substitute_variables_helper */
			ast_debug(5, "Logging format '%s' to level '%s'\n", logmsg, loglevel);
			verify_log(chan, loglevel, ast_str_buffer(strbuf));
			ast_str_reset(strbuf);
		}
	}

	/* if there's an additional blacklist check to perform, do that now */
	if (!ast_strlen_zero(blacklist_endpoint)) { /* don't even bother if it's already failed */
		char substituted[SUB_BUFLEN];

		pbx_substitute_variables_helper(chan, blacklist_endpoint, substituted, sizeof(substituted) - 1);
		if (verify_curl(chan, strbuf, substituted)) {
			ast_debug(1, "Failed to check blacklist for number '%s', %s (%s)\n", callerid, blacklist_failopen ? "accepting" : "rejecting", ast_str_buffer(strbuf));
		} else {
			float blacklistfloat;

			/* don't bother resetting strbuf, since it's about to disappear anyways */
			if (sscanf(ast_str_buffer(strbuf), "%30f", &blacklistfloat) != 1) {
				ast_log(LOG_WARNING, "Failed to parse blacklist result '%s' for number: %s\n", ast_str_buffer(strbuf), callerid);
			} else {
				ast_debug(1, "Blacklist score for caller '%s' is %f\n", callerid, blacklistfloat);
				if (blacklistfloat >= blacklist_threshold) {
					ast_verb(3, "Dropping blacklisted caller '%s' (score %f meets threshold %f)\n", callerid, blacklistfloat, blacklist_threshold);
					blacklisted = 1;
				}
			}
		}
	}

	ast_free(strbuf);

	if (blacklisted) {
		v_blacklisted(name);
		return -1; /* if caller is sufficiently blacklisted, then say goodbye */
	}

	if (!success && *failgroup) { /* assign bad calls to special group if requested */
		char grpcat[4176]; /* avoid compiler warning about insufficient buffer */
		int count = ast_app_group_get_count(name, failgroup);
		if (count >= threshold) { /* if we're alraedy over the threshold, drop the call */
			ast_verb(3, "Current number of unverified calls (%d) is greater than threshold (%d). Dropping call\n", count, threshold);
			return -1;
		}
		snprintf(grpcat, sizeof(grpcat), "%s@%s", name, failgroup);
		if (ast_app_group_set_channel(chan, grpcat)) {
			ast_log(LOG_WARNING, "Group assignment '%s@%s' failed\n", name, failgroup);
		}
	}

	if (!success && *failureaction) { /* do something special with this call */
		if (!strcmp(failureaction, "hangup")) {
			ast_verb(3, "Dropping unverified call\n");
			return -1;
		}
		if (!strcmp(failureaction, "playback")) {
			char *file, *tmp = failurefile;
			if (!tmp) {
				ast_log(LOG_WARNING, "Playback option specified, but failurefile option not set\n");
				return 0; /* just continue */
			}
			ast_indicate(chan, AST_CONTROL_PROGRESS); /* Need at least progress to guarantee audio will be audible */
			while ((file = strsep(&tmp, "&"))) {
				int res;
				if (!ast_fileexists(file, NULL, ast_channel_language(chan))) {
					ast_log(LOG_WARNING, "Playback failed, file '%s' specified in failurefile option does not exist\n", file);
					continue;
				}
				res = ast_streamfile(chan, file, ast_channel_language(chan));
				if (!res) {
					res = ast_waitstream(chan, "");
				}
			}
		}
		if (!strcmp(failureaction, "redirect")) {
			if (!*failurelocation) {
				ast_log(LOG_WARNING, "Redirect option enabled, but no redirect location specified\n");
				return 0;
			}
			ast_verb(3, "Redirecting call to %s\n", failurelocation);
			ast_parseable_goto(chan, failurelocation);
		}
	}

	return 0;
}

#define VERIFY_ASSERT_VAR_EXISTS(varname) \
if (!*varname || ast_strlen_zero(v->varname)) { \
	ast_log(LOG_WARNING, "Missing or empty value for %s\n", #varname); \
	pbx_builtin_setvar_helper(chan, "OUTVERIFYSTATUS", "FAILURE"); \
	return -1; \
}

static int outverify_exec(struct ast_channel *chan, const char *data)
{
	struct call_verify *v;
	char *argstr, *localvar;
	char *cnam, *currentcode;
	int success = 1;
	int len;

	int allowtoken, allowdisathru, allowpstnthru, flagprivateip;
	char name[AST_MAX_CONTEXT], verifyrequest[PATH_MAX], local_var[AST_MAX_CONTEXT], remote_var[AST_MAX_CONTEXT], via_remote_var[AST_MAX_CONTEXT], setoutvars[PATH_MAX], token_remote_var[AST_MAX_CONTEXT], validatetokenrequest[AST_MAX_CONTEXT], obtaintokenrequest[PATH_MAX], via_number[AST_MAX_CONTEXT], clli[AST_MAX_CONTEXT], region[AST_MAX_CONTEXT], outregex[PATH_MAX];

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(profile);
		AST_APP_ARG(lookup);
	);

	argstr = ast_strdupa((char *) data);

	AST_STANDARD_APP_ARGS(args, argstr);

	if (ast_strlen_zero(args.profile)) {
		ast_log(LOG_WARNING, "%s requires an argument (verify profile)\n", app);
		return -1;
	}

	/* don't restrict to IAX2 channels, because it's not technically one yet */

	AST_RWLIST_RDLOCK(&verifys);
	AST_LIST_TRAVERSE(&verifys, v, entry) {
		if (!strcasecmp(v->name, args.profile)) {
			break;
		}
	}
	AST_RWLIST_UNLOCK(&verifys);

	/* No realtime support, but if it were added in the future, check that here */

	if (!v) {
		ast_log(LOG_WARNING, "Verification profile '%s' requested, but not found in the configuration.\n", data);
		return -1;
	}

	ast_mutex_lock(&v->lock);
	v->out++;
	allowtoken = v->allowtoken;
	allowdisathru = v->allowdisathru;
	allowpstnthru = v->allowpstnthru;
	flagprivateip = v->flagprivateip;
	VERIFY_STRDUP(name);
	VERIFY_STRDUP(verifyrequest);
	VERIFY_STRDUP(via_remote_var);
	VERIFY_STRDUP(local_var);
	VERIFY_STRDUP(remote_var);
	VERIFY_STRDUP(token_remote_var);
	VERIFY_STRDUP(validatetokenrequest);
	VERIFY_STRDUP(obtaintokenrequest);
	VERIFY_STRDUP(setoutvars);
	VERIFY_STRDUP(via_number);
	VERIFY_STRDUP(clli);
	VERIFY_STRDUP(region);
	VERIFY_STRDUP(outregex);
	ast_mutex_unlock(&v->lock);

	VERIFY_ASSERT_VAR_EXISTS(remote_var);
	VERIFY_ASSERT_VAR_EXISTS(local_var);
	VERIFY_ASSERT_VAR_EXISTS(via_remote_var);
	VERIFY_ASSERT_VAR_EXISTS(via_number);

	localvar = local_var;

	/* The config file var could be prefixed with 1 or 2 underscores, so that when the var is set, it increases the scope. */
	/* These aren't used for reading vars, as we are now, so eat any leading underscores. */
	while (*localvar == '_') {
		localvar++;
	}

	/* always send these variables, even though it's only strictly necessary if this call did NOT originate from this node */
	len = strlen(via_remote_var) + 4;
	do { /* limit the scope of this nonsense... */
		char score[64];
		char iaxvartmp[len]; /* ${} + null terminator */
		snprintf(iaxvartmp, len, "${%s}", localvar);
		ast_channel_lock(chan);
		pbx_substitute_variables_helper(chan, iaxvartmp, score, sizeof(score));
		ast_channel_unlock(chan);
		pbx_builtin_setvar_helper(chan, remote_var, score);
		ast_debug(1, "Setting %s to '%s' and %s to '%s'\n", remote_var, score, via_remote_var, via_number);
		currentcode = ast_strdupa(score);
	} while (0);

	pbx_builtin_setvar_helper(chan, via_remote_var, via_number);

	/* If we need to request a verification token, do and set that now */
	if (allowtoken && *obtaintokenrequest && !ast_strlen_zero(obtaintokenrequest)) {
		struct ast_str *strbuf = NULL;
		char substituted[SUB_BUFLEN];

		VERIFY_ASSERT_VAR_EXISTS(token_remote_var);

		if (!(strbuf = ast_str_create(512))) {
			ast_log(LOG_ERROR, "Could not allocate memory for response.\n");
			pbx_builtin_setvar_helper(chan, "OUTVERIFYSTATUS", "FAILURE");
			return -1;
		}

		pbx_substitute_variables_helper(chan, obtaintokenrequest, substituted, sizeof(substituted) - 1);
		if (verify_curl(chan, strbuf, substituted)) {
			ast_log(LOG_WARNING, "Failed to obtain verification token. This call may not succeed.\n");
			pbx_builtin_setvar_helper(chan, "OUTVERIFYSTATUS", "FAILURE");
			success = 0;
		}
		ast_debug(1, "curl result is: %s\n", ast_str_buffer(strbuf));
		pbx_builtin_setvar_helper(chan, token_remote_var, ast_str_buffer(strbuf));
		ast_free(strbuf);
	}

	cnam = ast_channel_caller(chan)->id.name.str;

	if (!allowdisathru && strstr(cnam, " via ")) {
		pbx_builtin_setvar_helper(chan, "OUTVERIFYSTATUS", "QUARANTINEDISA");
		success = 0;
	}
	if (!allowpstnthru && strstr(cnam, " PSTN")) {
		pbx_builtin_setvar_helper(chan, "OUTVERIFYSTATUS", "QUARANTINEPSTN");
		success = 0;
	}

	if (*clli && !ast_strlen_zero(clli) && (!ast_strlen_zero(cnam) && !strstr(cnam, " via ")) && !ast_strlen_zero(currentcode)) {
		int pstn = 0;
		int len = strlen(cnam) + strlen(clli) + 11 + (*region ? strlen(region) + 2 : 0);
		/* if the call didn't originate here, tag the call so people know how it got there, if it isn't tagged already */
		char newcnam[len]; /* enough for adding " via ", " ", country code, " ", " PSTN", original CNAM, CLLI, and null terminator */

		/*! \todo add PSTN detection, through codes... */

		if (*region && !ast_strlen_zero(region) && pstn) {
			if (snprintf(newcnam, sizeof(newcnam), "%s via %s %s PSTN", cnam, clli, region) > sizeof(newcnam)) {
				ast_log(LOG_WARNING, "CNAM '%s' will be truncated when appended... this shouldn't happen\n", cnam);
			}
		} else {
			if (snprintf(newcnam, sizeof(newcnam), pstn ? "%s via %s PSTN" : "%s via %s", cnam, clli) > sizeof(newcnam)) {
				ast_log(LOG_WARNING, "CNAM '%s' will be truncated when appended... this shouldn't happen\n", cnam);
			}
		}
		ast_verb(3, "Updating CALLERID(name) from '%s' to '%s'\n", cnam, newcnam);
		if (ast_channel_caller(chan)->id.name.str) {
			ast_free(ast_channel_caller(chan)->id.name.str);
		}
		ast_channel_caller(chan)->id.name.str = ast_strdup(newcnam);
		ast_trim_blanks(ast_channel_caller(chan)->id.name.str);
	}

	if (!ast_strlen_zero(args.lookup) || *setoutvars) {
		struct ast_str *strbuf = NULL; /* use the same both strbuf for both lookup check and setoutvars */

		if (!(strbuf = ast_str_create(512))) {
			ast_log(LOG_ERROR, "Could not allocate memory for response.\n");
			pbx_builtin_setvar_helper(chan, "OUTVERIFYSTATUS", "FAILURE");
			return -1;
		}

		if (!ast_strlen_zero(args.lookup)) {
			char ip[64];
			char *peer, *lookup;
			int malicious = 0;

			lookup = ast_strdupa(args.lookup);

			parse_iax2_dial_string(lookup, strbuf); /* get the host */
			peer = ast_strdupa(ast_str_buffer(strbuf));
			if (hostname_to_ip(chan, peer, ip)) { /* yes, this works with both hostnames and IP addresses, so just try to resolve everything */
				ast_debug(1, "Failed to resolve hostname '%s'\n", peer);
				malicious = 1;
			} else if (flagprivateip && is_private_ipv4(ip)) { /* make sure it's not a private IPv4 address */
				ast_debug(1, "Hostname '%s' resolves to private or invalid IP address: %s\n", peer, ip);
				malicious = 1;
			} else if (strncasecmp(args.lookup, "IAX2/", 5)) {
				ast_debug(1, "Detected non-IAX2 channel technology for lookup: %s\n", args.lookup);
				malicious = 1;
			} else if (*outregex) {
				char buf[BUFLEN2];
				int errcode;
				regex_t regexbuf;

				if ((errcode = regcomp(&regexbuf, outregex, REG_EXTENDED | REG_NOSUB))) {
					regerror(errcode, &regexbuf, buf, BUFLEN2);
					ast_log(LOG_WARNING, "Malformed input %s(%s): %s\n", "REGEX", outregex, buf);
				} else if (regexec(&regexbuf, args.lookup, 0, NULL, 0)) {
					ast_debug(1, "URI '%s' does not match with regex '%s'\n", args.lookup, outregex);
					malicious = 1;
				}
				regfree(&regexbuf);
			}
			if (malicious) {
				pbx_builtin_setvar_helper(chan, "OUTVERIFYSTATUS", "MALICIOUS");
				success = 0;
			}
			ast_str_reset(strbuf);
		}

		if (*setoutvars) {
			assign_vars(chan, strbuf, setoutvars);
		}
		ast_free(strbuf);
	}

	if (success) { /* technically, might fail if it's gone? So we need to iterate again and check... */
		if (v_success(name, 0)) {
			return -1;
		}
		pbx_builtin_setvar_helper(chan, "OUTVERIFYSTATUS", "PROCEED");
	}

	return 0;
}

static char *handle_show_stirshaken(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	unsigned int total[2];
	char c;

	total[0] = total[1] = 0;

	switch(cmd) {
	case CLI_INIT:
		e->command = "verify show stirshaken";
		e->usage =
			"Usage: verify show stirshaken\n"
			"       Display STIR/SHAKEN call statistics.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	ast_cli(a->fd, "%-30s %14s %14s\n", "STIR/SHAKEN Rating", "# Direct Calls", "Passthru Calls");
	for (c = 'A'; c <= 'F'; c++) {
		ast_cli(a->fd, "%c: %-27s %14u %14u\n", c, stir_shaken_name(c), stir_shaken_stats[0][c - 'A'], stir_shaken_stats[1][c - 'A']);
		total[0] += stir_shaken_stats[0][c - 'A'];
		total[1] += stir_shaken_stats[1][c - 'A'];
	}
	ast_cli(a->fd, "------------------------------ ----------\n");
	ast_cli(a->fd, "%-30s %14u %14u\n", "Total # STIR/SHAKEN Calls", total[0], total[1]);
	ast_cli(a->fd, "%-30s %14s %14u\n", "", "=", total[0] + total[1]);

	return CLI_SUCCESS;
}

/*! \brief CLI command to list verification profiles */
static char *handle_show_profiles(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-20s %-7s %-8s %-10s %-13s %-11s %-9s %-11s %-14s\n"
#define FORMAT2 "%-20s %7s %8u %10u %13u %11u %9u %11u %14u\n"
	struct call_verify *v;

	switch(cmd) {
	case CLI_INIT:
		e->command = "verify show profiles";
		e->usage =
			"Usage: verify show profiles\n"
			"       Lists all verification profiles.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	ast_cli(a->fd, FORMAT, "Name", "Method", "Total In", "Success In", "% In Verified", "Blacklisted", "Total Out", "Success Out", "% Out Verified");
	ast_cli(a->fd, FORMAT, "--------------------", "-------", "--------", "----------", "-------------", "-----------", "---------", "-----------", "--------------");
	AST_RWLIST_RDLOCK(&verifys);
	AST_LIST_TRAVERSE(&verifys, v, entry) {
		ast_cli(a->fd, FORMAT2, v->name, v->verifymethod, v->in, v->insuccess, (int) (v->in == 0 ? 0 : (100.0 * v->insuccess) / v->in),
			v->total_blacklisted,
			v->out, v->outsuccess, (int) (v->out == 0 ? 0 : (100.0 * v->outsuccess) / v->out));
	}
	AST_RWLIST_UNLOCK(&verifys);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

/*! \brief CLI command to dump verification profile */
static char *handle_show_profile(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-32s : %s\n"
#define FORMAT2 "%-32s : %f\n"
	struct call_verify *v;
	int which = 0;
	char *ret = NULL;

	switch(cmd) {
	case CLI_INIT:
		e->command = "verify show profile";
		e->usage =
			"Usage: verify show profile <profile name>\n"
			"       Displays information about a verification profile.\n";
		return NULL;
	case CLI_GENERATE:
		if (a->pos != 3) {
			return NULL;
		}
		AST_LIST_TRAVERSE(&verifys, v, entry) {
			if (!strncasecmp(a->word, v->name, strlen(a->word)) && ++which > a->n) {
				ret = ast_strdup(v->name);
				break;
			}
		}

		return ret;
	}

	if (a->argc != 4) {
		return CLI_SHOWUSAGE;
	}

	AST_RWLIST_RDLOCK(&verifys);
	AST_LIST_TRAVERSE(&verifys, v, entry) {
		if (!strcasecmp(a->argv[3], v->name)) {
			ast_cli(a->fd, FORMAT, "Name", v->name);
			ast_cli(a->fd, FORMAT, "Verification Method", v->verifymethod);
			ast_cli(a->fd, FORMAT, "Request Method", v->requestmethod);
			ast_cli(a->fd, FORMAT, "Verification Request", v->verifyrequest);
			ast_cli(a->fd, FORMAT, "Verification Context", v->verifycontext);
			ast_cli(a->fd, FORMAT, "Validate Token Request", v->validatetokenrequest);
			ast_cli(a->fd, FORMAT, "Obtain Token Request", v->obtaintokenrequest);
			ast_cli(a->fd, FORMAT, "Verify Variable Name", v->local_var);
			ast_cli(a->fd, FORMAT, "Via Variable Name", v->via_number);
			ast_cli(a->fd, FORMAT, "Remote Verify Variable Name", v->remote_var);
			ast_cli(a->fd, FORMAT, "Remote Via Variable Name", v->via_remote_var);
			ast_cli(a->fd, FORMAT, "Remote Token Variable Name", v->token_remote_var);
			ast_cli(a->fd, FORMAT, "Incoming Variable Assignments", v->setinvars);
			ast_cli(a->fd, FORMAT, "Outgoing Variable Assignments", v->setoutvars);
			ast_cli(a->fd, FORMAT, "Sanity Checks", AST_CLI_YESNO(v->sanitychecks));
			ast_cli(a->fd, FORMAT, "Allow Extending Trust", AST_CLI_YESNO(v->extendtrust));
			ast_cli(a->fd, FORMAT, "Allow DISA Thru", AST_CLI_YESNO(v->allowdisathru));
			ast_cli(a->fd, FORMAT, "Allow PSTN Thru", AST_CLI_YESNO(v->allowpstnthru));
			ast_cli(a->fd, FORMAT, "Allow Token", AST_CLI_YESNO(v->allowtoken));
			ast_cli(a->fd, FORMAT, "Flag Private IP Addresses", AST_CLI_YESNO(v->flagprivateip));
			ast_cli(a->fd, FORMAT, "Success Regex", v->successregex);
			ast_cli(a->fd, FORMAT, "Outgoing Regex", v->outregex);
			ast_cli(a->fd, FORMAT, "Blacklist Endpoint", v->blacklist_endpoint);
			ast_cli(a->fd, FORMAT2, "Blacklist Threshold", v->blacklist_threshold);
			ast_cli(a->fd, FORMAT, "Blacklist Fail Open", AST_CLI_YESNO(v->blacklist_failopen));
			ast_cli(a->fd, FORMAT, "Exception Context", v->exceptioncontext);
			ast_cli(a->fd, FORMAT, "Failure Action", v->failureaction);
			ast_cli(a->fd, FORMAT, "Failure Playback File", v->failurefile);
			ast_cli(a->fd, FORMAT, "Failure Dialplan Location", v->failurelocation);
			ast_cli(a->fd, FORMAT, "Region Identifier", v->region);
			ast_cli(a->fd, FORMAT, "CLLI", v->clli);
			ast_cli(a->fd, FORMAT, "Fail Group", v->failgroup);
			ast_cli(a->fd, FORMAT, "code_good", v->code_good);
			ast_cli(a->fd, FORMAT, "code_fail", v->code_fail);
			ast_cli(a->fd, FORMAT, "code_requestfail", v->code_requestfail);
			ast_cli(a->fd, FORMAT, "code_spoof", v->code_spoof);
			ast_cli(a->fd, FORMAT, "Log Level", v->loglevel);
			ast_cli(a->fd, FORMAT, "Log Msg Fmt", v->logmsg);
			ast_cli(a->fd, "%s%u of %u%s incoming calls in profile '%s' have successfully passed verification\n",
				ast_term_color(v->insuccess == 0 ? COLOR_RED : COLOR_GREEN, COLOR_BLACK),
				v->insuccess, v->in, ast_term_reset(), v->name);
			ast_cli(a->fd, "%s%u of %u%s outgoing calls in profile '%s' have successfully been out-verified\n",
				ast_term_color(v->outsuccess == 0 ? COLOR_RED : COLOR_GREEN, COLOR_BLACK),
				v->outsuccess, v->out, ast_term_reset(), v->name);
			break;
		}
	}
	AST_RWLIST_UNLOCK(&verifys);
	if (!v) {
		ast_cli(a->fd, "No such verify profile: '%s'\n", a->argv[3]);
	}

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

/*! \brief CLI command to reset verification stats */
static char *handle_reset_stats(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct call_verify *v;
	char c;

	switch(cmd) {
	case CLI_INIT:
		e->command = "verify reset stats";
		e->usage =
			"Usage: verify reset stats\n"
			"       Resets call verification statistics.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	AST_RWLIST_RDLOCK(&verifys);
	AST_LIST_TRAVERSE(&verifys, v, entry) {
		v->in = 0;
		v->insuccess = 0;
		v->out = 0;
		v->outsuccess = 0;
	}
	AST_RWLIST_UNLOCK(&verifys);

	/* Reset STIR/SHAKEN stats */
	ast_mutex_lock(&ss_lock);
	for (c = 'A'; c <= 'F'; c++) {
		stir_shaken_stats[0][c - 'A'] = 0;
		stir_shaken_stats[1][c - 'A'] = 0;
	}
	ast_mutex_unlock(&ss_lock);

	return CLI_SUCCESS;
}

static struct ast_cli_entry verify_cli[] = {
	AST_CLI_DEFINE(handle_show_stirshaken, "Display STIR/SHAKEN statistics"),
	AST_CLI_DEFINE(handle_show_profiles, "Display statistics about verification profiles"),
	AST_CLI_DEFINE(handle_show_profile, "Displays information about a verification profile"),
	AST_CLI_DEFINE(handle_reset_stats, "Resets call verification statistics for all profiles"),
};

static int unload_module(void)
{
	struct call_verify *v;

	ast_unregister_application(app);
	ast_unregister_application(app2);
	ast_cli_unregister_multiple(verify_cli, ARRAY_LEN(verify_cli));

	AST_RWLIST_WRLOCK(&verifys);
	while ((v = AST_RWLIST_REMOVE_HEAD(&verifys, entry))) {
		ast_free(v);
	}

	AST_RWLIST_UNLOCK(&verifys);

	ast_mutex_destroy(&ss_lock);
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

	ast_mutex_init(&ss_lock);

	if (reload_verify(0)) {
		ast_mutex_destroy(&ss_lock);
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_cli_register_multiple(verify_cli, ARRAY_LEN(verify_cli));

	res = ast_register_application_xml(app, verify_exec);
	res |= ast_register_application_xml(app2, outverify_exec);

	return res;
}

static int reload(void)
{
	return reload_verify(1);
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Call Verification Application",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.reload = reload,
);
