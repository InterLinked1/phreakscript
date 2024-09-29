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
 * \brief Callback Monitor Application
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<depend>func_query</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/format_cache.h"
#include "asterisk/cli.h"
#include "asterisk/causes.h"

/*** DOCUMENTATION
	<application name="RequestCallback" language="en_US">
		<synopsis>
			Requests a callback to a local or remote destination.
		</synopsis>
		<syntax>
			<parameter name="callbackcaller">
				<para>Dialplan context to ring back caller in if callback succeeds.</para>
				<para>Extension is the callback number.</para>
			</parameter>
			<parameter name="callbackwatched">
				<para>Dialplan context to ring watched number in if callback succeeds.</para>
				<para>Extension is the callback number.</para>
			</parameter>
			<parameter name="localdevicestate">
				<para>Context providing local device state.</para>
			</parameter>
			<parameter name="remotedialcontext">
				<para>Context providing remote device state. Will be dialed using a Local channel. Use Dial in this context if needed.</para>
				<para>This will use the <literal>TEXT_QUERY</literal> function under the hood.</para>
				<para>The remote endpoint needs to send a textual transfer of the queried
				device state, e.g. <literal>SendText(${DEVICE_STATE(MYDEVICE)})</literal>.</para>
			</parameter>
			<parameter name="number">
				<para>The globally unique number to attempt callback against.</para>
			</parameter>
			<parameter name="caller">
				<para>The globally unique number performing the callback.</para>
				<para>Default is CALLERID(num).</para>
			</parameter>
			<parameter name="timeout">
				<para>Time, in seconds, to wait for callback completion.</para>
				<para>Default is 1800 (30 minutes).</para>
			</parameter>
			<parameter name="ringtime">
				<para>Time, in seconds, to wait for caller to answer on a successful callback.</para>
				<para>Default is 30 seconds.</para>
			</parameter>
			<parameter name="poll">
				<para>Polling interval of device state. An override will apply to both local and remote intervals.
				To specify both local and remote interval, delimit using a pipe.</para>
				<para>Default is 5 seconds for local destinations and 30 seconds for remote destinations.</para>
				<para>WARNING: A low interval for remote polling could significantly increase network traffic.</para>
			</parameter>
			<parameter name="tagname">
				<para>An optional tag name to specify the type of service. This allows callbacks for different services
				to be treated separately.</para>
				<para>The s option as well as cancel requests only apply to callbacks with the same tag (including the default "empty" tag).</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="l">
						<para>Only initiate callback if the local caller is also idle.</para>
						<para>If this option is not provided and the caller is busy, the callback will complete successfully
						even if the caller is unable to receive it, which may not be desirable. Specifying this option
						ensures the caller is called back only if both parties are free. This option should not be used
						if Call Waiting can be used.</para>
					</option>
					<option name="s">
						<para>Limit a caller to only a single callback at a time, regardless of destination.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Request a callback to a local or remote destination.</para>
			<para>This can be used to easily implement features such as Busy Redial
			(a.k.a. Repeat Dialing and Continuous Redial) and Last Call Return.</para>
			<variablelist>
				<variable name="CALLBACK_REQUEST_STATUS">
					<para>This indicates the result of the callback origination.</para>
					<value name="FAILURE">
						Failure occured.
					</value>
					<value name="QUEUED">
						Destination currently busy, but a callback has been queued. This is the first callback.
					</value>
					<value name="ANOTHER">
						Destination currently busy, but a callback has been queued. This is not the first callback.
					</value>
					<value name="IDLE">
						Destination currently idle, no callback queued but call may proceed.
					</value>
					<value name="DUPLICATE">
						Callback to this destination already exists. No callback queued.
					</value>
					<value name="ALREADY">
						Callback to a destination (but not the requested one) already exists. No callback queued.
					</value>
					<value name="UNSUPPORTED">
						Callback to a destination is not possible (no route available for busy determination).
					</value>
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">CancelCallback</ref>
		</see-also>
	</application>
	<application name="CancelCallback" language="en_US">
		<synopsis>
			Cancels one or more in-progress queued callbacks
		</synopsis>
		<syntax>
			<parameter name="caller">
				<para>The globally unique number performing the callback.</para>
				<para>Default is CALLERID(num).</para>
			</parameter>
			<parameter name="tag">
				<para>Optional tag name. Only callbacks with the specified tag will be cancelled.</para>
				<para>If not provided, only callbacks with no tag name will be cancelled.</para>
			</parameter>
		</syntax>
		<description>
			<para>Cancels all callbacks currently queued for a caller.</para>
			<variablelist>
				<variable name="CALLBACK_CANCEL_STATUS">
					<para>This indicates the result of the callback cancellation.</para>
					<value name="FAILURE">
						No callbacks could be cancelled.
					</value>
					<value name="SUCCESS">
						All callbacks by this caller (with matching tag) cancelled.
					</value>
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">RequestCallback</ref>
		</see-also>
	</application>
 ***/

static char *app = "RequestCallback";
static char *app2 = "CancelCallback";

struct callback_monitor_item {
	ast_mutex_t lock;
	pthread_t thread;
	char number[AST_MAX_EXTENSION];
	char caller[AST_MAX_EXTENSION];
	int watch_start;
	int remaining;
	int timeout_ms;
	int ringtime;
	int poll_local;
	int poll_remote;
	char *localstate;
	char *remotedialcontext;
	char *callbackcaller;
	char *callbackwatched;
	char *tagname;
	unsigned int require_local_idle:1;
	unsigned int cancel:1;
	AST_RWLIST_ENTRY(callback_monitor_item) entry;		/*!< Next record */
};

static AST_RWLIST_HEAD_STATIC(callbacks, callback_monitor_item);

/*! \brief Allocate and initialize callback */
static struct callback_monitor_item *alloc_callback(const char *caller, const char *number)
{
	struct callback_monitor_item *cb;

	if (!(cb = ast_calloc(1, sizeof(*cb)))) {
		return NULL;
	}

	ast_mutex_init(&cb->lock);
	if (!ast_strlen_zero(caller)) {
		ast_copy_string(cb->caller, caller, sizeof(cb->caller));
	} else {
		*cb->caller = '\0';
	}
	if (!ast_strlen_zero(number)) {
		ast_copy_string(cb->number, number, sizeof(cb->number));
	} else {
		*cb->number = '\0';
	}

	cb->watch_start = (int) time(NULL);

	return cb;
}

#define free_if_exists(ptr) if (ptr) ast_free(ptr);

static void callback_free(struct callback_monitor_item *cb)
{
	/* Don't free anything that's NULL. */
	free_if_exists(cb->localstate);
	free_if_exists(cb->remotedialcontext);
	free_if_exists(cb->callbackcaller);
	free_if_exists(cb->callbackwatched);
	free_if_exists(cb->tagname);
	ast_free(cb);
}

static int local_endpoint_busy(const char *endpoints, const char *number)
{
	int res = -1;
	char *device, *devices = ast_strdupa(endpoints);

	if (ast_strlen_zero(endpoints)) {
		/* Should really only happen if require_local_idle */
		ast_log(LOG_WARNING, "Missing endpoint to determine local device status of %s\n", number);
		return res;
	}

	while ((device = strsep(&devices, "&"))) {
		enum ast_device_state devstate = ast_device_state(device);
		ast_debug(2, "Device state of %s is %s\n", device, ast_devstate_str(devstate));
		if (devstate == AST_DEVICE_BUSY || devstate == AST_DEVICE_ONHOLD || devstate == AST_DEVICE_RINGINUSE || devstate == AST_DEVICE_RINGING || devstate == AST_DEVICE_INUSE) {
			/* Definitely busy */
			return -1;
		}
		if (devstate == AST_DEVICE_NOT_INUSE) {
			/* At least one endpoint needs to be free for the line to be considered idle. Otherwise, it's unavailable. */
			res = 0;
		}
	}

	return res;
}

static int remote_endpoint_busy(const char *number, const char *remotedialcontext, const char *caller, int timeout)
{
	struct ast_channel *chan;
	char devstate[32];
	int res = 0;
	char queryread[256];

	snprintf(queryread, sizeof(queryread), "TEXT_QUERY(Local/%s@%s,%d)", number, remotedialcontext, timeout);
	devstate[0] = '\0';

	/* Need a channel so that we can propogate our Caller ID */
	chan = ast_dummy_channel_alloc();

	if (!chan) {
		ast_log(LOG_WARNING, "Failed to allocate dummy channel\n");
		return -1;
	}

	/* Let the other end know who we are. */
	ast_set_callerid(chan, caller, NULL, NULL);

	if (ast_func_read(chan, queryread, devstate, sizeof(devstate))) {
		res = -1;
	}

	if (res) {
		/* Try to use the query result, otherwise fallback to using hangup cause. */
		const char *hangupcasestr;

		ast_channel_lock(chan);
		hangupcasestr = pbx_builtin_getvar_helper(chan, "HANGUPCAUSE");
		if (hangupcasestr) {
			int cause = atoi(hangupcasestr);

			ast_debug(1, "Hangup cause was %d\n", cause);

			switch (cause) {
			case AST_CAUSE_CALL_AWARDED_DELIVERED: /* 7 */
				res = 0;
				break; /* Available */
			case AST_CAUSE_CHANNEL_UNACCEPTABLE: /* 6 */
			case AST_CAUSE_USER_BUSY: /* 17 */
				res = -1; /* Not available */
				break;
			default:
				ast_log(LOG_WARNING, "Failed to query remote device state, and unexpected hangup cause code: %d\n", cause);
			}
		} else {
			ast_log(LOG_WARNING, "Failed to query remote device state, and no hangup cause available\n");
		}
		ast_channel_unlock(chan);
	} else {
		ast_debug(2, "Device state queried at Local/%s@%s returned %s\n", number, remotedialcontext, devstate);
		if (strcmp(devstate, "NOT_INUSE")) {
			res = -1; /* If not NOT_INUSE, then busy. */
		}
	}

	ast_channel_unref(chan);

	return res;
}

static void *callback_monitor(void *data)
{
	char endpoints[256];
	char callerhint[256];
	int remote, poll_ms = 0;
	struct callback_monitor_item *cb2 = NULL, *cb = data;
	struct timeval start, pollstart;
	int timeout;

	if (!cb) {
		ast_log(LOG_WARNING, "Missing data\n");
		return NULL;
	}

	/* Determine if the endpoint is local or not. */
	remote = ast_get_hint(endpoints, sizeof(endpoints), NULL, 0, NULL, cb->localstate, cb->number) ? 0 : 1; /* chan is NULL, there isn't one, and we don't need it. */
	poll_ms = remote ? cb->poll_remote : cb->poll_local;

	start = ast_tvnow();
	pollstart = ast_tvnow();

	if (cb->require_local_idle) {
		callerhint[0] = '\0';
		if (!ast_get_hint(callerhint, sizeof(callerhint), NULL, 0, NULL, cb->localstate, cb->caller)) {
			ast_log(LOG_WARNING, "Couldn't find hint for %s\n", cb->caller);
		}
	}

	ast_verb(3, "Callback activated for %s by %s for %d minutes\n", cb->number, cb->caller, cb->timeout_ms / 60000);

	timeout = 4;
	if (poll_ms <= timeout * 1000) {
		timeout = 2;
		if (poll_ms <= timeout * 1000) {
			ast_log(LOG_WARNING, "Poll timeout %d is too short.\n", poll_ms);
		}
	}

	while ((cb->remaining = ast_remaining_ms(start, cb->timeout_ms)) > 0) {
		usleep(250000); /* sleep for 250ms */
		if (cb->cancel) { /* Thread was cancelled. */
			ast_debug(1, "Callback %s/%s is ending early\n", cb->caller, cb->number);
			break;
		}
		if (ast_remaining_ms(pollstart, poll_ms) <= 0) {
			ast_debug(2, "Polling availability of %s...\n", cb->number);
			if ((!remote && !local_endpoint_busy(endpoints, cb->number)) || (remote && !remote_endpoint_busy(cb->number, cb->remotedialcontext, cb->caller, timeout))) {
				if (cb->require_local_idle && local_endpoint_busy(callerhint, cb->caller)) {
					ast_debug(1, "%s is now free, but caller (%s) is not, delaying callback...\n", cb->number, cb->caller);
				} else {
					char dialbuf[256];
					struct ast_format_cap *cap;
					int outgoing_status = 0;
					ast_verb(3, "Destination %s is now idle! Queuing callback for %s\n", cb->number, cb->caller);
					/* Async originate call to caller. If/when answered, ring watched number. */
					/* Caller ID is that of the watched number. Anything else just doesn't make sense. */
					/* As for Caller ID Name, dunno, but try to be informative with "CALLBACK", so user knows. */
					cap = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT);
					if (!cap) {
						ast_log(LOG_WARNING, "Failed to allocate capabilities\n");
						break;
					}
					ast_format_cap_append(cap, ast_format_slin, 0);
					snprintf(dialbuf, sizeof(dialbuf), "%s@%s", cb->caller, cb->callbackcaller);
					ast_pbx_outgoing_exten("Local", cap, dialbuf, cb->ringtime * 1000,
						cb->callbackwatched, cb->number, 1,
						&outgoing_status, AST_OUTGOING_NO_WAIT, cb->number, "CALLBACK", NULL, NULL, NULL, 0, NULL);
					ao2_cleanup(cap);
					break; /* Our work here is done. */
				}
			}
			pollstart = ast_tvnow();
		}
	}

	if (cb->remaining <= 0) {
		/* Somebody's a chatty Kathy... */
		ast_log(LOG_NOTICE, "Callback request from %s to %s expired without completion\n", cb->caller, cb->number);
	}

	ast_debug(1, "Callback monitor from %s to %s is terminating\n", cb->caller, cb->number);

	if (!cb->cancel) { /* If we were cancelled, the list is already locked by someone traversing it, and locking would lead to deadlock since they're waiting for us to die. */
		AST_RWLIST_WRLOCK(&callbacks);
		/* Look for an existing one */
		AST_RWLIST_TRAVERSE_SAFE_BEGIN(&callbacks, cb2, entry) {
			if (cb == cb2) { /* Remove this callback from the list, if we find it. */
				AST_RWLIST_REMOVE_CURRENT(entry);
				break;
			}
		}
		AST_RWLIST_TRAVERSE_SAFE_END;
		AST_RWLIST_UNLOCK(&callbacks);
	}

	if (cb2) {
		ast_debug(3, "Removed entry from list ourselves.\n");
	}

	ast_assert(cb->cancel || cb2 != NULL); /* We better have found the entry we were using in the list, if we really exited of our own volition. */

	callback_free(cb);
	return NULL;
}

#define SET_STATUS "CALLBACK_REQUEST_STATUS"
#define CANCEL_STATUS "CALLBACK_CANCEL_STATUS"

static void cancel_thread(struct callback_monitor_item *cb, int join)
{
	pthread_t thread = cb->thread;

	ast_debug(3, "Instructing callback thread %lu to exit\n", (long) thread);

	ast_mutex_lock(&cb->lock);
	cb->cancel = 1;
	/* Let thread exit of its own volition */
	/* i.e. no: pthread_cancel(thread); */
	/* i.e. no: pthread_kill(thread, SIGURG); */
	ast_mutex_unlock(&cb->lock);
	if (join) {
		/*! \todo join can cause a segfault on module unload/refresh, but why??? */
		pthread_join(thread, NULL); /* Thread will now free cb and clean up. */
	}
	ast_debug(3, "Thread %ld has exited\n", (long) thread);
}

static int cancel_exec(struct ast_channel *chan, const char *data)
{
	const char *caller = NULL, *tagname = NULL;
	struct callback_monitor_item *cb;
	int success = 0;
	char *appdata;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(caller);
		AST_APP_ARG(tagname);
	);

	if (!ast_strlen_zero(data)) {
		appdata = ast_strdupa(data);
		AST_STANDARD_APP_ARGS(args, appdata);
		caller = args.caller;
		tagname = args.tagname;
	}

	caller = !ast_strlen_zero(caller) ? caller : S_OR(ast_channel_caller(chan)->id.number.str, "");

	AST_RWLIST_WRLOCK(&callbacks);
	/* Look for an existing one */
	AST_RWLIST_TRAVERSE_SAFE_BEGIN(&callbacks, cb, entry) {
		if (!strcmp(cb->caller, caller)) { /* Cancel any callbacks requested by the caller. */
			if ((ast_strlen_zero(cb->tagname) && ast_strlen_zero(tagname)) || (!ast_strlen_zero(cb->tagname) && !ast_strlen_zero(tagname) && !strcmp(cb->tagname, tagname))) {
				success = 1;
				ast_verb(3, "Cancelling callback from %s to %s\n", cb->caller, cb->number);
				AST_RWLIST_REMOVE_CURRENT (entry);
				cancel_thread(cb, 1);
			}
		}
	}
	AST_RWLIST_TRAVERSE_SAFE_END;
	AST_RWLIST_UNLOCK(&callbacks);

	pbx_builtin_setvar_helper(chan, CANCEL_STATUS, success ? "SUCCESS" : "FAILURE");

	return 0;
}

static int callback_exec(struct ast_channel *chan, const char *data)
{
	char endpoints[256];
	char *caller;
	int ringtime = 30, timeout_ms = 1800000, poll_local = 0, poll_remote = 0, single = 0, require_local_idle = 0;
	int remote, already_had = 0;
	char *appdata;
	struct callback_monitor_item *cb;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(callbackcaller);
		AST_APP_ARG(callbackwatched);
		AST_APP_ARG(localdevicestate);
		AST_APP_ARG(remotedialcontext);
		AST_APP_ARG(number);
		AST_APP_ARG(caller);
		AST_APP_ARG(timeout);
		AST_APP_ARG(ringtime);
		AST_APP_ARG(poll);
		AST_APP_ARG(tagname);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "Missing arguments\n");
		return -1;
	}

	appdata = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, appdata);

	if (ast_strlen_zero(args.callbackcaller) || ast_strlen_zero(args.callbackwatched) || ast_strlen_zero(args.localdevicestate) || ast_strlen_zero(args.remotedialcontext) || ast_strlen_zero(args.number)) {
		ast_log(LOG_WARNING, "Missing required arguments\n");
		return -1;
	}

	if (!ast_strlen_zero(args.timeout)) {
		ast_app_parse_timelen(args.timeout, &timeout_ms, TIMELEN_SECONDS);
	}
	if (!ast_strlen_zero(args.poll)) {
		char *pollremote = strchr(args.poll, '|');
		if (pollremote) {
			*pollremote = '\0';
			pollremote++;
			if (!ast_strlen_zero(pollremote)) {
				poll_remote = atoi(pollremote);
				if (poll_remote <= 0) {
					ast_log(LOG_WARNING, "Invalid remote poll interval: %d\n", poll_remote);
					poll_remote = 30;
				}
			}
		}
		poll_local = atoi(args.poll);
		if (!pollremote) {
			poll_remote = poll_local;
		}
		if (poll_local <= 0) {
			ast_log(LOG_WARNING, "Invalid local poll interval: %d\n", poll_local);
			poll_local = 5;
		}
	}
	if (!ast_strlen_zero(args.ringtime)) {
		ringtime = atoi(args.ringtime);
		if (ringtime <= 0) {
			ast_log(LOG_WARNING, "Invalid ring time: %d\n", ringtime);
			ringtime = 30;
		}
	}
	if (!ast_strlen_zero(args.options)) {
		single = strchr(args.options, 's') ? 1 : 0;
		require_local_idle = strchr(args.options, 'l') ? 1 : 0;
	}

	caller = !ast_strlen_zero(args.caller) ? args.caller : S_OR(ast_channel_caller(chan)->id.number.str, "");

	AST_RWLIST_WRLOCK(&callbacks);
	/* Look for an existing one */
	AST_RWLIST_TRAVERSE(&callbacks, cb, entry) {
		ast_debug(3, "Comparing %s with %s\n", cb->caller, caller);
		if ((ast_strlen_zero(cb->caller) && ast_strlen_zero(caller)) || !strcmp(cb->caller, caller)) {
			already_had++;
			if (!strcmp(cb->number, args.number)) {
				ast_verb(3, "Callback from %s to %s already pending\n", caller, args.number);
				pbx_builtin_setvar_helper(chan, SET_STATUS, "DUPLICATE");
				break;
			} else if (single) {
				ast_verb(3, "%s already has a callback pending (to %s)\n", caller, cb->number);
				pbx_builtin_setvar_helper(chan, SET_STATUS, "ALREADY");
				break;
			}
		}
	}

	if (!cb) { /* We're good to request a callback */
		cb = alloc_callback(caller, args.number);
		if (cb) {
			char tmpbuf[1]; /* Result not needed */
			cb->thread = AST_PTHREADT_NULL;
			cb->timeout_ms = timeout_ms;
			cb->ringtime = ringtime;
			cb->poll_local = poll_local ? poll_local * 1000 : 5000;
			cb->poll_remote = poll_remote ? poll_remote * 1000 : 30000;
			cb->require_local_idle = require_local_idle;
			cb->localstate = args.localdevicestate ? ast_strdup(args.localdevicestate) : NULL;
			cb->remotedialcontext = args.remotedialcontext ? ast_strdup(args.remotedialcontext) : NULL;
			cb->callbackcaller = args.callbackcaller ? ast_strdup(args.callbackcaller) : NULL;
			cb->callbackwatched = args.callbackwatched ? ast_strdup(args.callbackwatched) : NULL;
			cb->tagname = args.tagname ? ast_strdup(args.tagname) : NULL;

			remote = ast_get_hint(endpoints, sizeof(endpoints), NULL, 0, NULL, cb->localstate, cb->number) ? 0 : 1;
			/* Check if it's available now. */
			if (!remote && !local_endpoint_busy(endpoints, cb->number)) {
				ast_verb(3, "Destination %s is currently idle.\n", cb->number);
				pbx_builtin_setvar_helper(chan, SET_STATUS, "IDLE");
				/* The call can just complete directly now, no callback is necessary. */
				callback_free(cb);
			} else if (ast_get_extension_data(tmpbuf, sizeof(tmpbuf), chan, cb->remotedialcontext, cb->number, 1)) {
				ast_verb(3, "Can't determine status of destination %s.\n", cb->number);
				pbx_builtin_setvar_helper(chan, SET_STATUS, "UNSUPPORTED");
				/* Not a local endpoint, and no route to the remote status. */
				callback_free(cb);
			} else if (remote && !remote_endpoint_busy(cb->number, cb->remotedialcontext, cb->caller, 4)) {
				ast_verb(3, "Destination %s is currently idle.\n", cb->number);
				pbx_builtin_setvar_helper(chan, SET_STATUS, "IDLE");
				/* The call can just complete directly now, no callback is necessary. */
				callback_free(cb);
			} else if (ast_pthread_create_background(&cb->thread, NULL, callback_monitor, (void *) cb)) {
				ast_log(LOG_ERROR, "Unable to monitor for callback completion\n");
				callback_free(cb);
				pbx_builtin_setvar_helper(chan, SET_STATUS, "FAILURE");
			} else {
				AST_RWLIST_INSERT_TAIL(&callbacks, cb, entry);
				pbx_builtin_setvar_helper(chan, SET_STATUS, already_had ? "ANOTHER" : "QUEUED");
			}
		}
	}

	AST_RWLIST_UNLOCK(&callbacks);

	return 0;
}

static char *handle_show_callbacks(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct callback_monitor_item *cb;
	int i = 0;

	switch(cmd) {
	case CLI_INIT:
		e->command = "callback show all";
		e->usage =
			"Usage: callback show all\n"
			"       List all active callback requests.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	ast_cli(a->fd, "%4s | %15s | %15s | %14s | %14s\n", "#", "Caller No.", "Watched No.", "Time Elapsed", "Time Remaining");
	AST_RWLIST_RDLOCK(&callbacks);
	AST_RWLIST_TRAVERSE(&callbacks, cb, entry) {
		int elapsed = (int) time(NULL) - cb->watch_start;
		int remaining = cb->remaining / 1000;
		ast_cli(a->fd, "%4d | %15s | %15s | %11d:%02d | %11d:%02d\n", ++i, cb->caller, cb->number, elapsed / 60, elapsed % 60, remaining / 60, remaining % 60);
	}
	AST_RWLIST_UNLOCK(&callbacks);

	return CLI_SUCCESS;
}

static char *handle_cancel(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct callback_monitor_item *cb;
	int all = 0;

	switch(cmd) {
	case CLI_INIT:
		e->command = "callback cancel";
		e->usage =
			"Usage: callback cancel <caller|all>\n"
			"       Cancel some or all active callback requests.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	all = !strcasecmp(a->argv[2], "all") ? 1 : 0;

	AST_RWLIST_WRLOCK(&callbacks);
	AST_RWLIST_TRAVERSE_SAFE_BEGIN(&callbacks, cb, entry) {
		if (all || !strcmp(cb->caller, a->argv[2])) {
			ast_cli(a->fd, "Cancelling callback from %s to %s\n", cb->caller, cb->number);
			AST_RWLIST_REMOVE_CURRENT(entry);
			cancel_thread(cb, 1);
		}
	}
	AST_RWLIST_TRAVERSE_SAFE_END;
	AST_RWLIST_UNLOCK(&callbacks);

	return CLI_SUCCESS;
}

static struct ast_cli_entry callback_cli[] = {
	AST_CLI_DEFINE(handle_show_callbacks, "Display active callbacks"),
	AST_CLI_DEFINE(handle_cancel, "Cancel some or all active callbacks"),
};

static int unload_module(void)
{
	int res;
	struct callback_monitor_item *cb;

	res = ast_unregister_application(app);
	res |= ast_unregister_application(app2);
	ast_cli_unregister_multiple(callback_cli, ARRAY_LEN(callback_cli));

	AST_RWLIST_WRLOCK(&callbacks);
	while ((cb = AST_RWLIST_REMOVE_HEAD(&callbacks, entry))) {
		cancel_thread(cb, 0); /* Thread will free cb itself. */
	}
	AST_RWLIST_UNLOCK(&callbacks);

	return res;
}

static int load_module(void)
{
	int res;

	ast_cli_register_multiple(callback_cli, ARRAY_LEN(callback_cli));

	res = ast_register_application_xml(app, callback_exec);
	res |= ast_register_application_xml(app2, cancel_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Callback Request Application");
