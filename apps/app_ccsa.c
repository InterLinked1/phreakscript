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
 * \brief Switched Services Networks, e.g. Common Control Switching Arrangements (CCSA)
 * including:
 * - Alternate Routing
 * - Automatic Route Selection (Basic and Deluxe)
 * - Facility Restriction Levels
 * - Authorization Codes
 * - Call Back and Off Hook Queuing
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 */

/*! \li \ref app_ccsa.c uses the configuration file \ref ccsa.conf
 * \addtogroup configuration_file Configuration Files
 */

/*!
 * \page ccsa.conf ccsa.conf
 */

/*** MODULEINFO
	<use type="module">app_dtmfstore</use>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/cli.h"
#include "asterisk/indications.h"
#include "asterisk/config.h"
#include "asterisk/app.h"
#include "asterisk/utils.h"
#include "asterisk/strings.h"
#include "asterisk/musiconhold.h"
#include "asterisk/format_cache.h"
#include "asterisk/devicestate.h"

/*** DOCUMENTATION
	<application name="CCSA" language="en_US">
		<synopsis>
			Make a CCSA (Common Control Switching Arrangement) call
		</synopsis>
		<syntax>
			<parameter name="exten" required="true">
				<para>Destination to be called over CCSA (typically a 7 digit on-net or 11-digit off-net number).</para>
				<para>This will be made available as the variable <literal>CCSA_EXTEN</literal>, which can be used in configuration.</para>
			</parameter>
			<parameter name="ccsa" required="true">
				<para>CCSA profile to use for call.</para>
			</parameter>
			<parameter name="routes" required="false">
				<para>Ordered pipe-separated list of routes to use for call.</para>
				<para>Any MER routes should be provided last, after non-MER routes.</para>
				<para>If not specified, the routes configured for the specified CCSA will be used in the order specified in the config.</para>
			</parameter>
			<parameter name="options" required="false">
				<optionlist>
					<option name="a">
						<para>Call made using Remote Access.</para>
						<para>Remote calls are subject to additional restrictions (e.g. Call Back Queuing is not available).</para>
						<para>Specify this option to indicate a remote call and automatically inhibit Call Back Queuing, as well
						as to ensure that authorization codes not designated as valid for remote access cannot be used.</para>
					</option>
					<option name="c">
						<para>Allow Call Back Queuing for this call.</para>
						<para>Call Back Queuing may (should) only be used with main, tandem, and satellite switches/PBXs.
						It may (should) not be used with tributary PBXs or calls over other private facilities.</para>
					</option>
					<option name="f">
						<para>Facility Restriction Level (FRL) of caller, possibly derived from Traveling Class Mark (TCM).</para>
						<para>The caller will not be allowed to use routes with a minimum FRL exceeding this FRL, unless an
						authorization code is provided that has an FRL of at least the FRL required by the facility.</para>
					</option>
					<option name="i">
						<para>Force outgoing Caller ID to this value.</para>
						<para>This can be used to set the Caller ID on any outgoing attempts to a specific value.
						To always set the Caller ID to a specific value for a given route, the 'f' Dial option should
						instead be included in the <literal>dialstr</literal>configuration option.</para>
						<para>This option is mainly useful when the outgoing Caller ID for certain calls will sometimes,
						but not always, be forced to a particular value, avoiding the need to define two routes,
						one with the f option and one without.</para>
						<para>This option is incompatible with including a URL in the <literal>dialstr</literal> option,
						since this will simply append the f option to the dial string for each route.</para>
					</option>
					<option name="m">
						<para>Music on hold class for use with Off Hook Queuing.</para>
					</option>
					<option name="o">
						<para>Maximum duration, in seconds, for Off-Hook Queuing.</para>
						<para>If not specified, the default, 0, will disable Off-Hook Queuing for this call.</para>
						<para>If specified with no arguments, the default of 60 seconds will be used.</para>
						<para>Off Hook Queuing may be used with any call.</para>
					</option>
					<option name="p">
						<para>Initial priority of call, from 0-3, in queue if Call Back Queuing is used.</para>
						<para>If not specified, the default priority (0) is used.</para>
						<para>The highest priority (3) is always used for Off Hook Queued calls.</para>
					</option>
					<option name="r">
						<para>MLPP preemption/priority capabilities for the call (e.g. those used in AUTOVON).</para>
						<para>Should be A (Flash Override), B (Flash), C (Immediate), or D (Priority), if specified.</para>
						<para>Default is no MLPP priority (Routine).</para>
					</option>
					<option name="u">
						<para>Disable FRL upgrades for this call. Normally, FRL may be upgraded if <literal>frl_allow_upgrade</literal>
						is set to yes. However, on certain calls, particularly for users without authorization codes,
						it may be pointless to allow for an FRL upgrade mid-call, in which case this can be disabled, and all routes
						with an FRL exceeding the callers will be silently skipped.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Places an on-net or off-net call over default or specified routes using private facilities.</para>
			<para>This module provides a generic Switched Services Network (CCSA, ETN, and EPSCS) implementation
			that can be used for alternate routing, automatic route selection, facility access control, and off-hook and ringback queuing.</para>
			<note><para>WARNING: Please be aware that improper configuration of this module, either in the module configuration file or
			in your dialplan, can lead to security issues. In particular, improper route definitions and matching can lead
			to "route leakage" or other unauthorized calls. This module assumes familiarity with configuration of Switched Services Networks.</para></note>
			<para>If the caller does not hangup, the call result can be obtained as follows:</para>
			<variablelist>
				<variable name="CCSA_RESULT">
					<para>Outcome of the CCSA call attempt</para>
					<value name="SUCCESS">
						Successfully completed call, either immediately, with an FRL upgrade, or after an Off Hook Queue.
						Keep in mind that you may still need to check the DIALSTATUS for call disposition,
						if the far end provided a cause code instead of using in-band signaling.
					</value>
					<value name="CBQ">
						Call was Call Back Queued due to no available preferred routes
					</value>
					<value name="NO_CCSA">
						No such CCSA
					</value>
					<value name="NO_ROUTES">
						No routes available for CCSA destination
					</value>
					<value name="UNROUTABLE">
						Destination cannot be reached using any routes
					</value>
					<value name="UNAUTHORIZED">
						Not authorized for the available CCSA routes
					</value>
					<value name="AUTH_CODE">
						Invalid authorization code entered
					</value>
					<value name="OHQ_FAILED">
						Off Hook Queued for available route but timed out
					</value>
					<value name="CBQ_FAILED">
						Call Back Queue failed
					</value>
					<value name="PREEMPTED">
						Call completed but was preempted before its conclusion
					</value>
					<value name="PREEMPTION_FAILED">
						Preemption attempted but failed due to insufficient precedence to preempt
					</value>
					<value name="FAILED">
						Other failure not covered above
					</value>
				</variable>
				<variable name="TCM">
					<para>The Facility Restriction Level (FRL) is encoded into a Traveling Class Mark (TCM)
					when sent on inter-tandem tie trunks so that call restrictions can be enforced throughout
					a network of private facilities.</para>
					<para>The TCM variable will be set before any outgoing calls. However, this module does not automatically
					send the TCM on its own. If the TCM is to be sent on a tie trunk, it should be sent as the last digit,
					after any routing digits. (Of course, it could be conveyed using another means as well, such as
					out of band using an IAXVAR on IAX2 trunks, if the other end supported that, but on standard
					in-band DTMF and MF facilities, it is conveyed as the last digit).</para>
					<para>Hence, if TCM is required on an outgoing call, the dial string for a route should contain
					the TCM as the last routing digit as appropriate.</para>
				</variable>
				<variable name="CCSA_EXTEN">
					<para>The provided extension argument will be used to set this variable, which can then be used
					in configuration for the <literal>dialstr</literal> setting. Don't provide this as the extension
					argument itself!</para>
				</variable>
				<variable name="CCSA_CHANNEL">
					<para>Will be set to the current channel name, with inheritance, so that spawned channels can refer to it,
					if needed, for setting CDR variables.</para>
				</variable>
			</variablelist>
		</description>
	</application>
	<configInfo name="app_ccsa" language="en_US">
		<synopsis>Module to provide Common Control Switching Arrangement functionality</synopsis>
		<configFile name="ccsa.conf">
			<configObject name="general">
				<synopsis>Options that apply globally to app_ccsa</synopsis>
				<configOption name="cdrvar_frl">
					<synopsis>CDR field in which to store original FRL</synopsis>
					<description>
						<para>If provided, this (custom) CDR field will be filled with the original FRL of the caller.</para>
					</description>
				</configOption>
				<configOption name="cdrvar_frl_req">
					<synopsis>CDR field in which to store required FRL</synopsis>
					<description>
						<para>If provided, this (custom) CDR field will be filled with the FRL required for the facility.</para>
					</description>
				</configOption>
				<configOption name="cdrvar_frl_eff">
					<synopsis>CDR field in which to store the effective (upgraded) FRL</synopsis>
					<description>
						<para>If provided, this (custom) CDR field will be filled with the effective (upgraded) FRL used for a call.</para>
					</description>
				</configOption>
				<configOption name="cdrvar_aiod">
					<synopsis>CDR field in which to store the Automatic Identified Outward Dialing number, if overridden</synopsis>
					<description>
						<para>If provided, this (custom) CDR field will be filled with the AIOD of a call, if overridden.
						This includes if the <literal>aiod</literal> option is set for a route or if the <literal>i</literal>
						option is used at runtime.</para>
						<para>This option is incompatible with including a URL in the <literal>dialstr</literal> option,
						since this will simply append the f option to the dial string for each route.</para>
					</description>
				</configOption>
				<configOption name="cdrvar_mlpp">
					<synopsis>CDR field in which to store the MLPP precedence for a call</synopsis>
					<description>
						<para>If provided, this (custom) CDR field will be filled with the MLPP precedence for a call, if present.</para>
					</description>
				</configOption>
				<configOption name="cdrvar_authcode">
					<synopsis>CDR field in which to store the auth code for a call</synopsis>
					<description>
						<para>If provided, this (custom) CDR field will be filled with the authorization code used for the call.</para>
					</description>
				</configOption>
				<configOption name="cdrvar_facility">
					<synopsis>CDR field in which to store the facility (trunk group) used for a call</synopsis>
					<description>
						<para>If provided, this (custom) CDR field will be filled with the facility used for the call.</para>
						<para>Note that this refers to the facility used by the route taken, not the route itself.</para>
					</description>
				</configOption>
				<configOption name="cdrvar_route">
					<synopsis>CDR field in which to store the route used for a call</synopsis>
					<description>
						<para>If provided, this (custom) CDR field will be filled with the route used for the call.</para>
					</description>
				</configOption>
				<configOption name="cdrvar_queuestart">
					<synopsis>CDR field in which to store the queue start timestamp</synopsis>
					<description>
						<para>If provided, this (custom) CDR field will be filled with the timestamp at which queuing starts.</para>
					</description>
				</configOption>
				<configOption name="cdrvar_digits">
					<synopsis>CDR field in which to store any dialed digits</synopsis>
					<description>
						<para>If provided, this (custom) CDR field will be filled with up to 32 digits dialed by the caller during the call.</para>
						<para>This functionally internally invokes the <literal>StoreDTMF</literal> application.</para>
						<note><para>Use of this option could be a privacy violation in your jurisdiction. You are responsible for compliance with local laws and regulations.</para></note>
					</description>
				</configOption>
			</configObject>
			<configObject name="route">
				<synopsis>Individual CCSA route (trunk group).</synopsis>
				<configOption name="facility">
					<synopsis>Name of facility</synopsis>
					<description>
						<para>Name of facility that this route uses.</para>
						<para>The default name for the facility is the same name as the route itself (if none is provided). However, multiple routes may use the same facility, such as on-net calls to another branch office using a tie line as well as MTS calls over a tie line using tail end hop-off. In order to associate these routes as using the same facility, the same facility name should be specified. This will ensure that calls on routes with the same facility name are grouped together for certain operations, such as dequeuing and maximum call concurrency restrictions.</para>
						<para>Generally, a facility will be shared by two routes: one for on-net destinations using that facility and one for off-net destinations using that facility either directly or indirectly for tail end hop-off; however, there could be more as well.</para>
					</description>
				</configOption>
				<configOption name="route_type" default="">
					<synopsis>Type of route.</synopsis>
					<description>
						<para>Each route represents a usage arrangement of a trunk group, possibly consisting of multiple trunks, that may be used for a route. The main difference between a facility and a route is single facility may be used for multiple different routes. A facility consitutes the actual channels and facilities through which call setup proceeds, and a route is a logical "map" that ultimately uses one or more facilities to complete a call.</para>
						<para>Each route may be configured as one of the following:</para>
						<enumlist>
							<enum name="tie"><para>
								Tie trunk line
							</para></enum>
							<enum name="fx"><para>
								Foreign exchange line
							</para></enum>
							<enum name="wats"><para>
								WATS (Wide Area Telephone Service) line
							</para></enum>
						</enumlist>
					</description>
				</configOption>
				<configOption name="dialstr">
					<synopsis>Dial string for Dial application to use for calls</synopsis>
					<description>
						<para>Dial string as accepted by the Dial application</para>
						<para>This should include any extension modification necessary, such as appending and truncating
						digits, outpulsing (MF, SF, DTMF, etc.), modifying the outgoing Caller ID to a suitable Caller ID
						for the facility, etc. The CCSA module does not inherently do any of this for you.</para>
					</description>
				</configOption>
				<configOption name="aiod">
					<synopsis>Override the Automatic Identified Outward Dialing</synopsis>
					<description>
						<para>Set the outgoing Caller ID on all calls using this route to this value, by default.
						This is useful if all calls using a particular facility are expected to have a certain number,
						as opposed to inheriting the caller's number, or for routes where the outgoing number
						cannot be set dynamically, such as a POTS line. In that case, you can set this to whatever
						the outgoing number will be, so that your CDRs can reflect that in the appropriate field.</para>
						<para>This functionality can also be accomplished by including the <literal>f</literal> dial option
						in the <literal>dialstr</literal> setting.
						However, this option will also set the <literal>cdrvar_aiod</literal> CDR variable to this value.</para>
						<para>This option can be overridden at runtime using the <literal>i</literal> option to <literal>CCSA</literal>.</para>
					</description>
				</configOption>
				<configOption name="threshold" default="0">
					<synopsis>Threshold of queued priority 3 calls at which it is unlikely an additional call would be able to queue successfully at priority 3 without timing out.</synopsis>
					<description>
						<para>If this threshold of queued priority 3 calls is exceeded, additional calls will not be allowed to Off Hook Queue until queued priority 3 calls drop below this threshold again.</para>
						<para>To completely disable queuing against this route, specify a negative value (e.g. <literal>-1</literal>). This is useful when dealing with virtual routes against which queuing does not make sense, such as IP-based (e.g. SIP) trunks.</para>
					</description>
				</configOption>
				<configOption name="limit" default="0">
					<synopsis>Maximum number of simultaneous calls that may use this route</synopsis>
					<description>
						<para>Maximum number of calls that may use this route. If exceeded, this facility is treated as trunk busy, and calls will either need to finish or be preempted for additional calls to use this facility.</para>
						<para>For analog trunk groups, this should be set to the number of trunks in your trunk group. This will allow trunk busy conditions to be detected without actually trying to use the facility.</para>
						<para>For IP trunks, this may not be necessary, but maximum concurrent call limits may be enforced using this option, such as to keep number of concurrent calls on a facility within your number of provisioned channels.</para>
						<para>Note that this option is a route option, but applies to the facility that this route uses. If there are multiple routes using the same facility, the limits for each route should be identical or this setting will not be applied consistently to the multiple routes using the facility.</para>
						<para>Default is 0 (disabled).</para>
					</description>
				</configOption>
				<configOption name="frl" default="0">
					<synopsis>Facility Restriction Level required for usage</synopsis>
					<description>
						<para>Minimum FRL (Facility Restriction Level) required by a caller in order to be able to place a call using this route.</para>
					</description>
				</configOption>
				<configOption name="mer" default="no">
					<synopsis>More Expensive Route</synopsis>
					<description>
						<para>Whether this is a More Expensive Route.</para>
						<para>These routes should be ordered last, after any non-MER routes in a route sequence.</para>
						<para>More Expensive Route (MER) a.k.a. Expensive Route Warning Tone (ERWT) may optionally be supplied to callers when a MER route is used to complete a call. MER route will only be played
						the first time a call is attempted using a MER facility.</para>
					</description>
				</configOption>
				<configOption name="busyiscongestion" default="no">
					<synopsis>Whether busy constitutes trunk busy/congestion</synopsis>
					<description>
						<para>Indicates whether a busy dial disposition on this facility indicates that the facility itself is actually busy, as opposed to the called party being busy.</para>
						<para>When using individual analog lines or trunks, Asterisk may see them as "BUSY" if the facility itself is already in use, and any actual called number busy status is conveyed purely in-band. In contrast, this is not the case with most kinds of IP trunks, such as IAX2 or SIP.</para>
						<para>In order to properly determine if a call succeeded over the facility and was actually busy, or if the call could not be completed using the facility at all, this option is required to indicate how BUSY should be treated.</para>
						<para>By default, when set to no, BUSY is treated as a successful call outcome, and alternate routing will stop at this point (appropriate for IP trunking). For analog trunking, you will probably want to set this to yes, so that if BUSY is received, alternate routing will continue using other routes.</para>
					</description>
				</configOption>
				<configOption name="devstate">
					<synopsis>Aggregate device state for trunk group</synopsis>
					<description>
						<para>If specified, a list of devices whose aggregate device state will be used to determine the current status of the trunk group (whether idle or in use).</para>
					</description>
				</configOption>
				<configOption name="time">
					<synopsis>Time restrictions for route</synopsis>
					<description>
						<para>Optional time restrictions for the route, as provided in the GotoIfTime application or IFTIME function.</para>
						<para>This logic may also be done in the dialplan in order to determine what facilities are eligible for a CCSA call, but if configured here, this time restriction must be satisfied for any calls placed using this route.</para>
						<para>Remember that times must be provided in your system's time zone.</para>
					</description>
				</configOption>
			</configObject>
			<configObject name="ccsa">
				<synopsis>Individual CCSA configuration.</synopsis>
				<configOption name="route" default="">
					<synopsis>Default routes.</synopsis>
					<description>
						<para>Ordered pattern of routes to try for calls if routes are not explicitly specified. One route should be specified per use of this option.</para>
						<para>Multiple routes may be specified.</para>
						<para>Route profiles may be shared between CCSAs, but this is generally not recommended
						since this would not fully isolate CCSAs using the same routes (for example, calls in a different CCSA could be preempted).
						As a best practice, if the same route is used for multiple CCSAs, you should create a separate route profile for each CCSA/route pair.</para>
					</description>
				</configOption>
				<configOption name="mer_tone" default="yes">
					<synopsis>More Expensive Route Tone</synopsis>
					<description>
						<para>Whether to provide More Expensive Route Tone to the caller on a MER route to indicate
						a MER route is being used for the call.</para>
					</description>
				</configOption>
				<configOption name="auth_code_len" default="6">
					<synopsis>Authorization code length</synopsis>
					<description>
						<para>Length of authorization codes, so the system knows when a full code has been entered.</para>
					</description>
				</configOption>
				<configOption name="extension_len" default="4">
					<synopsis>Extension length</synopsis>
					<description>
						<para>Length of callback extensions (for Call Back Queuing), so the system knows when a full extension has been entered.</para>
					</description>
				</configOption>
				<configOption name="frl_allow_upgrade" default="no">
					<synopsis>FRL Upgrades Allowed</synopsis>
					<description>
						<para>Whether callers can upgrade the FRL (Facility Restriction Level) for a call using an authorization code.</para>
						<para>The <literal>dialrecall</literal> indications frequencies are used by default for this prompt.
						This can be overridden, if desired, by defining the <literal>authcodeprompt</literal> indication in <literal>indications.conf</literal>.</para>
					</description>
				</configOption>
				<configOption name="auth_code_remote_allowed" default="no">
					<synopsis>Remote Usage of Authorization Codes Allowed</synopsis>
					<description>
						<para>Whether authorization codes may be used remotely to upgrade FRL for a call.</para>
					</description>
				</configOption>
				<configOption name="auth_sub_context">
					<synopsis>Authorization code validation subroutine</synopsis>
					<description>
						<para>Dialplan subroutine context name to validate authorization codes for this CCSA.</para>
						<para>The subroutine should return the FRL (0-7) associated with the authorization code, if valid.</para>
						<para>If not valid, -1 or an empty return value may be returned.</para>
						<para>The argument <literal>r</literal> will be passed in as ARG1 if the authorization code is being used remotely,
						so that it is possible to distinguish between authorization codes that are allowed for Remote Access and those that
						are not.</para>
					</description>
				</configOption>
				<configOption name="hold_announcement">
					<synopsis>Hold announcement</synopsis>
					<description>
						<para>Hold announcement to optionally play when beginning Off Hook Queue.</para>
					</description>
				</configOption>
				<configOption name="extension_prompt">
					<synopsis>Extension prompt</synopsis>
					<description>
						<para>Call Back Queue prompt to get callback extension from user.</para>
					</description>
				</configOption>
				<configOption name="queue_promo_timer" default="0">
					<synopsis>Queue Promotion Timer</synopsis>
					<description>
						<para>When in Call Back Queue, at intervals defined by the queue promotion timer, the priority of the call is incremented until it reaches its maximum priority. Each time the call priority is incremented, its position in the CBQ is advanced.</para>
						<para>The default (0) disables timed queue promotion.</para>
					</description>
				</configOption>
				<configOption name="route_advance_timer" default="0">
					<synopsis>Route Advance Timer</synopsis>
					<description>
						<para>If the route advance timer reaches its maximum value before the call can be terminated on a route in the initial set, the call is then queued against an extended route, if available.</para>
						<para>The default (0) disables route advances.</para>
					</description>
				</configOption>
				<configOption name="callback_caller_context">
					<synopsis>Callback caller context</synopsis>
					<description>
						<para>Dialplan context to call in order to initiate a callback to a user for Call Back Queue.</para>
						<para>This destination will be called as a local channel.</para>
						<para>This setting is required for Call Back Queue to operate.</para>
					</description>
				</configOption>
				<configOption name="callback_dest_context">
					<synopsis>Callback destination context</synopsis>
					<description>
						<para>Dialplan context to which to place the actual callback to the destination.</para>
						<para>If not provided, the caller will be rebridged with the CCSA module to complete the call to the destination
						over the same route that the queue was called in (in most cases, this would be the desired behavior).</para>
					</description>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
 ***/

#define CONFIG_FILE "ccsa.conf"

static char *app = "CCSA";

#define MAX_YN_STRING		4

#define MAX_FRL 7

enum facility_type {
	FACILITY_UNKNOWN = 0,
	FACILITY_TIELINE,		/* Tie line */
	FACILITY_FX,			/* Foreign exchange line */
	FACILITY_WATS,			/* WATS (Wide Area Telephone Service) line */
};

static char cdrvar_frl[AST_MAX_CONTEXT];
static char cdrvar_frl_req[AST_MAX_CONTEXT];
static char cdrvar_frl_eff[AST_MAX_CONTEXT];
static char cdrvar_aiod[AST_MAX_CONTEXT];
static char cdrvar_mlpp[AST_MAX_CONTEXT];
static char cdrvar_authcode[AST_MAX_CONTEXT];
static char cdrvar_facility[AST_MAX_CONTEXT];
static char cdrvar_route[AST_MAX_CONTEXT];
static char cdrvar_queuestart[AST_MAX_CONTEXT];
static char cdrvar_digits[AST_MAX_CONTEXT];

#define CHRARRAY_ZERO(var) var[0] = '\0';

static void reset_cdr_var_names(void)
{
	CHRARRAY_ZERO(cdrvar_frl);
	CHRARRAY_ZERO(cdrvar_frl_req);
	CHRARRAY_ZERO(cdrvar_frl_eff);
	CHRARRAY_ZERO(cdrvar_aiod);
	CHRARRAY_ZERO(cdrvar_mlpp);
	CHRARRAY_ZERO(cdrvar_authcode);
	CHRARRAY_ZERO(cdrvar_facility);
	CHRARRAY_ZERO(cdrvar_route);
	CHRARRAY_ZERO(cdrvar_queuestart);
	CHRARRAY_ZERO(cdrvar_digits);
}

struct route {
	ast_mutex_t lock;
	char name[AST_MAX_CONTEXT];			/*!< Name */
	char facility[AST_MAX_CONTEXT];		/*!< Facility Name */
	enum facility_type factype;			/*!< Facility Type */
	char dialstr[PATH_MAX];				/*!< Dial string */
	char aiod[AST_MAX_CONTEXT];			/*!< AIOD override */
	char *devstate;						/*!< Device state */
	unsigned int threshold;				/*!< Threshold at which facility is "saturated" */
	unsigned int limit;					/*!< Concurrent call limit */
	unsigned int frl:3;					/*!< Minimum Facility Restriction Level required */
	unsigned int mer:1;					/*!< More Expensive Route? */
	unsigned int busyiscongestion:1;	/*!< Whether facility should be considered "in use" if disposition is BUSY */
	char time[PATH_MAX];				/*!< Simple time restrictions */
	AST_LIST_ENTRY(route) entry;		/*!< Next route record */
};

struct ccsa {
	ast_mutex_t lock;
	char name[AST_MAX_CONTEXT];			/*!< Name */
	char routes[PATH_MAX];				/*!< List of routes */
	unsigned int auth_code_len;			/*!< Auth code length */
	unsigned int extension_len;			/*!< Extension length */
	unsigned int mer_tone:1;			/*!< More Expensive Route Tone */
	unsigned int frl_allow_upgrade:1;	/*!< FRL upgrade allowed */
	unsigned int auth_code_remote_allowed:1;	/* Auth codes allowed for remote access */
	char auth_sub_context[AST_MAX_CONTEXT];	/* Auth code validation subroutine */
	char hold_announcement[PATH_MAX];	/*!< Optional hold announcement */
	char extension_prompt[PATH_MAX];	/*!< Extension callback prompt */
	char callback_caller_context[AST_MAX_CONTEXT];
	char callback_dest_context[AST_MAX_CONTEXT];
	/* CBQ Timers */
	unsigned int queue_promo_timer;
	unsigned int route_advance_timer;
	/* CDR variables */
	AST_LIST_ENTRY(ccsa) entry;			/*!< Next CCSA record */
};

struct ccsa_call {
	char *channel;
	char *ccsa;
	char *route;
	char *nextroute;
	char *facility;
	char *caller;
	char *called;
	char *cbqexten;
	int call_priority; /* MLPP (none, A-D) */
	int queue_priority; /* Queue priority (0-3) */
	unsigned int active:1; /* 1 = actual call, 0 = queued call */
	unsigned int cbq:1;		/* 1 if this is a CBQ call */
	unsigned int aborted:1;	/* 1 if CBQ call aborted */
	unsigned int preempted:1; /* If call was preempted */
	pthread_t cbqthread;
	int queue_alert_pipe[2];
	unsigned int effective_frl;
	unsigned int queue_promo_timer;
	unsigned int route_advance_timer;
	char *callback_caller_context;
	char *callback_dest_context;
	/* Not used internally, but stored for statistics */
	int start;
	AST_LIST_ENTRY(ccsa_call) entry;
};

static AST_RWLIST_HEAD_STATIC(routes, route); /* Routes are not inherently tied to a single CCSA (nor a single facility), but often will be */
static AST_RWLIST_HEAD_STATIC(ccsas, ccsa);
static AST_RWLIST_HEAD_STATIC(calls, ccsa_call);

static enum facility_type facility_from_str(const char *str)
{
	if (!strcasecmp(str, "tie") || !strcasecmp(str, "tieline")) {
		return FACILITY_TIELINE;
	} else if (!strcasecmp(str, "fx")) {
		return FACILITY_FX;
	} else if (!strcasecmp(str, "wats")) {
		return FACILITY_WATS;
	}
	return FACILITY_UNKNOWN;
}

static const char *facility_type_str(enum facility_type factype)
{
	switch (factype) {
	case FACILITY_TIELINE:
		return "Tie Line";
	case FACILITY_FX:
		return "FX";
	case FACILITY_WATS:
		return "WATS";
	default:
		break;
	}
	return "Unknown";
}

static struct route *alloc_route(const char *name)
{
	struct route *f;

	if (!(f = ast_calloc(1, sizeof(*f)))) {
		return NULL;
	}

	ast_mutex_init(&f->lock);
	ast_copy_string(f->name, name, sizeof(f->name));
	ast_copy_string(f->facility, name, sizeof(f->facility));

	return f;
}

static struct ccsa *alloc_ccsa(const char *name)
{
	struct ccsa *c;

	if (!(c = ast_calloc(1, sizeof(*c)))) {
		return NULL;
	}

	ast_mutex_init(&c->lock);
	ast_copy_string(c->name, name, sizeof(c->name));

	return c;
}

static struct route *find_route(const char *name, int lock)
{
	struct route *f = NULL;
	if (lock) {
		AST_RWLIST_RDLOCK(&routes);
	}
	AST_LIST_TRAVERSE(&routes, f, entry) {
		if (!strcasecmp(f->name, name)) {
			break;
		}
	}
	if (lock) {
		AST_RWLIST_UNLOCK(&routes);
	}
	return f;
}

/*! \brief Retrieve the facility used by a route */
static int get_route_facility(const char *routename, char *buf, size_t len)
{
	int res = -1;
	struct route *f = NULL;
	AST_RWLIST_RDLOCK(&routes);
	AST_LIST_TRAVERSE(&routes, f, entry) {
		if (!strcasecmp(f->name, routename)) {
			res = 0;
			ast_copy_string(buf, f->facility, len);
			break;
		}
	}
	AST_RWLIST_UNLOCK(&routes);
	return res;
}

static struct ccsa *find_ccsa(const char *name, int lock)
{
	struct ccsa *c = NULL;
	if (lock) {
		AST_RWLIST_RDLOCK(&ccsas);
	}
	AST_LIST_TRAVERSE(&ccsas, c, entry) {
		if (!strcasecmp(c->name, name)) {
			break;
		}
	}
	if (lock) {
		AST_RWLIST_UNLOCK(&ccsas);
	}
	return c;
}

/*! \note Must be called with list write locked */
static int call_queue_remove(struct ccsa_call *call)
{
	struct ccsa_call *call2;

	AST_LIST_TRAVERSE_SAFE_BEGIN(&calls, call2, entry) {
		if (call2 == call) {
			AST_LIST_REMOVE_CURRENT(entry);
			break;
		}
	}
	AST_LIST_TRAVERSE_SAFE_END;

	return call2 ? 0 : -1;
}

/*! \note Must be called with list write locked */
static int call_queue_insert(struct ccsa_call *call, const char *route)
{
	if (call->active) { /* Don't really care about actual queue position */
		AST_RWLIST_INSERT_TAIL(&calls, call, entry);
	} else {
		int inserted = 0;
		/* Maintain ordering for any queued calls. We insert at the end, but of each priority, so higher priorities are always before lower ones.
		 * Newer entries should be at the beginning of the list naturally, since we tail insert, so no need to compare timestamps, really. */
		struct ccsa_call *ecall;
		AST_RWLIST_TRAVERSE_SAFE_BEGIN(&calls, ecall, entry) {
			if (!strcmp(route, ecall->route) && !ecall->active && call->queue_priority > ecall->queue_priority) { /* Same route, and lower priority? */
				AST_RWLIST_INSERT_BEFORE_CURRENT(call, entry);
				inserted = 1;
				break;
			}
		}
		AST_RWLIST_TRAVERSE_SAFE_END;
		if (!inserted) {
			AST_RWLIST_INSERT_TAIL(&calls, call, entry);
		}
		ast_debug(2, "Joined queue %s\n", inserted ? "ahead of another call" : "at the very end");
	}
	return 0;
}

#define call_free(call, remove) __call_free(call, remove, __FUNCTION__, __LINE__)

static void __call_free(struct ccsa_call *call, int remove, const char *function, int lineno)
{
	if (remove) {
		AST_RWLIST_WRLOCK(&calls);
		if (call_queue_remove(call)) {
			ast_log(LOG_WARNING, "%s:%d: Unable to find call %p in call list\n", function, lineno, call);
		}
		AST_RWLIST_UNLOCK(&calls);
	}
	ast_alertpipe_close(call->queue_alert_pipe);
	ast_free(call->channel);
	ast_free(call->route);
	if (call->ccsa) {
		ast_free(call->ccsa);
	}
	if (call->cbqexten) {
		ast_free(call->cbqexten);
	}
	if (call->nextroute) {
		ast_free(call->nextroute);
	}
	if (call->callback_caller_context) {
		ast_free(call->callback_caller_context);
	}
	if (call->callback_dest_context) {
		ast_free(call->callback_dest_context);
	}
	ast_free(call->facility);
	ast_free(call->caller);
	ast_free(call->called);
	ast_free(call);
}

static struct ccsa_call *call_add(const char *channel, const char *facility, const char *route, const char *caller, const char *called, int active, int call_priority, int queue_priority)
{
	struct ccsa_call *call;
	char *chandup, *facdup, *routedup, *callerdup, *calleddup;

	/* This started out innocent enough... */
	chandup = ast_strdup(channel);
	if (!chandup) {
		return NULL;
	}
	facdup = ast_strdup(route);
	if (!facdup) {
		ast_free(chandup);
		return NULL;
	}
	routedup = ast_strdup(route);
	if (!routedup) {
		ast_free(facdup);
		ast_free(chandup);
		return NULL;
	}
	callerdup = ast_strdup(S_OR(caller, ""));
	if (!callerdup) {
		ast_free(facdup);
		ast_free(chandup);
		ast_free(routedup);
		return NULL;
	}
	calleddup = ast_strdup(S_OR(called, ""));
	if (!calleddup) {
		ast_free(facdup);
		ast_free(chandup);
		ast_free(routedup);
		ast_free(callerdup);
		return NULL;
	}

	if (!(call = ast_calloc(1, sizeof(*call)))) {
		ast_free(facdup);
		ast_free(chandup);
		ast_free(routedup);
		ast_free(callerdup);
		ast_free(calleddup);
		return NULL;
	}

	call->channel = chandup;
	call->facility = facdup;
	call->route = routedup;
	call->caller = callerdup;
	call->called = calleddup;
	call->call_priority = call_priority; /* Needed for both actual and queued calls */
	call->active = active;
	call->start = (int) time(NULL);

	if (!active) {
		call->queue_priority = queue_priority;
	}

	call->queue_alert_pipe[0] = call->queue_alert_pipe[1] = -1;
	ast_alertpipe_init(call->queue_alert_pipe);

	AST_RWLIST_WRLOCK(&calls);
	call_queue_insert(call, call->route);
	AST_RWLIST_UNLOCK(&calls);

	return call;
}

/*! \retval -1 if no calls to preempt, 0 if not authorized to preempt, 1 if successfully preempted */
static int try_facility_preempt(const char *facility, char preempt_priority)
{
	/* Look for an active call on same facility (even if a different route) with lower priority than us. */
	int preempted = -1;
	struct ccsa_call *call;
	char *target = NULL;

	AST_RWLIST_WRLOCK(&calls);
	AST_LIST_TRAVERSE(&calls, call, entry) {
		if (call->active && !strcmp(facility, call->facility)) {
			preempted = 0;
			if (call->call_priority > preempt_priority) {
				/* If they have a strictly higher priority than us, it must be an actual priority, so no need to isprint guard their priority. */
				ast_debug(2, "Can't preempt %s, their priority: %c, ours: %c\n", call->channel, call->call_priority, preempt_priority);
				continue;
			}
			/* Don't do the actual preempt with the list lock held. */
			target = ast_strdupa(call->channel);
			call->preempted = 1; /* Let the poor channel know we just killed its call. */
			preempted = 1;
			break;
		}
	}
	AST_RWLIST_UNLOCK(&calls);

	if (preempted) { /* Actually do the preempt */
		struct ast_channel *ochan = ast_channel_get_by_name(target);
		if (!ochan) {
			ast_log(LOG_WARNING, "Channel to prempt (%s) doesn't exist?\n", target);
			return 0;
		}

		/* Kick this channel out of its call. */
		ast_verb(3, "Preempting call on %s (%s)\n", facility, target);

		/* The called party of the connection IS disconnected essentially immediately.
		 * See BSP 981-210-100:
		 * 1.06: When either a trunk or a line must be preempted to complete another connection of higher precedence,
		 * a preemption-notification wink signal is used. Basically, this signal is an on-hook wink signal of 0.33
		 * to 0.36 seconds from the switching center. Whenever necessary, it will be preceded or followed by an off-hook signal of 0.10 second
		 * 3.08: When a station is in a busy condition, either with a normal voice call or a data call, or is in a hold condition
		 * and some link in the connection is preempted, the audible bell of the key telphone set will ring steadily and the
		 * line lamp will flash at a 120-ipm rate (0.25 seconds on, 0.25 seconds off). Also, on voice calls a 440/620~ycle tone is heard.
		 * These signals continue until the station goes on-hook.
		 * Also see 480-611-100.
		 */

		/* i.e. from the above, preemption is conveyed to the distant end using a timed wink, not in-band, i.e. only makes sense on analog trunks,
		 * we can send a wink over IAX2, but it's just a wink, not a wink of a specific time, so we can't just rely on an arbitrary wink being
		 * interpreted as preempted.
		 * The far end generates preemption tone locally and indefinitely. Obviously, this makes a lot of sense.
		 * Playing preemption tone from our end to the called party would prevent releasing the trunk until we drop the far end. */

		/*! \todo So, for now, we really have no way of conveying preemption to the far end. The call will just hang up immediately.
		 * We can, of course, easily facilitate playing preemption tone to OUR guy, but not the one on the other end.
		 * The ideal thing to do would be to send AST_CAUSE_PRE_EMPTED to the far end, as this is the appropriate hangup cause.
		 * That would require support from the specific channel driver to convey preemption, and the other end must also support this
		 * and generate preemption tone locally. */

		ast_channel_lock(ochan);
		ast_softhangup_nolock(ochan, AST_SOFTHANGUP_ASYNCGOTO); /* This is more lightweight than the locked version. */
		ast_channel_unlock(ochan);
		ast_channel_unref(ochan);
	}

	return preempted;
}

static int queue_cancel_cbq(int fd, const char *caller)
{
	struct ccsa_call *call;
	int total = 0;
	pthread_t cbq_thread;

	AST_RWLIST_WRLOCK(&calls); /* call_queue_remove does modify the list itself. */
	AST_LIST_TRAVERSE(&calls, call, entry) {
		if (!call->active && call->cbq && !call->aborted) {
			if (!ast_strlen_zero(caller) && strcmp(call->caller, caller)) {
				continue; /* Doesn't match filter */
			}
			cbq_thread = call->cbqthread;
			call->aborted = 1;
			if (ast_alertpipe_write(call->queue_alert_pipe)) {
				ast_log(LOG_WARNING, "%s: write() failed: %s\n", __FUNCTION__, strerror(errno));
			} else {
				total++;
				if (fd >= 0) {
					ast_cli(fd, "Requested cancellation of CBQ for %s\n", call->caller);
				} else {
					ast_debug(2, "Requested cancellation of CBQ for %s\n", call->caller);
				}
				/* Wait for the CBQ to cancel and kill itself. */
				if (pthread_join(cbq_thread, NULL)) {
					ast_log(LOG_WARNING, "Failed to wait for CBQ to terminate: %s\n", strerror(errno));
				}
				/* Since we already have a list lock, aborted calls cannot free themselves / remove themselves from the list. */
				call_queue_remove(call);
				call_free(call, 0); /* Already been removed from the list, don't try to remove it again (that would try to grab another WRLOCK too) */
			}
			break;
		}
	}
	AST_RWLIST_UNLOCK(&calls);

	ast_debug(2, "Notified %d CBQ call%s in queue about shutdown\n", total, ESS(total));

	return total;
}

/*! \brief Notify the call at the head of the line for this facility that it's its turn */
static int queue_notify_facility_head(const char *facility)
{
	struct ccsa_call *call;
	int total = 0;

	AST_RWLIST_RDLOCK(&calls);
	AST_LIST_TRAVERSE(&calls, call, entry) {
		if (!call->active && !strcmp(facility, call->facility)) {
			if (ast_alertpipe_write(call->queue_alert_pipe)) {
				ast_log(LOG_WARNING, "%s: write() failed: %s\n", __FUNCTION__, strerror(errno));
			} else {
				total++;
			}
			break;
		}
	}
	AST_RWLIST_UNLOCK(&calls);

	ast_debug(2, "Notified %d call%s in queue about availability of facility %s\n", total, ESS(total), facility);

	return total;
}

static int facility_num_calls(const char *facility)
{
	struct ccsa_call *call;
	int total = 0;

	AST_RWLIST_RDLOCK(&calls);
	AST_LIST_TRAVERSE(&calls, call, entry) {
		if (!strcmp(facility, call->facility)) {
			if (call->active) {
				total++;
			}
		}
	}
	AST_RWLIST_UNLOCK(&calls);

	ast_debug(2, "Facility %s currently has %d active calls\n", facility, total);

	return total;
}

static int route_num_priority3_calls(const char *route)
{
	struct ccsa_call *call;
	int pri3calls = 0;

	AST_RWLIST_RDLOCK(&calls);
	AST_LIST_TRAVERSE(&calls, call, entry) {
		if (!strcmp(route, call->route)) {
			if (call->queue_priority == 3) {
				pri3calls++;
			}
		}
	}
	AST_RWLIST_UNLOCK(&calls);

	ast_debug(2, "Route %s currently has %d priority 3 calls\n", route, pri3calls);

	return pri3calls;
}

static int route_has_any_calls(const char *route)
{
	struct ccsa_call *call;

	AST_RWLIST_RDLOCK(&calls);
	AST_LIST_TRAVERSE(&calls, call, entry) {
		if (!strcmp(route, call->route)) {
			AST_RWLIST_UNLOCK(&calls);
			return 1;
		}
	}
	AST_RWLIST_UNLOCK(&calls);

	return 0;
}

static int cbq_calls_pending(const char *caller)
{
	struct ccsa_call *call;
	int already = 0;

	AST_RWLIST_RDLOCK(&calls);
	AST_LIST_TRAVERSE(&calls, call, entry) {
		if (!call->active && !strcmp(call->caller, caller)) { /* Check ALL routes */
			already++;
		}
	}
	AST_RWLIST_UNLOCK(&calls);

	ast_debug(2, "Currently %d calls queued for %s\n", already, caller);

	return already;
}

/*! \brief 0 if time matches condition, -1 if failure, -1 if doesn't match */
static int time_matches(const char *expr)
{
	struct ast_timing timing;
	int res;

	if (!ast_build_timing(&timing, expr)) {
		ast_log(LOG_WARNING, "Invalid Time Spec: %s\n", expr);
		res = -1;
	} else {
		res = ast_check_timing(&timing) ? 0 : 1;
	}

	ast_destroy_timing(&timing);

	return res;
}

static int cdr_write(struct ast_channel *chan, const char *name, const char *val)
{
	char full[AST_MAX_CONTEXT + 5];
	snprintf(full, sizeof(full), "CDR(%s)", name);
	if (ast_func_write(chan, full, val)) {
		ast_log(LOG_WARNING, "Failed to set CDR(%s) = %s\n", full, val);
		return -1;
	}
	return 0;
}

static int cdr_set_var(struct ast_channel *chan, const char *name, const char *value)
{
	if (ast_strlen_zero(name)) {
		return -1;
	}
	return cdr_write(chan, name, value);
}

static int cdr_set_var_int(struct ast_channel *chan, const char *name, int value)
{
	char val[64];
	if (ast_strlen_zero(name)) {
		return -1;
	}
	snprintf(val, sizeof(val), "%d", value);
	return cdr_write(chan, name, val);
}

#define DATE_FORMAT 	"%Y-%m-%d %T"
static int cdr_set_var_now(struct ast_channel *chan, const char *name)
{
	char now_time[80] = "";
	struct ast_tm tm;
	struct timeval tv;
	if (ast_strlen_zero(name)) {
		return -1;
	}
	tv = ast_tvnow();
	ast_localtime(&tv, &tm, NULL);
	ast_strftime(now_time, sizeof(now_time), DATE_FORMAT, &tm);
	return cdr_write(chan, name, now_time);
}

/* Seems logical, but on the other hand, might well want to keep digits between routes */
#define RESET_DIGITS_EACH_ROUTE 0

static int store_dtmf(struct ast_channel *chan)
{
#if RESET_DIGITS_EACH_ROUTE
	char cdrvar_name[AST_MAX_CONTEXT + 5];
	int exists;
#endif
	char app_args[AST_MAX_CONTEXT + 11];
	if (ast_strlen_zero(cdrvar_digits)) {
		return 0;
	}

#if RESET_DIGITS_EACH_ROUTE
	snprintf(cdrvar_name, sizeof(cdrvar_name), "CDR(%s)", cdrvar_digits);
	ast_channel_lock(chan);
	exists = !ast_strlen_zero(pbx_builtin_getvar_helper(chan, cdrvar_name)) ? 1 : 0;
	ast_channel_unlock(chan);
	if (exists) { /* Zero out any existing digits, since call hasn't succeeded yet. */
		pbx_builtin_setvar_helper(chan, cdrvar_name, NULL);
	}
#endif

	snprintf(app_args, sizeof(app_args), "RX,CDR(%s),32", cdrvar_digits); /* Store up to 32 digits, dialed by the caller (not the called number) */

	/* This is kind of a kludge. StoreDTMF can only be activated, and emits a warning if activated again, so make sure we don't call more than once, just to keep things clean. */
	if (pbx_builtin_getvar_helper(chan, "CCSADTMFSTORESTARTED")) {
		return 0; /* We're not getting the actual value, so don't bother locking the channel. If it exists, then we already activated StoreDTMF previously. */
	}
	pbx_builtin_setvar_helper(chan, "CCSADTMFSTORESTARTED", "1");

	/*! \todo A timeout (say a minute) for StoreDTMF would be good... */
	if (ast_pbx_exec_application(chan, "StoreDTMF", app_args)) {
		ast_log(LOG_WARNING, "Failed to initialize digit store on %s\n", ast_channel_name(chan));
		if (ast_check_hangup(chan)) {
			return -1;
		}
	}
	/* StoreDTMF should execute immediately, so it's unlikely we'll get a hangup in here. */
	return 0;
}

static int route_permits_ohq_locked(struct route *f, time_t elapsed, int is_simulation)
{
	/* Nortel 553-2751-101 4.00, Off-Hook Queuing availability:
	 * Each trunk route has a threshold value that indicates the maximum number of priority 3 calls
	 * that can be queued against it before OHQ timeout becomes a high probability.
	 * Before a call is placed in the OHQ, the current queue count is compared with the
	 * threshold value for each eligible trunk route in the initial set of routes.
	 * If at least one of the trunk routes has a count less than or equal to the threshold value,
	 * the call is allowed to perform OHQ against all OHQ eligible routes. */
	int pri3calls;

	/* XXX If the route currently has no calls, it may not make sense to queue.
	 * For example, if we tried route 0 and the call failed, but route 0 was
	 * using a trunk group with 0 calls, then there is no point in off-hook
	 * queuing on route 0, since it wasn't in use in the first place,
	 * so off-hooking queuing on it won't accomplish anything.
	 *
	 * Note that routes do not correspond 1:1 to trunk groups,
	 * so to determine if the underlying trunk group is currently saturated or not,
	 * we need to be able to determine the "device state" of the trunk group. */
	if (!f->limit) {
		/* If there is no limit, then this is a "virtual" trunk group, essentially,
		 * i.e. there is no hard cap on allowed calls. Common for enterprise SIP trunking. */
		ast_debug(6, "Facility %s does not have a defined capacity, won't queue against it\n", f->name);
		return 0;
	}

	if (f->threshold < 0) {
		/* Queuing explicitly disabled */
		ast_debug(6, "Facility %s queuing disabled\n", f->name);
		return 0;
	}

	if (!ast_strlen_zero(f->time) && time_matches(f->time)) {
		ast_debug(6, "Facility %s skipped due to time restrictions\n", f->name);
		return 0; /* Route time restrictions not satisfied, so OHQ not permitted either */
	}

	pri3calls = route_num_priority3_calls(f->name);
	if (pri3calls > f->threshold) {
		ast_debug(6, "Facility %s skipped (too many priority 3 calls queued: %d)\n", f->name, pri3calls);
		return 0; /* So many queued calls already that it's unlikely OHQ call will succeed in queue before timing out */
	}

	/* Only check current call status if not in simulation */
	if (!is_simulation) {
		if (f->devstate) {
			/* Even if there is a call limit, like for analog trunk groups with a fixed number of circuits,
			 * if the entire trunk group is idle, it is still inappropriate to queue against it,
			 * since the trunk group isn't currently saturated.
			 *
			 * However, there is no way to automatically determine ourselves if the trunk group is completely
			 * saturated in this case. Multiple routes could use certain circuits, and those circuits could
			 * also be used outside of this module, so we can't just calculate the device state.
			 * In this case, it would make sense to lookup an "aggregate device state" for the entire trunk group,
			 * and only if that comes back as "all trunks in the trunk group are busy" would queuing make sense.
			 * Conversely, if no trunks are busy, then queuing also doesn't make sense. */
			enum ast_device_state devstate = ast_device_state(f->devstate);
			switch (devstate) {
			case AST_DEVICE_NOT_INUSE:
				/* The entire trunk group is currently "idle",
				 * so if a previous attempt to use this route just now failed,
				 * why try it again?
				 * It's important that we bound this based on the time to do the
				 * first pass of all the routes. Otherwise, a facility might have
				 * been busy on the first attempt but might be free now.
				 *
				 * XXX A more reliable way to do this would be keep track
				 * of the device state associated with each route as we attempt using
				 * it the first time. If it was NOTINUSE, then that means we should
				 * NOT try it the second pass, no matter what.
				 * The logic here just assumes if not very much time has elapsed since
				 * the first pass, the state now will be whatever it was a moment ago,
				 * but technically it's the older state that we want here.
				 * Because some calls could technically still clear in the same second,
				 * there is a slight chance some calls could've queued that will now be excluded.
				 * Beyond 1 second, the chance of a race condition is too great to do this.*/
				if (elapsed < 1) {
					ast_debug(6, "Facility %s skipped (trunk group isn't in use now and probably wasn't on last attempt)\n", f->name);
					return 0;
				}
				/* Fall through */
			default:
				/* Okay to queue against, or at least, these don't preclude queuing */
				break;
			}
		}

		if (!route_has_any_calls(f->name)) {
			/* The only way that off-hook queuing will end successfully is if there is a call currently
			 * in the queue that ends, which will write to the alertpipe and wake up a call
			 * to go ahead and try again.
			 * If there aren't any calls, even if the trunk group has capacity, there isn't anything
			 * to wake us up, so it won't work anyways, so don't queue for no reason.
			 *
			 * XXX In the future, the option could be added to "poll" the device state periodically to retry
			 * the call attempt, which would better handle facilities used outside of this application. */
			ast_debug(6, "Facility %s skipped (doesn't have any calls currently, so nothing to wait for)\n", f->name);
			return 0;
		}
	}

	return 1;
}

static int route_permits_ohq(const char *route, time_t elapsed, int is_simulation)
{
	struct route *f;
	int can_queue;

	AST_RWLIST_RDLOCK(&routes);
	f = find_route(route, 0);
	if (!f) {
		/* Would've already thrown a warning about this route not existing if it didn't, so don't do it again. */
		can_queue = 0;
	} else {
		can_queue = route_permits_ohq_locked(f, elapsed, is_simulation);
	}
	AST_RWLIST_UNLOCK(&routes);

	return can_queue;
}

static int route_permits_cbq(const char *route)
{
	int permitted;
	AST_RWLIST_RDLOCK(&routes);
	permitted = route_has_any_calls(route);
	AST_RWLIST_UNLOCK(&routes);
	return permitted;
}

static int off_hook_queue(struct ast_channel *chan, struct ccsa_call *call, int ohq)
{
	int timeout, ms, remaining_time;
	struct timeval start;
	struct ast_frame *frame = NULL;

	timeout = remaining_time = ohq * 1000; /* convert s to ms */
	start = ast_tvnow();

	/* ${STRFTIME(${EPOCH},,%Y-%m-%d %H:%M:%S)} */
	cdr_set_var_now(chan, cdrvar_queuestart);

	/* Wait for something to happen... */
	while (remaining_time > 0) {
		int ofd, exception;

		ms = 1000;
		errno = 0;
		if (ast_waitfor_nandfds(&chan, 1, &call->queue_alert_pipe[0], 1, &exception, &ofd, &ms)) { /* channel won */
			if (!(frame = ast_read(chan))) { /* channel hung up */
				ast_debug(1, "Channel '%s' did not return a frame; probably hung up.\n", ast_channel_name(chan));
				return -1;
			}
			ast_frfree(frame); /* handle frames */
		} else if (ofd == call->queue_alert_pipe[0]) { /* fd won */
			if (ast_alertpipe_read(call->queue_alert_pipe) == AST_ALERT_READ_SUCCESS) {
				ast_debug(1, "Alert pipe has data for us\n");
				/* It's our turn to exit the queue. OHQ actually succeeded and finished before the timer expired! */
				return 1;
			} else {
				ast_log(LOG_WARNING, "Alert pipe does not have data for us?\n");
			}
		} else { /* nobody won */
			if (ms && (ofd < 0)) {
				if (!((errno == 0) || (errno == EINTR))) {
					ast_log(LOG_WARNING, "Something bad happened while channel '%s' was polling.\n", ast_channel_name(chan));
					break;
				}
			} /* else, nothing happened */
		}
		remaining_time = ast_remaining_ms(start, timeout);
		if (remaining_time <= 0) {
			return 0;
		}
	}

	return 0;
}

#define ccsa_log(chan, fd, ...) if (chan) {ast_debug(1, "CCSA: " __VA_ARGS__);} else {ast_verb(4,  "CCSA Simulation: " __VA_ARGS__);}

enum facility_disp {
	FACILITY_DISP_HANGUP = -1, /* Caller hung up */
	FACILITY_DISP_SUCCESS = 0, /* Successful, stop */
	FACILITY_DISP_FAILURE = 1, /* General failure: couldn't attempt this facility */
	FACILITY_DISP_UNAVAILABLE = 2, /* Facility unavailable */
	FACILITY_DISP_UNAUTHORIZED = 3, /* Unauthorized to use facility */
	FACILITY_DISP_INVALID_AUTH_CODE = 4, /* Invalid authorization code */
	FACILITY_DISP_PREEMPTION_FAILED = 5, /* Unauthorized to preempt call */
	FACILITY_DISP_PREEMPTED = 6, /* Call was preempted */
};

/*! \brief CBQ monitor thread (per CBQ call) */
static void *call_back_queue(void *data)
{
	struct ccsa_call *call = data;
	struct timeval queue_promo_start, route_advance_start;
	int use_queue_promo_timer = 1, use_route_advance_timer = 1;
	int res;

	ast_debug(1, "Queue Promotion Timer: %u, Route Advance Timer: %u\n", call->queue_promo_timer, call->route_advance_timer);
	/* Convert from s to ms */
	if (call->queue_promo_timer) {
		call->queue_promo_timer *= 1000;
	} else {
		use_queue_promo_timer = 0;
	}
	if (call->route_advance_timer) {
		call->route_advance_timer *= 1000;
	} else {
		use_route_advance_timer = 0;
	}

	/*
	 * Nortel Meridian documentation has a Queue Promotion Timer and Route Advance Timer.
	 * At QPT intervals, priority is incremented until it reaches the maximum (thus advancing within the queue)
	 * When RAT is reached, extended routes are added to the routes against which the call was initially queued.
	 * No limit on how long calls may stay in CBQ (if not cancelled using Ring-Again functionality).
	 * BSPs do not mention limits on CBQ duration or whether they can be cancelled at all.
	 * Outpulsing of Meridian on success is wait 10 seconds and then 2.56 seconds per digit, to allow for
	 * answer within 10-30 seconds (and just 6 seconds for analog phones, for some reason).
	 *
	 * The implementation is similar to this, mainly differing in that this module is only set up
	 * to allow queuing on one facility at a time, so route advance merely advances to the next route,
	 * rather than adding the next route.
	 */

	queue_promo_start = ast_tvnow();
	route_advance_start = ast_tvnow();

	for (;;) {
		struct pollfd fds = { call->queue_alert_pipe[0], POLLIN, 0 };
		/* Sleep for (up to) 10 seconds.
		 * If alertpipe is written to, we'll exit the poll immediately.
		 * This means we don't need a tighter loop per se for module hold,
		 * because at that point we could write to the pipe if we wanted
		 * (ordinarily, we can't write to the alertpipe whenever because
		 * it generally means the head queued call is free to proceed),
		 * and a CLI hangup will *eventually* hang this up. */
		res = ast_poll(&fds, 1, 10000);
		if (res < 0) {
			ast_log(LOG_WARNING, "Polling error: %s\n", strerror(errno));
			break;
		} else if (res) {
			ast_alertpipe_read(call->queue_alert_pipe);
			/* Something happened: perhaps it's our turn to do a callback? Or maybe we were aborted. */
		}
		if (call->aborted) { /* CBQ was cancelled. */
			ast_debug(1, "CBQ thread %lu is ending early\n", call->cbqthread);
			break;
		} else if (res) {
			char cbq_dest[AST_MAX_CONTEXT]; /* Technically not enough for exten@context, but in practice, should be... */
			struct ast_format_cap *capabilities;
			char *caller_context, *dest_context;
			int outgoing_status;
			int total_queue_time;
			int timeout = 0; /* Timeout in seconds */
			/* It's our turn now for the facility, yay! */
			/* This is kind of the whole reason that we have an alertpipe per individual call,
			 * as opposed to a global once that everyone checks. That way we notify just specifically
			 * the call whose turn it actually is, which means we can be sure it's our turn here without
			 * checking anything. */

			dest_context = call->callback_dest_context;
			caller_context = call->callback_caller_context;
			ast_assert(!ast_strlen_zero(caller_context));
			timeout = (int) (2.56 * strlen(call->called) + 10.00); /* This is the math that Nortel Meridians do, so why not? */

			snprintf(cbq_dest, sizeof(cbq_dest), "%s@%s", call->cbqexten, caller_context);
			capabilities = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT);
			if (!capabilities) {
				ast_log(LOG_WARNING, "Failed to allocate capabilities\n");
				break;
			}
			ast_format_cap_append(capabilities, ast_format_slin, 0);
			total_queue_time = (int) time(NULL) - call->start;

			/* Spawn the callback. */
			ast_verb(3, "Call Back Queue callback for %s -> %s (%ds elapsed, %ds timeout)\n", call->cbqexten, call->called, total_queue_time, timeout);
			if (!ast_strlen_zero(dest_context)) {
				/* Connect to a user-provided destination context. */
				res = ast_pbx_outgoing_exten_predial("Local", capabilities, cbq_dest,
					timeout * 1000, dest_context, call->called, 1, &outgoing_status,
					AST_OUTGOING_NO_WAIT, call->called, "CALLBACK", NULL, NULL, NULL, 0, NULL, NULL);
			} else {
				char app_args[256];
				/* Connect the called back caller back to this module, with same exten,ccsa,route, with priority 3, just in case we miss it. Also add with the same FRL we had just now, so we don't have to enter an auth code to access this facility again. */
				snprintf(app_args, sizeof(app_args), "%s,%s,%s,cp(3)f(%d)", call->called, call->ccsa, call->route, call->effective_frl);
				/* Reconnect to the CCSA module using the same settings that we have now. */
				res = ast_pbx_outgoing_app_predial("Local", capabilities, cbq_dest,
					timeout * 1000, app, app_args, &outgoing_status,
					AST_OUTGOING_NO_WAIT, call->called, "CALLBACK", NULL, NULL, NULL, NULL, NULL);
			}
			/* Async, because we're still in the queue, and will block the callback from actually seizing the facility.
			 * When we disappear, it'll free the spot that just became open in the queue and, barring any fantastic race
			 * condition, the callback call should be able to grab it.
			 * We don't need to artificially introduce a delay because the called back caller is not going to answer
			 * instantly, so by the time that call is picked up, we'll be long gone already.
			 */

			ao2_cleanup(capabilities);
			break;
		}
		if (use_queue_promo_timer && ast_remaining_ms(queue_promo_start, call->queue_promo_timer) <= 0) {
			ast_debug(1, "Queue Promotion Timer expired for %s\n", call->caller);
			if (call->queue_priority >= 3) {
				ast_debug(1, "CBQ priority is already maximum, not incrementing priority anymore\n");
				use_queue_promo_timer = 0;
			} else {
				call->queue_priority += 1;
				ast_debug(1, "CBQ priority is now %d\n", call->queue_priority);
				AST_RWLIST_WRLOCK(&calls);
				/* Rather than trying to reorder the list, just brute force remove and reinsert,
				 * which will reinsert at an appropriate spot. Much better than removing
				 * the call itself and making a new one. */
				call_queue_remove(call);
				call_queue_insert(call, call->route);
				AST_RWLIST_UNLOCK(&calls);
			}
		}
		if (use_route_advance_timer && ast_remaining_ms(route_advance_start, call->route_advance_timer) <= 0) {
			ast_debug(1, "Route Advance Timer expired for %s\n", call->caller);
			use_route_advance_timer = 0;

			/* Swap the current route for the next one in the list, if there is one. */
			if (call->nextroute) {
				char *newroute, *newfacility;
				char next_facility[AST_MAX_CONTEXT];
				if (get_route_facility(call->nextroute, next_facility, sizeof(next_facility))) {
					ast_log(LOG_WARNING, "Failed to determine facility used by route %s?\n", call->nextroute); /* Shouldn't ever happen. */
				} else {
					struct ccsa_call *ecall;
					int advanced = 0;

					newroute = ast_strdup(call->nextroute);
					if (newroute) {
						newfacility = ast_strdup(next_facility);
						if (newfacility) {
							AST_RWLIST_WRLOCK(&calls);
							/* Make sure we're still in the list, too. */
							AST_LIST_TRAVERSE(&calls, ecall, entry) {
								if (call == ecall) {
									/* Shift next to now. */
									ast_free(call->route);
									call->route = newroute;
									call->facility = newfacility;
									ast_free(call->nextroute); /* Don't actually need this guy anymore. */
									advanced = 1;
									break;
								}
							}

							/* Just like in the above case, remove and reinsert to get it to the correct spot. */
							if (advanced) {
								call_queue_remove(call);
								call_queue_insert(call, call->route);
							} else {
								ast_log(LOG_WARNING, "Route Advance failed for CBQ call\n");
							}
							AST_RWLIST_UNLOCK(&calls);
						}
					}
				}
			}
		}
	}

	if (!call->aborted) {
		/* Hey, we weren't aborted, which means nobody is going to clean up our mess for us. */
		ast_debug(2, "Thread %lu is ending of its own volition\n", call->cbqthread);
		pthread_detach(call->cbqthread);
		call_free(call, 1);
	}

	return NULL;
}

static int offer_ohq(struct ast_channel *chan)
{
	if (!chan) {
		return 0;
	}
	if (!ast_playtones_start(chan, 0, "440", 0)) { /* OHQ offer: 1 second burst of 440 Hz */
		if (ast_safe_sleep(chan, 1000)) {
			ccsa_log(chan, fd, "User declined Off Hook Queue offer\n");
			return -1;
		}
	}
	/* To decline OHQ offer, user goes on hook (hangs up). To accept, stay off hook */
	ast_playtones_stop(chan);
	return 0;
}

/*! \retval -1 on hangup, 0 on timeout, positive integer on complete input */
static int get_auth_code(struct ast_channel *chan, char *buf, size_t len)
{
	int res = 0;
	struct ast_tone_zone_sound *ts = NULL;
	int x = 0;

	ast_assert(chan && buf && len);
	*buf = '\0';

	/* According to 309-400-000, authorization code prompt is simply RDT (Recall Dial Tone).
	 * However, as evidenced by Evan Doorbell's Early 80 series, in context of remote access
	 * to CCSAs, a unique double tone prompt was also used (possibly only in independents?)
	 * This "PIN prompt tone" is 800/300/800 on/off/on of appx. 345+440 Hz (according to Audacity spectrum plot)
	 * However, the striking closeness to dial tone (350+440), plus the documentation of RDT,
	 * makes me suspect maybe it was 350+440 for 800/300/800, as opposed to 345+440 (recording could've had distortion).
	 * In fact, I'm quite positive it is 350+440.
	 */
	ts = ast_get_indication_tone(ast_channel_zone(chan), "authcodeprompt"); /* Doesn't exist by default. */
	if (!ts) {
		ts = ast_get_indication_tone(ast_channel_zone(chan), "dialrecall"); /* Exists by default. */
	}

	if (!ts) {
		ast_log(LOG_WARNING, "Missing tone definition for dialrecall\n");
		return 0; /* Can't read authorization code without tone, so just skip. */
	}

	ast_playtones_start(chan, 0, ts->data, 0);
	ts = ast_tone_zone_sound_unref(ts);

	/* Sadly, unlike ast_app_getdata_terminator, there is no core function equivalent for tones (yet...) */
	for (x = 0; x < len - 1; ) {
		res = ast_waitfordigit(chan, 6000);
		ast_playtones_stop(chan);
		if (res < 1) {
			break; /* 0 and -1 */
		}
		buf[x++] = res;
		if (strchr("#", buf[x - 1])) {
			buf[x - 1] = '\0';
			break;
		}
		if (x >= len - 1) {
			break;
		}
	}

	return res;
}

/*! \retval -1 on failure, -2 on invalid code, 0-7 FRL of authorization code if valid */
static int verify_auth_code(struct ast_channel *chan, const char *auth_sub_context, const char *code, int remote)
{
	const char *retval;
	char location[AST_MAX_CONTEXT + 20];
	int ret;

	ast_debug(3, "Received authorization code: %s\n", code);

	if (!ast_exists_extension(chan, auth_sub_context, code, 1, NULL)) {
		ast_debug(2, "Location %s,%s,1 does not exist\n", auth_sub_context, code);
		return -2; /* Dialplan location doesn't even exist, don't Gosub to it and crash (the call). */
	}

	/* Call userland dialplan subroutine so authorization code can be verified in whatever preferred mechanism, however simple or complex */
	snprintf(location, sizeof(location), "%s,%s,1", auth_sub_context, code);
	if (ast_app_run_sub(NULL, chan, location, remote ? "r" : "", 1)) {
		return -1;
	}

	ast_channel_lock(chan);
	retval = pbx_builtin_getvar_helper(chan, "GOSUB_RETVAL");
	ret = !ast_strlen_zero(retval) ? atoi(retval) : -1; /* atoi of empty string is 0, so don't default to that, since 0 is a valid FRL */
	ast_channel_unlock(chan);

	if (ret < 0) {
		return -2; /* Invalid code. */
	}

	if (ret > MAX_FRL) {
		ast_log(LOG_WARNING, "Invalid FRL: %d\n", ret);
		return -2; /* Invalid FRL */
	}

	return ret;
}

static int trunk_busy(const char *dialstatus, int hangupcause, int busyiscongestion)
{
	/* Determine if this was a "trunk busy", as opposed to actual destination being busy.
	 * If this a trunk busy, we should advance to the next route and retry. Otherwise, we
	 * should stop.
	 * If SIP providers return 486 Busy or ISDN code 17, then we should NOT treat that
	 * as a trunk busy (e.g. busyiscongestion should be 0). For something like an FXO
	 * line, where busy simply means the line (trunk) is in use, busyiscongestion should be 1.
	 */
	if (hangupcause == 34 || hangupcause == 3 || hangupcause == 21) {
		return 1;
	}
	if (!strcmp(dialstatus, "CHANUNAVAIL") || !strcmp(dialstatus, "CONGESTION")) {
		return 1;
	}
	if (busyiscongestion && !strcmp(dialstatus, "BUSY")) {
		return 1;
	}
	return 0;
}

/*! \brief Set the Traveling Class Mark from the Facility Restriction Level */
static int set_tcm(struct ast_channel *chan, int frl)
{
	char new_tcm[2];

	if (frl < 0 || frl > MAX_FRL) {
		ast_log(LOG_WARNING, "Invalid FRL: %d. TCM not set!\n", frl);
		return -1;
	}

	/*
	 * 309-400-000 5.11. The facility restriction level is encoded into a traveling class mark (TCM)
	 * for calls routed from an originating PBX/CTX tandem to a distant tandem. The TCM,
	 * a single TOUCH-TONE or MF digit, is relayed to the distant PBX/CTX tandem together with the called number.
	 * This one-digit code (0-7) informs a distant PBX/CTX tandem what facility choices are to be
	 * afforded the routing of the call on MTS (tail-end hop-off). TCMs can be used to limit MTS overflow to certain users.
	 */

	snprintf(new_tcm, sizeof(new_tcm), "%d", frl);
	pbx_builtin_setvar_helper(chan, "TCM", new_tcm);

	/*
	 * Set the TCM variable, and individual routes can be configured to send the TCM as desired/appropriate.
	 * The standard is for the TCM to be sent as the LAST digit (I know, I would have thought the FIRST
	 * would make more sense...)
	 * https://manualsdump.com/en/manuals/lucent_technologies-8.2/206171/1362
	 */

	return 0;
}

#define ccsa_set_result_val(chan, disp) if (chan) pbx_builtin_setvar_helper(chan, "CCSA_RESULT", disp)

static int comma_count(const char *s)
{
	int c = 0;
	while (*s) {
		if (*s == ',') {
			c++;
		}
		s++;
	}
	return c;
}

static enum facility_disp ccsa_try_route(struct ast_channel *chan, int fd, int *have_mer, char try_preempt, const char *exten, const char *route, int *callerfrl, int *frl_upgraded, int mer_tone, int frl_allow_upgrade, int auth_code_remote_allowed, int remote, const char *auth_sub_context, const char *outgoing_clid)
{
	int res;
	struct route *f;
	char dialstr[PATH_MAX + 84]; /* Minimum needed to avoid snprintf truncation warnings */
	char time[PATH_MAX];
	char facility[AST_MAX_CONTEXT];
	const char *aiod;
	int frl, mer, busyiscongestion, limit;

	dialstr[0] = time[0] = '\0';

	AST_RWLIST_RDLOCK(&routes);
	f = find_route(route, 0);
	if (!f) {
		ast_log(LOG_WARNING, "No such route: %s\n", route);
		AST_RWLIST_UNLOCK(&routes);
		return FACILITY_DISP_FAILURE;
	}
	frl = f->frl;
	mer = f->mer;
	busyiscongestion = f->busyiscongestion;
	limit = f->limit;

	ast_copy_string(time, f->time, sizeof(time));
	ast_copy_string(facility, f->facility, sizeof(facility));
	AST_RWLIST_UNLOCK(&routes);

	if (ast_strlen_zero(f->dialstr)) {
		ast_log(LOG_WARNING, "Route %s has no dial string?\n", route);
		return FACILITY_DISP_FAILURE;
	}
	aiod = S_OR(outgoing_clid, f->aiod);
	if (!ast_strlen_zero(aiod)) {
		int commas = comma_count(f->dialstr);
		/* This is concatenated to the dial string, so it is assumed a URL is not present in the dialstr */
		cdr_set_var(chan, cdrvar_aiod, aiod);
		snprintf(dialstr, sizeof(dialstr), "%s%s%sf(%s)", f->dialstr, commas <= 0 ? "," : "", commas <= 1 ? "," : "", aiod);
	} else {
		cdr_set_var(chan, cdrvar_aiod, ""); /* Reset in case it was already set */
		ast_copy_string(dialstr, f->dialstr, sizeof(dialstr));
	}

	ast_debug(4, "Route %s: Limit: %d, FRL: %d, MER: %d, Busy Is Cong.: %d, DSTR: %s, Time: %s\n", route, limit, frl, mer, busyiscongestion, dialstr, time);

	/* If time condition exists, we need to satisfy it: consider if the caller is actually eligible for this route. */
	if (!ast_strlen_zero(time) && time_matches(time)) {
		ccsa_log(chan, fd, "Time condition '%s' is not satisfied\n", time);
		return FACILITY_DISP_UNAVAILABLE;
	}

	ccsa_log(chan, fd, "Considering route: %s\n", route);
	if (chan) { /* If we're just doing a simulation, don't try to set any CDR variables, since there's no channel */
		cdr_set_var(chan, cdrvar_facility, facility);
		cdr_set_var(chan, cdrvar_route, route);
	}

	if (frl > *callerfrl && *frl_upgraded == -2) { /* If FRL upgrade allowed (and haven't done yet), prompt for authorization code. Otherwise, skip. */
		*frl_upgraded = -1; /* At this point, mark that we've tried, at least, as we shouldn't prompt for authorization code more than once. */
		cdr_set_var_int(chan, cdrvar_frl_req, frl);
		cdr_set_var_int(chan, cdrvar_frl, *callerfrl);

		/* No upgrades allowed, so no chance of being able to use this route. */
		if (!frl_allow_upgrade) {
			ccsa_log(chan, fd, "Route %s FRL required > FRL present: %d > %d, no upgrade allowed\n", route, frl, *callerfrl);
			return FACILITY_DISP_UNAUTHORIZED;
		} else if (remote && !auth_code_remote_allowed) {
			ccsa_log(chan, fd, "Route %s FRL required > FRL present: %d > %d, no remote upgrade allowed\n", route, frl, *callerfrl);
			return FACILITY_DISP_UNAUTHORIZED;
		}

		/* Get authorization code. */
		if (chan) {
			char auth_code[frl_allow_upgrade + 1]; /* +1 for null terminator */
			res = get_auth_code(chan, auth_code, sizeof(auth_code));
			if (res < 0) {
				return FACILITY_DISP_HANGUP;
			}
			if (!res) {
				ccsa_log(chan, fd, "No authorization code entered\n");
				return FACILITY_DISP_UNAUTHORIZED;
			}
			cdr_set_var(chan, cdrvar_authcode, auth_code);
			res = verify_auth_code(chan, auth_sub_context, auth_code, remote);
			if (res == -1) {
				return FACILITY_DISP_HANGUP;
			} else if (res < 0) { /* -2 = invalid */
				ccsa_log(chan, fd, "Invalid authorization code entered\n");
				return FACILITY_DISP_INVALID_AUTH_CODE;
			}
			*frl_upgraded = res; /* Update new effective FRL */
			cdr_set_var_int(chan, cdrvar_frl_eff, *frl_upgraded);
		} else { /* If simulated, assume we got an authorization code of the required FRL */
			/*! \todo better would be to simulate an auth code or upgrade FRL directly */
			ccsa_log(chan, fd, "Simulating FRL upgrade from %d to %d\n", *callerfrl, frl);
			*frl_upgraded = frl;
		}
	}

	if (frl > *callerfrl && frl > *frl_upgraded) { /* If FRL >= original FRL and upgraded FRL, not authorized to use this route. */
		ccsa_log(chan, fd, "Not authorized to use route %s (requires FRL %d, actual is %d)\n", route, frl, MAX(*callerfrl, *frl_upgraded));
		return FACILITY_DISP_UNAUTHORIZED;
	}

	/* If this is a more expensive route, play MER (More Expensive Route) Tone */
	if (mer && !*have_mer) {
		*have_mer = 1; /* Play MER (More Expensive Route) tone, since this is the first more expensive route under consideration */
		ccsa_log(chan, fd, "More expensive route: %s\n", route); /* Only logged for first MER route under consideration. */
		if (mer_tone) { /* If play MER tone, play it. */
			ccsa_log(chan, fd, "Expensive Route Warning Tone\n");
			if (chan) {
				/* Expensive Route Warning Tone (ERWT): three 256-ms bursts of 440 Hz */
				if (ast_playtones_start(chan, 0, "440/256,0/256,440/256,0/256,440/256,0/256", 0)) {
					return FACILITY_DISP_HANGUP;
				}
				if (ast_safe_sleep(chan, 256 * 6)) {
					return FACILITY_DISP_HANGUP;
				}
				ast_playtones_stop(chan);
			}
			*have_mer = 1; /* Played MER tone once, don't play it again. */
		}
	}

	if (try_preempt) {
		int pres = try_facility_preempt(facility, try_preempt);
		if (pres < 1) {
			ccsa_log(chan, fd, "Failed to preempt call on route %s\n", route);
			return pres == 0 ? FACILITY_DISP_PREEMPTION_FAILED : FACILITY_DISP_UNAVAILABLE;
		}
		ccsa_log(chan, fd, "Successfully preempted call on route %s\n", route);
		/* Now that we preempted a call, try to make the call */
		/* Wait a few hundred MS to make doubly sure call has been removed from call list. */
		if (ast_safe_sleep(chan, 400)) {
			return FACILITY_DISP_HANGUP;
		}
	}

	/* If route has a concurrent call limit, make sure we won't exceed it. */
	if (limit) {
		int current = facility_num_calls(facility);
		if (current >= limit) {
			ccsa_log(chan, fd, "Concurrent call limit on facility %s reached (%d)\n", facility, current);
			return FACILITY_DISP_UNAVAILABLE;
		}
	}

	ccsa_log(chan, fd, "Attempting call on route %s\n", route);
	if (chan) {
		struct ccsa_call *call;
		const char *dialstatus, *hangupcausestr;
		int hangupcause;
		int trunkbusy;
		int preempted;

		/* Set the Traveling Class Mark */
		set_tcm(chan, *frl_upgraded >= 0 ? *frl_upgraded : *callerfrl);

		if (store_dtmf(chan)) {
			return -1;
		}

		ccsa_log(chan, fd, "Dial(%s)\n", dialstr);
		call = call_add(ast_channel_name(chan), facility, route, ast_channel_caller(chan)->id.number.str, exten, 1, try_preempt, 0); /* Push to call queue */
		if (!call) {
			ast_log(LOG_ERROR, "Failed to add call to call list, aborting\n");
			return FACILITY_DISP_FAILURE;
		}
		pbx_builtin_setvar_helper(chan, "CCSA_EXTEN", exten);
		res = ast_pbx_exec_application(chan, "Dial", dialstr); /* This performs variable substitution, so we're good. */
		preempted = call->preempted;
		call_free(call, 1); /* Pop from call queue */

		if (preempted) { /* Find out if our reason for exiting the call was we got preempted. */
			queue_notify_facility_head(facility); /* If the call was up long enough to get preempted, most likely it was successfuly, without actually checking. */
			if (!(ast_channel_softhangup_internal_flag(chan) & AST_SOFTHANGUP_ASYNCGOTO)) {
				ast_log(LOG_WARNING, "Strange... soft hangup flag NOT set?\n");
			} else {
				ast_channel_clear_softhangup(chan, AST_SOFTHANGUP_ASYNCGOTO); /* Clear the soft hangup signal */
			}
			ccsa_log(chan, fd, "Call on %s was preempted\n", ast_channel_name(chan));
			return FACILITY_DISP_PREEMPTED;
		}

		/* Handle attempt result */
		ast_channel_lock(chan);
		dialstatus = pbx_builtin_getvar_helper(chan, "DIALSTATUS");
		if (ast_strlen_zero(dialstatus)) {
			ast_log(LOG_WARNING, "Call ended without a DIALSTATUS?\n");
			dialstatus = "CONGESTION";
		}
		hangupcausestr = pbx_builtin_getvar_helper(chan, "HANGUPCAUSE");
		if (ast_strlen_zero(hangupcausestr)) { /* It can happen */
			hangupcause = 0;
		} else {
			hangupcause = atoi(hangupcausestr);
		}
		trunkbusy = trunk_busy(dialstatus, hangupcause, busyiscongestion);
		ccsa_log(chan, fd, "Dial on %s %s: %s (%d)\n", route, trunkbusy ? "failed" : "succeeded", dialstatus, hangupcause);
		ast_channel_unlock(chan);

		if (!trunkbusy) {
			queue_notify_facility_head(facility); /* Notify the next call in line on THIS FACILITY. */
		}

		if (res < 0) {
			return FACILITY_DISP_HANGUP;
		}

		return trunkbusy ? FACILITY_DISP_UNAVAILABLE : FACILITY_DISP_SUCCESS; /* Succeeded, we're done. */
	} else {
		/* Since we're simulating, pretend the route is busy so we can enumerate the entire call plan. */
		ccsa_log(chan, fd, "Route %s simulated unavailable\n", route);
		return FACILITY_DISP_UNAVAILABLE;
	}
}

/*! \brief Channel and CLI command for common runtime and emulation logic */
/*! \param chan Channel, if running for real */
/*! \param fd CLI fd, if simulating */
/*! \param exten Destination, if running for real */
static int ccsa_run(struct ast_channel *chan, int fd, const char *exten, const char *ccsa, const char *faclist, const char *musicclass, int remote, int cbq, int ohq, int priority, char preempt,
	int callerfrl, int no_frl_upgrade, const char *outgoing_clid)
{
	enum facility_disp fres;
	char *route, *routes;
	int have_mer = 0, frl_upgraded = -2; /* -2 = not prompted yet, -1 = invalid, 0-7 = FRL of auth code */
	int total_attempted = 0, total_unavailable = 0, total_unauthorized = 0, total_preempt_fails = 0;
	char try_preempt = 0;
	struct ccsa *c;
	int mer_tone, frl_allow_upgrade, auth_code_remote_allowed;
	int extension_len;
	char *hold_announcement = NULL, *extension_prompt = NULL;
	char *auth_sub_context = NULL;
	char *callback_caller_context = NULL, *callback_dest_context = NULL;
	unsigned int queue_promo_timer, route_advance_timer;
	time_t start;

	AST_RWLIST_RDLOCK(&ccsas);
	c = find_ccsa(ccsa, 0);
	if (!c) {
		AST_RWLIST_UNLOCK(&ccsas);
		ast_log(LOG_WARNING, "No such CCSA: %s\n", ccsa);
		return -1;
	}
	mer_tone = c->mer_tone;
	frl_allow_upgrade = c->frl_allow_upgrade;
	auth_code_remote_allowed = c->auth_code_remote_allowed;
	if (!ast_strlen_zero(c->auth_sub_context)) {
		auth_sub_context = ast_strdupa(c->auth_sub_context);
	}
	if (!ast_strlen_zero(c->hold_announcement)) {
		hold_announcement = ast_strdupa(c->hold_announcement);
	}
	if (!ast_strlen_zero(c->extension_prompt)) {
		extension_prompt = ast_strdupa(c->extension_prompt);
	}
	if (remote && auth_code_remote_allowed) {
		frl_allow_upgrade = 0; /* If upgrades not allowed for remote access, don't allow FRL upgrades */
	}
	if (no_frl_upgrade) {
		frl_allow_upgrade = 0;
	}

	extension_len = c->extension_len;
	queue_promo_timer = c->queue_promo_timer;
	route_advance_timer = c->route_advance_timer;
	if (!ast_strlen_zero(c->callback_caller_context)) {
		callback_caller_context = ast_strdupa(c->callback_caller_context);
	}
	if (!ast_strlen_zero(c->callback_dest_context)) {
		callback_dest_context = ast_strdupa(c->callback_dest_context);
	}

	/* This field also doubles to contain the number of digits of authorization codes, if FRL upgrades are allowed. */
	if (frl_allow_upgrade) {
		frl_allow_upgrade = c->auth_code_len;
	}

	AST_RWLIST_UNLOCK(&ccsas);

	/* General Notes:
	 *
	 * Important DEFINITIONS and ACRONYMS:
	 * SSN (Switched Services Network)
	 * - CCSA (Common Control Switching Arrangement)
	 * - EPSCS (Enhanced Private Switched Communications Service)
	 * - ETN (Electronic Tandem Network)
	 * AAR - Automatic Alternate Routing: (first choice/high usage, bypass, final)
	 * ARS - Automatic Route Selection: routing of MTS calls through on-net facilities to the extent possible (tail-end hop-off)
	 * MTS - Message Toll System (i.e. PSTN)
	 * WATS - Wide Area Telecommunications Service
	 * FRL - Facility Restriction Level: code 0-7 identifying each station's max privilege level or a trunk's min. required privilege level
	 * TCM - Traveling Class Mark: encoding of FRL over tie trunks using DTMF or MF, intended to enforce tail-end hop-off (to MTS) only to sufficiently priviledged callers.
	 * AC - Authorization Code: Code to override or "upgrade" FRL for a call. May not begin with 1, and can be customer-controlled.
	 *		Each authorization code may optionally be designated as being valid for remote access.
	 * Deluxe queuing: wait for idle trunk; on any given call, takes place on first choice trunk group.
	 * - OHQ - Off-Hook Queue: tandem, main, satellites, and tributaries (higher priority)
	 * - CBQ/RBQ - Call-Back Queue / Ring-Back Queue: tandem, main, satellites only (lower priority)
	 * MER = More Expensive Route (as in MER Tone)
	 * ERWT = Expensive Route Warning Tone
	 *
	 * GENERAL REFERENCES:
	 * - Section 309 of Bell System Practices: https://www.telecomarchive.com/309.html
	 * - Section 231-190 of Bell System Practices: https://www.telecomarchive.com/231.html
	 * - Evan Doorbell - Personal correspondance, and early 80s CCSA series: http://www.evan-doorbell.com/production/#early80s
	 * - Nortel Meridian Automatic Route Selection documentation
	 * -- http://docs.smoe.org/nortel/www142.nortelnetworks.com/bvdoc/meridian1/m1254x/P0944032.pdf
	 * -- OHQ/CBQ: http://docs.smoe.org/nortel/www142.nortelnetworks.com/bvdoc/meridian1/m1254x/P0944033.pdf
	 *
	 * In particular, there are 2 primary resources that are used for much of this module's logic:
	 * - BSP 309-400-000: https://www.telecomarchive.com/docs/bsp-archive/309/309-400-000_I2.pdf
	 * - Nortel 553-2751-101 (Network Queuing): http://docs.smoe.org/nortel/www142.nortelnetworks.com/bvdoc/meridian1/m1254x/P0944033.pdf
	 *
	 * Although this module is called CCSA, it is intentionally generic enough that it can be used
	 * to implement a CCSA, EPSCS, or ETN, all of which are very similar. All of these are Switched Services Networks (SSNs).
	 * There are a lot of technical details about SSNs, and if you are not sufficiently familiar with them, you will
	 * probably need to peruse the documentation.
	 */

	ccsa_log(chan, fd, "Eligible Routes: %s\n", faclist);
	ccsa_log(chan, fd, "Caller FRL: %d, Preempt: %c, Priority: %d, CBQ: %d, OHQ: %d\n", callerfrl, isprint(preempt) ? preempt : '?', priority, cbq, ohq);

	if (ast_strlen_zero(faclist)) {
		ast_log(LOG_WARNING, "No route eligible for CCSA route\n");
		ccsa_set_result_val(chan, "NO_ROUTES");
		return -1;
	}

	ast_assert(chan || fd >= 0);
	ccsa_log(chan, fd, "Beginning CCSA call\n");
	start = time(NULL);

	routes = ast_strdupa(faclist);
	while ((route = strsep(&routes, "|"))) {
		if (ast_strlen_zero(route)) {
			ccsa_log(chan, fd, "Skipping empty route\n");
			continue; /* Allow for empty routes so that using IFTIME in dialplan resolving to empty route is OK. */
		}
		total_attempted++;
		fres = ccsa_try_route(chan, fd, &have_mer, try_preempt, exten, route, &callerfrl, &frl_upgraded, mer_tone, frl_allow_upgrade, auth_code_remote_allowed, remote, auth_sub_context, outgoing_clid);
		switch (fres) {
		case FACILITY_DISP_HANGUP:
			return -1;
		case FACILITY_DISP_SUCCESS:
			ccsa_log(chan, fd, "Successful completion\n");
			ccsa_set_result_val(chan, "SUCCESS");
			return 0;
		case FACILITY_DISP_UNAVAILABLE:
			total_unavailable++;
			break;
		case FACILITY_DISP_UNAUTHORIZED:
			total_unauthorized++;
			if (frl_upgraded == -1) {
				ccsa_set_result_val(chan, "UNAUTHORIZED"); /* Prompted for authorization code, and none provided. */
				return 0;
			}
			break;
		case FACILITY_DISP_INVALID_AUTH_CODE:
			ccsa_set_result_val(chan, "AUTH_CODE");
			return 0;
		case FACILITY_DISP_PREEMPTION_FAILED:
			ast_assert(0);
			break;
		case FACILITY_DISP_PREEMPTED:
			ccsa_set_result_val(chan, "PREEMPTED");
			return 0;
		case FACILITY_DISP_FAILURE:
			break; /* General failure, move on to next one */
		default:
			ast_log(LOG_WARNING, "Unexpected return value: %d\n", fres);
		}
	}

	/* Cursory try of all eligible routes failed. */
	ccsa_log(chan, fd, "First pass unsuccessful\n");

	if (total_unauthorized == total_attempted) {
		/* Weren't even authorized to use any of the available routes, so can't try to preempt or queue. */
		ccsa_log(chan, fd, "Not authorized for any routes\n");
		ccsa_set_result_val(chan, "UNAUTHORIZED");
		return 0;
	}

	/* If we don't have any preemption capabilities, don't try again, but queue if possible. */
	if (!preempt) {
		ccsa_log(chan, fd, "Not authorized for any preemption, so skipping second pass\n");
	} else if (preempt < 'A' || preempt > 'D') {
		ast_log(LOG_WARNING, "Invalid MLPP preemption capability: %c\n", isprint(preempt) ? preempt : '?');
	} else {
		/* Now, try preempting an existing call before we queue. */
		try_preempt = preempt;
		ccsa_log(chan, fd, "Trying to preempt calls < '%c'\n", preempt);
		routes = ast_strdupa(faclist);
		while ((route = strsep(&routes, "|"))) {
			if (ast_strlen_zero(route)) {
				continue; /* Allow for empty facilities so that using IFTIME in dialplan resolving to empty route is OK. */
			}
			total_attempted++;
			fres = ccsa_try_route(chan, fd, &have_mer, try_preempt, exten, route, &callerfrl, &frl_upgraded, mer_tone, frl_allow_upgrade, auth_code_remote_allowed, remote, auth_sub_context, outgoing_clid);
			switch (fres) {
			case FACILITY_DISP_HANGUP:
				return -1;
			case FACILITY_DISP_SUCCESS:
				ccsa_log(chan, fd, "Successful completion\n");
				ccsa_set_result_val(chan, "SUCCESS");
				return 0;
			case FACILITY_DISP_UNAVAILABLE:
				total_unavailable++;
				break;
			case FACILITY_DISP_UNAUTHORIZED:
				total_unauthorized++;
				ast_assert(frl_upgraded != -1);
				break;
			case FACILITY_DISP_INVALID_AUTH_CODE:
				ccsa_set_result_val(chan, "AUTH_CODE");
				return 0;
			case FACILITY_DISP_PREEMPTION_FAILED:
				total_preempt_fails++;
				ccsa_set_result_val(chan, "PREEMPTION_FAILED");
				break;
			case FACILITY_DISP_PREEMPTED:
				ccsa_set_result_val(chan, "PREEMPTED");
				return 0;
			case FACILITY_DISP_FAILURE:
				break; /* General failure, move on to next one */
			default:
				ast_log(LOG_WARNING, "Unexpected return value: %d\n", fres);
			}
		}
	}

	/* We couldn't preempt any calls (including not authorized to) */

	if (!ohq && !cbq) {
		if (preempt) {
			/* If we tried preempting and failed and we can't queue, report that preemption failed. */
			ccsa_log(chan, fd, "Failure: unable to preempt and can't queue\n");
			ccsa_set_result_val(chan, "PREEMPTION_FAILED"); /* Play announcement */
		} else {
			ccsa_log(chan, fd, "Failure: not authorized to preempt and can't queue\n");
			ccsa_set_result_val(chan, "FAILED"); /* Reorder */
		}
		return 0;
	}

	if (ohq) { /* Off Hook Queue takes precedence over Call Back Queue, if permitted: anyone can OHQ, unlike CBQ */
		int eligible = 0;
		time_t elapsed;
		/* Start off by trying to off hook queue on non-MER routes */
		/* To be eligible for OHQ, at least 1 trunk route must have a call count <= threshold,
		 * then all routes are eligible for OHQ. */

		/* Nortel Meridian doesn't do ERWT for queued calls, but we'll do it anyways */
		have_mer = 0;

		routes = ast_strdupa(faclist);
		elapsed = time(NULL) - start;
		while ((route = strsep(&routes, "|"))) {
			if (route_permits_ohq(route, elapsed, fd != -1)) {
				eligible = 1;
				break; /* No need to check any more */
			}
		}

		if (!eligible) {
			ccsa_log(chan, fd, "Ineligible for Off-Hook Queue\n");
		} else { /* Offer OHQ on first choice route */
			char ohq_facility[AST_MAX_CONTEXT];
			ast_assert(route != NULL);
			ccsa_log(chan, fd, "Offering Off-Hook Queue\n");
			if (offer_ohq(chan)) {
				return -1;
			}

			if (get_route_facility(route, ohq_facility, sizeof(ohq_facility))) {
				ast_log(LOG_WARNING, "Failed to determine facility used by route %s?\n", route);
				return -1; /* Shouldn't ever happen. */
			}

			/* According to Nortel 553-2751-101, while in Off-Hook Queue, stations cannot do any kind of call modification
			 * e.g. hook flash, 3-way calling, etc. Personally,
			 * a) this seem like an archaic limitation to me
			 * b) Most of the time, that will inherently be true anyways, assuming this is the only call, because we wouldn't have suped yet
			 * c) I'm just going to ignore this aspect, we can't easily prevent this from a generic module like this anyhow.
			 */

			if (chan) {
				int res;
				struct ccsa_call *call;

				/* If hold announcement configured, play it. */
				if (!ast_strlen_zero(hold_announcement)) {
					if (ast_stream_and_wait(chan, hold_announcement, "")) {
						return -1;
					}
				}

				/* 553-2751-101: OHQ assigned max priority (3), because other network facilities can be held while call queued */
				call = call_add(ast_channel_name(chan), ohq_facility, route, ast_channel_caller(chan)->id.number.str, exten, 0, preempt, 3);
				if (!call) {
					ast_log(LOG_ERROR, "Failed to add call to call list, aborting\n");
					ccsa_set_result_val(chan, "FAILURE");
					return 0;
				}
				/* Wait for a trunk to free up, for up to maximum amount of allowed seconds. */
				if (!ast_strlen_zero(musicclass)) {
					if (ast_moh_start(chan, musicclass, NULL)) {
						ast_log(LOG_WARNING, "Failed to start music on hold class: %s\n", musicclass);
					}
				}
				res = off_hook_queue(chan, call, ohq);
				call_free(call, 1);
				if (res < 0) {
					return -1;
				}
				if (!ast_strlen_zero(musicclass)) {
					ast_moh_stop(chan);
				}
				if (res) {
					/* Hopefully, we didn't lose our spot just now due to a race condition... */
					fres = ccsa_try_route(chan, fd, &have_mer, try_preempt, exten, route, &callerfrl, &frl_upgraded, mer_tone, frl_allow_upgrade, auth_code_remote_allowed, remote, auth_sub_context, outgoing_clid);
					switch (fres) {
					case FACILITY_DISP_HANGUP:
						return -1;
					case FACILITY_DISP_SUCCESS:
						ccsa_log(chan, fd, "Successful completion\n");
						ccsa_set_result_val(chan, "SUCCESS");
						return 0;
					case FACILITY_DISP_PREEMPTION_FAILED:
						total_preempt_fails++;
						ccsa_set_result_val(chan, "PREEMPTION_FAILED");
						break;
					case FACILITY_DISP_PREEMPTED:
						ccsa_set_result_val(chan, "PREEMPTED");
						return 0;
					case FACILITY_DISP_UNAVAILABLE:
					case FACILITY_DISP_UNAUTHORIZED:
					case FACILITY_DISP_INVALID_AUTH_CODE:
					case FACILITY_DISP_FAILURE:
					default:
						ast_log(LOG_WARNING, "Off-Hook queued call on %s fail to complete using route %s\n", ast_channel_name(chan), route);
					}
					return 0; /* The buck stops here, we're done no matter what. */
				}
			} /* else, simulating time out anyways */
			/* Timer expired before call could be terminated */
			ccsa_log(chan, fd, "Off-Hook Queue timed out\n");

			/* Now, examine the remaining routes (if any), and attempt them, including MER routes if needed, e.g. DDD/MTS overflow. */
			while ((route = strsep(&routes, "|"))) {
				if (ast_strlen_zero(route)) {
					continue; /* Allow for empty facilities so that using IFTIME in dialplan resolving to empty route is OK. */
				}
				total_attempted++;
				fres = ccsa_try_route(chan, fd, &have_mer, try_preempt, exten, route, &callerfrl, &frl_upgraded, mer_tone, frl_allow_upgrade, auth_code_remote_allowed, remote, auth_sub_context, outgoing_clid);
				switch (fres) {
				case FACILITY_DISP_HANGUP:
					return -1;
				case FACILITY_DISP_SUCCESS:
					ccsa_log(chan, fd, "Successful completion\n");
					ccsa_set_result_val(chan, "SUCCESS");
					return 0;
				case FACILITY_DISP_PREEMPTION_FAILED:
					total_preempt_fails++;
					ccsa_set_result_val(chan, "PREEMPTION_FAILED");
					break;
				case FACILITY_DISP_PREEMPTED:
					ccsa_set_result_val(chan, "PREEMPTED");
					return 0;
				case FACILITY_DISP_UNAVAILABLE:
				case FACILITY_DISP_UNAUTHORIZED:
				case FACILITY_DISP_INVALID_AUTH_CODE:
				case FACILITY_DISP_FAILURE:
					break;
				default:
					ast_log(LOG_WARNING, "Unexpected return value: %d\n", fres);
				}
			}

			ccsa_log(chan, fd, "Off-Hook Queue failed on %s and no other routes available\n", chan ? ast_channel_name(chan) : "simulation");
			/* Queuing failed, give congestion tone (can't do CBQ, already did OHQ) */
			ccsa_set_result_val(chan, "OHQ_FAILED");
			return 0;
		}
	}

	/* Incoming tie trunks can only be arranged for off-hook queue: anything remote must abort now. */
	if (remote) {
		ccsa_log(chan, fd, "Remote calls cannot Call Back Queue, exiting\n");
		ccsa_set_result_val(chan, "CBQ_FAILED");
		return 0;
	}

	if (cbq) { /* Call/Ring Back Queue (at originating node only, i.e. local stations only) */
		char cbq_exten[extension_len + 1]; /* +1 for null terminator */
		int res = 0, pending = 0;
		/* User enters extension number for callback: 309-400-000 5.14
		 * BSPs and CCSA tapes from Evan Doorbell tape require entering extension number for callback.
		 * Nortel Meridian has people just overloads *66 for this.
		 * Our motto is "the BSPs are always right" :)
		 */

		if (fd == -1) {
			/* If not simulation, ensure we can actually CBQ */
			routes = ast_strdupa(faclist);
			while ((route = strsep(&routes, "|"))) {
				if (route_permits_cbq(route)) {
					break; /* No need to check any more */
				}
			}
			if (!route) {
				ast_debug(3, "Call ineligible for Call Back Queue\n");
				ccsa_set_result_val(chan, "UNROUTABLE");
				return 0;
			}
		}

		if (!chan) {
			ccsa_log(chan, fd, "Call Back Queue treatment\n");
			return 0;
		} else if (ast_strlen_zero(ast_channel_caller(chan)->id.number.str)) {
			ast_log(LOG_WARNING, "Channel %s has no Caller ID number\n", ast_channel_name(chan));
			ccsa_set_result_val(chan, "CBQ_FAILED");
			return 0;
		} else if (ast_strlen_zero(extension_prompt)) {
			ast_log(LOG_WARNING, "No extension prompt is configured.\n");
		}

		/* Offer the user a delayed callback */
		res = ast_app_getdata_terminator(chan, extension_prompt, cbq_exten, extension_len, 0, NULL);
		if (res < 0) {
			return -1;
		}
		if (ast_strlen_zero(cbq_exten)) {
			ccsa_log(chan, fd, "No callback extension received\n");
			ccsa_set_result_val(chan, "CBQ_FAILED");
			return 0;
		}

		/* A given caller may only have one call in CBQ at any given time.
		 * Since remote callers are not eligible for CBQ, we shouldn't need to worry about numbering conflicts. */
		pending = cbq_calls_pending(ast_channel_caller(chan)->id.number.str);
		if (pending) {
			ccsa_log(chan, fd, "Caller %s already has %d pending CBQ call%s\n", ast_channel_caller(chan)->id.number.str, pending, ESS(pending));
			ccsa_set_result_val(chan, "CBQ_FAILED");
		} else {
			char *nextroute;
			struct ccsa_call *call;
			char cbq_facility[AST_MAX_CONTEXT];
			ccsa_log(chan, fd, "Starting CBQ for extension %s\n", cbq_exten);

			/* Start by queuing on first choice first. */
			routes = ast_strdupa(faclist);
			route = strsep(&routes, "|");
			nextroute = strsep(&routes, "|"); /* Unlike route, this could be NULL (if there was only one route to begin with) */
			if (get_route_facility(route, cbq_facility, sizeof(cbq_facility))) {
				ast_log(LOG_WARNING, "Failed to determine facility used by route %s?\n", route);
				return -1; /* Shouldn't ever happen. */
			}

			if (ast_strlen_zero(callback_caller_context)) {
				ast_log(LOG_WARNING, "Config option callback_caller_context is not defined, CBQ will fail!\n");
				return -1;
			}

			/* Add to queue with initial CBQ priority */
			call = call_add(ast_channel_name(chan), cbq_facility, route, ast_channel_caller(chan)->id.number.str, exten, 0, preempt, priority);
			if (!call) {
				ast_log(LOG_ERROR, "Failed to add call to call list, aborting\n");
				return -1;
			} else {
				call->cbq = 1;
				call->effective_frl = frl_upgraded >= 0 ? frl_upgraded : callerfrl;
				call->queue_promo_timer = queue_promo_timer;
				call->route_advance_timer = route_advance_timer;
				call->cbqexten = ast_strdup(cbq_exten);
				if (!call->cbqexten) {
					ast_log(LOG_WARNING, "strdup failed\n");
					call_free(call, 1);
					return -1;
				}
				call->ccsa = ast_strdup(ccsa);
				if (!call->ccsa) {
					ast_log(LOG_WARNING, "strdup failed\n");
					call_free(call, 1); /* No leak, call->cbqexten will get cleaned up here. */
					return -1;
				}
				if (nextroute) { /* If there's another route we should try if the Route Advance Timer expires, indicate so */
					call->nextroute = ast_strdup(nextroute);
				}
				call->callback_caller_context = ast_strdup(callback_caller_context);
				if (callback_dest_context) {
					call->callback_dest_context = ast_strdup(callback_dest_context);
				}
				if (ast_pthread_create_background(&call->cbqthread, NULL, call_back_queue, (void *) call)) {
					call_free(call, 1);
					ast_log(LOG_WARNING, "Failed to create thread for Call Back Queue\n");
					ccsa_set_result_val(chan, "FAILURE");
				} else {
					/* Channel going to return immediately, without freeing this call. */
					ccsa_set_result_val(chan, "CBQ");
				}
			}
		}
	}

	return 0;
}

enum {
	OPT_OHQ =			(1 << 0),	/* Off Hook Queue */
	OPT_CBQ =			(1 << 1),	/* Call Back Queue */
	OPT_PRIORITY =		(1 << 2),	/* Initial Priority if queued (0-3) */
	OPT_PREEMPT =		(1 << 3),	/* Max MLPP preemption capability (e.g AUTOVON FO/F/I/P) */
	OPT_MUSICCLASS = 	(1 << 4),	/* Music On Hold */
	OPT_FRL =			(1 << 5),	/* Facility Restriction Level */
	OPT_REMOTE_ACCESS =	(1 << 6),	/* Remote Access */
	OPT_FORCE_CALLERID = (1 << 7),	/* Force outgoing Caller ID */
	OPT_NO_FRL_UPGRADE = (1 << 8),	/* Disallow FRL upgrades */
};

enum {
	OPT_ARG_OHQ,
	OPT_ARG_CBQ,
	OPT_ARG_PRIORITY,
	OPT_ARG_PREEMPT,
	OPT_ARG_MUSICCLASS,
	OPT_ARG_FRL,
	OPT_ARG_FORCE_CALLERID,
	/* note: this entry _MUST_ be the last one in the enum */
	OPT_ARG_ARRAY_SIZE,
};

AST_APP_OPTIONS(app_opts,{
	AST_APP_OPTION('a', OPT_REMOTE_ACCESS),
	AST_APP_OPTION_ARG('c', OPT_CBQ, OPT_ARG_CBQ),
	AST_APP_OPTION_ARG('f', OPT_FRL, OPT_ARG_FRL),
	AST_APP_OPTION_ARG('i', OPT_FORCE_CALLERID, OPT_ARG_FORCE_CALLERID),
	AST_APP_OPTION_ARG('m', OPT_MUSICCLASS, OPT_ARG_MUSICCLASS),
	AST_APP_OPTION_ARG('o', OPT_OHQ, OPT_ARG_OHQ),
	AST_APP_OPTION_ARG('p', OPT_PRIORITY, OPT_ARG_PRIORITY),
	AST_APP_OPTION_ARG('r', OPT_PREEMPT, OPT_ARG_PREEMPT),
	AST_APP_OPTION('u', OPT_NO_FRL_UPGRADE),
});

#define CCSA_ARG_REQUIRE(var, name) { \
	if (ast_strlen_zero(var)) { \
		ast_log(LOG_WARNING, "%s requires %s\n", app, name); \
		return -1; \
	} \
}

static int ccsa_exec(struct ast_channel *chan, const char *data)
{
	int res = -1;
	char *parse;
	struct ast_flags opts = { 0, };
	char *opt_args[OPT_ARG_ARRAY_SIZE];
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(exten);
		AST_APP_ARG(ccsa);
		AST_APP_ARG(routes); /* Pipe-separated list of routes to consider */
		AST_APP_ARG(options);
	);
	struct ccsa *c;
	int cbq = 0, ohq = 0;
	char preempt = 0;
	int remote = 0, priority = 1, callerfrl = 0;
	const char *faclist;
	const char *musicclass = NULL;
	const char *outgoing_clid = NULL;
	int no_frl_upgrade = 0;

	CCSA_ARG_REQUIRE(data, "an argument");

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	CCSA_ARG_REQUIRE(args.exten, "an extension");
	CCSA_ARG_REQUIRE(args.ccsa, "a CCSA");

	AST_RWLIST_RDLOCK(&ccsas);
	c = find_ccsa(args.ccsa, 0);
	if (!c) {
		ast_log(LOG_WARNING, "No such CCSA: %s\n", args.ccsa);
		ccsa_set_result_val(chan, "NO_CCSA");
		goto initcleanup;
	}

	/* Determine eligible routes */
	if (!ast_strlen_zero(args.routes)) {
		faclist = args.routes;
	} else {
		if (ast_strlen_zero(c->routes)) {
			ast_log(LOG_WARNING, "No routes defined for CCSA %s and none provided to CCSA()\n", args.ccsa);
			goto initcleanup;
		}
		faclist = ast_strdupa(c->routes); /* Default: assume all routes are eligible for routing */
	}

	if (!ast_strlen_zero(args.options))	{
		ast_app_parse_options(app_opts, &opts, opt_args, args.options);
		if (ast_test_flag(&opts, OPT_OHQ)) {
			ohq = !ast_strlen_zero(opt_args[OPT_ARG_OHQ]) ? atoi(opt_args[OPT_ARG_OHQ]) : 60;
		}
		if (ast_test_flag(&opts, OPT_CBQ)) {
			cbq = 1;
		}
		if (ast_test_flag(&opts, OPT_REMOTE_ACCESS)) {
			remote = 1;
		}
		if (ast_test_flag(&opts, OPT_PRIORITY) && !ast_strlen_zero(opt_args[OPT_ARG_PRIORITY])) {
			priority = atoi(opt_args[OPT_ARG_PRIORITY]);
			if (priority < 0 || priority > 3) {
				ast_log(LOG_WARNING, "Invalid priority: %d. Resetting to 0\n", priority);
				priority = 0;
			}
		}
		if (ast_test_flag(&opts, OPT_FRL) && !ast_strlen_zero(opt_args[OPT_ARG_FRL])) {
			callerfrl = atoi(opt_args[OPT_ARG_FRL]);
			if (callerfrl < 0 || callerfrl > MAX_FRL) {
				ast_log(LOG_WARNING, "Invalid FRL: %d. Resetting to 0\n", callerfrl);
				callerfrl = 0;
			}
		}
		if (ast_test_flag(&opts, OPT_FORCE_CALLERID) && !ast_strlen_zero(opt_args[OPT_ARG_FORCE_CALLERID])) {
			outgoing_clid = opt_args[OPT_ARG_FORCE_CALLERID];
		}
		if (ast_test_flag(&opts, OPT_PREEMPT) && !ast_strlen_zero(opt_args[OPT_ARG_PREEMPT])) {
			preempt = *opt_args[OPT_ARG_PREEMPT];
		}
		if (ast_test_flag(&opts, OPT_MUSICCLASS) && !ast_strlen_zero(opt_args[OPT_ARG_MUSICCLASS])) {
			musicclass = opt_args[OPT_ARG_MUSICCLASS];
		}
		no_frl_upgrade = ast_test_flag(&opts, OPT_NO_FRL_UPGRADE) ? 1 : 0;
	}

	res = 0;
initcleanup:
	AST_RWLIST_UNLOCK(&ccsas);
	if (res) {
		return -1;
	}

	pbx_builtin_setvar_helper(chan, "__CCSA_CHANNEL", ast_channel_name(chan));
	return ccsa_run(chan, -1, args.exten, args.ccsa, faclist, musicclass, remote, cbq, ohq, priority, preempt, callerfrl, no_frl_upgrade, outgoing_clid);
}

static char *handle_simulate_route(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-32s : %s\n"
#define FORMAT2 "%-32s : %d\n"
	struct ccsa *c;
	int cbq = 1, ohq = 1;
	int callerfrl = MAX_FRL;
	const char *faclist;
	char *ret = NULL;
	int which = 0;

	switch(cmd) {
	case CLI_INIT:
		e->command = "ccsa simulate route";
		e->usage =
			"Usage: ccsa simulate route <ccsa> [<routes> [<frl>]]\n"
			"       Simulate routing of a CCSA call.\n";
		return NULL;
	case CLI_GENERATE:
		if (a->pos == 3) {
			size_t wlen = strlen(a->word);
			AST_RWLIST_RDLOCK(&ccsas);
			AST_RWLIST_TRAVERSE(&ccsas, c, entry) {
				if (!strncasecmp(a->word, c->name, wlen) && ++which > a->n) {
					ret = ast_strdup(c->name);
					break;
				}
			}
			AST_RWLIST_UNLOCK(&ccsas);
		} else if (a->pos == 4) {
			struct route *r;
			size_t wlen = strlen(a->word);
			AST_RWLIST_RDLOCK(&routes);
			AST_RWLIST_TRAVERSE(&routes, r, entry) {
				if (!strncasecmp(a->word, r->name, wlen) && ++which > a->n) {
					ret = ast_strdup(r->name);
					break;
				}
			}
			AST_RWLIST_UNLOCK(&routes);
		}
		return ret;
	}

	if (a->argc < 4) {
		return CLI_SHOWUSAGE;
	}

	AST_RWLIST_RDLOCK(&ccsas);
	c = find_ccsa(a->argv[3], 0);
	if (c) {
		faclist = a->argc > 4 && !ast_strlen_zero(a->argv[4]) ? a->argv[4] : ast_strdupa(S_OR(c->routes, ""));
		callerfrl = a->argc > 5 && !ast_strlen_zero(a->argv[5]) ? atoi(a->argv[5]) : callerfrl;
	}
	AST_RWLIST_UNLOCK(&ccsas);
	if (!c) {
		ast_cli(a->fd, "No such route: '%s'\n", a->argv[3]);
		return CLI_FAILURE;
	}

	ccsa_run(NULL, a->fd, NULL, a->argv[3], faclist, NULL, 0, cbq, ohq, 1, 0, callerfrl, 0, NULL);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static char *cli_show_calls(int fd, const char *facname, int show_active, int show_queued)
{
	struct ccsa_call *call;
	int total = 0;
	int now = (int) time(NULL);

	if (facname) {
		if (!find_route(facname, 1)) {
			ast_cli(fd, "No such route: %s\n", facname);
			return CLI_FAILURE;
		}
	}

	AST_RWLIST_RDLOCK(&calls);
	AST_LIST_TRAVERSE(&calls, call, entry) {
		int diff, hr, min, sec;
		if (!ast_strlen_zero(facname) && strcmp(facname, call->route)) {
			continue; /* Doesn't match filter */
		}
		if (!total++) {
			ast_cli(fd, "%10s %15s %-30s %-6s %8s %4s %4s %-10s %s\n", "Caller", "Called No.", "Route", "Status", "Duration", "MLPP", "QPri", "CBQ Ext.", "Channel");
		}

		/* Calculate duration */
		diff = now - call->start;
		hr = diff / 3600;
		min = (diff % 3600) / 60;
		sec = diff % 60;

		ast_cli(fd, "%10s %15s %-30s %-6s %02d:%02d:%02d %4c %4c %10s %s\n",
			call->caller, call->called, call->route, call->active ? "Active" : call->cbq ? "CBQ" : "OHQ", hr, min, sec, isprint(call->call_priority) ? call->call_priority : ' ', !call->active ? '0' + call->queue_priority : '-', S_OR(call->cbqexten, ""), call->channel);
	}
	AST_RWLIST_UNLOCK(&calls);

	if (!total) {
		ast_cli(fd, "No calls\n");
	}

	return CLI_SUCCESS;
}

static char *handle_show_calls(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	const char *facname = NULL;

	switch(cmd) {
	case CLI_INIT:
		e->command = "ccsa show calls";
		e->usage =
			"Usage: ccsa show calls [<route>]\n"
			"       Lists all CCSA calls, active and queued.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc < 3) {
		return CLI_SHOWUSAGE;
	}

	facname = a->argc > 3 && !ast_strlen_zero(a->argv[3]) ? a->argv[3] : NULL;
	return cli_show_calls(a->fd, facname, 1, 1);
}

static char *handle_show_active(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	const char *facname = NULL;

	switch(cmd) {
	case CLI_INIT:
		e->command = "ccsa show active";
		e->usage =
			"Usage: ccsa show active [<route>]\n"
			"       Lists all active CCSA calls.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc < 3) {
		return CLI_SHOWUSAGE;
	}

	facname = a->argc > 3 && !ast_strlen_zero(a->argv[3]) ? a->argv[3] : NULL;
	return cli_show_calls(a->fd, facname, 1, 0);
}

static char *handle_show_queued(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	const char *facname = NULL;

	switch(cmd) {
	case CLI_INIT:
		e->command = "ccsa show queued";
		e->usage =
			"Usage: ccsa show queued [<route>]\n"
			"       Lists all queued CCSA calls.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc < 3) {
		return CLI_SHOWUSAGE;
	}

	facname = a->argc > 3 && !ast_strlen_zero(a->argv[3]) ? a->argv[3] : NULL;
	return cli_show_calls(a->fd, facname, 0, 1);
}

static char *handle_preempt_facility(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	int res;
	const char *facility, *priority;

	switch(cmd) {
	case CLI_INIT:
		e->command = "ccsa preempt facility";
		e->usage =
			"Usage: ccsa preempt facility <facility> [<MLPP>]\n"
			"       Preempt a call on a facility.\n"
			"       Note that this command requires a facility"
			"       name, not a route name.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc < 4) {
		return CLI_SHOWUSAGE;
	}

	facility = a->argv[3];
	priority = a->argc > 4 && !ast_strlen_zero(a->argv[4]) ? a->argv[4] : NULL;
	if (!priority) {
		ast_cli(a->fd, "Assuming MLPP priority 'D'\n"); /* Can't assume routine, otherwise we couldn't preempt anything. */
	} else if (*priority < 'A' || *priority > 'D') {
		ast_cli(a->fd, "Invalid MLPP priority: '%c'\n", isprint(*priority) ? *priority : '?');
		return CLI_FAILURE;
	}
	res = try_facility_preempt(facility, priority ? *priority : 'D');
	if (res < 0) {
		ast_cli(a->fd, "No calls available to preempt.\n");
	} else if (!res) {
		ast_cli(a->fd, "Not authorized to preempt any calls.\n");
	} else {
		ast_cli(a->fd, "Successfully preempted a call.\n");
	}
	return CLI_SUCCESS;
}

static char *handle_cancel_cbq(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	const char *caller;

	switch(cmd) {
	case CLI_INIT:
		e->command = "ccsa cancel cbq";
		e->usage =
			"Usage: ccsa cancel cbq <caller|all>\n"
			"       Cancel a pending Call Back Queued call.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc < 4) {
		return CLI_SHOWUSAGE;
	}

	caller = a->argv[3];
	if (!strcasecmp(a->argv[3], "all")) {
		caller = NULL;
	}

	if (!queue_cancel_cbq(a->fd, caller)) {
		ast_cli(a->fd, "No Call Back Queue calls pending.\n"); /* Nothing to cancel. */
	}

	return CLI_SUCCESS;
}

static char *handle_show_routes(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-12s\n"
#define FORMAT2 "%-12s\n"
	struct route *f;

	switch(cmd) {
	case CLI_INIT:
		e->command = "ccsa show routes";
		e->usage =
			"Usage: ccsa show routes\n"
			"       Lists all routes.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	ast_cli(a->fd, FORMAT, "Name");
	ast_cli(a->fd, FORMAT, "------------");
	AST_RWLIST_RDLOCK(&routes);
	AST_LIST_TRAVERSE(&routes, f, entry) {
		ast_cli(a->fd, FORMAT2, f->name);
	}
	AST_RWLIST_UNLOCK(&routes);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static char *handle_show_route(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-32s : %s\n"
#define FORMAT2 "%-32s : %d\n"
	struct route *f;
	int which = 0;
	char *ret = NULL;

	switch(cmd) {
	case CLI_INIT:
		e->command = "ccsa show route";
		e->usage =
			"Usage: ccsa show route <name>\n"
			"       Displays information about a route.\n";
		return NULL;
	case CLI_GENERATE:
		if (a->pos != 3) {
			return NULL;
		}
		AST_LIST_TRAVERSE(&routes, f, entry) {
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

	AST_RWLIST_RDLOCK(&routes);
	AST_LIST_TRAVERSE(&routes, f, entry) {
		if (!strcasecmp(a->argv[3], f->name)) {
			ast_cli(a->fd, FORMAT, "Name", f->name);
			ast_cli(a->fd, FORMAT, "Facility Name", f->facility);
			ast_cli(a->fd, FORMAT, "Facility Type", facility_type_str(f->factype));
			ast_cli(a->fd, FORMAT, "Dial String", f->dialstr);
			ast_cli(a->fd, FORMAT, "More Expensive Route", AST_CLI_YESNO(f->mer));
			ast_cli(a->fd, FORMAT, "Busy is Congestion", AST_CLI_YESNO(f->busyiscongestion));
			ast_cli(a->fd, FORMAT2, "Minimum FRL Req.", f->frl);
			ast_cli(a->fd, FORMAT, "Time Restrictions", f->time);
			ast_cli(a->fd, FORMAT2, "Threshold", f->threshold);
			ast_cli(a->fd, FORMAT2, "Max Limit", f->limit);
			break;
		}
	}
	AST_RWLIST_UNLOCK(&routes);
	if (!f) {
		ast_cli(a->fd, "No such route: '%s'\n", a->argv[3]);
	}

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static char *handle_show_ccsas(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-12s\n"
#define FORMAT2 "%-12s\n"
	struct ccsa *c;

	switch(cmd) {
	case CLI_INIT:
		e->command = "ccsa show ccsas";
		e->usage =
			"Usage: ccsa show ccsas\n"
			"       Lists all CCSAs.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	ast_cli(a->fd, FORMAT, "Name");
	ast_cli(a->fd, FORMAT, "------------");
	AST_RWLIST_RDLOCK(&ccsas);
	AST_LIST_TRAVERSE(&ccsas, c, entry) {
		ast_cli(a->fd, FORMAT2, c->name);
	}
	AST_RWLIST_UNLOCK(&ccsas);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static char *handle_show_ccsa(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-32s : %s\n"
#define FORMAT2 "%-32s : %f\n"
#define FORMAT3 "%-32s : %u\n"
	struct ccsa *c;
	int which = 0;
	char *ret = NULL;

	switch(cmd) {
	case CLI_INIT:
		e->command = "ccsa show ccsa";
		e->usage =
			"Usage: ccsa show ccsa <name>\n"
			"       Displays information about a CCSA.\n";
		return NULL;
	case CLI_GENERATE:
		if (a->pos != 3) {
			return NULL;
		}
		AST_LIST_TRAVERSE(&ccsas, c, entry) {
			if (!strncasecmp(a->word, c->name, strlen(a->word)) && ++which > a->n) {
				ret = ast_strdup(c->name);
				break;
			}
		}

		return ret;
	}

	if (a->argc != 4) {
		return CLI_SHOWUSAGE;
	}

	AST_RWLIST_RDLOCK(&ccsas);
	AST_LIST_TRAVERSE(&ccsas, c, entry) {
		if (!strcasecmp(a->argv[3], c->name)) {
			ast_cli(a->fd, FORMAT, "Name", c->name);
			ast_cli(a->fd, FORMAT, "MER Tone", AST_CLI_YESNO(c->mer_tone));
			ast_cli(a->fd, FORMAT, "FRL Upgrades", AST_CLI_YESNO(c->frl_allow_upgrade));
			ast_cli(a->fd, FORMAT, "Remote Auth", AST_CLI_YESNO(c->auth_code_remote_allowed));
			ast_cli(a->fd, FORMAT3, "Auth Code Length", c->auth_code_len);
			ast_cli(a->fd, FORMAT3, "Extension Length", c->extension_len);
			ast_cli(a->fd, FORMAT, "Auth Code Validation Context", S_OR(c->auth_sub_context, ""));
			ast_cli(a->fd, FORMAT, "Hold Announcement", S_OR(c->hold_announcement, ""));
			ast_cli(a->fd, FORMAT, "Extension Prompt", S_OR(c->extension_prompt, ""));
			ast_cli(a->fd, FORMAT3, "Queue Promo Timer", c->queue_promo_timer);
			ast_cli(a->fd, FORMAT3, "Route Advance Timer", c->route_advance_timer);
			ast_cli(a->fd, FORMAT, "Callback Caller Context", S_OR(c->callback_caller_context, ""));
			ast_cli(a->fd, FORMAT, "Callback Dest. Context", S_OR(c->callback_dest_context, ""));
			break;
		}
	}
	AST_RWLIST_UNLOCK(&ccsas);
	if (!c) {
		ast_cli(a->fd, "No such CCSA: '%s'\n", a->argv[3]);
	}

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static struct ast_cli_entry ccsa_cli[] = {
	AST_CLI_DEFINE(handle_show_ccsas, "Display list of CCSAs"),
	AST_CLI_DEFINE(handle_show_ccsa, "Displays information about a CCSA"),
	AST_CLI_DEFINE(handle_show_routes, "Display list of routes"),
	AST_CLI_DEFINE(handle_show_route, "Displays information about a route"),
	AST_CLI_DEFINE(handle_show_calls, "Displays list of all CCSA calls"),
	AST_CLI_DEFINE(handle_show_active, "Displays list of all active CCSA calls"),
	AST_CLI_DEFINE(handle_show_queued, "Displays list of all queued CCSA calls"),
	AST_CLI_DEFINE(handle_simulate_route, "Simulate routing of a CCSA call"),
	AST_CLI_DEFINE(handle_preempt_facility, "Try to preempt a call on a facility"),
	AST_CLI_DEFINE(handle_cancel_cbq, "Cancel pending CBQ calls"),
};

/*! \brief Reload application module */
static int ccsa_reload(int reload)
{
	char *cat = NULL;
	struct ccsa *c;
	struct route *f;
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

	AST_RWLIST_WRLOCK(&ccsas);
	AST_RWLIST_WRLOCK(&routes);

	/* Reset Global Var Values (currently none) */

	/* General section */

	/* Remaining sections */
	while ((cat = ast_category_browse(cfg, cat))) {
		const char *type;
		int new = 0;

		if (!strcasecmp(cat, "general")) {
			reset_cdr_var_names();
			var = ast_variable_browse(cfg, cat);
			while (var) {
				if (!strcasecmp(var->name, "cdrvar_frl") && !ast_strlen_zero(var->value)) {
					ast_copy_string(cdrvar_frl, var->value, sizeof(cdrvar_frl));
				} else if (!strcasecmp(var->name, "cdrvar_frl_req") && !ast_strlen_zero(var->value)) {
					ast_copy_string(cdrvar_frl_req, var->value, sizeof(cdrvar_frl_req));
				} else if (!strcasecmp(var->name, "cdrvar_frl_eff") && !ast_strlen_zero(var->value)) {
					ast_copy_string(cdrvar_frl_eff, var->value, sizeof(cdrvar_frl_eff));
				} else if (!strcasecmp(var->name, "cdrvar_aiod") && !ast_strlen_zero(var->value)) {
					ast_copy_string(cdrvar_aiod, var->value, sizeof(cdrvar_aiod));
				} else if (!strcasecmp(var->name, "cdrvar_mlpp") && !ast_strlen_zero(var->value)) {
					ast_copy_string(cdrvar_mlpp, var->value, sizeof(cdrvar_mlpp));
				} else if (!strcasecmp(var->name, "cdrvar_authcode") && !ast_strlen_zero(var->value)) {
					ast_copy_string(cdrvar_authcode, var->value, sizeof(cdrvar_authcode));
				} else if (!strcasecmp(var->name, "cdrvar_facility") && !ast_strlen_zero(var->value)) {
					ast_copy_string(cdrvar_facility, var->value, sizeof(cdrvar_facility));
				} else if (!strcasecmp(var->name, "cdrvar_route") && !ast_strlen_zero(var->value)) {
					ast_copy_string(cdrvar_route, var->value, sizeof(cdrvar_route));
				} else if (!strcasecmp(var->name, "cdrvar_queuestart") && !ast_strlen_zero(var->value)) {
					ast_copy_string(cdrvar_queuestart, var->value, sizeof(cdrvar_queuestart));
				} else if (!strcasecmp(var->name, "cdrvar_digits") && !ast_strlen_zero(var->value)) {
					ast_copy_string(cdrvar_digits, var->value, sizeof(cdrvar_digits));
				} else {
					ast_log(LOG_WARNING, "Unknown keyword in profile '%s': %s at line %d of %s\n", var->name, var->name, var->lineno, CONFIG_FILE);
				}
				var = var->next;
			}
			continue;
		}
		if (!(type = ast_variable_retrieve(cfg, cat, "type"))) {
			ast_log(LOG_WARNING, "Invalid entry in %s: %s defined with no type!\n", CONFIG_FILE, cat);
			continue;
		}

		if (!strcasecmp(type, "route")) {
			enum facility_type factype;
			const char *factypestr, *dialstr;

			if (!(factypestr = ast_variable_retrieve(cfg, cat, "facility_type"))) {
				ast_log(LOG_WARNING, "Invalid entry in %s: %s defined with no facility type!\n", CONFIG_FILE, cat);
				continue;
			}
			factype = facility_from_str(factypestr);
			if (factype == FACILITY_UNKNOWN) {
				ast_log(LOG_WARNING, "Unknown facility type: %s\n", factypestr);
				continue;
			}

			if (!(dialstr = ast_variable_retrieve(cfg, cat, "dialstr"))) {
				ast_log(LOG_WARNING, "Invalid entry in %s: %s defined with no dial string!\n", CONFIG_FILE, cat);
				continue;
			}

			f = find_route(cat, 0);

			if (!f) {
				f = alloc_route(cat);
				new = 1;
			}
			if (!f) {
				ast_log(LOG_WARNING, "Failed to create route profile for '%s'\n", cat);
				continue;
			}
			if (!new) {
				ast_mutex_lock(&f->lock);
			}
			/* Re-initialize the defaults (none currently) */

			f->factype = factype;
			ast_copy_string(f->dialstr, dialstr, sizeof(f->dialstr));

			var = ast_variable_browse(cfg, cat);
			while (var) {
				if (!strcasecmp(var->name, "type") || !strcasecmp(var->name, "facility_type") || !strcasecmp(var->name, "dialstr")) {
					var = var->next;
					continue;
				}

				if (!strcasecmp(var->name, "facility") && !ast_strlen_zero(var->value)) {
					ast_copy_string(f->facility, var->value, sizeof(f->facility));
				} else if (!strcasecmp(var->name, "mer") && !ast_strlen_zero(var->value)) {
					f->mer = ast_true(var->value);
				} else if (!strcasecmp(var->name, "busyiscongestion") && !ast_strlen_zero(var->value)) {
					f->busyiscongestion = ast_true(var->value);
				} else if (!strcasecmp(var->name, "frl") && !ast_strlen_zero(var->value)) {
					f->frl = atoi(var->value);
				} else if (!strcasecmp(var->name, "aiod") && !ast_strlen_zero(var->value)) {
					ast_copy_string(f->aiod, var->value, sizeof(f->aiod));
				} else if (!strcasecmp(var->name, "time") && !ast_strlen_zero(var->value)) {
					ast_copy_string(f->time, var->value, sizeof(f->time));
				} else if (!strcasecmp(var->name, "threshold") && !ast_strlen_zero(var->value)) {
					f->threshold = atoi(var->value);
				} else if (!strcasecmp(var->name, "limit") && !ast_strlen_zero(var->value)) {
					f->limit = atoi(var->value);
				} else if (!strcasecmp(var->name, "devstate")) {
					f->devstate = ast_strdup(var->value);
				} else {
					ast_log(LOG_WARNING, "Unknown keyword in profile '%s': %s at line %d of %s\n", var->name, var->name, var->lineno, CONFIG_FILE);
				}
				var = var->next;
			} /* End while(var) loop */

			if (!new) {
				ast_mutex_unlock(&f->lock);
			} else {
				AST_RWLIST_INSERT_HEAD(&routes, f, entry);
			}
		} else if (!strcasecmp(type, "ccsa")) {
			char routes[PATH_MAX] = "";
			char *facptr = routes;
			c = find_ccsa(cat, 0);
			if (!c) {
				c = alloc_ccsa(cat);
				new = 1;
			}
			if (!c) {
				ast_log(LOG_WARNING, "Failed to create CCSA profile for '%s'\n", cat);
				continue;
			}
			if (!new) {
				ast_mutex_lock(&c->lock);
			}
			/* Re-initialize the defaults */
			c->mer_tone = 1;
			c->auth_code_len = 6;
			c->extension_len = 4;

			/* Search Config */
			var = ast_variable_browse(cfg, cat);
			while (var) {
				if (!strcasecmp(var->name, "type")) {
					var = var->next;
					continue;
				}
				if (!strcasecmp(var->name, "route")) {
					int addfac = snprintf(facptr, sizeof(routes) - (facptr - routes), "%s%s", facptr > routes ? "|" : "", var->value);
					if (addfac > (sizeof(routes) - (facptr - routes))) {
						ast_log(LOG_WARNING, "Too many routes in route list: truncation has occured\n");
					}
					facptr += addfac;
				} else if (!strcasecmp(var->name, "auth_code_len") && !ast_strlen_zero(var->value)) {
					c->frl_allow_upgrade = atoi(var->value);
				} else if (!strcasecmp(var->name, "extension_len") && !ast_strlen_zero(var->value)) {
					c->extension_len = atoi(var->value);
				} else if (!strcasecmp(var->name, "mer_tone") && !ast_strlen_zero(var->value)) {
					c->mer_tone = ast_true(var->value);
				} else if (!strcasecmp(var->name, "frl_allow_upgrade") && !ast_strlen_zero(var->value)) {
					c->frl_allow_upgrade = ast_true(var->value);
				} else if (!strcasecmp(var->name, "auth_code_remote_allowed") && !ast_strlen_zero(var->value)) {
					c->auth_code_remote_allowed = ast_true(var->value);
				} else if (!strcasecmp(var->name, "auth_sub_context") && !ast_strlen_zero(var->value)) {
					ast_copy_string(c->auth_sub_context, var->value, sizeof(c->auth_sub_context));
				} else if (!strcasecmp(var->name, "hold_announcement") && !ast_strlen_zero(var->value)) {
					ast_copy_string(c->hold_announcement, var->value, sizeof(c->hold_announcement));
				} else if (!strcasecmp(var->name, "extension_prompt") && !ast_strlen_zero(var->value)) {
					ast_copy_string(c->extension_prompt, var->value, sizeof(c->extension_prompt));
				} else if (!strcasecmp(var->name, "queue_promo_timer") && !ast_strlen_zero(var->value)) {
					c->queue_promo_timer = atoi(var->value);
				} else if (!strcasecmp(var->name, "route_advance_timer") && !ast_strlen_zero(var->value)) {
					c->route_advance_timer = atoi(var->value);
				} else if (!strcasecmp(var->name, "callback_caller_context") && !ast_strlen_zero(var->value)) {
					ast_copy_string(c->callback_caller_context, var->value, sizeof(c->callback_caller_context));
				} else if (!strcasecmp(var->name, "callback_dest_context") && !ast_strlen_zero(var->value)) {
					ast_copy_string(c->callback_dest_context, var->value, sizeof(c->callback_dest_context));
				} else {
					ast_log(LOG_WARNING, "Unknown keyword in profile '%s': %s at line %d of %s\n", var->name, var->name, var->lineno, CONFIG_FILE);
				}
				var = var->next;
			} /* End while(var) loop */

			ast_copy_string(c->routes, routes, sizeof(c->routes));

			if (!new) {
				ast_mutex_unlock(&c->lock);
			} else {
				AST_RWLIST_INSERT_HEAD(&ccsas, c, entry);
			}
		} else {
			ast_log(LOG_WARNING, "Unknown type: '%s'\n", type);
		}
	}

	ast_config_destroy(cfg);
	AST_RWLIST_UNLOCK(&routes);
	AST_RWLIST_UNLOCK(&ccsas);

	return 0;
}

static int unload_module(void)
{
	struct ccsa *c;
	struct route *f;
	struct ccsa_call *call;

	/* Signal any CBQ threads to abort now.
	 * We don't care about OHQ calls or active calls, because this module
	 * can't be unloaded if there are calls in it anyways. */
	queue_cancel_cbq(-1, NULL); /* Cancel all of them. */

	ast_unregister_application(app);
	ast_cli_unregister_multiple(ccsa_cli, ARRAY_LEN(ccsa_cli));

	AST_RWLIST_WRLOCK(&ccsas);
	while ((c = AST_RWLIST_REMOVE_HEAD(&ccsas, entry))) {
		ast_free(c);
	}
	AST_RWLIST_UNLOCK(&ccsas);

	AST_RWLIST_WRLOCK(&routes);
	while ((f = AST_RWLIST_REMOVE_HEAD(&routes, entry))) {
		if (f->devstate) {
			ast_free(f->devstate);
		}
		ast_free(f);
	}
	AST_RWLIST_UNLOCK(&routes);

	AST_RWLIST_WRLOCK(&calls);
	while ((call = AST_RWLIST_REMOVE_HEAD(&calls, entry))) {
		call_free(call, 0);
	}
	AST_RWLIST_UNLOCK(&calls);

	return 0;
}

static int load_module(void)
{
	int res;

	if (ccsa_reload(0)) {
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_cli_register_multiple(ccsa_cli, ARRAY_LEN(ccsa_cli));

	res = ast_register_application_xml(app, ccsa_exec);

	return res;
}

static int reload(void)
{
	return ccsa_reload(1);
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Common Control Switching Arrangements",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.reload = reload,
	.requires = "app_dtmfstore",
);
