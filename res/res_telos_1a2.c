/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2025, Naveen Albert
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
 * \brief Telos 1A2 Interface Module
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <pthread.h>
#include <signal.h>
#include <termios.h>

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/config.h"
#include "asterisk/cli.h"
#include "asterisk/causes.h"
#include "asterisk/stasis_channels.h"
#include "asterisk/format_cache.h"
#include "asterisk/conversions.h"

/*** DOCUMENTATION
	<configInfo name="res_telos_1a2" language="en_US">
		<synopsis>Telos 1A2 Interface Module</synopsis>
		<configFile name="res_telos_1a2.conf">
			<configObject name="general">
				<configOption name="device" default="/dev/ttyS0">
					<synopsis>Serial device</synopsis>
					<description>
						<para>Serial device to use (at 1200 baud).</para>
						<para>Currently, only one serial device is supported.</para>
					</description>
				</configOption>
			</configObject>
			<configObject name="lines">
				<configOption name="line">
					<synopsis>1A2 line number</synopsis>
					<description>
						<para>Line number of 1A2 channel (e.g. 1, 2, 3, 4, etc.)</para>
					</description>
				</configOption>
				<configOption name="fxodevice">
					<synopsis>Asterisk FXO device</synopsis>
					<description>
						<para>Asterisk FXO device corresponding to this 1A2 line.</para>
						<para>Music on hold will be streamed onto the line using this FXO port.</para>
						<para>This does not require that the FXS line be served by this Asterisk system,
						but it may not work with all equipment. If you do not hear music on hold when
						putting a call on hold from the 1A2 system, the resistance may be too high
						for audio to pass. If the line is served by Asterisk, you can use
						<literal>fxsdevice</literal> instead.</para>
						<para>Music on hold provided using this option will generally have
						very low volume, due to the high resistance of the 1A2 line on hold.
						For better quality, use <literal>fxsdevice</literal> if the line is
						served by Asterisk.</para>
					</description>
				</configOption>
				<configOption name="fxsdevice">
					<synopsis>Asterisk FXS device</synopsis>
					<description>
						<para>Asterisk FXS device corresponding to this 1A2 line. This is specified
						to indicate that this 1A2 line obtains dial tone from this FXS device.</para>
						<para>If provided, music on hold will be queued on the active channel
						of this device, rather than using the FXO device. This may be necessary
						if simply specifying the FXO <literal>device</literal> does not work;
						for instance, on Digium analog cards, the resistance may be too high for
						music on hold audio to pass while a line is on hold by the 1A2 system.
						This option furnishes music on hold directly on the Asterisk channel,
						thus resulting in better quality and compatibility. However, this option
						only works for lines served by the specified Asterisk FXS device.</para>
						<para>Currently, only DAHDI devices are supported for this setting.</para>
					</description>
				</configOption>
				<configOption name="moh_class">
					<synopsis>Music on hold class</synopsis>
					<description>
						<para>The music on hold class to use for this line.</para>
						<para>If specified, whenever this 1A2 line is put on hold,
						this music on hold class will be streamed to the line.</para>
						<para>An FXO device must be specified for use with this option.</para>
					</description>
				</configOption>
				<configOption name="hold_context">
					<synopsis>Dialplan context to execute when a 1A2 line goes on hold.</synopsis>
					<description>
						<para>Dialplan context to execute when a 1A2 line goes on hold.</para>
						<para>The s extension will be executed, beginning at priority 1.</para>
						<para>If both <literal>moh_class</literal> and <literal>hold_context</literal>
						are specified, <literal>hold_context</literal> will take precedence.</para>
					</description>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
	<function name="1A2_LINE_STATE" language="en_US">
		<synopsis>
			Returns the line state of a 1A2 key system line
		</synopsis>
		<syntax>
			<parameter name="line" required="true">
				<para>1A2 line number</para>
			</parameter>
		</syntax>
		<description>
			<para>Returns the line state of the specified line.</para>
			<para>The possible values are:</para>
			<enumlist>
				<enum name="UNKNOWN"/>
				<enum name="ONHOOK"/>
				<enum name="OFFHOOK"/>
				<enum name="HOLD"/>
				<enum name="RINGING"/>
			</enumlist>
		</description>
	</function>
 ***/

#define MODULE_NAME "res_telos_1a2"
#define CONFIG_FILE "res_telos_1a2.conf"

/* From Telos 1A2 Interface Module manual, p. 49 */
enum telos_action {
	TELOS_OFF,	/*!< Indicates line is off (on hook) */
	TELOS_ON,	/*!< Indicates line is active on main bank */
	TELOS_HOLD,	/*!< Indicates line is on hold */
	TELOS_SCREENED_HOLD,	/*!< Indicates line is in "screened hold" mode */
	TELOS_CONF,	/*!< Indicates line is active on conference bank */
	TELOS_LOCK,	/*!< Indicates line has been locked on */
	TELOS_BUSIED,	/*!< Indicates line has been "busied-out" */
	TELOS_ELSEWHERE,	/*!< Indicates line is on "elsewhere" (off hook) */
	TELOS_RINGING,		/*!< Indicates line is ringing-in */
	TELOS_FUNCTION,		/*!< Indicates press of function button on console */
};

static const char *telos_action_name(enum telos_action action)
{
	switch (action) {
	case TELOS_OFF: return "Off";
	case TELOS_ON: return "On";
	case TELOS_HOLD: return "Hold";
	case TELOS_SCREENED_HOLD: return "Screened Hold";
	case TELOS_CONF: return "Conference";
	case TELOS_LOCK: return "Locked On";
	case TELOS_BUSIED: return "Busied Out";
	case TELOS_ELSEWHERE: return "On Elsewhere";
	case TELOS_RINGING: return "Ringing In";
	case TELOS_FUNCTION: return "Function Button";
	}
	return "";
}

enum line_state {
	/* Until activity occurs on a line, the state will stay as 'unknown', which is true since we don't know what's going on. */
	UNKNOWN = 0,
	ONHOOK,
	OFFHOOK,
	HOLD,
	RINGING,
};

struct telos_line {
	int lineno;							/*!< Line number on telos system */
	const char *fxodevice;				/*!< Asterisk FXO device */
	const char *fxsdevice;				/*!< Asterisk FXS device */
	const char *moh_class;				/*!< Music on hold class */
	const char *hold_context;			/*!< Hold dialplan context */
	struct ast_channel *moh_chan;		/*!< Music on hold channel */
	enum line_state state;				/*!< Current line state */
	AST_LIST_ENTRY(telos_line) entry;	/*!< Next channel */
	char fxs_activechan[80];			/*!< Channel on which hold was queued */
	char data[];
};

static AST_RWLIST_HEAD_STATIC(lines, telos_line);

static pthread_t serial_thread = AST_PTHREADT_NULL;
static int thread_running = 0;

#define MAX_POLL_DELAY 2000

static char serial_device[256] = "";
static int serial_fd = -1;
static int query_success = 0;
static int unloading = 0;

static const char *state_str_cli(enum line_state state)
{
	switch (state) {
	case UNKNOWN:
		return "Unknown";
	case ONHOOK:
		return "On Hook";
	case OFFHOOK:
		return "Off Hook";
	case HOLD:
		return "Hold";
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
	case HOLD:
		return "HOLD";
	case RINGING:
		return "RINGING";
	}
	__builtin_unreachable();
}

static int serial_getchunk(struct pollfd *pfd, char *restrict buf, size_t len)
{
	const char *bufstart = buf;
	size_t total = 0;
	for (;;) {
		ssize_t bufres = 1;
		errno = 0;
		bufres = poll(pfd, 1, MAX_POLL_DELAY);
		if (bufres <= 0) {
			if (!total) {
				ast_log(LOG_WARNING, "Failed to poll serial port (%ld): %s\n", bufres, strerror(errno));
				return -1;
			}
			buf[bufres] = '\0';
			ast_debug(3, "Read partial line (%ld bytes) '%s'\n", total, buf);
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
		/* Should begin with one of these */
		if (!strchr("FNHSCLBERU", bufstart[0])) {
			ast_log(LOG_WARNING, "First character received is not valid (%c)\n", bufstart[0]);
			return -1;
		}
		/* Check if received a complete message (3 characters) */
		if (total == 3) {
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

static void cleanup_moh_chan(struct ast_channel *chan)
{
	/* We don't own the channel, we just have a reference to it. Make it hang up before unreffing it. */
	ast_channel_lock(chan);
	ast_softhangup(chan, AST_SOFTHANGUP_EXPLICIT);
	ast_channel_unlock(chan);

	ast_debug(3, "Unreffing channel %s\n", ast_channel_name(chan));
	ast_channel_unref(chan);
}

/* Stop music on hold. Just hang up the FXO device */
#define STOP_MOH(t) \
	if (t->moh_chan) { \
		ast_verb(5, "Stopping music on hold for 1A2 line %d\n", t->lineno); \
		cleanup_moh_chan(t->moh_chan); \
		t->moh_chan = NULL; \
	} else if (t->fxs_activechan[0]) { \
		struct ast_channel *ochan = ast_channel_get_by_name(t->fxs_activechan); \
		if (ochan) { \
			ast_queue_unhold(ochan); \
			ast_channel_unref(ochan); \
		} else { \
			ast_debug(2, "Channel %s doesn't exist anymore\n", t->fxs_activechan); \
		} \
		t->fxs_activechan[0] = '\0'; \
	}

static int start_moh(struct telos_line *t)
{
	struct ast_format_cap *cap;
	char *chantech, *chandata;
	int res, reason;

	if (ast_strlen_zero(t->hold_context) && ast_strlen_zero(t->moh_class)) {
		ast_debug(3, "No hold context or music on hold class specified\n");
		return 0;
	}

	/* We need an FXO device (though not necessarily a DAHDI device) for music on hold */
	if (!ast_strlen_zero(t->fxsdevice)) {
		if (!strncasecmp(t->fxsdevice, "DAHDI/", 6)) {
			struct ast_channel *chan;
			char active_channel[80] = ""; /* AST_MAX_CHANNEL */
			char func_args[64];
			int dahdichan = atoi(t->fxsdevice + 6);
			/* We need to obtain the Asterisk channel that is currently in the "foreground"
			 * on this DAHDI channel - that would be the owner.
			 * This is first and foremost because we need an Asterisk channel onto which to queue MOH,
			 * not just a DAHDI channel, but also so we can stop MOH on the right channel
			 * if the subchannels switch around while the 1A2 line is on hold. */
			snprintf(func_args, sizeof(func_args), "DAHDI_CHANNEL(owner,%d)", dahdichan);
			if (ast_func_read(NULL, func_args, active_channel, sizeof(active_channel))) {
				ast_log(LOG_WARNING, "Couldn't obtain active Asterisk channel on DAHDI channel %d\n", dahdichan);
				return -1;
			}
			chan = ast_channel_get_by_name(active_channel);
			if (chan) {
				/* Indicate music on hold by queuing a HOLD on the active channel for this device */
				ast_verb(5, "Queued hold on %s using class '%s'\n", active_channel, t->moh_class);
				ast_queue_hold(chan, t->moh_class);
				ast_channel_unref(chan);
				ast_copy_string(t->fxs_activechan, active_channel, sizeof(t->fxs_activechan));
				return 0;
			} else {
				ast_log(LOG_ERROR, "Couldn't find channel %s\n", active_channel);
				return -1;
			}
		} else {
			ast_log(LOG_ERROR, "Only DAHDI channels are supported for fxsdevice\n");
			return -1;
		}
	}
	if (ast_strlen_zero(t->fxodevice)) {
		ast_log(LOG_ERROR, "Music on hold requested for line %d, but no FXO device specified\n", t->lineno);
		return -1;
	}

	/* Start music on hold */
	if (t->moh_chan) {
		STOP_MOH(t); /* If we already have one, for some reason, get rid of it */
	}

	chandata = ast_strdupa(t->fxodevice);
	chantech = strsep(&chandata, "/");
	if (!chandata) {
		ast_log(LOG_ERROR, "No data provided after channel type (%s)\n", t->fxodevice);
		return -1;
	}

	if (!(cap = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT))) {
		return -1;
	}
	ast_format_cap_append(cap, ast_format_slin, 0);
	if (t->hold_context) {
		res = ast_pbx_outgoing_exten(chantech, cap, chandata, 5000, t->hold_context, "s", 1,
			&reason, AST_OUTGOING_NO_WAIT, NULL, NULL, NULL, NULL, &t->moh_chan, 1, NULL);
	} else {
		res = ast_pbx_outgoing_app(chantech, cap, chandata, 5000, "MusicOnHold", t->moh_class,
			&reason, AST_OUTGOING_NO_WAIT, NULL, NULL, NULL, NULL, &t->moh_chan, NULL);
	}
	ao2_ref(cap, -1);
	if (res) {
		const char *desc;
		switch (reason) {
		case 0:
		case AST_CONTROL_ANSWER:
			desc = "Success";
			break;
		case AST_CONTROL_BUSY:
			desc = "Busy";
			break;
		case AST_CONTROL_CONGESTION:
			desc = "Congestion";
			break;
		case AST_CONTROL_HANGUP:
			desc = "Hangup";
			break;
		case AST_CONTROL_RINGING:
			desc = "Ringing";
			break;
		default:
			desc = "Unknown";
			break;
		}
		ast_log(LOG_WARNING, "Couldn't dial %s (%s)\n", t->fxodevice, desc);
	} else {
		ast_channel_unlock(t->moh_chan); /* Was returned locked and reference bumped */
		ast_verb(5, "%s (%s) for 1A2 line %d\n", !ast_strlen_zero(t->hold_context) ? "Launched hold dialplan" : "Started music on hold", S_OR(t->hold_context, t->moh_class), t->lineno);
	}
	return 0;
}

static int __process_serial_read(struct telos_line *t, int lineno, enum telos_action action)
{
	enum line_state oldstate = t->state;

	switch (action) {
	case TELOS_CONF:
	case TELOS_LOCK:
	case TELOS_BUSIED:
	case TELOS_FUNCTION:
		ast_log(LOG_NOTICE, "Ignoring %s on line %d\n", telos_action_name(action), lineno);
		return 0;
	case TELOS_OFF: /* On hook */
		t->state = ONHOOK;
		ast_verb(5, "On-hook on 1A2 line %d\n", lineno);
		break;
	case TELOS_ON:
	case TELOS_ELSEWHERE: /* Off hook */
		t->state = OFFHOOK;
		ast_verb(5, "Off-hook on 1A2 line %d\n", lineno);
		break;
	case TELOS_HOLD:
	case TELOS_SCREENED_HOLD: /* Hold */
		t->state = HOLD;
		ast_verb(5, "Hold on 1A2 line %d\n", lineno);
		break;
	case TELOS_RINGING:
		t->state = RINGING;
		ast_verb(5, "Ringing on 1A2 line %d\n", lineno);
		break;
	}

	/*** DOCUMENTATION
		<managerEvent language="en_US" name="TelosLineStateChange">
			<managerEventInstance class="EVENT_FLAG_CALL">
				<synopsis>Raised when the state of a 1A2 line connected via a Telos 1A2 Interface Module changes.</synopsis>
				<syntax>
					<channel_snapshot/>
					<parameter name="LineNumber">
						<para>The 1A2 line number</para>
					</parameter>
					<parameter name="CurrentState">
						<para>The current state of the line.</para>
						<para>The possible values are:</para>
						<enumlist>
							<enum name="UNKNOWN"/>
							<enum name="ONHOOK"/>
							<enum name="OFFHOOK"/>
							<enum name="HOLD"/>
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
							<enum name="HOLD"/>
							<enum name="RINGING"/>
						</enumlist>
					</parameter>
				</syntax>
				<description>
					<para>This event is raised whenever the line state of a 1A2 line changes.</para>
				</description>
			</managerEventInstance>
		</managerEvent>
	***/
	manager_event(EVENT_FLAG_CALL, "TelosLineStateChange",
		"LineNumber: %d\r\n"
		"CurrentState: %s\r\n"
		"OldState: %s\r\n",
		t->lineno,
		state_str_func(t->state),
		state_str_func(oldstate));

	switch (action) {
	case TELOS_OFF: /* On hook - if line hung up while on hold, also stop */
	case TELOS_ON:
	case TELOS_ELSEWHERE: /* Off hook */
		STOP_MOH(t); /* Stop music on hold or dialplan for this 1A2 line */
		break;
	case TELOS_HOLD:
	case TELOS_SCREENED_HOLD: /* Hold */
		start_moh(t); /* Start music on hold for this 1A2 line */
		break;
	default:
		break;
	}

	return 0;
}

static int process_serial_read(char *buf)
{
	struct telos_line *t;
	enum telos_action action;
	int lineno;
	int res = -1;

	switch (buf[0]) {
	case 'F':
		action = TELOS_OFF;
		break;
	case 'N':
		action = TELOS_ON;
		break;
	case 'H':
		action = TELOS_HOLD;
		break;
	case 'S':
		action = TELOS_SCREENED_HOLD;
		break;
	case 'C':
		action = TELOS_CONF;
		break;
	case 'L':
		action = TELOS_LOCK;
		break;
	case 'B':
		action = TELOS_BUSIED;
		break;
	case 'E':
		action = TELOS_ELSEWHERE;
		break;
	case 'R':
		action = TELOS_RINGING;
		break;
	case 'U':
		action = TELOS_FUNCTION;
		break;
	default:
		ast_log(LOG_ERROR, "Unexpected command: %s\n", buf);
		return -1;
	}

	/* Next two characters are the line number, parse it out */
	lineno = atoi(buf + 1); /* Don't care about leading 0s */
	if (lineno < 0) {
		ast_log(LOG_WARNING, "Invalid line number: %s\n", buf + 1);
		return -1;
	}
	lineno++; /* They are 0-indexed on the Telos so add one */

	AST_RWLIST_TRAVERSE(&lines, t, entry) {
		if (t->lineno == lineno) {
			res = __process_serial_read(t, lineno, action);
			break;
		}
	}

	if (!t) {
		/* We couldn't find a line for this line number */
		ast_log(LOG_WARNING, "No configuration for line %d\n", lineno);
	}

	return res;
}

static int query_line(struct pollfd *pfd, int lineno)
{
	char inbuf[32], outbuf[4];
	ssize_t res;
	int bytes, i = 0;

	snprintf(inbuf, sizeof(inbuf), "Q%02d", lineno - 1); /* 0-indexed and 0-padded */

	/* First, if there's any data pending, drain it */
	if (tcflush(serial_fd, TCIOFLUSH)) {
		ast_log(LOG_ERROR, "Failed to flush serial line: %s\n", strerror(errno));
	}

	/* Query the line
	 * This command is useful since it always solicits a response, so we can use it as a health check on the unit,
	 * and it gives us the status of the line currently when the module initializes, so we don't need to wait for an event. */
	if (write(serial_fd, inbuf, 3) != 3) {
		ast_log(LOG_ERROR, "write failed: %s\n", strerror(errno));
		return -1;
	}

	res = poll(pfd, 1, 2000);
	if (!res) {
		ast_log(LOG_ERROR, "No response from unit to command when querying line %d\n", lineno);
		return -1;
	} else if (res < 0) {
		ast_log(LOG_ERROR, "poll failed: %s\n", strerror(errno));
		return -1;
	}

	/* We expect 3 bytes in the response. If we haven't gotten them all yet, wait a bit.
	 * Since the serial line is running at 1200 baud, we expect ~150 characters per second,
	 * or a character about once every 6-7 milliseconds.
	 * We've read at least one character already, so we should need to wait no more than ~15 ms.
	 * In practice, sometimes it can take longer. */
	do {
		res = ioctl(serial_fd, FIONREAD, &bytes);
		if (res < 0) {
			ast_log(LOG_ERROR, "ioctl failed: %s\n", strerror(errno));
		} else if (bytes >= 3) {
			break;
		} else {
			/* poll is no good here, it'll return immediately since there's already data waiting to be read. */
			ast_debug(3, "Only received %d byte%s so far, sleeping...\n", bytes, ESS(bytes));
			usleep(10000);
		}
	} while (i++ < 10);

	res = read(serial_fd, outbuf, sizeof(outbuf) - 1);
	if (res <= 0) {
		ast_log(LOG_ERROR, "read failed: %s\n", strerror(errno));
		return -1;
	}
	outbuf[res] = '\0';
	ast_debug(3, "Sent '%s', received '%s'\n", inbuf, outbuf);
	if (res != 3 || strcmp(inbuf + 1, outbuf + 1)) {
		ast_log(LOG_WARNING, "Received mismatched response for line %d: '%s'\n", lineno, outbuf);
		return -1;
	}
	process_serial_read(outbuf); /* Save status */
	return 0;
}

static int resync(struct pollfd *pfd)
{
	struct telos_line *t;
	int res = 0, qres;

	/* Initialize the status of each line.
	 * We grab a WRLOCK to prevent doing operations at the same time as the actual serial thread (if this is being run from the CLI command). */
	AST_RWLIST_WRLOCK(&lines);
	AST_RWLIST_TRAVERSE(&lines, t, entry) {
		qres = query_line(pfd, t->lineno);
		if (qres) {
			ast_log(LOG_ERROR, "Failed to query status of line %d\n", t->lineno);
		}
		res |= qres;
	}
	query_success = res ? 0 : 1;
	AST_RWLIST_UNLOCK(&lines);

	return res;
}

static int serial_loop(struct pollfd *pfd)
{
	for (;;) {
		int res, bytes;
		ssize_t bufres;
		char buf[4]; /* Each chunk is only 3 characters */

		bufres = poll(pfd, 1, -1);
		if (bufres <= 0) {
			if (unloading) {
				return -1;
			}
			ast_log(LOG_ERROR, "Failed to poll serial port: %s\n", strerror(errno));
			break;
		}
		AST_RWLIST_RDLOCK(&lines);
		/* It's possible we woke up in poll() due to a resync from a CLI thread.
		 * Now that we grabbed the WRLOCK, we no that no other thread is doing I/O on this file descriptor.
		 * Check if there's anything still waiting or if it was a CLI thread that already read the data. */
		res = ioctl(serial_fd, FIONREAD, &bytes);
		if (res) {
			ast_log(LOG_ERROR, "ioctl failed: %s\n", strerror(errno));
		} else if (bytes > 0) {
			bufres = serial_getchunk(pfd, buf, sizeof(buf));
			if (bufres == 3) {
				process_serial_read(buf);
			}
		}
		AST_RWLIST_UNLOCK(&lines);
	}

	ast_debug(1, "Serial loop thread exiting\n");
	return 0;
}

static void *serial_monitor(void *varg)
{
	struct pollfd pfd;

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = serial_fd;
	pfd.events = POLLIN | POLLERR | POLLNVAL | POLLHUP;

	while (!ast_fully_booted && !unloading) {
		usleep(50000);
	}
	if (resync(&pfd)) {
		ast_log(LOG_ERROR, "Failed to synchronize with Telos unit, aborting thread! Refresh module to resync\n");
		return NULL;
	}

	thread_running = 1;
	if (!unloading) {
		do {
			serial_loop(&pfd);
		} while (!unloading && serial_fd != -1);
	}
	thread_running = 0;

	if (!unloading) {
		ast_log(LOG_ERROR, "Serial monitor thread exited prematurely, refresh module to reinitialize\n");
	}
	return NULL;
}

static int telos_line_state_read(struct ast_channel *chan, const char *cmd, char *parse, char *buffer, size_t buflen)
{
	int lineno;
	struct telos_line *t;

	if (ast_strlen_zero(parse)) {
		ast_log(LOG_ERROR, "Line number required for %s\n", cmd);
		return -1;
	}

	lineno = atoi(parse);
	AST_RWLIST_RDLOCK(&lines);
	AST_RWLIST_TRAVERSE(&lines, t, entry) {
		if (t->lineno == lineno) {
			ast_copy_string(buffer, state_str_func(t->state), buflen);
			break;
		}
	}
	AST_RWLIST_UNLOCK(&lines);

	return t ? 0 : -1;
}

static struct ast_custom_function acf_telos = {
	.name = "1A2_LINE_STATE",
	.read = telos_line_state_read,
};

static char *handle_show_lines(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct telos_line *t;

	switch (cmd) {
	case CLI_INIT:
		e->command = "telos show lines";
		e->usage =
			"Usage: telos show lines\n"
			"       Show any configured Telos 1A2 lines.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}
	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	ast_cli(a->fd, "Serial device is %s (fd %d)\n", serial_device, serial_fd);
	ast_cli(a->fd, "Serial status is %s\n", thread_running ? query_success ? "RUNNING" : "STALLED" : "ABORTED");
	ast_cli(a->fd, "%4s %-8s %17s %17s %20s %s\n", "Line", "State", "FXO Device", "FXS Device/Chan", "MOH Class", "Hold Context");
	AST_RWLIST_RDLOCK(&lines);
	AST_RWLIST_TRAVERSE(&lines, t, entry) {
		const char *fxschannel = S_OR(t->fxs_activechan, S_OR(t->fxsdevice, "")); /* If we have an active FXS channel, use that as it's more specific, otherwise just the device itself */
		ast_cli(a->fd, "%4d %-8s %17s %17s %20s %s\n", t->lineno, state_str_cli(t->state), S_OR(t->fxodevice, ""), fxschannel, S_OR(t->moh_class, ""), S_OR(t->hold_context, ""));
	}
	AST_RWLIST_UNLOCK(&lines);

	return CLI_SUCCESS;
}

static char *handle_resync(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct pollfd pfd;

	switch (cmd) {
	case CLI_INIT:
		e->command = "telos resync";
		e->usage =
			"Usage: telos resync\n"
			"       Resynchronize with the connected unit.\n";
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

	if (resync(&pfd)) {
		return CLI_FAILURE;
	}

	return CLI_SUCCESS;
}

static struct ast_cli_entry telos_cli[] = {
	AST_CLI_DEFINE(handle_show_lines, "List Telos 1A2 lines"),
	AST_CLI_DEFINE(handle_resync, "Resynchronize with Telos unit"),
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
			struct telos_line *t;
			char *data;
			const char *fxodevice = NULL, *fxsdevice = NULL, *moh_class = NULL, *hold_context = NULL;
			for (var = ast_variable_browse(cfg, cat); var; var = var->next) {
				if (!strcasecmp(var->name, "line")) {
					if (ast_str_to_int(var->value, &tmp)) {
						ast_log(LOG_WARNING, "Invalid line number '%s'\n", var->value);
						ast_config_destroy(cfg);
						return -1;
					}
				} else if (!strcasecmp(var->name, "fxodevice")) {
					fxodevice = var->value;
				} else if (!strcasecmp(var->name, "fxsdevice")) {
					if (strncasecmp(var->value, "DAHDI/", 6)) {
						ast_log(LOG_WARNING, "Only DAHDI devices are supported for fxsdevice\n");
						continue;
					}
					fxsdevice = var->value;
				} else if (!strcasecmp(var->name, "moh_class")) {
					moh_class = var->value;
				} else if (!strcasecmp(var->name, "hold_context")) {
					hold_context = var->value;
				} else {
					ast_log(LOG_WARNING, "Unknown setting at line %d: '%s'\n", var->lineno, var->name);
				}
			}
			AST_RWLIST_TRAVERSE(&lines, t, entry) {
				if (t->lineno == tmp) {
					ast_log(LOG_ERROR, "Line %d already configured\n", tmp);
					return -1;
				}
			}
			t = ast_calloc(1, sizeof(*t) + (fxodevice ? strlen(fxodevice) + 1 : 0) + (fxsdevice ? strlen(fxsdevice) + 1 : 0)
				+ (moh_class ? strlen(moh_class) + 1 : 0) + (hold_context ? strlen(hold_context) + 1 : 0));
			if (!t) {
				ast_config_destroy(cfg);
				return -1;
			}
			t->lineno = tmp;
			data = t->data;
			if (!ast_strlen_zero(fxodevice)) {
				strcpy(data, fxodevice); /* Safe */
				t->fxodevice = data;
				data += strlen(fxodevice) + 1;
			}
			if (!ast_strlen_zero(fxsdevice)) {
				strcpy(data, fxsdevice); /* Safe */
				t->fxsdevice = data;
				data += strlen(fxsdevice) + 1;
			}
			if (!ast_strlen_zero(moh_class)) {
				strcpy(data, moh_class); /* Safe */
				t->moh_class = data;
				data += strlen(moh_class) + 1;
			}
			if (!ast_strlen_zero(hold_context)) {
				strcpy(data, hold_context); /* Safe */
				t->hold_context = data;
			}
			if (ast_strlen_zero(fxodevice) && !ast_strlen_zero(hold_context)) {
				ast_log(LOG_ERROR, "The 'hold_context' setting requires 'fxodevice' to be set\n");
			}
			if (!ast_strlen_zero(fxsdevice) && ast_strlen_zero(moh_class)) {
				ast_log(LOG_WARNING, "The 'fxsdevice' setting requires 'moh_class' to be set to customize music on hold\n");
			}
			AST_RWLIST_INSERT_TAIL(&lines, t, entry);
		}
	}

	ast_config_destroy(cfg);

	/* This option intentionally has no default,
	 * so that if the stock config file has not been modified,
	 * we assume this module isn't needed and decline to load,
	 * since we can't do anything useful without the associated equipment. */
	if (ast_strlen_zero(serial_device)) {
		ast_log(LOG_NOTICE, "No serial device specified, declining to load\n");
		return -1;
	}

	return 0;
}

static void free_lines(void)
{
	struct telos_line *t;

	AST_RWLIST_WRLOCK(&lines);
	while ((t = AST_RWLIST_REMOVE_HEAD(&lines, entry))) {
		STOP_MOH(t);
		ast_free(t);
	}
	AST_RWLIST_UNLOCK(&lines);
}

static int unload_module(void)
{
	unloading = 1;
	ast_cli_unregister_multiple(telos_cli, ARRAY_LEN(telos_cli));
	ast_custom_function_unregister(&acf_telos);
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

	free_lines();
	return 0;
}

static int load_module(void)
{
	int res = 0;
	struct termios attr;

	if (load_config()) {
		free_lines();
		return AST_MODULE_LOAD_DECLINE;
	}

	/* 1200/N/1 */
	serial_fd = open(serial_device, O_RDWR | O_NONBLOCK | O_NOCTTY);
	if (serial_fd < 0) {
		ast_log(LOG_ERROR, "Failed to open serial device %s: %s\n", serial_device, strerror(errno));
		unload_module();
		return AST_MODULE_LOAD_DECLINE;
	}

	/* Set speed to 1200 baud. Ignore errors in case an IP/serial device is being used. */
	tcgetattr(serial_fd, &attr);
	cfsetispeed(&attr, B1200);
	cfsetospeed(&attr, B1200);
	tcsetattr(serial_fd, TCSANOW, &attr);

	ast_verb(5, "Opened serial device %s\n", serial_device);

	if (ast_pthread_create_background(&serial_thread, NULL, serial_monitor, NULL) < 0) {
		ast_log(LOG_ERROR, "Unable to start periodic thread\n");
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_custom_function_register(&acf_telos);
	ast_cli_register_multiple(telos_cli, ARRAY_LEN(telos_cli));
	return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "1A2 Music On Hold Module",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
);
