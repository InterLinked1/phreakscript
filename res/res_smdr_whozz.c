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
					</description>
				</configOption>
				<configOption name="device">
					<synopsis>Asterisk device</synopsis>
					<description>
						<para>Asterisk device corresponding to this SMDR line, if applicable.</para>
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
	enum line_state state;				/*!< Current line state */
	enum ast_device_state startstate;	/*!< Starting device state of associated device, if applicable */
	struct varshead varshead;			/*!< Variables to set on channel for CDR */
	AST_LIST_ENTRY(whozz_line) entry;	/*!< Next channel */
	char data[];
};

static AST_RWLIST_HEAD_STATIC(lines, whozz_line);

static pthread_t serial_thread = AST_PTHREADT_NULL;

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
		return -1; \
	} \
	/* These can end in CR, but since we have a LF after, it's okay */ \
	ast_debug(3, "Received %ld bytes from serial port: %s\n", bufres, buf); \
}

static const char *state_str(enum line_state state)
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

static int serial_getline(struct pollfd *pfd, int pollfirst, char *restrict buf, size_t len)
{
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

static int set_settings(struct pollfd *pfd)
{
	struct timeval when;
	struct ast_tm tm;
	ssize_t bufres;
	char buf[64];

	SERIAL_WRITE("V", 1);
	SERIAL_READ_STRING(buf, sizeof(buf), 1);

/* Must wait at least 50 ms between setting each setting to non-volatile memory */
#define SET_SETTING_DELAY 50000

#define SET_SETTING(c) \
	if (!strchr(buf, c)) { \
		ast_verb(5, "Setting WHOZZ Calling? register '%c'\n", c); \
		usleep(SET_SETTING_DELAY); \
	}

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

/*! \brief A channel technology used for the unit tests */
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

static int handle_hook(struct whozz_line *w, int outbound, int end, int duration, const char *numberstr, const char *cnam)
{
	if (end) {
		if (w->chan) {
			struct ast_var_t *var;

			/* End call and finalize */
			if (!ast_strlen_zero(w->device)) {
				enum ast_device_state endstate = ast_device_state(w->device);
				if (w->startstate == AST_DEVICE_INUSE && endstate == AST_DEVICE_NOT_INUSE) {
					/* Avoid a duplicate CDR record, since the call was made through Asterisk. */
					ast_verb(6, "Call was made through associated device, ignoring this call for SMDR purposes\n");
					__cleanup_stale_cdr(w->chan);
					w->chan = NULL;
					return 0;
				}
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
				pbx_builtin_setvar_helper(w->chan, varkey, subbuf);
			}

			ast_channel_hangupcause_set(w->chan, AST_CAUSE_NORMAL);
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
			w->startstate = ast_device_state(w->device);
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

static int __process_serial_read(struct whozz_line *w, int lineno, const char *action, char *buf, size_t len)
{
	if (!strcasecmp(action, "F")) {
		ast_verb(5, "Off-hook on line %d\n", lineno);
		if (w->state == RINGING) {
			ast_verb(5, "Incoming call on line %d was just answered\n", w->lineno);
			/* Answer it on the channel */
			if (w->chan) {
				mark_answered(w->chan);
			} else {
				ast_log(LOG_WARNING, "No call in progress, ignoring call answer\n");
			}
		}
		w->state = OFFHOOK;
	} else if (!strcasecmp(action, "N")) {
		ast_verb(5, "On-hook on line %d\n", lineno);
		w->state = ONHOOK;
	} else if (!strcasecmp(action, "R")) {
		ast_verb(5, "First ring on line %d\n", lineno);
		w->state = RINGING;
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
		} else {
			ast_log(LOG_NOTICE, "%s call %s on line %d to %s\n", outbound ? "Outbound" : "Inbound", "began", lineno, numberstr);
		}
		/* Log/store the appropriate details */
		handle_hook(w, outbound, callend, duration, numberstr, cnam);
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
			return -1;
		}
	}

	if (set_settings(&pfd)) {
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
	if (!unloading) {
		serial_monitor(varg);
	}
	if (!unloading) {
		ast_log(LOG_ERROR, "Serial monitor thread exited prematurely, refresh module to reinitialize\n");
	}
	return NULL;
}

static char *handle_show_whozz(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
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
	ast_cli(a->fd, "%4s %-8s %s\n", "Line", "State", "Associated Device");
	AST_RWLIST_RDLOCK(&lines);
	AST_RWLIST_TRAVERSE(&lines, w, entry) {
		ast_cli(a->fd, "%4d %-8s %s\n", w->lineno, state_str(w->state), S_OR(w->device, ""));
	}
	AST_RWLIST_UNLOCK(&lines);

	return CLI_SUCCESS;
}

static struct ast_cli_entry whozz_cli[] = {
	AST_CLI_DEFINE(handle_show_whozz, "List WHOZZ Calling lines"),
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
