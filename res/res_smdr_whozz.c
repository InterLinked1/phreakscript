/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2024, Naveen Albert
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
 * \brief SMDR (Station Messaging Detail Record) server for WHOZZ Calling? Call Accounting units
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <pthread.h>
#include <signal.h>

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/config.h"
#include "asterisk/cli.h"
#include "asterisk/cdr.h"
#include "asterisk/causes.h"
#include "asterisk/stasis_channels.h"
#include "asterisk/devicestate.h"
#include "asterisk/conversions.h"

/*** DOCUMENTATION
	<configInfo name="res_smdr_whozz" language="en_US">
		<synopsis>WHOZZ Calling? SMDR</synopsis>
		<configFile name="res_smdr_whozz.conf">
			<configObject name="general">
				<configOption name="device" default="/dev/ttyS0">
					<synopsis>Serial device</synopsis>
					<description>
						<para>Serial device to use.</para>
						<para>Currently, only one serial device is supported. The units themselves allow them to be chained together
						so that only one serial connection to a PC is required.</para>
					</description>
				</configOption>
			</configObject>
			<configObject name="line">
				<configOption name="line">
					<synopsis>Line number</synopsis>
					<description>
						<para>Line number of SMDR channel as configured on the WHOZZ Calling? device.</para>
						<para>Three-way calling is not supported. Please note that WHOZZ Calling? devices do not themselves support pulse dialing.</para>
						<para>Note that if you wish to log unanswered incoming calls (outgoing calls are always assumed to be answered by the WHOZZ Calling
						unit), you need to set <literal>unanswered = yes</literal> in <literal>cdr.conf</literal>.</para>
					</description>
				</configOption>
				<configOption name="device">
					<synopsis>Asterisk device</synopsis>
					<description>
						<para>Asterisk device corresponding to this SMDR line, if applicable.</para>
						<para>If provided, this must be a <literal>chan_dahdi</literal> FXO (FXS-signalled) device connected to the same phone line.</para>
						<para>If specified, SMDR entries will be ignored if this channel is in use during the logged call.
						This is useful if certain calls are made through an FXO port with CDR already directly logged by Asterisk directly,
						but other calls are made directly on the line and not through the FXO port. Setting this option appropriately
						ensures that only the latter type of calls are logged by this module, to avoid redundant CDR entries.</para>
					</description>
				</configOption>
				<configOption name="setvar">
					<synopsis>Variable to set</synopsis>
					<description>
						<para>Variable to set on the internal Asterisk channel used for CDR purposes.</para>
						<para>This may be set multiple times to set multiple variables.</para>
						<para>For example, this can be used to set custom CDR variables, much the same way this option works
						in a channel driver configuration file.</para>
						<para>The following variables are available for substitution:</para>
						<para><literal>WHOZZ_LINE</literal> - the line number used for the call</para>
						<para><literal>WHOZZ_DIRECTION</literal> - the direction of the call, IN or OUT.</para>
						<para>The called number is available in <literal>${CDR(dst)}</literal>. The call duration is available in <literal>${CDR(duration)}</literal>. The answer time is set equal to the call duration, since with analog there is generally no way to receive supervision (and WHOZZ Calling? does not support any such mechanisms). Thus, billing time is generally an overestimate of what the phone company will bill.</para>
						<para>If Caller ID information is available, they will be set in the appropriate fields.</para>
						<para>For outgoing calls, you may wish to set other CDR fields, such as <literal>${CDR(amaflags)}</literal> or a custom variable indicating if the call was a toll call or not, based on the called number. You can do this using a dialplan mechanism such as the <literal>EVAL_EXTEN</literal> function.</para>
					</description>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
	<function name="WHOZZ_LINE_STATE" language="en_US">
		<synopsis>
			Returns the line state of a WHOZZ Calling? line
		</synopsis>
		<syntax>
			<parameter name="line" required="true">
				<para>Line number</para>
			</parameter>
		</syntax>
		<description>
			<para>Returns the line state of the specified line.</para>
			<para>The possible values are:</para>
			<enumlist>
				<enum name="UNKNOWN"/>
				<enum name="ONHOOK"/>
				<enum name="OFFHOOK"/>
				<enum name="RINGING"/>
			</enumlist>
		</description>
	</function>
 ***/

#define MODULE_NAME "res_smdr_whozz"
#define CONFIG_FILE "res_smdr_whozz.conf"

enum line_state {
	/* Until activity occurs on a line, the state will stay as 'unknown', which is true since we don't know what's going on.
	 * The WHOZZ Calling? units do not provide a way to query the line status upon initialization,
	 * even though the hardware knows from the line voltage whether the line is on-hook or not. */
	UNKNOWN = 0,
	ONHOOK,
	OFFHOOK,
	RINGING,
};

struct whozz_line {
	int lineno;							/*!< Line number on WHOZZ Calling? system */
	struct ast_channel *chan;			/*!< Dummy channel for CDR */
	const char *device;					/*!< Asterisk device */
	unsigned int detect_dialing:1;		/*!< Whether to detect dialing in Asterisk, instead of relying on the WHOZZ Calling hardware */
	enum line_state state;				/*!< Current line state */
	enum ast_device_state startstate;	/*!< Starting device state of associated FXO device, if applicable */
	enum ast_device_state answerstate;	/*!< Device state of associated FXO device, if applicable, at time of off-hook on incoming call */
	struct varshead varshead;			/*!< Variables to set on channel for CDR */
	AST_LIST_ENTRY(whozz_line) entry;	/*!< Next channel */
	char data[];
};

static AST_RWLIST_HEAD_STATIC(lines, whozz_line);

static pthread_t serial_thread = AST_PTHREADT_NULL;
static int thread_running = 0;

static char serial_device[256] = "/dev/ttyS0"; /* Default serial port on a Linux system (especially if there's only one) */
static int serial_fd = -1;
static int unloading = 0;

#define MAX_POLL_DELAY 1000

#define SERIAL_WRITE(buf, siz) { \
	if (write(pfd->fd, buf, siz) != siz) { \
		ast_log(LOG_ERROR, "Failed to write to serial port: %s\n", strerror(errno)); \
		return -1; \
	} \
	ast_debug(4, "Wrote %lu bytes to serial port: %.*s\n", (size_t) siz, (int) siz, buf); \
}

#define SERIAL_READ(buf, siz) { \
	for (;;) { \
		bufres = poll(pfd, 1, MAX_POLL_DELAY); \
		if (bufres != 1) { \
			ast_log(LOG_WARNING, "Failed to poll serial port (%ld): %s\n", bufres, strerror(errno)); \
			return -1; \
		} \
		bufres = read(pfd->fd, buf, siz); \
		if (bufres <= 0) { \
			ast_log(LOG_WARNING, "Failed to read any data from serial port (%ld): %s\n", bufres, strerror(errno)); \
			return -1; \
		} \
		break; \
	} \
	ast_debug(3, "Received %ld bytes from serial port: %s\n", bufres, buf); \
}

#define SERIAL_READ_STRING(buf, siz, pollfirst) { \
	bufres = serial_getline(pfd, pollfirst, buf, siz); \
	if (bufres <= 0) { \
		ast_log(LOG_WARNING, "Failed to read string from serial port\n"); \
		return -1; \
	} \
	/* These can end in CR, but since we have a LF after, it's okay */ \
	ast_debug(3, "Received %ld bytes from serial port: %s\n", bufres, buf); \
}

static const char *state_str_cli(enum line_state state)
{
	switch (state) {
	case UNKNOWN:
		return "Unknown";
	case ONHOOK:
		return "On Hook";
	case OFFHOOK:
		return "Off Hook";
	case RINGING:
		return "Ringing";
	}
	__builtin_unreachable();
}

static const char *state_str_func(enum line_state state)
{
	switch (state) {
	case UNKNOWN:
		return "UNKNOWN";
	case ONHOOK:
		return "ONHOOK";
	case OFFHOOK:
		return "OFFHOOK";
	case RINGING:
		return "RINGING";
	}
	__builtin_unreachable();
}

static int serial_getline(struct pollfd *pfd, int pollfirst, char *restrict buf, size_t len)
{
	const char *bufstart = buf;
	size_t total = 0;
	for (;;) {
		ssize_t bufres = 1;
		errno = 0;
		if (pollfirst) {
			bufres = poll(pfd, 1, MAX_POLL_DELAY);
		}
		pollfirst = 1; /* Poll from now on out in future loop iterations */
		if (bufres <= 0) {
			if (!total) {
				ast_log(LOG_WARNING, "Failed to poll serial port (%ld): %s\n", bufres, strerror(errno));
				return -1;
			}
			ast_debug(3, "Read partial line (%ld bytes)\n", total);
			return total;
		}
		bufres = read(pfd->fd, buf, len);
		if (bufres <= 0) {
			if (unloading) {
				return -1;
			}
			ast_log(LOG_WARNING, "Failed to read any data from serial port (%ld): %s\n", bufres, strerror(errno));
			return -1;
		}
		buf[bufres] = '\0';
		total += bufres;
		if (strchr(buf, '\r')) { /* EOT */
			return total;
		}
		buf += bufres;
		len -= bufres;
		if (len <= 0) {
			ast_log(LOG_ERROR, "Buffer exhausted before complete line could be read? (read so far: '%s')\n", bufstart);
			return -1;
		}
	}
}

static int serial_putstring(struct pollfd *pfd, char *restrict buf, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++) {
		if (write(pfd->fd, buf + i, 1) != 1) {
			ast_log(LOG_ERROR, "Failed to write to serial port: %s\n", strerror(errno));
			return -1;
		}
		usleep(50000);
	}
	ast_debug(4, "Wrote %lu bytes to serial port: %.*s\n", (size_t) i, (int) i, buf);
	return i;
}

static int serial_sync(struct pollfd *pfd)
{
	ssize_t bufres;
	char c;

	SERIAL_WRITE("@", 1);
	SERIAL_READ(&c, 1);

	ast_debug(1, "Successfully synchronized with WHOZZ Calling? unit on %s\n", serial_device);
	return 0;
}

static int set_settings(struct pollfd *pfd, int reset)
{
	struct timeval when;
	struct ast_tm tm;
	ssize_t bufres;
	char buf[64];
	char ch;

/* Must wait at least 50 ms between setting each setting to non-volatile memory */
#define SET_SETTING_DELAY 50000

#define SET_SETTING(c) \
	if (!strchr(buf, c)) { \
		ast_verb(5, "Setting WHOZZ Calling? register '%c'\n", c); \
		ch = c; \
		SERIAL_WRITE(&ch, 1); \
		usleep(SET_SETTING_DELAY); \
	}

	if (reset) {
		buf[0] = '\0';
		SET_SETTING('R');
	}

	SERIAL_WRITE("V", 1);
	SERIAL_READ_STRING(buf, sizeof(buf), 1);

	SET_SETTING('E'); /* Echo off */
	SET_SETTING('c'); /* Remove dashes from phone number, leading $ */
	SET_SETTING('X'); /* Duration and checksum */
	SET_SETTING('d'); /* Detail information sent */
	SET_SETTING('A'); /* Data sent before and after */
	SET_SETTING('o'); /* Include outbound and inbound info */

#undef SET_SETTING
#define DATE_FORMAT "%m%d%H%M"

	/* Set the unit's time, in case the line(s) connected to it don't have Caller ID */
	ast_get_timeval(NULL, &when, ast_tvnow(), NULL);
	ast_localtime(&when, &tm, NULL);
	buf[0] = 'Z';
	bufres = ast_strftime(buf + 1, sizeof(buf) - 1, DATE_FORMAT, &tm);
	if (bufres <= 1) {
		ast_log(LOG_ERROR, "Failed to calculate current time\n");
	} else {
		bufres++; /* For initial 'Z' */
		ast_verb(5, "Setting WHOZZ Calling? time to %s\n", buf + 1);
		buf[bufres++] = '\r'; /* Needs to end in <CR> */
		if (serial_putstring(pfd, buf, bufres) < 0) { /* Need to send characters one at a time */
			ast_log(LOG_ERROR, "Failed to set unit time\n");
			return -1;
		}
	}
	return 0;
}

static struct ast_channel_tech whozz_smdr_cdr_chan_tech = {
	.type = "WHOZZ",
	.description = "WHOZZ SMDR CDR",
};

static void __cleanup_stale_cdr(struct ast_channel *chan)
{
	pbx_builtin_setvar_helper(chan, "CDR_PROP(disable)", "1"); /* Don't write an inaccurate CDR record, just discard it */
	ast_hangup(chan);
}

static void cleanup_stale_cdr(struct ast_channel *chan)
{
	ast_log(LOG_WARNING, "Discarding unfinished call on %s?\n", ast_channel_name(chan));
	__cleanup_stale_cdr(chan);
}

static void mark_answered(struct ast_channel *chan)
{
	ast_channel_state_set(chan, AST_STATE_UP);
	ast_raw_answer(chan); /* This, among other things, does set the channel answer time, which is used for CDR answer time */

	/* The channel snapshot must be updated whenever we set the answer time for it to affect the CDR. */
	ast_channel_lock(chan);
	ast_channel_publish_snapshot(chan); /* So core show channels reflects channel is up, answered duration is correct, etc. */
	ast_channel_unlock(chan);
}

static int fxo_device_state(const char *device)
{
	/* Non-ISDN DAHDI channels (e.g. analog) are always "unknown" when not in use...
	 * not sure I agree with this, but this is the way it is currently, so account for that.
	 *
	 * Furthermore, the device state is misleading and CANNOT BE USED for determining the line state.
	 * - When the connected phone line is idle, the state will be "Unknown".
	 * - When the FXO port is in use, the state will be "In Use" (good).
	 * - When there is an incoming call ringing to the FXO port, its device state will be... "In Use".
	 *
	 * To understand what's going on, recall the Asterisk fallback for computing device state simply
	 * finds a channel associated with the device and converts its channel state to a mapped device state.
	 *
	 * AST_STATE_RING -> AST_DEVICE_INUSE
	 * AST_STATE_RINGING -> AST_DEVICE_RINGING
	 *
	 * STATE_RING essentially corresponds to audible ring being present on the channel, while
	 * STATE_RINGING corresponds to the device actually ringing, e.g. actual physical 90V power ring.
	 *
	 * However, RINGING only makes sense when Asterisk is ringing a device (whether it's DAHDI or not).
	 * For example, when ringing a phone, its DAHDI channel will have channel state (and device state) "Ringing".
	 *
	 * However, on incoming calls to an FXO port, Asterisk isn't ringing it... the opposite, in fact!
	 * The network is indicating to us that there is an incoming call. And in all other scenarios,
	 * the incoming channel side that is ringing a phone has state "Ring". FXO ports are a special edge
	 * case where there is something physically ringing (perhaps phones on the connected line), but it's
	 * not really "Ringing" in the semantic sense that Asterisk uses it.
	 *
	 * TL;DR To distinguish between FXO port being rung and actually active and in use, do not use
	 * ${DEVICE_STATE(DAHDI/1)}
	 * In both cases, it will return "INUSE".
	 * Instead, do:
	 * ${IMPORT(DAHDI/1-1,CHANNEL(state))}
	 * This is because chan_dahdi doesn't support call waiting on FXO ports (currently),
	 * so there will only ever be one channel max on an FXO port, and so we know what it will be called.
	 * Therefore, if we know the device is in use, then we can use this to check if Asterisk is off-hook on the FXO port.
	 * If it returns "Ring", it's just ringing and the FXO port is idle.
	 * If it returns "Up", then we're actually off-hook on the FXO port.
	 *
	 * The code below does the internal equivalent of ${IMPORT(DAHDI/1-1,CHANNEL(state))}
	 */

	/* Use the device state to get started, but for the reasons described at length above, we can't use that alone.
	 * We need to narrow it down further to distinguish between ringing and actually in use. */
	enum ast_device_state devstate = ast_device_state(device);
	if (devstate == AST_DEVICE_INUSE) {
		struct ast_channel *chan2;
		char channel_name[64];
		/* The DAHDI channel naming scheme is predictable, and FXO ports are only going to have 1 channel, currently,
		 * so that simplifies this to a straightforward translation. */
		snprintf(channel_name, sizeof(channel_name), "%s-1", device);
		if ((chan2 = ast_channel_get_by_name(channel_name))) {
			enum ast_channel_state state;
			ast_channel_lock(chan2);
			state = ast_channel_state(chan2);
			ast_channel_unlock(chan2);
			chan2 = ast_channel_unref(chan2);

			/* Based on the state, do a conversion. */
			ast_debug(3, "Channel state of %s is %s\n", channel_name, ast_state2str(state));
			switch (state) {
			case AST_STATE_RING:
				/* Normally, this would map to AST_DEVICE_INUSE.
				 * For our purposes, we return AST_DEVICE_RINGING,
				 * to reflect the FXO port ringing. Semantically,
				 * this breaks with Asterisk's idea of what device state
				 * refers to, but we're really just repurposing the enum
				 * for something specific here. */
				devstate = AST_DEVICE_RINGING;
				break;
			default:
				/* Leave it alone */
				break;
			}
		} else {
			/* Not really any way to determine the truth... */
			ast_log(LOG_ERROR, "Channel %s does not exist\n", channel_name);
		}
	} else if (devstate == AST_DEVICE_UNKNOWN) {
		/* chan_dahdi doesn't set specific states for non-ISDN.
		 * If it's unknown, then what that really indicates is not in use. */
		devstate = AST_DEVICE_NOT_INUSE;
	}
	return devstate;
}

/* NOTE: Do not use the ast_device_state function directly below this point! Use fxo_device_state instead! */

static int handle_hook(struct whozz_line *w, int outbound, int end, int duration, const char *numberstr, const char *cnam)
{
	if (end) {
		if (w->chan) {
			struct ast_var_t *var;

			/* End call and finalize */
			if (!ast_strlen_zero(w->device)) {
				int ast_device_used;
				enum ast_device_state endstate = fxo_device_state(w->device);
				if (outbound) {
					ast_device_used = w->startstate == AST_DEVICE_INUSE && endstate == AST_DEVICE_NOT_INUSE;
				} else {
					/* The inbound case is a little bit different because there's an extra step.
					 * Initially, the state should always be AST_DEVICE_RINGING here,
					 * because you can't answer a phone call through Asterisk before the FXO port even starts ringing!
					 * But RINGING to start with and NOT_INUSE at the end doesn't tell us whether we answered through Asterisk or not.
					 * The critical thing is detecting DEVICE_INUSE at some point while the line is off-hook,
					 * and conveniently we check this right if/when an off-hook occurs. */
					ast_device_used = w->answerstate == AST_DEVICE_INUSE && endstate == AST_DEVICE_NOT_INUSE;
				}
				if (ast_device_used) {
					/* Avoid a duplicate CDR record, since the call was made through Asterisk. */
					ast_verb(6, "Call was made through associated device, ignoring this call for SMDR purposes\n");
					__cleanup_stale_cdr(w->chan);
					w->chan = NULL;
					return 0;
				} else {
					ast_debug(2, "FXO state of %s was %s and is now %s\n", w->device, ast_devstate_str(w->startstate), ast_devstate_str(endstate));
				}
				w->startstate = w->answerstate = AST_DEVICE_UNKNOWN; /* Reset */
			}

			/* Now, add any variables */
			AST_VAR_LIST_TRAVERSE(&w->varshead, var) {
				char subbuf[4096];
				const char *varkey, *varval;
				varkey = ast_var_name(var);
				varval = ast_var_value(var);
				/* Substitute if needed */
				pbx_substitute_variables_helper(w->chan, varval, subbuf, sizeof(subbuf) - 1);
				/* Replace or add variable */
				ast_debug(8, "Setting variable %s=%s\n", varkey, subbuf);
				pbx_builtin_setvar_helper(w->chan, varkey, subbuf);
			}

			ast_channel_hangupcause_set(w->chan, AST_CAUSE_NORMAL);
			ast_debug(5, "Finalized CDR for channel %s\n", ast_channel_name(w->chan));
			ast_hangup(w->chan); /* Kill the channel and force the CDR to be processed, with variable substitution */
			w->chan = NULL;
		} else {
			/* e.g. call was in progress when this module initialized, we'll see a call end without a corresponding call start */
			ast_log(LOG_WARNING, "No call in progress, ignoring call end\n");
		}
	} else {
		char intbuf[32];

		if (w->chan) {
			cleanup_stale_cdr(w->chan);
			w->chan = NULL;
		}

		if (!ast_strlen_zero(w->device)) {
			/* Check the device state when the call began.
			 * If it's in use, then we know that the call in question
			 * is being made through Asterisk, and thus we'll probably
			 * end up ignoring this call for CDR purposes. */
			w->startstate = fxo_device_state(w->device);
		}

		/* Unfortunately, we cannot use a dummy channel for CDR.
		 * For variable substitution, it's fine,
		 * but CDR only gets processed on "real" channels.
		 * Original plan was to create the channel when the call ends, just for an instant,
		 * but the start time seems to be tied to channel creation time through snapshots,
		 * so for ease, just create it now. The channel won't really consume any resources anyways,
		 * apart from being in the channels container. Another advantage is the non-Asterisk channels
		 * have corresponding dummy Asterisk channels that can show their current uptime, etc. */

		/* Like DAHDI channels, this channel name is not unique across time */
		w->chan = ast_channel_alloc(0, AST_STATE_DOWN, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, "WHOZZ/%d", w->lineno);
		if (!w->chan) {
			return -1;
		}

		if (outbound) {
			ast_channel_exten_set(w->chan, numberstr); /* Leave context alone, but stuff dialed number in the extension */
		}
		ast_channel_appl_set(w->chan, "AppDial");
		ast_channel_data_set(w->chan, outbound ? "(Outgoing Call)" : "(Incoming Call)");

		if (!outbound) {
			struct ast_party_caller caller;
			memset(&caller, 0, sizeof(caller));
			/* Outbound calls will not have a Caller ID, since all we know is that somebody is making the call.
			 * Inbound calls, assuming they are not answered before the first ring, and the line has Caller ID,
			 * may have calling identification. */
			if (!ast_strlen_zero(numberstr) && strcmp(numberstr, "No-CallerID")) { /* No-CallerID is used to indicate lack of CID, ignore that */
				caller.id.number.str = ast_strdup(numberstr);
				caller.id.number.valid = 1;
			}
			if (!ast_strlen_zero(cnam)) {
				caller.id.name.str = ast_strdup(cnam);
				caller.id.name.valid = 1;
			}
			ast_channel_set_caller(w->chan, &caller, NULL);
		}

		/* Don't mark as outgoing since outgoing legs can't be "answered" */
		/* No formats required, since this is still technically a dummy channel, no audio required */
		ast_channel_unlock(w->chan);

		/* Set all our data on the channel */
		snprintf(intbuf, sizeof(intbuf), "%d", w->lineno);
		pbx_builtin_setvar_helper(w->chan, "WHOZZ_LINE", intbuf);
		pbx_builtin_setvar_helper(w->chan, "WHOZZ_DIRECTION", outbound ? "OUT" : "IN");
		pbx_builtin_setvar_helper(w->chan, "CDR_PROP(disable)", "0"); /* First, enable CDR for the channel, even if disabled by default */

		if (outbound) {
			/* Answer channel immediately. For analog, we don't get supervision so assume answered from the get-go. */
			mark_answered(w->chan);
		} else {
			/* mark_answered also does this, but if we don't call that, have to do it here: */
			ast_channel_lock(w->chan);
			ast_channel_publish_snapshot(w->chan); /* So core show channels reflects channel is up, answered duration is correct, etc. */
			ast_channel_unlock(w->chan);
		}

		/* Nothing is in charge of this channel really, after this point,
		 * until we manually clean it up in this module, either when the module is unloaded
		 * or when the call ends. Something else in Asterisk could request a hangup
		 * on the channel but it won't get processed, so there's no real need to ref the
		 * channel here. */
	}
	return 0;
}

#define EXPECT_NONEMPTY(s) \
	if (ast_strlen_zero(s)) { \
		ast_log(LOG_WARNING, "Expected %s to be non-empty\n", #s); \
		return -1; \
	}

static void update_state(struct whozz_line *w, enum line_state newstate, enum line_state oldstate)
{
	w->state = newstate;
	manager_event(EVENT_FLAG_CALL, "WHOZZLineStateChange",
		"LineNumber: %d\r\n"
		"CurrentState: %s\r\n"
		"OldState: %s\r\n",
		w->lineno,
		state_str_func(w->state),
		state_str_func(oldstate));
}

static int __process_serial_read(struct whozz_line *w, int lineno, const char *action, char *buf, size_t len)
{
	enum line_state oldstate = w->state;
	/*** DOCUMENTATION
		<managerEvent language="en_US" name="WHOZZLineStateChange">
			<managerEventInstance class="EVENT_FLAG_CALL">
				<synopsis>Raised when the state of a phone line connected to
				a WHOZZ Calling? device changes.</synopsis>
				<syntax>
					<channel_snapshot/>
					<parameter name="LineNumber">
						<para>The line number</para>
					</parameter>
					<parameter name="CurrentState">
						<para>The current state of the line.</para>
						<para>The possible values are:</para>
						<enumlist>
							<enum name="UNKNOWN"/>
							<enum name="ONHOOK"/>
							<enum name="OFFHOOK"/>
							<enum name="RINGING"/>
						</enumlist>
					</parameter>
					<parameter name="OldState">
						<para>The old state of the line.</para>
						<para>The possible values are:</para>
						<enumlist>
							<enum name="UNKNOWN"/>
							<enum name="ONHOOK"/>
							<enum name="OFFHOOK"/>
							<enum name="RINGING"/>
						</enumlist>
					</parameter>
				</syntax>
				<description>
					<para>This event is raised whenever the line state of a line changes.</para>
				</description>
			</managerEventInstance>
		</managerEvent>
		<managerEvent language="en_US" name="WHOZZLineCall">
			<managerEventInstance class="EVENT_FLAG_CALL">
				<synopsis>Raised when a call begins or ends on a phone line
				connected to a WHOZZ Calling? device.</synopsis>
				<syntax>
					<channel_snapshot/>
					<parameter name="LineNumber">
						<para>The line number</para>
					</parameter>
					<parameter name="Direction">
						<para>IN or OUT.</para>
					</parameter>
					<parameter name="Duration">
						<para>Call duration, in seconds.</para>
						<para>Only provided when a call ends.</para>
					</parameter>
					<parameter name="CalledNumber">
						<para>The called number.</para>
						<para>Only provided for outgoing calls.</para>
					</parameter>
					<parameter name="CallerNumber">
						<para>The calling number.</para>
						<para>Only provided for incoming calls.</para>
					</parameter>
					<parameter name="CallerName">
						<para>The calling name.</para>
						<para>Only provided for incoming calls.</para>
					</parameter>
				</syntax>
				<description>
					<para>This event is raised whenever the line state of a line changes.</para>
				</description>
			</managerEventInstance>
		</managerEvent>
	***/
	if (!strcasecmp(action, "F")) {
		ast_verb(5, "Off-hook on line %d\n", lineno);
		if (w->state == RINGING) {
			ast_verb(5, "Incoming call on line %d was just answered\n", w->lineno);
			/* Answer it on the channel */
			if (w->chan) {
				mark_answered(w->chan);
				if (!ast_strlen_zero(w->device)) {
					w->answerstate = fxo_device_state(w->device);
				}
			} else {
				ast_log(LOG_WARNING, "No call in progress, ignoring call answer\n");
			}
		}
		update_state(w, OFFHOOK, oldstate);
	} else if (!strcasecmp(action, "N")) {
		ast_verb(5, "On-hook on line %d\n", lineno);
		update_state(w, ONHOOK, oldstate);
	} else if (!strcasecmp(action, "R")) {
		ast_verb(5, "First ring on line %d\n", lineno);
		update_state(w, RINGING, oldstate);
		/* This could be followed by I S: Incoming Call Start (whether or not the line has Caller ID, and even if call is answered before first ring ends)
		 * That, in turn, is followed by F, for off hook if somebody answers it,
		 * or I E (Incoming Call End), if ring no answer. */
	} else if (!strcasecmp(action, "H")) {
		ast_verb(5, "Hook flash on line %d\n", lineno);
		/* Leave state alone */
	} else if (!strcasecmp(action, "I") || !strcasecmp(action, "O")) {
		char *startend, *durationstr, *checksum, *numringsstr, *date, *time, *ampm, *numberstr, *cnam;
		int callend, duration, outbound;
		outbound = *action == 'O';
		/* e.g. $01 O S 0 10/14  18004444444 */
		startend = strsep(&buf, " ");
		durationstr = strsep(&buf, " ");
		checksum = strsep(&buf, " ");
		numringsstr = strsep(&buf, " ");
		date = strsep(&buf, " ");
		time = strsep(&buf, " ");
		ampm = strsep(&buf, " ");
		numberstr = strsep(&buf, " "); /* Because we set setting 'c', there are no dashes in the phone number. For 'C', they'd need to be filtered out */
		cnam = strsep(&buf, " ");
		EXPECT_NONEMPTY(startend);
		EXPECT_NONEMPTY(durationstr);
		EXPECT_NONEMPTY(checksum);
		EXPECT_NONEMPTY(numringsstr);
		EXPECT_NONEMPTY(date);
		EXPECT_NONEMPTY(time);
		EXPECT_NONEMPTY(ampm);
		EXPECT_NONEMPTY(numberstr); /* Called number or calling number, if available */
		/* callerid and cnam can be empty */
		callend = !strcasecmp(startend, "E"); /* S for start, E for end */
		duration = atoi(durationstr);
		if (callend) {
			ast_log(LOG_NOTICE, "%s call %s on line %d to %s (%d s)\n", outbound ? "Outbound" : "Inbound", "ended", lineno, numberstr, duration);
			/* If we were previously ringing, and never went off-hook,
			 * there won't be an on-hook event that will reset the line state,
			 * do it here. */
			if (oldstate == RINGING) {
				update_state(w, ONHOOK, oldstate);
			}
		} else {
			ast_log(LOG_NOTICE, "%s call %s on line %d to %s\n", outbound ? "Outbound" : "Inbound", "began", lineno, numberstr);
		}
		/* Log/store the appropriate details */
		handle_hook(w, outbound, callend, duration, numberstr, cnam);
		if (callend) {
			if (outbound) {
				manager_event(EVENT_FLAG_CALL, "WHOZZLineCall",
					"LineNumber: %d\r\n"
					"Direction: %s\r\n"
					"Duration: %d\r\n"
					"CalledNumber: %s\r\n",
					w->lineno,
					outbound ? "OUT" : "IN",
					duration,
					numberstr);
			} else {
				manager_event(EVENT_FLAG_CALL, "WHOZZLineCall",
					"LineNumber: %d\r\n"
					"Direction: %s\r\n"
					"Duration: %d\r\n"
					"CallerNumber: %s\r\n"
					"CallerName: %s\r\n",
					w->lineno,
					outbound ? "OUT" : "IN",
					duration,
					numberstr,
					cnam);
			}
		} else {
			if (outbound) {
				manager_event(EVENT_FLAG_CALL, "WHOZZLineCall",
					"LineNumber: %d\r\n"
					"Direction: %s\r\n"
					"CalledNumber: %s\r\n",
					w->lineno,
					outbound ? "OUT" : "IN",
					numberstr);
			} else {
				manager_event(EVENT_FLAG_CALL, "WHOZZLineCall",
					"LineNumber: %d\r\n"
					"Direction: %s\r\n"
					"CallerNumber: %s\r\n"
					"CallerName: %s\r\n",
					w->lineno,
					outbound ? "OUT" : "IN",
					numberstr,
					cnam);
			}
		}
	}
	return 0;
}

static int process_serial_read(char *buf, size_t len)
{
	struct whozz_line *w;
	int lineno;
	int res = -1;
	char *linestr, *action;

	linestr = strsep(&buf, " ");
	action = strsep(&buf, " ");

	EXPECT_NONEMPTY(linestr);
	if (len == 1) {
		ast_debug(1, "Ignoring character %d\n", *buf);
		return -1; /* Ignore */
	}
	if (*linestr == '$') { /* Skip leading $, e.g. 01 or $01 */
		linestr++;
		EXPECT_NONEMPTY(linestr);
	}
	lineno = atoi(linestr);
	if (lineno < 1) {
		ast_log(LOG_WARNING, "Invalid line number: %s\n", linestr);
		return -1;
	}
	EXPECT_NONEMPTY(action);

	AST_RWLIST_RDLOCK(&lines);
	AST_RWLIST_TRAVERSE(&lines, w, entry) {
		if (w->lineno == lineno) {
			res = __process_serial_read(w, lineno, action, buf, len);
			break;
		}
	}
	AST_RWLIST_UNLOCK(&lines);

	if (!w) {
		/* We couldn't find a line for this line number */
		ast_log(LOG_WARNING, "No configuration for line %d\n", lineno);
	}

	return res;
}

static int serial_loop(struct pollfd *pfd)
{
	ssize_t bufres;

	for (;;) {
		char buf[128];
		char *pos;

		bufres = poll(pfd, 1, -1);
		if (bufres <= 0) {
			if (unloading) {
				return -1;
			}
			ast_log(LOG_ERROR, "Failed to poll serial port: %s\n", strerror(errno));
			break;
		}
		SERIAL_READ_STRING(buf, sizeof(buf), 0);
		if (bufres == 1) {
			continue; /* Not real... */
		}
		pos = buf;
		while (bufres > 0 && !isprint(*pos)) {
			/* Trim leading white space and line breaks */
			ast_debug(4, "Skipping character %d\n", *pos);
			pos++;
			bufres--;
		}
		if (bufres > 0) {
			process_serial_read(pos, bufres);
		}
	}

	ast_debug(1, "Serial loop thread exiting\n");
	return 0;
}

static int serial_monitor(void *varg)
{
	struct pollfd pfd;
	int delay = 1000000;

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = serial_fd;
	pfd.events = POLLIN | POLLERR | POLLNVAL | POLLHUP;

	if (serial_sync(&pfd)) {
		ast_log(LOG_ERROR, "Failed to synchronize with serial port, is %s already open by another application?\n", serial_device);
		/* Continue retrying periodically in case it comes online later */
		do {
			usleep(delay);
			if (delay < 32000000) {
				delay *= 2;
			}
		} while (!unloading && serial_sync(&pfd));
		if (unloading) {
			ast_debug(3, "Unload requested before initialization could complete\n");
			return -1;
		}
	}

	if (set_settings(&pfd, 0)) {
		ast_log(LOG_ERROR, "Failed to initialize device with SMDR settings\n");
		return -1;
	}

	do {
		serial_loop(&pfd);
	} while (!unloading && serial_fd != -1 && !serial_sync(&pfd));

	return 0;
}

static void *__serial_monitor(void *varg)
{
	while (!ast_fully_booted && !unloading) {
		usleep(50000);
	}
	thread_running = 1;
	if (!unloading) {
		serial_monitor(varg);
	}
	thread_running = 0;
	if (!unloading) {
		ast_log(LOG_ERROR, "Serial monitor thread exited prematurely, refresh module to reinitialize\n");
	}
	return NULL;
}

static int whozz_line_state_read(struct ast_channel *chan, const char *cmd, char *parse, char *buffer, size_t buflen)
{
	int lineno;
	struct whozz_line *w;

	if (ast_strlen_zero(parse)) {
		ast_log(LOG_ERROR, "Line number required for %s\n", cmd);
		return -1;
	}

	lineno = atoi(parse);
	AST_RWLIST_RDLOCK(&lines);
	AST_RWLIST_TRAVERSE(&lines, w, entry) {
		if (w->lineno == lineno) {
			ast_copy_string(buffer, state_str_func(w->state), buflen);
			break;
		}
	}
	AST_RWLIST_UNLOCK(&lines);

	return w ? 0 : -1;
}

static struct ast_custom_function acf_whozz = {
	.name = "WHOZZ_LINE_STATE",
	.read = whozz_line_state_read,
};

static char *handle_show_lines(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct whozz_line *w;

	switch (cmd) {
	case CLI_INIT:
		e->command = "whozz show lines";
		e->usage =
			"Usage: whozz show lines\n"
			"       Show any configured WHOZZ Calling lines.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	ast_cli(a->fd, "Serial device is %s (fd %d)\n", serial_device, serial_fd);
	ast_cli(a->fd, "Serial status is %s\n", thread_running ? "RUNNING" : "ABORTED");
	ast_cli(a->fd, "%4s %-8s %s\n", "Line", "State", "Associated Device");
	AST_RWLIST_RDLOCK(&lines);
	AST_RWLIST_TRAVERSE(&lines, w, entry) {
		ast_cli(a->fd, "%4d %-8s %s\n", w->lineno, state_str_cli(w->state), S_OR(w->device, ""));
	}
	AST_RWLIST_UNLOCK(&lines);

	return CLI_SUCCESS;
}

static char *handle_reset(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct pollfd pfd;

	switch (cmd) {
	case CLI_INIT:
		e->command = "whozz reset";
		e->usage =
			"Usage: whozz reset\n"
			"       Reset and reinitialize the connected unit.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 2) {
		return CLI_SHOWUSAGE;
	}

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = serial_fd;
	pfd.events = POLLIN | POLLERR | POLLNVAL | POLLHUP;

	if (set_settings(&pfd, 1)) {
		ast_cli(a->fd, "Failed to reinitialize device with SMDR settings\n");
		return CLI_FAILURE;
	}

	return CLI_SUCCESS;
}

static struct ast_cli_entry whozz_cli[] = {
	AST_CLI_DEFINE(handle_show_lines, "List WHOZZ Calling lines"),
	AST_CLI_DEFINE(handle_reset, "Reset and reinitialize the connected unit"),
};

static int load_config(void)
{
	const char *cat = NULL;
	struct ast_config *cfg;
	struct ast_flags config_flags = { 0 };
	struct ast_variable *var;
	int tmp;

	if (!(cfg = ast_config_load(CONFIG_FILE, config_flags))) {
		ast_debug(1, "Config file %s not found\n", CONFIG_FILE);
		return -1;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		ast_debug(1, "Config file %s unchanged, skipping\n", CONFIG_FILE);
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Config file %s is in an invalid format. Aborting.\n", CONFIG_FILE);
		return -1;
	}

	while ((cat = ast_category_browse(cfg, cat))) {
		if (!strcasecmp(cat, "general")) {
			for (var = ast_variable_browse(cfg, cat); var; var = var->next) {
				if (!strcasecmp(var->name, "device")) {
					ast_copy_string(serial_device, var->value, sizeof(serial_device));
				} else {
					ast_log(LOG_WARNING, "Unknown setting at line %d: '%s'\n", var->lineno, var->name);
				}
			}
		} else { /* it's a line definition */
			struct whozz_line *w;
			const char *device = NULL;
			for (var = ast_variable_browse(cfg, cat); var; var = var->next) {
				if (!strcasecmp(var->name, "line")) {
					if (ast_str_to_int(var->value, &tmp)) {
						ast_log(LOG_WARNING, "Invalid line number '%s'\n", var->value);
						ast_config_destroy(cfg);
						return -1;
					}
				} else if (!strcasecmp(var->name, "device")) {
					device = var->value;
					if (strncasecmp(device, "DAHDI/", 6)) {
						ast_log(LOG_WARNING, "Setting 'device' must be a DAHDI device\n");
						device = NULL;
					}
				} else if (!strcasecmp(var->name, "setvar")) {
					continue; /* Ignore on this pass */
				} else {
					ast_log(LOG_WARNING, "Unknown setting at line %d: '%s'\n", var->lineno, var->name);
				}
			}
			AST_RWLIST_TRAVERSE(&lines, w, entry) {
				if (w->lineno == tmp) {
					ast_log(LOG_ERROR, "Line %d already configured\n", tmp);
					return -1;
				}
			}
			w = ast_calloc(1, sizeof(*w) + (device ? strlen(device) + 1 : 0));
			if (!w) {
				ast_config_destroy(cfg);
				return -1;
			}
			w->lineno = tmp;
			if (!ast_strlen_zero(device)) {
				strcpy(w->data, device); /* Safe */
				w->device = w->data;
			}
			/* Now, add any variables */
			for (var = ast_variable_browse(cfg, cat); var; var = var->next) {
				if (!strcasecmp(var->name, "setvar")) {
					struct ast_var_t *v;
					char *name, *val = ast_strdupa(var->value);
					name = strsep(&val, "=");
					if (ast_strlen_zero(name)) {
						ast_log(LOG_WARNING, "Missing variable name\n");
						continue;
					} else if (ast_strlen_zero(val)) {
						ast_log(LOG_WARNING, "Missing variable value for '%s'\n", name);
						continue;
					}
					v = ast_var_assign(name, val);
					if (!v) {
						ast_log(LOG_ERROR, "Failed to allocate variable '%s'\n", name);
						continue;
					}
					AST_VAR_LIST_INSERT_TAIL(&w->varshead, v);
				}
			}
			AST_RWLIST_INSERT_TAIL(&lines, w, entry);
		}
	}

	ast_config_destroy(cfg);
	return 0;
}

static int unload_module(void)
{
	struct whozz_line *w;

	unloading = 1;
	ast_cli_unregister_multiple(whozz_cli, ARRAY_LEN(whozz_cli));
	ast_custom_function_unregister(&acf_whozz);
	if (serial_fd != -1) {
		if (serial_thread != AST_PTHREADT_NULL) {
			/* Interrupt any system call */
			pthread_kill(serial_thread, SIGURG);
		}
	}
	if (serial_thread != AST_PTHREADT_NULL) {
		ast_debug(1, "Waiting for serial thread to exit\n");
		pthread_join(serial_thread, NULL);
	}
	if (serial_fd != -1) {
		close(serial_fd);
		serial_fd = -1;
	}

	AST_RWLIST_WRLOCK(&lines);
	while ((w = AST_RWLIST_REMOVE_HEAD(&lines, entry))) {
		struct ast_var_t *var;
		if (w->chan) {
			cleanup_stale_cdr(w->chan);
			w->chan = NULL;
		}
		/* Don't use ast_var_list_destroy, since w->varshead is stack allocated */
		while ((var = AST_LIST_REMOVE_HEAD(&w->varshead, entries))) {
			ast_var_delete(var);
		}
		ast_free(w);
	}
	AST_RWLIST_UNLOCK(&lines);

	ast_channel_unregister(&whozz_smdr_cdr_chan_tech);
	return 0;
}

static int load_module(void)
{
	int res = 0;

	if (!ast_cdr_is_enabled()) {
		ast_log(LOG_WARNING, "CDR is disabled, declining to load\n");
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_channel_register(&whozz_smdr_cdr_chan_tech);
	if (load_config()) {
		unload_module();
		return AST_MODULE_LOAD_DECLINE;
	}

	/* 9600/N/1 */
	serial_fd = open(serial_device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (serial_fd < 0) {
		ast_log(LOG_ERROR, "Failed to open serial device %s: %s\n", serial_device, strerror(errno));
		unload_module();
		return AST_MODULE_LOAD_DECLINE;
	}
	ast_verb(5, "Opened serial device %s\n", serial_device);

	if (ast_pthread_create_background(&serial_thread, NULL, __serial_monitor, NULL) < 0) {
		ast_log(LOG_ERROR, "Unable to start periodic thread\n");
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_custom_function_register(&acf_whozz);
	ast_cli_register_multiple(whozz_cli, ARRAY_LEN(whozz_cli));
	return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "WHOZZ Calling SMDR",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_CDR_DRIVER,
	.requires = "cdr",
);
