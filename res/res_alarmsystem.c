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
 * \brief Simple Alarm System
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * *** WARNING *** WARNING *** WARNING *** WARNING *** WARNING *** WARNING *** WARNING *** WARNING ***
 *
 * Use at your own risk. Please consult the GNU GPL license document included with Asterisk.         *
 *
 * *** WARNING *** WARNING *** WARNING *** WARNING *** WARNING *** WARNING *** WARNING *** WARNING ***
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <signal.h> /* use pthread_kill */

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/channel.h"
#include "asterisk/app.h"
#include "asterisk/module.h"
#include "asterisk/config.h"
#include "asterisk/cli.h"
#include "asterisk/io.h"
#include "asterisk/sched.h"
#include "asterisk/format_cache.h"
#include "asterisk/causes.h"
#include "asterisk/indications.h"

/*** DOCUMENTATION
	<configInfo name="res_alarmsystem" language="en_US">
		<synopsis>Asterisk-based alarm system solution</synopsis>
		<configFile name="res_alarmsystem.conf">
			<configObject name="general">
				<configOption name="bindport" default="4589">
					<synopsis>Server bindport</synopsis>
					<description>
						<para>Port to which to bind for server instances. If only configuring alarm clients, this setting is not used.</para>
					</description>
				</configOption>
				<configOption name="bindaddr" default="0.0.0.0">
					<synopsis>Server bind address</synopsis>
					<description>
						<para>Specific bind address, useful if you have multiple IP interfaces.</para>
					</description>
				</configOption>
			</configObject>
			<configObject name="server">
				<synopsis>Configuration section for an alarm server, to which alarm clients can report</synopsis>
				<description>
					<para>A <emphasis>server</emphasis> simply receives reporting events from clients, but does not
					itself have any sensors connected to it. The server can then facilitate further actions based
					on the state of its clients.</para>
					<para>Configuring a server for client reporting is optional, but strongly recommended.</para>
				</description>
				<configOption name="type">
					<synopsis>Must be of type 'server'.</synopsis>
				</configOption>
				<configOption name="ip_loss_tolerance">
					<synopsis>IP loss tolerance</synopsis>
					<description>
						<para>Number of seconds the server will tolerate not receiving pings from clients before considering IP connectivity to a client to have been lost (which will trigger the internet_lost alarm).</para>
					</description>
				</configOption>
				<configOption name="contexts">
					<synopsis>Dialplan context map</synopsis>
					<description>
						<para>Refers to the config section defining which dialplan contexts to execute when certain alarm events occur.</para>
					</description>
				</configOption>
				<configOption name="logfile">
					<synopsis>Log file for this server</synopsis>
					<description>
						<para>Path of CSV log file to which to log events for this server.</para>
						<para>The logged parameters are timestamp, client ID, sequence number (or 0 if the event is inferred by the server), MMSS timestamp, event type name, sensor ID (if applicable), and any extra data (if applicable).</para>
						<para>Normally, the MMSS timestamp information is redundant; however, during an Internet outage, if events are reported by phone, the timestamp logged in the CSV will be delayed from when the event actually occured; the MMSS data allows deducing when the event actually occured, as indicated by the client.</para>
					</description>
				</configOption>
			</configObject>
			<configObject name="clients">
				<synopsis>Special configuration section defining all clients authorized to report to this server</synopsis>
				<description>
					<para>In this section, each key-value entry defines the client ID and corresponding client PIN, used for authenticating clients reporting to this server by IP or by phone.</para>
				</description>
			</configObject>
			<configObject name="client">
				<synopsis>Configuration section for each client instance</synopsis>
				<description>
					<para>A <emphasis>client</emphasis> is defined for each client that has sensors connected to it,
					and which reports to a corresponding alarm server for reporting purposes. A client/server relationship
					is referred to in the configuration as an 'alarm peering', though this is a client-server relationship,
					NOT a peer-to-peer relationship.</para>
					<para>Reporting to a server is optional and not required. However, it is strongly recommended
					to provide redundancy to some of the alarm functions. For example, if a burglar were to break in
					and immediately tamper with the alarm system client machine, the server machine would still
					know that a break-in occured and alert you.</para>
					<para>Each client is defined in its own configuration section.</para>
				</description>
				<configOption name="type">
					<synopsis>Must be of type 'client'.</synopsis>
				</configOption>
				<configOption name="client_id">
					<synopsis>Unique ID of client</synopsis>
					<description>
						<para>A unique telenumeric (0-9,A-D) ID which uniquely identifies this client both amongst all clients locally and amongst all clients reporting to the associated server.</para>
						<para>Note that there is no security mechanism to restrict reporting aside from the client ID.
						Therefore, if the alarm server is exposed to the Internet, you may wish to use long, hard-to-guess client IDs to prevent spoofed reports, or lock down your firewall accordingly.</para>
					</description>
				</configOption>
				<configOption name="client_pin">
					<synopsis>PIN to authenticate client</synopsis>
					<description>
						<para>A telenumeric (0-9,A-D) PIN used for client authentication, if required by the server.</para>
						<para>This is useful for authentication purposes, particularly if the server is publicly accessible,
						to prevent spoofing of events by rogue clients.</para>
					</description>
				</configOption>
				<configOption name="server_ip">
					<synopsis>Reporting server address</synopsis>
					<description>
						<para>Server hostname/IP address and port to reach the reporting server over IP.</para>
					</description>
				</configOption>
				<configOption name="server_dialstr">
					<synopsis>Reporting server dial string</synopsis>
					<description>
						<para>A dial string for Dial that will reach the reporting server by phone.</para>
						<para>It is RECOMMENDED that you use an analog POTS line and that the dial string above access this line through an RJ-31X jack.</para>
						<para>This is used as a backup if IP reporting is unavailable. The server receiving this call should call <literal>AlarmEventReceiver</literal> to process the events.</para>
						<para>When reporting an alarm trigger, the line will stay open until disarm_delay has been reached, to avoid making multiple calls in succession.</para>
					</description>
				</configOption>
				<configOption name="ping_interval">
					<synopsis>Client ping interval</synopsis>
					<description>
						<para>Frequency, in seconds, with which to ping the server.</para>
						<para>This should match the equivalent setting on the server side.</para>
					</description>
				</configOption>
				<configOption name="egress_delay">
					<synopsis>Egress delay</synopsis>
					<description>
						<para>Number of seconds grace period to exit without re-triggering alarm.</para>
					</description>
				</configOption>
				<configOption name="contexts">
					<synopsis>Dialplan context map</synopsis>
					<description>
						<para>Refers to the config section defining which dialplan contexts to execute when certain alarm events occur.</para>
					</description>
				</configOption>
				<configOption name="logfile">
					<synopsis>Log file for this client</synopsis>
					<description>
						<para>Path of CSV log file to which to log events for this client.</para>
						<para>The logged parameters are timestamp, client ID, sequence number, MMSS timestamp, event type name, sensor ID (if applicable), and any extra data (if applicable).</para>
					</description>
				</configOption>
			</configObject>
			<configObject name="contexts">
				<synopsis>Configuration section defining dialplan contexts to use for alarm events</synopsis>
				<description>
					<para>This section optionally maps each alarm event to a dialplan context to execute when this event occurs.</para>
					<para>The [exten@]context can be specified. If the exten is omitted, s will be used. The priority will always be 1.</para>
					<para>The following variables will be available to the executed dialplan:</para>
					<variablelist>
						<variable name="ALARMSYSTEM_CLIENTID">
							<para>Client ID of alarm client associated with event.</para>
						</variable>
						<variable name="ALARMSYSTEM_SENSORID">
							<para>Sensor ID of alarm sensor associated with event, if applicable.</para>
						</variable>
						<variable name="ALARMSYSTEM_EVENT">
							<para>Numeric ID of event.</para>
						</variable>
					</variablelist>
				</description>
				<configOption name="type">
					<synopsis>Must be of type 'contexts'.</synopsis>
				</configOption>
				<configOption name="okay">
					<synopsis>Dialplan location to execute upon normal initialization</synopsis>
				</configOption>
				<configOption name="triggered">
					<synopsis>Dialplan location to execute when an alarm sensor is triggered</synopsis>
				</configOption>
				<configOption name="restored">
					<synopsis>Dialplan location to execute when an alarm sensor returns to normal state</synopsis>
				</configOption>
				<configOption name="disarmed">
					<synopsis>Dialplan location to execute when the alarm is disarmed by user</synopsis>
				</configOption>
				<configOption name="tempdisarmed">
					<synopsis>Dialplan location to execute when the alarm is temporarily disarmed by user</synopsis>
					<description>
						<para>Temporarily disarmed in this context refers to indicating that the user is about to egress and thus the next sensor trigger
						should be ignored (not treated as a trigger for the alarm). The delay can be controlled using the
						<literal>egress_delay</literal> setting.</para>
						<para>Currently, there is no way to permanently disarm the alarm.</para>
					</description>
				</configOption>
				<configOption name="breach">
					<synopsis>Dialplan location to execute when a breach (failure to disarm active alarm) has occured</synopsis>
				</configOption>
				<configOption name="internet_lost">
					<synopsis>Dialplan location to execute when Internet connectivity to alarm peer has been lost</synopsis>
				</configOption>
				<configOption name="internet_restored">
					<synopsis>Dialplan location to execute when Internet connectivity to alarm peer has been restored</synopsis>
				</configOption>
			</configObject>
			<configObject name="sensor">
				<synopsis>Configuration section for each sensor</synopsis>
				<description>
					<para>A sensor is a specific sensor input to the alarm system that will trigger an alarm, such as a door sensor or a window sensor. Any normal sensor can be used; simply wire the sensor appropriately to an extension on the system so that in the normal state, the sensor loop is open (on hook) and in the off-normal state, the sensor loop is closed (off hook). This can be accomplished by wiring either tip/ring to COMMON and the other wire to either NORMALLY OPEN or NORMALLY CLOSED, depending on your sensor.</para>
					<para>Note that if the sensor loop is cut, no event will be triggered, unlike in a conventional alarm loop system.</para>
					<para>Each sensor is defined in its own configuration section.</para>
					<para>It is strongly recommend that sensor loops be connected to analog telephony cards (using chan_dahdi), as any other
					configuration involves additional failure points. This approach should work as long as your Asterisk server has power.</para>
				</description>
				<configOption name="type">
					<synopsis>Must be of type 'sensor'.</synopsis>
				</configOption>
				<configOption name="sensor_id">
					<synopsis>Unique telenumeric (0-9,A-D) ID of sensor</synopsis>
					<description>
						<para>A unique numeric ID which uniquely identifies this sensor both amongst all clients locally and amongst all clients reporting to the associated server.</para>
					</description>
				</configOption>
				<configOption name="client">
					<synopsis>Client associated with this sensor</synopsis>
					<description>
						<para>The name of the client associated with this sensor.</para>
					</description>
				</configOption>
				<configOption name="device">
					<synopsis>Device used for this sensor</synopsis>
					<description>
						<para>Device used for this sensor</para>
						<para>This setting is optional but, if configured, will automatically associate calls to <literal>AlarmSensor</literal> with this sensor by the device used, so the sensor ID does not be specified there. This way, the same context can be specified for all sensors, using immediate=yes in <literal>chan_dahdi.conf</literal>.</para>
					</description>
				</configOption>
				<configOption name="disarm_delay" default="60">
					<synopsis>Disarm delay</synopsis>
					<description>
						<para>Number of seconds grace period permitted to disarm an active alarm after this sensor triggers before considering it a breach.</para>
						<para>If set to 0, activation of this sensor will never trigger an alarm. This can be useful for certain sensors, like window sensors on windows that are not a breach threat. Events will still be generated for these sensors, allowing automation actions to be taken if desired.</para>
						<para>Consequently, to have a sensor that always triggers a breach alarm immediately, set this option to 1 second.</para>
					</description>
				</configOption>
			</configObject>
			<configObject name="keypad">
				<synopsis>Configuration section for a keypad</synopsis>
				<description>
					<para>A keypad is a mechanism for interacting with the alarm using an Asterisk channel, in much the same manner as interaction with a conventional physical alarm keypad would proceed.</para>
					<para>This section does not instantiate a keypad itself (that can be done ad hoc by using the <literal>AlarmKeypad</literal>
					application), but rather defines the settings to use for keypad interaction.</para>
					<para>Keypad operation can function in two modes: automatic or custom. Automatic mode requires setting dialstr, pin, and optionally audio. Custom operation requires providing keypad_context.</para>
				</description>
				<configOption name="type">
					<synopsis>Must be of type 'keypad'.</synopsis>
				</configOption>
				<configOption name="client">
					<synopsis>Client associated with this keypad</synopsis>
					<description>
						<para>The name of the client associated with this sensor.</para>
					</description>
				</configOption>
				<configOption name="keypad_device">
					<synopsis>Keypad device</synopsis>
					<description>
						<para>Keypad device to autodial whenever an alarm is triggered. You can also do something custom by defining your own dialplan on the <literal>triggered</literal> event.</para>
						<para>If you need to use a predial Gosub to autoanswer an IP phone, you will want to use a Local channel here to encapsulate the Dial call.</para>
						<para>Once the call is answered, it will be bridged to the <literal>AlarmKeypad</literal> application. You may also call this application directly in your own dialplan extensions.</para>
					</description>
				</configOption>
				<configOption name="pin">
					<synopsis>Disarm PIN</synopsis>
					<description>
						<para>Hardcoded PIN which must be entered to disarm the alarm. Multiple PINs can be permitted by providing multiple comma-separated PINs.</para>
					</description>
				</configOption>
				<configOption name="audio">
					<synopsis>Audio file</synopsis>
					<description>
						<para>An optional audio file to play while waiting for alarm to be disarmed. By default, a tone will be played.</para>
					</description>
				</configOption>
				<configOption name="cid_num">
					<synopsis>Caller ID number</synopsis>
					<description>
						<para>String to use for Caller ID number for autodialed calls to the keypad device.</para>
					</description>
				</configOption>
				<configOption name="cid_name">
					<synopsis>Caller ID name</synopsis>
					<description>
						<para>String to use for Caller ID name for autodialed calls to the keypad device.</para>
					</description>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
	<application name="AlarmSensor" language="en_US">
		<synopsis>
			Indicate an alarm sensor has been triggered
		</synopsis>
		<syntax>
			<parameter name="client" required="yes">
				<para>Name of associated alarm client (as named by section, NOT the client ID).</para>
			</parameter>
			<parameter name="sensor">
				<para>Name of associated alarm sensor (as named by section, NOT the sensor ID).</para>
				<para>This parameter is mandatory if this sensor's device is not defined in the configuration file.</para>
			</parameter>
		</syntax>
		<description>
			<para>Indicates an alarm sensor has been triggered.</para>
			<para>Hook your alarm sensor up to a telephone line such that when in the triggered state, it closes the circuit and takes the phone line off hook. Your channel driver
			configuration should ensure that this immediately begins executing dialplan which calls this application.</para>
			<para>There is no need to hang up explicitly afterwards, since this application will never return.</para>
			<example title="Immediate execution">
			exten => s,1,AlarmSensor(myclient)
			</example>
		</description>
		<see-also>
			<ref type="application">AlarmKeypad</ref>
			<ref type="application">AlarmEventReceiver</ref>
		</see-also>
	</application>
	<application name="AlarmKeypad" language="en_US">
		<synopsis>
			Provide keypad access for a user to interact with the alarm system for a particular client
		</synopsis>
		<syntax>
			<parameter name="client" required="yes">
				<para>Name of associated alarm client.</para>
			</parameter>
		</syntax>
		<description>
			<para>This provides access to the alarm system's keypad, typically used for disarming an alarm after it has been triggered.</para>
			<para>This application is best used by specifying keypads explicitly in the configuration with associate device(s) to autodial, which will autoanswer,
			for the most authentic operation. However, this application could be manually accessed, e.g. by dialing an extension from any phone.</para>
		</description>
		<see-also>
			<ref type="application">AlarmSensor</ref>
			<ref type="application">AlarmEventReceiver</ref>
		</see-also>
	</application>
	<application name="AlarmEventReceiver" language="en_US">
		<synopsis>
			Receive alarm events by phone
		</synopsis>
		<syntax>
			<parameter name="server" required="yes">
				<para>Name of alarm server profile to use.</para>
			</parameter>
		</syntax>
		<description>
			<para>This application is only for use by an alarm server. It allows alarm clients to manually report alarm events to the server by telephone if IP connectivity between the client and server has been lost.</para>
			<para>This application is NOT compatible with the <literal>AlarmReceiver</literal> application (and vice versa!)</para>
		</description>
		<see-also>
			<ref type="application">AlarmSensor</ref>
			<ref type="application">AlarmKeypad</ref>
			<ref type="application">AlarmReceiver</ref>
		</see-also>
	</application>
	<function name="ALARMSYSTEM_SENSOR_TRIGGERED" language="en_US">
		<synopsis>
			Returns whether an alarm sensor is currently triggered
		</synopsis>
		<syntax>
			<parameter name="client" required="true">
				<para>Client name as configured in <literal>res_alarmsystem.conf</literal></para>
			</parameter>
			<parameter name="sensor" required="true">
				<para>Sensor name as configured in <literal>res_alarmsystem.conf</literal></para>
			</parameter>
		</syntax>
		<description>
			<para>Returns whether an alarm system sensor is currently triggered.</para>
		</description>
	</function>
 ***/

#define MODULE_NAME "res_alarmsystem"
#define CONFIG_FILE "res_alarmsystem.conf"

/*! \brief Default bindport is 4589 */
#define DEFAULT_BINDPORT 4589

/*! \brief Default ping interval, in seconds */
#define DEFAULT_PING_INTERVAL 5

/*! \brief How long before server considers client to be offline */
#define DEFAULT_SERVER_IP_LOSS_TOLERANCE (DEFAULT_PING_INTERVAL * 2)

/*! \brief Number of seconds to disarm before considering it a breach */
#define DEFAULT_DISARM_DELAY 60

/*! \brief Number of seconds to keep line open after reporting event(s) by phone */
#define DEFAULT_KEEP_LINE_OPEN_DELAY (DEFAULT_DISARM_DELAY - 10)

enum alarm_event_type {
	EVENT_ALARM_OKAY = 0, /* Normal initialization or status */
	EVENT_ALARM_SENSOR_TRIGGERED, /* Alarm has been triggered by a sensor and the alarm must now be disarmed */
	EVENT_ALARM_SENSOR_RESTORED, /* Sensor has been restored to normal state. However, this will not auto disarm any pending alarms. */
	EVENT_ALARM_DISARMED, /* Alarm has been disarmed by user */
	EVENT_ALARM_TEMP_DISARMED, /* Alarm has been temporarily disarmed by user */
	EVENT_ALARM_BREACH, /* Alarm triggered but not disarmed. Note this is not sent by the client to the server (it's inferred) */
	EVENT_INTERNET_LOST, /* Internet connectivity has been lost. Note this is not sent by the client to the server (it's inferred) */
	EVENT_INTERNET_RESTORED, /* Internet connectivity has been restored. Note this is not sent by the client to the server (it's inferred) */
	EVENT_PING, /* Internal: ping */
	NUM_ALARM_EVENTS, /* Must be the last value in the enum! */
};

enum alarm_state {
	ALARM_STATE_OK = 0,
	ALARM_STATE_TRIGGERED,
	ALARM_STATE_BREACH,
};

static int event_str2int(const char *name)
{
	if (!strcasecmp(name, "okay")) {
		return EVENT_ALARM_OKAY;
	} else if (!strcasecmp(name, "triggered")) {
		return EVENT_ALARM_SENSOR_TRIGGERED;
	} else if (!strcasecmp(name, "restored")) {
		return EVENT_ALARM_SENSOR_RESTORED;
	} else if (!strcasecmp(name, "disarmed")) {
		return EVENT_ALARM_DISARMED;
	} else if (!strcasecmp(name, "tempdisarmed")) {
		return EVENT_ALARM_TEMP_DISARMED;
	} else if (!strcasecmp(name, "breach")) {
		return EVENT_ALARM_BREACH;
	} else if (!strcasecmp(name, "internet_lost")) {
		return EVENT_INTERNET_LOST;
	} else if (!strcasecmp(name, "internet_restored")) {
		return EVENT_INTERNET_RESTORED;
	} else {
		return -1;
	}
}

static const char *event_int2str(enum alarm_event_type event)
{
	switch (event) {
	case EVENT_ALARM_OKAY: return "OK";
	case EVENT_ALARM_SENSOR_TRIGGERED: return "SENSOR_TRIGGERED";
	case EVENT_ALARM_SENSOR_RESTORED: return "SENSOR_RESTORED";
	case EVENT_ALARM_DISARMED: return "DISARMED";
	case EVENT_ALARM_TEMP_DISARMED: return "TEMP_DISARMED";
	case EVENT_ALARM_BREACH: return "BREACH";
	case EVENT_INTERNET_LOST: return "INTERNET_LOST";
	case EVENT_INTERNET_RESTORED: return "INTERNET_RESTORED";
	case EVENT_PING: return "PING";
	case NUM_ALARM_EVENTS: return "NUM_ALARM_EVENTS"; /* Not used, but to complete the enumeration */
	}
	__builtin_unreachable();
}

static unsigned int bindport = DEFAULT_BINDPORT;

struct reporting_client {
	const char *client_id;
	const char *client_pin;
	unsigned int next_ack;
	time_t last_ip_contact;
	time_t breach_time;
	enum alarm_state state;
	unsigned int ip_connected:1;
	unsigned int received_msg:1;
	ast_mutex_t lock;
	AST_LIST_ENTRY(reporting_client) entry;
	char data[];
};

AST_LIST_HEAD(reporting_clients, reporting_client);

static struct reporting_client *find_reporting_client(struct reporting_clients *rc, const char *client_id)
{
	struct reporting_client *r;

	AST_RWLIST_TRAVERSE(rc, r, entry) {
		if (!strcmp(client_id, r->client_id)) {
			return r;
		}
	}
	return NULL;
}

struct alarm_server {
	int listen_fd;
	unsigned int ip_loss_tolerance;
	struct reporting_clients clients;
	char *contexts[NUM_ALARM_EVENTS];
	char logfile[PATH_MAX];
};

/* There can only be one alarm server (only one bindport) */
static struct alarm_server *this_alarm_server = NULL;

struct alarm_sensor {
	const char *name;
	char sensor_id[AST_MAX_EXTENSION];
	int disarm_delay;
	const char *device;
	AST_LIST_ENTRY(alarm_sensor) entry;
	unsigned int triggered:1; /* Sensor triggered? */
	char data[];
};

AST_LIST_HEAD(alarm_sensors, alarm_sensor);

struct alarm_event {
	const char *encoded;
	AST_LIST_ENTRY(alarm_event) entry;
	unsigned int seqno;
	unsigned int attempts; /* Attempts to deliver this message */
	char data[];
};

AST_LIST_HEAD(alarm_events, alarm_event);

struct alarm_client {
	int sfd; /* Socket file descriptor */
	int sequence_no; /* Event sequence_no */
	const char *name;
	enum alarm_state state; /* Internal aggregate alarm state */
	unsigned int ip_connected:1; /* IP connectivity good or lost? */
	pthread_t thread;
	int alertpipe[2];
	char client_id[AST_MAX_EXTENSION];
	char client_pin[AST_MAX_EXTENSION];
	AST_RWLIST_ENTRY(alarm_client) entry;
	char server_ip[128];
	char server_dialstr[AST_MAX_EXTENSION];
	unsigned int phone_hangup_delay;
	struct ast_channel *phonechan; /* Phone failover channel */
	unsigned int ping_interval;
	time_t last_ip_ack;
	time_t autoservice_start;
	time_t breach_time;
	time_t last_arm;
	time_t ip_lost_time; /* Time that IP connectivity was last lost */
	int egress_delay;
	char *contexts[NUM_ALARM_EVENTS];
	struct alarm_sensors sensors;
	struct alarm_events events;
	/* Keypad */
	char keypad_device[AST_MAX_EXTENSION];
	char pin[AST_MAX_EXTENSION];
	char *audio;
	char *cid_num;
	char *cid_name;
	/* Log file */
	char logfile[PATH_MAX];
	char data[];
};

static AST_RWLIST_HEAD_STATIC(clients, alarm_client);

static const char *state2text(int state)
{
	switch (state) {
	case ALARM_STATE_OK: return "Normal (OK)";
	case ALARM_STATE_TRIGGERED: return "Triggered";
	case ALARM_STATE_BREACH: return "Breach";
	}
	__builtin_unreachable();
}

static struct alarm_client *find_client_locked(const char *name)
{
	struct alarm_client *c;

	AST_RWLIST_TRAVERSE(&clients, c, entry) {
		if (!strcmp(name, c->name)) {
			return c;
		}
	}
	return NULL;
}

static struct alarm_sensor *find_sensor(struct alarm_client *c, const char *name)
{
	struct alarm_sensor *s;

	AST_RWLIST_TRAVERSE(&c->sensors, s, entry) {
		if (!strcmp(name, s->name)) {
			return s;
		}
	}
	return NULL;
}

static struct alarm_sensor *find_sensor_by_device(struct alarm_client *c, const char *device)
{
	struct alarm_sensor *s;

	AST_RWLIST_TRAVERSE(&c->sensors, s, entry) {
		if (!ast_strlen_zero(s->device) && !strcmp(device, s->device)) {
			return s;
		}
	}
	return NULL;
}

static void free_contexts(char *contexts[])
{
	int i;
	for (i = 0; i < NUM_ALARM_EVENTS; i++) {
		if (contexts[i]) {
			ast_free(contexts[i]);
		}
	}
}

static void free_reporting_clients(struct reporting_clients *rc)
{
	struct reporting_client *r;

	while ((r = AST_LIST_REMOVE_HEAD(rc, entry))) {
		ast_mutex_destroy(&r->lock);
		ast_free(r);
	}
}

static void cleanup_server(struct alarm_server *s)
{
	if (s->listen_fd != -1) {
		close(s->listen_fd);
	}
	free_reporting_clients(&s->clients);
	free_contexts(s->contexts);
	ast_free(s);
}

static void cleanup_sensor(struct alarm_sensor *s)
{
	ast_free(s);
}

static void cleanup_sensors(struct alarm_sensors *sensors)
{
	struct alarm_sensor *s;

	while ((s = AST_LIST_REMOVE_HEAD(sensors, entry))) {
		cleanup_sensor(s);
	}
}

static void cleanup_client(struct alarm_client *c)
{
	if (c->thread != AST_PTHREADT_NULL) {
		pthread_kill(c->thread, SIGURG);
		pthread_join(c->thread, NULL);
		c->thread = AST_PTHREADT_NULL;
	}
	if (c->sfd != -1) {
		close(c->sfd);
	}
	free_contexts(c->contexts);
	cleanup_sensors(&c->sensors);
	if (c->audio) {
		ast_free(c->audio);
	}
	if (c->cid_num) {
		ast_free(c->cid_num);
	}
	if (c->cid_name) {
		ast_free(c->cid_name);
	}
	ast_alertpipe_close(c->alertpipe);
	ast_free(c);
}

static void cleanup_clients(void)
{
	struct alarm_client *c;

	AST_RWLIST_WRLOCK(&clients);
	while ((c = AST_RWLIST_REMOVE_HEAD(&clients, entry))) {
		cleanup_client(c);
	}
	AST_RWLIST_UNLOCK(&clients);
}

static int link_contexts(struct ast_config *cfg, char *contexts[], const char *cat_name)
{
	struct ast_variable *var;

	for (var = ast_variable_browse(cfg, cat_name); var; var = var->next) {
		int event;
		if (!strcasecmp(var->name, "type") && !ast_strlen_zero(var->value)) {
			if (strcasecmp(var->value, "contexts")) {
				ast_log(LOG_ERROR, "Context map config section has type '%s'?\n", var->value);
				return -1;
			}
			continue;
		}

		if (ast_strlen_zero(var->value)) {
			continue; /* XXX I've forgotten, can this happen? */
		}

		event = event_str2int(var->name);
		if (event == -1) {
			ast_log(LOG_WARNING, "Unknown alarm event type '%s'\n", var->name);
			continue;
		}
		if (contexts[event]) {
			ast_free(contexts[event]);
		}
		contexts[event] = ast_strdup(var->value);
		/* XXX Could parse [exten@]context here and warn if that location doesn't exist */
	}

	return 0;
}

/* Borrowed from pbx_dundi.c */
static int get_ipaddress(char *ip, size_t size, const char *str, int family)
{
	struct ast_sockaddr *addrs;

	if (!ast_sockaddr_resolve(&addrs, str, 0, family)) {
		return -1;
	}

	ast_copy_string(ip, ast_sockaddr_stringify_host(&addrs[0]), size);
	ast_free(addrs);

	return 0;
}

static int orig_app_dialplan(const char *chandata, struct ast_variable *vars, const char *app, const char *data)
{
	char locationbuf[AST_MAX_CONTEXT + AST_MAX_EXTENSION + 2];
	int reason = 0;
	struct ast_format_cap *cap;

	if (!strchr(chandata, '@')) {
		snprintf(locationbuf, sizeof(locationbuf), "s@%s", chandata);
		chandata = locationbuf;
	}

	if (!(cap = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT))) {
		ast_log(LOG_ERROR, "Failed to allocate capabilities\n");
		return -1;
	}
	ast_format_cap_append(cap, ast_format_slin, 0);

	ast_debug(1, "Spawning dialplan: %s -> %s(%s)\n", chandata, app, data);
	ast_pbx_outgoing_app("Local", cap, chandata, 999999, app, data,
			&reason, AST_OUTGOING_NO_WAIT, NULL, NULL, vars, NULL,
			NULL, NULL);
	ao2_ref(cap, -1);

	return 0;
}

static int orig_app_device(const char *chandata, struct ast_variable *vars, const char *app, const char *data, const char *cid_num, const char *cid_name)
{
	char locationbuf[AST_MAX_CONTEXT + AST_MAX_EXTENSION + 2];
	int reason = 0;
	char *tech, *device;
	struct ast_format_cap *cap;

	ast_copy_string(locationbuf, chandata, sizeof(locationbuf));
	device = locationbuf;
	tech = strsep(&device, "/");

	if (!(cap = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT))) {
		ast_log(LOG_ERROR, "Failed to allocate capabilities\n");
		return -1;
	}
	ast_format_cap_append(cap, ast_format_slin, 0);

	ast_debug(1, "Spawning dialplan: %s/%s -> %s(%s)\n", tech, device, app, data);
	ast_pbx_outgoing_app(tech, cap, device, 999999, app, data,
			&reason, AST_OUTGOING_NO_WAIT, cid_num, cid_name, vars, NULL,
			NULL, NULL);
	ao2_ref(cap, -1);

	return 0;
}

static void spawn_dialplan(const char *location, const char *type, enum alarm_event_type event, const char *clientid, const char *sensorid)
{
	struct ast_variable *vars;
	struct ast_variable *var1, *var2, *var3;
	char eventbuf[15];

	memset(&vars, 0, sizeof(vars));
	snprintf(eventbuf, sizeof(eventbuf), "%d", event);

	var1 = ast_variable_new("ALARMSYSTEM_CLIENTID", clientid, "");
	var2 = ast_variable_new("ALARMSYSTEM_EVENT", eventbuf, "");
	var3 = sensorid ? ast_variable_new("ALARMSYSTEM_SENSORID", sensorid, "") : NULL;

	ast_variable_list_append(&vars, var1);
	ast_variable_list_append(&vars, var2);
	if (var3) {
		ast_variable_list_append(&vars, var3);
	}

	ast_debug(3, "Executing async %s dialplan %s\n", type, location);
	orig_app_dialplan(location, vars, "Wait", "999999");
	ast_variables_destroy(vars);
}

static int module_shutting_down = 0;
static pthread_t netthreadid = AST_PTHREADT_NULL;
static struct io_context *io = NULL;
struct ast_sched_context *sched = NULL;

#define MIN_PACKET_SIZE 3
#define MAX_PACKET_SIZE 256

#define ALARM_EVENT_ACK '*'

#define CSV_DATE_FORMAT "%Y-%m-%d %T"
#define CSV_FORCE_UTC 0

/* File logging is done in a CSV format with the following fields:
 * - timestamp
 * - client ID
 * - sequence number (or 0 if unavailable for some reason)
 * - MMSS timestamp
 * - event type name
 * - sensor ID (if applicable)
 * - extra data (if applicable)
 *
 * writes are flushed immediately, since they are relatively infrequent
 * Furthermore, we open the log file each log, since again, they're infrequent,
 * and also so we don't need to handle log rotation specially.
 */
#define csv_log(logfile, clientid, sequence_no, timestamp, event, sensorid, data) { \
	if (!ast_strlen_zero(logfile)) { \
		FILE *fp = fopen(logfile, "a"); \
		if (fp) { \
			char tmp[80] = ""; \
			struct ast_tm tm; \
			struct timeval when = ast_tvnow(); \
			ast_localtime(&when, &tm, CSV_FORCE_UTC ? "GMT" : NULL); \
			ast_strftime(tmp, sizeof(tmp), CSV_DATE_FORMAT, &tm); \
			fprintf(fp, "%s,%s,%d,%s,%s,%s,%s\n", tmp, clientid, sequence_no, timestamp, event, sensorid, data); \
			fclose(fp); \
		} else { \
			ast_log(LOG_WARNING, "Failed to open log file '%s': %s\n", logfile, strerror(errno)); \
		} \
	} \
}

#define alarm_server_log(s, clientid, sequence_no, timestamp, event, sensorid, data) \
	ast_debug(1, "Client '%s', seqno %d, timestamp %s, event %s, sensor %s, data '%s'\n", clientid, sequence_no, timestamp, event_int2str(event), sensorid, data); \
	csv_log(s->logfile, clientid, sequence_no, timestamp, event_int2str(event), sensorid, data);

#define alarm_client_log(c, sequence_no, timestamp, event, s, data) \
	ast_debug(1, "Client '%s', seqno %d, timestamp %s, event %s, sensor %s, data '%s'\n", c->client_id, sequence_no, timestamp, event_int2str(event), s ? s->sensor_id : "", S_OR(data, "")); \
	csv_log(c->logfile, c->client_id, sequence_no, timestamp, event_int2str(event), s ? s->sensor_id : "", S_OR(data, ""));

/*! \brief Authenticate the reporting client */
static struct reporting_client *authenticate_client(struct alarm_server *s, const char *clientid, const char *clientpin)
{
	struct reporting_client *r = find_reporting_client(&s->clients, clientid);
	if (!r) {
		ast_log(LOG_WARNING, "Client '%s' is not configured to report to us, ignoring\n", clientid);
		return NULL;
	}
	if (!ast_strlen_zero(r->client_pin) && (ast_strlen_zero(clientpin) || strcmp(r->client_pin, clientpin))) {
		ast_log(LOG_WARNING, "Client PIN mismatch for client %s\n", clientid);
		return NULL;
	}
	return r;
}

static void process_received_event(struct alarm_server *s, struct reporting_client *r, enum alarm_event_type event, const char *sensorid, const char *data)
{
	/* Any special per-event handling */
	if (event == EVENT_ALARM_SENSOR_TRIGGERED) {
		time_t breach_time = 0;
		if (!ast_strlen_zero(data)) {
			breach_time = atol(data);
		}
		if (breach_time) {
			r->state = ALARM_STATE_TRIGGERED;
			/* This indicates the epoch at which this will be considered a breach,
			 * if a disarm is not received prior to that time. */
			if (r->breach_time && breach_time > r->breach_time) {
				/* This just means the sensor permits more time to disarm or was triggered later. Just ignore. */
				ast_debug(1, "Received breach time %lu is further in the future than existing one %lu, ignoring\n", breach_time, r->breach_time);
				/* Keep the soonest one */
			} else {
				r->breach_time = breach_time;
				ast_debug(1, "A breach will occur if system is not disarmed by %lu (%ld s from now)\n", r->breach_time, r->breach_time - time(NULL));
			}
		} else {
			/* if breach_time is 0,
			 * that means that the sensor trigger should NOT trigger an alarm.
			 * This is because egress was permitted, i.e. this is an egress event,
			 * not an ingress event, so no need to alarm. */
			ast_debug(1, "Not triggering alarm since this is an egress event\n");
		}
	} else if (event == EVENT_ALARM_DISARMED) {
		r->state = ALARM_STATE_OK;
		r->breach_time = 0; /* Reset, since nothing is pending that would cause a breach */
	}

	/* If there is dialplan to execute for this event, do it async now */
	if (s->contexts[event]) {
		spawn_dialplan(s->contexts[event], "server", event, r->client_id, sensorid);
	}
}

/*! \brief Process a received event, whether delivered by phone or IP */
static int process_message(struct alarm_server *s, struct reporting_client *r, const char *clientid,  int sequence_no, const char *timestamp, enum alarm_event_type event, const char *sensorid, const char *data)
{
	ast_mutex_lock(&r->lock); /* Only one thread processes all IP events, but phone call reporting can be in separate threads, so we need to lock */

	if (sequence_no != r->next_ack) {
		/* Special case, our sequence number is > 1,
		 * but the client's sequence number is 1.
		 * This can happen if the client module unloads/loads,
		 * which starts its sequence number at 1 again.
		 *
		 * Accordingly, if WE unload/load, we will start at 1,
		 * while the client may be at some higher number, in which case
		 * we will use the first sequence number sent to us as the
		 * starting point. */
		if (!r->received_msg) {
			/* We haven't received any messages yet,
			 * so use whatever the client's sequence number is as our starting point. */
			ast_debug(1, "Using client's sequence number %d as our starting point, since this is the first we've heard from it\n", sequence_no);
			r->next_ack = sequence_no; /* Now, we can use our normal logic */
		} else if (sequence_no == 1) {
			/* The client must be starting over, so just go along with it */
			ast_log(LOG_NOTICE, "Client message has sequence number 1, starting over at 1 to resynchronize\n");
			r->next_ack = sequence_no;
		}
	}

	r->received_msg = 1;

	if (sequence_no != r->next_ack) {
		/* Unlike TCP, we don't temporarily store out of order packets, for simplicity.
		 * The client will just need to redeliver this one after the one we expect. */
		ast_log(LOG_WARNING, "Ignoring message %d from client %s (expecting message %d)\n", sequence_no, r->client_id, r->next_ack);
		if (sequence_no < r->next_ack) {
			/* If this is an old message we already got, then just ignore it and continue.
			 * This shouldn't happen, but if it does, we can safely gracefully ignore it
			 * and continue on to later events that could be new. */
			ast_mutex_unlock(&r->lock);
			return 1;
		}
		ast_mutex_unlock(&r->lock);
		return -1;
	}

	/* Accept the event and process it */
	alarm_server_log(s, clientid, sequence_no, timestamp, event, sensorid, data);
	process_received_event(s, r, event, sensorid, data);
	r->next_ack++;

	ast_mutex_unlock(&r->lock);
	return 0;
}

static void build_mmss(char *restrict buf, size_t len)
{
	struct timeval when = {0,};
	struct ast_tm tm;
	when = ast_tvnow();
	
	/* Include MM:SS timestamp, in case we need to dial the event in by phone, which would delay it,
	 * or if a UDP packet needs to be retransmitted. */
	ast_localtime(&when, &tm, NULL);
	/* Conveniently since it's just minutes and seconds, the time zone doesn't matter, no need to convert to UTC here */
	ast_strftime(buf, len, "%M%S", &tm);
}

static int socket_read(int *id, int fd, short events, void *sock)
{
	struct ast_sockaddr sin;
	int res;
	char buf[MAX_PACKET_SIZE];
	char ackbuf[32];
	struct reporting_client *r;
	struct alarm_server *s;
	int acklen;
	char *clientid, *clientpin, *seqno, *timestamp, *eventid, *sensorid, *data;
	int event;
	char *term;

	res = ast_recvfrom(*((int *)sock), buf, sizeof(buf), 0, &sin);
	if (res < 0) {
		if (errno != ECONNREFUSED) {
			ast_log(LOG_WARNING, "recvfrom failed: %s\n", strerror(errno));
		}
		return 1;
	}
	if (res < MIN_PACKET_SIZE) {
		ast_log(LOG_WARNING, "Midget packet received (%d of %d min)\n", res, MIN_PACKET_SIZE);
		return 1;
	}
	buf[res] = '\0';

	ast_debug(9, "Received %d-byte message from client: '%s'\n", res, buf);

	/* XXX It would be cleaner to have a direct handle to s, but since there is only one server possible, we can use the global ref */
	s = this_alarm_server;

	/* First, parse out the event */
	data = buf;
	term = strchr(data, '#');
	if (term) {
		*term = '\0';
	} else {
		ast_log(LOG_WARNING, "Received event '%s' was not #-terminated?\n", data);
	}

	clientid = strsep(&data, "*");
	clientpin = strsep(&data, "*");

	r = authenticate_client(s, clientid, clientpin);
	if (!r) {
		return 1;
	}

	seqno = strsep(&data, "*");
	timestamp = strsep(&data, "*");
	eventid = strsep(&data, "*");
	sensorid = strsep(&data, "*");

	if (ast_strlen_zero(clientid) || ast_strlen_zero(eventid)) {
		ast_log(LOG_WARNING, "Received malformed event from client\n");
		return 1;
	}
	event = atoi(eventid);
	if (event >= NUM_ALARM_EVENTS) {
		ast_log(LOG_WARNING, "Invalid event ID: %d\n", event);
	}

	/* Infer if Internet connectivity has been restored */
	if (!r->ip_connected) {
		char mmss[5];
		/* Build our own timestamp now, since if we got a ping, it has no timestamp.
		 * Not that it really matters, since we log a granular timestamp in the log anyways. */
		build_mmss(mmss, sizeof(mmss));
		ast_log(LOG_NOTICE, "Client '%s' is now ONLINE\n", r->client_id);
		alarm_server_log(s, clientid, 0, mmss, EVENT_INTERNET_RESTORED, "", ""); /* Inferred event */
		r->ip_connected = 1;
	}

	/* Don't log pings */
	if (event != EVENT_PING) {
		unsigned int sequence_no;
		/* Check if this is the next message we expect.
		 * If not, ignore it. */
		if (ast_strlen_zero(seqno)) {
			ast_log(LOG_WARNING, "Message missing sequence number\n");
			return 1;
		}
		sequence_no = atoi(seqno);
		if (process_message(s, r, clientid, sequence_no, timestamp, event, sensorid, data)) {
			return 1;
		}
	}

	ast_debug(9, "Sending IP ACK %d\n", r->next_ack);
	acklen = snprintf(ackbuf, sizeof(ackbuf), "%c%d#", ALARM_EVENT_ACK, r->next_ack);
	r->last_ip_contact = time(NULL); /* Since we just heard from the client by IP, it's online as far as we're concerned about */

	if (ast_sendto(*((int *)sock), ackbuf, acklen, 0, &sin) == -1) {
		ast_log(LOG_WARNING, "sendto failed: %s\n", strerror(errno));
	}

	return 1;
}

#define SERVER_THINKS_CLIENT_IS_OFFLINE(s, r, now) (r->last_ip_contact < now - s->ip_loss_tolerance)

static void server_inferred_event(struct alarm_server *s, struct reporting_client *r, const char *clientid, int seqno, const char *timestamp, enum alarm_event_type event, const char *sensorid, const char *data)
{
	alarm_server_log(s, clientid, seqno, timestamp, event, sensorid, data);
	process_received_event(s, r, event, sensorid, data);
}

static void check_client_connectivity(void)
{
	char mmss[5];
	struct reporting_client *r;
	time_t now = time(NULL);

	build_mmss(mmss, sizeof(mmss));

	AST_LIST_TRAVERSE(&this_alarm_server->clients, r, entry) {
		if (r->ip_connected && SERVER_THINKS_CLIENT_IS_OFFLINE(this_alarm_server, r, now)) {
			/* This is an inferred event, by loss of recent pings */
			ast_debug(3, "Last ping from client was at %lu, tolerance is %u, time is currently %lu\n", r->last_ip_contact, this_alarm_server->ip_loss_tolerance, now);
			server_inferred_event(this_alarm_server, r, r->client_id, 0, mmss, EVENT_INTERNET_LOST, "", "");
			r->ip_connected = 0;
			ast_log(LOG_NOTICE, "Client '%s' is now OFFLINE\n", r->client_id);
		}
		if (r->state == ALARM_STATE_TRIGGERED) {
			/* Check if we need to transition to BREACH state */
			if (now >= r->breach_time) {
				ast_log(LOG_NOTICE, "Client '%s' has not yet been disarmed, active breach!\n", r->client_id);
				r->state = ALARM_STATE_BREACH;
				server_inferred_event(this_alarm_server, r, r->client_id, 0, mmss, EVENT_ALARM_BREACH, "", "");
			}
		}
	}
}

static void *server_thread(void *ignore)
{
	int res;

	/* Establish I/O callback for socket read */
	int *socket_read_id = ast_io_add(io, this_alarm_server->listen_fd, socket_read, AST_IO_IN, &this_alarm_server->listen_fd);

	while (!module_shutting_down) {
		res = ast_sched_wait(sched);
		if ((res > 1000) || (res < 0)) {
			res = 1000;
		}
		res = ast_io_wait(io, res);
		if (res >= 0) {
			ast_sched_runq(sched);
		}
		/* Periodic tasks */
		check_client_connectivity();
	}

	ast_io_remove(io, socket_read_id);
	return NULL;
}

/* Forward declaration */
static void set_ip_connected(struct alarm_client *c, int connected);

static int send_event_ip(struct alarm_client *c, const char *msg, int len)
{
	ast_debug(9, "Sending event by IP: '%s'\n", msg);
	if (send(c->sfd, msg, len, 0) != len) {
		/* Not a WARNING or we would see warnings every time the Internet went out */
		ast_verb(8, "Failed to send alarm event via IP: %s\n", strerror(errno));
		set_ip_connected(c, 0);
		return -1;
	}
	return 0;
}

#define INFERRED_EVENT(e) (e == EVENT_INTERNET_LOST || e == EVENT_INTERNET_RESTORED || e == EVENT_ALARM_BREACH)

/*! \brief Generate an event and add it to the send queue, and schedule it for delivery (but don't actually send it yet) */
static int generate_event(struct alarm_client *c, enum alarm_event_type event, struct alarm_sensor *s, const char *data)
{
	char msgbuf[MAX_PACKET_SIZE];
	char seqno[12];
	char mmss[5];
	int len;
	unsigned int sequence_no;
	struct alarm_event *e;

	AST_LIST_LOCK(&c->events);
	sequence_no = c->sequence_no;
	if (event == EVENT_PING) {
		strcpy(seqno, ""); /* No sequence number usage for pings */
		strcpy(mmss, ""); /* No timestamp for pings */
	} else {
		/* INTERNET events: Since these events aren't sent to the server (they are inferred events by the server),
		 * don't consume a sequence number for them, or that will mess up synchronization. */
		if (INFERRED_EVENT(event)) {
			sequence_no = 0;
		}
		snprintf(seqno, sizeof(seqno), "%u", sequence_no);
		build_mmss(mmss, sizeof(mmss));
	}
	len = snprintf(msgbuf, sizeof(msgbuf), "%s*%s*%s*%s*%d*%s*%s#", c->client_id, S_OR(c->client_pin, ""), seqno, mmss, event, s ? s->sensor_id : "", S_OR(data, ""));
	if (event != EVENT_PING || !c->sequence_no) {
		/* Don't log pings, except the first one, since that would be too noisy */
		alarm_client_log(c, sequence_no, mmss, event, s, data);
	}

	if (event != EVENT_PING && !INFERRED_EVENT(event)) {
		/* Don't increment sequence number for pings,
		 * since it doesn't really matter for those,
		 * and that would unnecessarily increment every few seconds. */
		c->sequence_no++;
	}

	/* If there is dialplan to execute for this event, do it async now */
	if (c->contexts[event]) {
		spawn_dialplan(c->contexts[event], "client", event, c->client_id, s ? s->sensor_id : "");
	}

	/* Certain events aren't actually sent across the wire, they can be inferred by the server */
	if (INFERRED_EVENT(event)) {
		AST_LIST_UNLOCK(&c->events);
		return 0;
	}

	if (event == EVENT_PING) {
		AST_LIST_UNLOCK(&c->events);
		/* Send the event directly via UDP now, and don't store it in the queue,
		 * since we'll keep retrying pings on a timer, no need to retry specific pings.
		 * Since it's UDP, this won't block.
		 * Also, we attempt pings regardless of the value of c->ip_connected.
		 * No way to know if the connection is back without constantly retrying. */
		send_event_ip(c, msgbuf, len);
	} else {
		if (c->sfd == -1 && c->server_dialstr[0] == '\0') {
			/* IP and phone are disabled, so we can't report to any server. */
			AST_LIST_UNLOCK(&c->events);
			return 1;
		}
		/* Add the event to the send queue for guaranteed FIFO delivery. */
		e = ast_calloc(1, sizeof(*e) + len + 1);
		if (!e) {
			AST_LIST_UNLOCK(&c->events);
			ast_log(LOG_ERROR, "Failed to add message to send queue\n");
			return -1;
		}
		strcpy(e->data, msgbuf); /* Safe */
		e->encoded = e->data;
		e->seqno = sequence_no;
		AST_LIST_INSERT_TAIL(&c->events, e, entry);

		/* Wake up the client thread to tell it to send the message */
		ast_alertpipe_write(c->alertpipe);

		AST_LIST_UNLOCK(&c->events);
	}
	return 0;
}

static void set_ip_connected(struct alarm_client *c, int connected)
{
	if (connected != c->ip_connected) {
		time_t now;
		ast_log(LOG_NOTICE, "Client '%s' is now %s\n", c->name, connected ? "ONLINE" : "OFFLINE");
		c->ip_connected = connected;
		generate_event(c, connected ? EVENT_INTERNET_RESTORED : EVENT_INTERNET_LOST, NULL, NULL);
		now = time(NULL);
		if (connected) {
			if (c->ip_lost_time >= now - 1) {
				/* Possible, and likely just highly coincidental. */
				ast_debug(1, "Interesting! IP connectivity restored immediately after it was lost!\n");
			}
		} else {
			c->ip_lost_time = now;
		}
	}
}

static int send_events_to_server_by_ip(struct alarm_client *c)
{
	struct alarm_event *e;

	/* Only attempt IP delivery in this loop.
	 * Since this is UDP (non-blocking),
	 * it's okay to keep the list locked in the loop. */
	AST_LIST_LOCK(&c->events);
	AST_LIST_TRAVERSE(&c->events, e, entry) {
		e->attempts++;
		/* If IP connectivity is possible, attempt to send the event over IP.
		 * Also, if this is the first event we're sending, then we don't
		 * know if IP connectivity is working yet, so attempt anyways to probe. */
		send_event_ip(c, e->encoded, strlen(e->encoded));
	}
	AST_LIST_UNLOCK(&c->events);

	return 0;
}

static struct ast_channel *new_channel(const char *tech, const char *resource)
{
	struct ast_format_cap *caps;
	struct ast_party_caller caller;
	struct ast_channel *chan;
	int cause = 0;

	caps = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT);
	if (!caps) {
		return NULL;
	}
	ast_format_cap_append(caps, ast_format_slin, 0);

	chan = ast_request(tech, caps, NULL, NULL, resource, &cause);
	if (!chan) {
		ast_log(LOG_WARNING, "Unable to create channel %s/%s (cause %d - %s)\n", tech, resource, cause, ast_cause2str(cause));
		return NULL;
	}

	ast_channel_lock(chan);

	ast_channel_appl_set(chan, "AppAlarmEventReceiver");
	ast_channel_data_set(chan, "(Outgoing Line)");

	memset(ast_channel_whentohangup(chan), 0, sizeof(*ast_channel_whentohangup(chan)));

	ast_party_caller_set_init(&caller, ast_channel_caller(chan));
	ast_channel_adsicpe_set(chan, 0);

	ast_channel_unlock(chan);

	return chan;
}

static int wait_for_answer(struct alarm_client *c)
{
	int res = -1;

	while (res < 0) {
		struct ast_channel *winner;
		int to = 1000;

		winner = ast_waitfor_n(&c->phonechan, 1, &to);
		if (winner == c->phonechan) {
			char frametype[64];
			char subclass[64];
			struct ast_frame *f = ast_read(c->phonechan);
			if (!f) {
				ast_hangup(c->phonechan);
				c->phonechan = NULL;
				ast_log(LOG_WARNING, "Phone failover channel disconnected before answer\n");
				return 1;
			}
			switch (f->frametype) {
			case AST_FRAME_CONTROL:
				switch (f->subclass.integer) {
				case AST_CONTROL_ANSWER:
					ast_verb(3, "Phone failover channel %s answered\n", ast_channel_name(c->phonechan));
					ast_channel_hangupcause_set(c->phonechan, AST_CAUSE_NORMAL_CLEARING);
					res = 0;
					break;
				case AST_CONTROL_BUSY:
				case AST_CONTROL_CONGESTION:
					return -1;
				default:
					/* Ignore everything else */
					ast_frame_subclass2str(f, subclass, sizeof(subclass), NULL, 0);
					ast_debug(1, "Ignoring control frame %s\n", subclass);
					break;
				}
				break;
			case AST_FRAME_VOICE:
				break;
			case AST_FRAME_DTMF_BEGIN:
			case AST_FRAME_DTMF_END:
			case AST_FRAME_VIDEO:
			case AST_FRAME_IMAGE:
			case AST_FRAME_TEXT:
			case AST_FRAME_NULL:
				break; /* Ignore */
			default:
				ast_frame_type2str(f->frametype, frametype, sizeof(frametype));
				ast_debug(1, "Ignoring callee frame type %u (%s)\n", f->frametype, frametype);
				break;
			}
			ast_frfree(f);
		}
	}

	return res;
}

/*! \brief Send a DTMF string, optionally *-terminated */
static int send_dtmf(struct ast_channel *chan, const char *digits, int addstar)
{
	int res = ast_dtmf_stream(chan, NULL, digits, 150, 100);
	if (res) {
		ast_log(LOG_WARNING, "Failed to send digits '%s'\n", digits);
		return res;
	}
	if (addstar) {
		res = ast_dtmf_stream(chan, NULL, "*", 150, 100);
		if (res) {
			ast_log(LOG_WARNING, "Failed to send digit '%s'\n", "*");
			return res;
		}
	}
	return 0;
}

static void purge_sent_events(struct alarm_client *c, int ack_seqno)
{
	struct alarm_event *e;

	/* Server has acknowledged receiving all events with sequence numbers less than this seqno.
	 * If they are still in the event queue, free them. */
	AST_LIST_LOCK(&c->events);
	AST_LIST_TRAVERSE_SAFE_BEGIN(&c->events, e, entry) {
		/* For example, say we just got an ACK 3.
		 * That means 1 and 2 are acknowledged
		 * and can be purged from the queue (but not 3).
		 *
		 * We just need one condition to check,
		 * since we know this list is sorted by sequence ID,
		 * as it's tail insert. */
		if (e->seqno >= ack_seqno) {
			break;
		}
		AST_LIST_REMOVE_CURRENT(entry);
		ast_debug(3, "Freeing event %u (ACKed by %d)\n", e->seqno, ack_seqno);
		ast_free(e);
	}
	AST_LIST_TRAVERSE_SAFE_END;
	AST_LIST_UNLOCK(&c->events);
}

/*! \brief Failover reporting by phone, if IP reporting is unavailable. */
static int send_events_to_server_by_phone(struct alarm_client *c)
{
	char buf[16];
	struct alarm_event *e;
	int event_index = 0;
	int ack_no, res;
	int last_sent_seqno = 0;

	/* The big difference between IP reporting and phone reporting
	 * is that phone reporting is connection oriented.
	 * Before we can do anything, we need to set up a connection
	 * for reporting purposes (if there's not currently one active).
	 */

	AST_LIST_LOCK(&c->events);
	if (AST_LIST_EMPTY(&c->events)) {
		/* No events that need to be reported, no need to establish any connection. */
		AST_LIST_UNLOCK(&c->events);
		return -1;
	}
	AST_LIST_UNLOCK(&c->events);

	/* Since this is the only thread that handles reporting,
	 * there's no risk of two phone channels existing. */
	if (c->phonechan) {
		ast_debug(3, "Resuming phone failover reporting using existing channel %s\n", ast_channel_name(c->phonechan));
		/* Take the channel out of autoservice */
		if (ast_autoservice_stop(c->phonechan)) {
			ast_log(LOG_ERROR, "Failed to stop autoservice on %s\n", ast_channel_name(c->phonechan));
			ast_hangup(c->phonechan);
			c->phonechan = NULL;
			return -1;
		}
	} else {
		int res;
		char *tech, *resource = ast_strdupa(c->server_dialstr);
		tech = strsep(&resource, "/");
		c->phonechan = new_channel(tech, resource);
		if (!c->phonechan) {
			ast_log(LOG_ERROR, "Failed to set up phone failover channel\n");
			return -1;
		}

		/* Place the call, but don't wait on the answer */
		res = ast_call(c->phonechan, resource, 0);
		if (res) {
			ast_log(LOG_ERROR, "Failed to call on %s\n", ast_channel_name(c->phonechan));
			ast_hangup(c->phonechan);
			c->phonechan = NULL;
			return -1;
		}
		ast_verb(3, "Called %s/%s\n", tech, resource);
		res = wait_for_answer(c);
		if (res) {
			ast_hangup(c->phonechan);
			c->phonechan = NULL;
			return res;
		}

		/* Wait for initial ACK from AlarmEventReceiver to synchronize */
		res = ast_app_getdata_terminator(c->phonechan, "", buf, sizeof(buf), 60000, "*");
		if (res != AST_GETDATA_EMPTY_END_TERMINATED) {
			ast_log(LOG_WARNING, "Failed to synchronize with server\n");
			ast_hangup(c->phonechan);
			c->phonechan = NULL;
			return -1;
		}

		/* Start by sending client ID and client PIN.
		 * Since this is connection oriented, no need to send these again
		 * subsequently if there is more than one event. */
		res = send_dtmf(c->phonechan, c->client_id, 1);
		if (res) {
			ast_log(LOG_WARNING, "Failed to send client ID\n");
		}
		res = send_dtmf(c->phonechan, S_OR(c->client_pin, ""), 1);
		if (res) {
			ast_log(LOG_WARNING, "Failed to send client PIN\n");
		}
	}

	/* Don't keep the list locked the entire time that we're sending events,
	 * as that would prevent producers from adding further events to the queue if needed.
	 * Instead, send the first event in the list, and keep track of its seqno,
	 * and each time we iterate the list, find the next event with a higher seqno,
	 * until there are no more events, then exit the loop. */
	for (;;) {
		int more_events = 0;
		AST_LIST_LOCK(&c->events);
		AST_LIST_TRAVERSE(&c->events, e, entry) {
			char encoded[MAX_PACKET_SIZE];
			char *tmp;
			if (module_shutting_down) {
				break;
			}
			if (last_sent_seqno >= e->seqno) {
				/* Already sent this event in a previous loop iteration */
				continue;
			}
			more_events = 1;
			/* Skip to after the second '*', since that's the rest of the encoded string */
			tmp = strchr(e->encoded, '*');
			ast_assert(tmp != NULL);
			tmp = strchr(tmp + 1, '*');
			ast_assert(tmp != NULL);
			tmp++;
			ast_copy_string(encoded, tmp, sizeof(encoded)); /* Duplicate so we can unlock now */
			last_sent_seqno = e->seqno;
			e->attempts++;
			AST_LIST_UNLOCK(&c->events);
			res = send_dtmf(c->phonechan, encoded, 0);
			if (res) {
				ast_log(LOG_WARNING, "Failed to send event as DTMF\n");
				ast_hangup(c->phonechan);
				c->phonechan = NULL;
				return -1;
			}
			event_index++;
			goto postunlock;
		}
		AST_LIST_UNLOCK(&c->events);
postunlock:
		if (!more_events) {
			break;
		}
		/* No need to suspend execution briefly here, since we unlock while sending DTMF */
	}

	if (event_index == 0) {
		ast_debug(1, "Hmm, events must've been flushed out of send queue while %s was dialing\n", ast_channel_name(c->phonechan));
		/* Hang up the channel immediately, since it's unlikely to be needed soon */
		ast_hangup(c->phonechan);
		c->phonechan = NULL;
	}

	/* Send a final # to indicate we're done, at least for now */
	res = send_dtmf(c->phonechan, "#", 0);
	if (res) {
		ast_log(LOG_WARNING, "Failed to send final '#'\n");
		ast_hangup(c->phonechan);
		c->phonechan = NULL;
		return -1;
	}

	ast_debug(3, "Sent %d event%s to alarm reporting server\n", event_index, ESS(event_index));

	/* Wait for acknowledgment of the events we just sent. */
	res = ast_app_getdata_terminator(c->phonechan, "", buf, sizeof(buf), 60000, "#");
	if (res != AST_GETDATA_COMPLETE) {
		ast_log(LOG_WARNING, "Failed to receive any acknowledgment from server\n");
		ast_hangup(c->phonechan);
		c->phonechan = NULL;
		return -1;
	}

	ack_no = atoi(buf);
	purge_sent_events(c, ack_no);

	/* Keep the channel alive for a little bit,
	 * just in case we need to use it again to send an event soon. */
	if (ast_autoservice_start(c->phonechan)) {
		ast_log(LOG_WARNING, "Failed to begin autoservice on %s\n", ast_channel_name(c->phonechan));
		ast_hangup(c->phonechan);
		c->phonechan = NULL;
		return -1;
	}
	c->autoservice_start = time(NULL);

	return 0;
}

static int process_server_ack(struct alarm_client *c)
{
	char buf[32];
	ssize_t res;
	int ack_seqno;

	res = recv(c->sfd, buf, sizeof(buf) - 1, 0);
	if (res <= 0) {
		ast_log(LOG_WARNING, "read failed: %s\n", strerror(errno));
		return -1;
	}
	buf[res] = '\0';

	/* Parse the ACK */
	if (buf[0] != '*') {
		ast_log(LOG_WARNING, "Received unexpected non-ACK data from server: '%s'\n", buf);
		return -1;
	}

	set_ip_connected(c, 1);
	c->last_ip_ack = time(NULL);

	if (!buf[1]) {
		/* ACK for a ping with no sequence number, we're done */
		ast_debug(9, "Received ACK from server\n");
		return 0;
	}

	/* Parse the ACK */
	ack_seqno = atoi(buf + 1);
	ast_debug(9, "Received ACK %d from server\n", ack_seqno);

	if (ack_seqno > c->sequence_no) {
		/* XXX Sometimes this happen if client unloads/loads while server has a higher sequence number,
		 * server does take the client's new sequence number (1) to start over,
		 * but it seems to take one event to resynchronize so this shows up once whenever the client reloads */
		ast_log(LOG_WARNING, "Received ACK %d from server, but haven't yet sent packet %d?\n", ack_seqno, c->sequence_no);
	}

	purge_sent_events(c, ack_seqno);
	return 0;
}

static void cleanup_phone_chan(struct alarm_client *c)
{
	ast_autoservice_stop(c->phonechan);
	ast_hangup(c->phonechan);
	c->phonechan = NULL;
}

static void *client_thread(void *arg)
{
	struct alarm_client *c = arg;
	struct pollfd pfds[2];
	int numfds = 1;
	int poll_interval = c->ping_interval * 1000; /* Convert s to ms */

	memset(pfds, 0, sizeof(pfds));
	pfds[0].fd = c->alertpipe[0];
	pfds[0].events = POLLIN;
	if (c->sfd != -1) {
		pfds[1].fd = c->sfd;
		pfds[1].events = POLLIN;
		numfds++;
	}

	/* Wait until the PBX is fully booted, similar to "core waitfullybooted"
	 * This is necessary because dialplan won't execute while the PBX is still booting,
	 * and events, including EVENT_ALARM_OKAY, can trigger user callbacks which execute dialplan. */
	while (!ast_test_flag(&ast_options, AST_OPT_FLAG_FULLY_BOOTED)) {
		usleep(1000);
	}

	/* First send a ping to initialize UDP communication and see if the network is up.
	 * This will generate the event but not actually process it until we start
	 * executing the loop, at which time we'll service the alertpipe immediately. */
	generate_event(c, EVENT_PING, NULL, NULL);

	/* Send our initialization event */
	generate_event(c, EVENT_ALARM_OKAY, NULL, NULL);

	while (!module_shutting_down) {
		int res;
		pfds[0].revents = pfds[1].revents = 0;
		res = poll(pfds, numfds, poll_interval);
		if (res < 0) {
			if (module_shutting_down) {
				ast_debug(3, "Client thread '%s' exiting\n", c->client_id);
				break;
			}
			ast_log(LOG_ERROR, "poll failed: %s\n", strerror(errno));
			break;
		} else if (res > 0) {
			if (pfds[0].revents & POLLIN) {
				/* We have at least one event to process. */
				ast_alertpipe_read(c->alertpipe);
				if (c->ip_connected) {
					send_events_to_server_by_ip(c);
				}
			}
			if (pfds[1].revents & POLLIN) {
				/* The server sent us something (probably an ACK, since we don't expect anything else) */
				process_server_ack(c);
			} else if (pfds[1].revents & POLLERR) {
				set_ip_connected(c, 0); /* Until proven otherwise, set as offline since we likely are */
				ast_debug(1, "Poll error with UDP socket, connection to alarm reporting server is likely broken\n");
				usleep(250000); /* Avoid tight loop */
			}
		}

		if (c->state == ALARM_STATE_TRIGGERED) {
			/* Check if we've hit the breach timer yet. */
			time_t now = time(NULL);
			if (now >= c->breach_time) {
				c->breach_time = 0;
				c->state = ALARM_STATE_BREACH;
				ast_log(LOG_NOTICE, "Client '%s' (%s) has not yet been disarmed, active breach!\n", c->client_id, c->name);
				generate_event(c, EVENT_ALARM_BREACH, NULL, NULL);
			}
		}

		/* This could be the else condition to the above else if conditionals,
		 * but the risk is that this branch gets starved, particularly
		 * if we get stuck in the POLLERR case above.
		 * This ensure that this will always get executed periodically,
		 * such as to report events by phone if needed. */
		if (res == 0 || c->ip_connected == 0) { /* res == 0 */
			time_t now = time(NULL);
			/* No need for pings if IP isn't enabled */
			if (c->ip_connected) {
				/* Handling to determine if we think the client is still online when it's really offline now */
				if (now >= c->last_ip_ack + c->ping_interval * 3 + 1) {
					/* Haven't gotten any ACKs over IP from the server in a while.
					 * Set client as offline. */
					ast_debug(1, "Confirmed connectivity loss: time is %lu, but haven't gotten an ACK since %lu\n", now, c->last_ip_ack);
					set_ip_connected(c, 0);
					/* It is possible at this point that we will get a reply to the next ping we send,
					 * later on in this function. The bizarre effect of this is that it is possible
					 * to have an INTERNET_LOST event immediately followed by INTERNET_RESTORED.
					 * It would require just the right number of pings to be lost followed by one that is not,
					 * immediately after we determine that connectivity has been lost. */
				} else if (now >= c->last_ip_ack + c->ping_interval * 2 + 1) {
					/* Haven't gotten any ACKs over IP from the server in a while.
					 * Don't immediately mark client as offline though. */
					ast_debug(1, "Likely connectivity loss: time is %lu, but haven't gotten an ACK since %lu\n", now, c->last_ip_ack);
					ast_log(LOG_NOTICE, "Significant packet loss encountered, possible connectivity loss\n");
					/* Don't set connected to 0 yet... allow one more ping, and send an extra one just to be sure. */
					generate_event(c, EVENT_PING, NULL, NULL);
					usleep(50000); /* Wait briefly before sending the next packet, since if this one is dropped, the next one probably will be too */
				}
			}
			/* There might still be some outstanding events that need to be delivered.
			 * For example, a few seconds ago, we were woken up to send events to server by IP,
			 * but haven't yet received ACKs for them (c->ip_connected is currently 1).
			 * Therefore, we go ahead and resend the events. */
			if (c->ip_connected) {
				send_events_to_server_by_ip(c);
			} else if (!ast_strlen_zero(c->server_dialstr)) {
				/* Attempt to deliver events by phone. */
				send_events_to_server_by_phone(c);
			} /* else, can't report events to a remote server */
			if (c->sfd != -1) {
				/* Nothing's happened. Ping the server to make sure the connection is still alive.
				 * Note that the network status may change while we're attempting to deliver events,
				 * so it's absolutely critical that we send pings periodically, particularly
				 * if there haven't been any recent ACKs.
				 *
				 * At most, we do not want to go more than twice the ping interval without a ping. */
				if (now > c->last_ip_ack + c->ping_interval / 2) {
					generate_event(c, EVENT_PING, NULL, NULL);
				} else {
					ast_debug(7, "Skipping ping this round since we got an IP ACK recently\n");
				}
			}
			if (c->phonechan) {
				/* If the phone channel is being autoserviced in the background,
				 * but hasn't been used in a while, get rid of it */
				if (c->autoservice_start < now - c->phone_hangup_delay) {
					ast_verb(5, "Disconnecting phone failover line\n");
					cleanup_phone_chan(c);
				}
			}
		}
	}

	if (c->phonechan) {
		cleanup_phone_chan(c);
	}

	return NULL;
}

static int invalid_telenumeric_id(const char *s, int permit_commas)
{
	while (*s) {
		if (!isdigit(*s) && *s != 'A' && *s != 'B' && *s != 'C' && *s != 'D' && (!permit_commas || *s != ',')) {
			return 1;
		}
		s++;
	}
	return 0;
}

static int load_config(void)
{
	const char *cat = NULL;
	struct ast_config *cfg;
	struct ast_flags config_flags = { 0 };
	struct ast_variable *var;
	struct ast_sockaddr sin;
	char bind_addr[80] = {0,};

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

	ast_sockaddr_setnull(&sin);

	/* Start with general */
	while ((cat = ast_category_browse(cfg, cat))) {
		if (!strcasecmp(cat, "general")) {
			for (var = ast_variable_browse(cfg, cat); var; var = var->next) {
				if (!strcasecmp(var->name, "bindport")) {
					bindport = atoi(var->value);
				} else if (!strcasecmp(var->name, "bindaddr")) {
					if (get_ipaddress(bind_addr, sizeof(bind_addr), var->value, AF_UNSPEC) == 0) {
						if (!ast_sockaddr_parse(&sin, bind_addr, 0)) {
							ast_log(LOG_WARNING, "Invalid host/IP '%s'\n", var->value);
						}
					}
				} else {
					ast_log(LOG_WARNING, "Unknown setting at line %d: '%s'\n", var->lineno, var->name);
				}
			}
		}
	}

	/* client and server */
	cat = NULL;
	while ((cat = ast_category_browse(cfg, cat))) {
		const char *type;
		if (!strcasecmp(cat, "general")) {
			continue;
		} else if (!strcasecmp(cat, "clients")) {
			continue;
		} else if (!(type = ast_variable_retrieve(cfg, cat, "type"))) {
			ast_log(LOG_WARNING, "Invalid entry in %s: %s defined with no type!\n", CONFIG_FILE, cat);
			continue;
		}

		if (!strcasecmp(type, "server")) {
			struct alarm_server *s;
			const int reuseFlag = 1;

			if (this_alarm_server) {
				ast_log(LOG_ERROR, "Only one server may be configured, ignoring '%s'\n", cat);
				continue;
			}

			s = ast_calloc(1, sizeof(*s));
			if (!s) {
				return -1;
			}
			s->listen_fd = -1;
			s->ip_loss_tolerance = DEFAULT_SERVER_IP_LOSS_TOLERANCE;

			for (var = ast_variable_browse(cfg, cat); var; var = var->next) {
				if (!strcasecmp(var->name, "type")) {
					continue;
				}

				if (!strcasecmp(var->name, "ip_loss_tolerance") && !ast_strlen_zero(var->value)) {
					s->ip_loss_tolerance = atoi(var->value);
				} else if (!strcasecmp(var->name, "logfile") && !ast_strlen_zero(var->value)) {
					ast_copy_string(s->logfile, var->value, sizeof(s->logfile));
				} else if (!strcasecmp(var->name, "contexts") && !ast_strlen_zero(var->value)) {
					link_contexts(cfg, s->contexts, var->value);
				} else {
					ast_log(LOG_WARNING, "Unknown keyword in section '%s': %s at line %d of %s\n", var->name, var->name, var->lineno, CONFIG_FILE);
				}
			}

			/* Start listening */
			ast_debug(3, "Initializing alarm server\n");
			io = io_context_create();
			sched = ast_sched_context_create();

			if (!io || !sched) {
				return -1;
			}

			if (ast_sockaddr_isnull(&sin)) {
				sprintf(bind_addr, "0.0.0.0:%d", bindport);
				ast_sockaddr_parse(&sin, bind_addr, 0);
			} else {
				ast_sockaddr_set_port(&sin, bindport);
			}

			s->listen_fd = socket(AST_AF_INET, SOCK_DGRAM, IPPROTO_IP);
			if (s->listen_fd < 0) {
				ast_log(LOG_ERROR, "Unable to create network socket: %s\n", strerror(errno));
				return -1;
			}
			if (setsockopt(s->listen_fd, SOL_SOCKET, SO_REUSEADDR, (char *) &reuseFlag, sizeof(reuseFlag)) < 0) {
				ast_log(LOG_WARNING, "Error setting SO_REUSEADDR on sockfd '%d'\n", s->listen_fd);
			}
			if (ast_bind(s->listen_fd, &sin)) {
				ast_log(LOG_ERROR, "Unable to bind to %s: %s\n", ast_sockaddr_stringify(&sin), strerror(errno));
				close(s->listen_fd);
				return -1;
			}

			ast_debug(1, "Started alarm system server on %s\n", ast_sockaddr_stringify(&sin));
			this_alarm_server = s; /* Only one server */
		} else if (!strcasecmp(type, "client")) {
			struct alarm_client *c;

			/* XXX Just assume no two clients are named with the same ID or have the same name */
			c = ast_calloc(1, sizeof(*c) + strlen(cat) + 1);
			if (!c) {
				return -1;
			}
			strcpy(c->data, cat); /* Safe */
			c->name = c->data;
			c->alertpipe[0] = c->alertpipe[1] = -1;
			if (ast_alertpipe_init(c->alertpipe)) {
				ast_log(LOG_ERROR, "Failed to initialize alertpipe\n");
				cleanup_client(c);
				return -1;
			}

			c->sequence_no = 1;
			c->thread = AST_PTHREADT_NULL;
			c->ping_interval = DEFAULT_PING_INTERVAL;
			c->phone_hangup_delay = DEFAULT_KEEP_LINE_OPEN_DELAY;

			for (var = ast_variable_browse(cfg, cat); var; var = var->next) {
				if (!strcasecmp(var->name, "type")) {
					continue;
				}

				if (!strcasecmp(var->name, "client_id") && !ast_strlen_zero(var->value)) {
					if (invalid_telenumeric_id(var->value, 0)) {
						ast_log(LOG_ERROR, "Invalid client ID '%s' (must contain only 0-9 and A-D)\n", var->value);
						cleanup_client(c);
						return -1;
					}
					ast_copy_string(c->client_id, var->value, sizeof(c->client_id));
				} else if (!strcasecmp(var->name, "client_pin") && !ast_strlen_zero(var->value)) {
					if (invalid_telenumeric_id(var->value, 0)) {
						ast_log(LOG_WARNING, "Invalid client PIN '%s' (must contain only 0-9 and A-D)\n", var->value);
					} else {
						ast_copy_string(c->client_pin, var->value, sizeof(c->client_pin));
					}
				} else if (!strcasecmp(var->name, "server_ip") && !ast_strlen_zero(var->value)) {
					int num_addrs, i;
					struct ast_sockaddr *addrs = NULL;

					if (!(num_addrs = ast_sockaddr_resolve(&addrs, var->value, PARSE_PORT_REQUIRE, AST_AF_UNSPEC))) {
						ast_log(LOG_ERROR, "Failed to resolve %s - requires a valid hostname and port\n", var->value);
						cleanup_client(c);
						return -1;
					}

					/* Try to connect */
					for (i = 0; i < num_addrs; i++) {
						if (!ast_sockaddr_port(&addrs[i])) {
							/* If there's no port, other addresses should have the same problem. Stop here. */
							ast_log(LOG_ERROR, "No port provided for %s\n", ast_sockaddr_stringify(&addrs[i]));
							i = num_addrs;
							goto postsocket;
						}

						if ((c->sfd = socket(addrs[i].ss.ss_family, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
							ast_log(LOG_WARNING, "Unable to create socket: %s\n", strerror(errno));
							continue;
						}

						if (ast_connect(c->sfd, &addrs[i])) {
							ast_log(LOG_ERROR, "Failed to connect to alarm server %s: %s\n", var->value, strerror(errno));
							goto postsocket;
						}
						ast_debug(1, "Connected to alarm server %s\n", ast_sockaddr_stringify(&addrs[i]));
						c->ip_connected = 1; /* Assume that we have connectivity unless proven otherwise by failed pings/unacked events */
						break;
					}
postsocket:
					if (addrs) {
						ast_free(addrs);
					}
					if (i == num_addrs) {
						ast_debug(2, "Failed to connect to any alarm server\n");
						cleanup_client(c);
						return -1;
					}
				} else if (!strcasecmp(var->name, "server_dialstr") && !ast_strlen_zero(var->value)) {
					ast_copy_string(c->server_dialstr, var->value, sizeof(c->server_dialstr));
				} else if (!strcasecmp(var->name, "phone_hangup_delay") && !ast_strlen_zero(var->value)) {
					c->phone_hangup_delay = atoi(var->value);
				} else if (!strcasecmp(var->name, "ping_interval") && !ast_strlen_zero(var->value)) {
					c->ping_interval = atoi(var->value);
				} else if (!strcasecmp(var->name, "egress_delay") && !ast_strlen_zero(var->value)) {
					c->egress_delay = atoi(var->value);
				} else if (!strcasecmp(var->name, "contexts") && !ast_strlen_zero(var->value)) {
					link_contexts(cfg, c->contexts, var->value);
				} else if (!strcasecmp(var->name, "logfile") && !ast_strlen_zero(var->value)) {
					ast_copy_string(c->logfile, var->value, sizeof(c->logfile));
				} else {
					ast_log(LOG_WARNING, "Unknown keyword in section '%s': %s at line %d of %s\n", cat, var->name, var->lineno, CONFIG_FILE);
				}
			}

			if (!c->client_id[0]) {
				ast_log(LOG_ERROR, "Client must have 'client_id' specified\n");
				cleanup_client(c);
				return -1;
			}

			if (ast_pthread_create_background(&c->thread, NULL, client_thread, c) < 0) {
				ast_log(LOG_ERROR, "Unable to start client thread\n");
				cleanup_client(c);
				return -1;
			}

			ast_debug(3, "Initializing alarm client '%s'\n", c->client_id);
			AST_RWLIST_INSERT_TAIL(&clients, c, entry);
		}
	}

	/* Now that all clients and servers are created, process anything that depends on their existence. */
	cat = NULL;
	while ((cat = ast_category_browse(cfg, cat))) {
		const char *type;
		if (!strcasecmp(cat, "general")) {
			continue;
		} else if (!strcasecmp(cat, "clients")) {
			struct alarm_server *s = this_alarm_server;
			if (!this_alarm_server) {
				ast_log(LOG_ERROR, "Can't configure reporting clients without configuring the server profile\n");
				return -1;
			}
			for (var = ast_variable_browse(cfg, cat); var; var = var->next) {
				struct reporting_client *r;
				size_t idlen, pinlen;

				idlen = strlen(var->name) + 1;
				pinlen = !ast_strlen_zero(var->value) ? strlen(var->value) + 1 : 0;

				r = ast_calloc(1, sizeof(*r) + idlen + pinlen);
				if (!r) {
					return -1;
				}
				ast_mutex_init(&r->lock);
				strcpy(r->data, var->name);
				r->client_id = r->data;
				if (!ast_strlen_zero(var->value)) {
					strcpy(r->data + idlen, var->value);
					r->client_pin = r->data + idlen;
				}
				r->next_ack = 1;
				AST_LIST_INSERT_TAIL(&s->clients, r, entry);
			}
			continue;
		} else if (!(type = ast_variable_retrieve(cfg, cat, "type"))) {
			/* Would've already thrown a warning about missing type, no need to do so a second time */
			continue;
		}

		if (!strcasecmp(type, "sensor")) {
			struct alarm_sensor *s;
			struct alarm_client *c;
			const char *client = ast_variable_retrieve(cfg, cat, "client");
			const char *device = ast_variable_retrieve(cfg, cat, "device");
			size_t devicelen = device ? strlen(device) + 1 : 0;

			if (!client) {
				ast_log(LOG_ERROR, "Ignoring sensor '%s', no client specified\n", cat); /* cat, not sensor ID */
				continue;
			}
			c = find_client_locked(client);
			if (!c) {
				ast_log(LOG_ERROR, "No such client '%s', unable to link sensor '%s' to it\n", client, cat);
				continue;
			}

			s = ast_calloc(1, sizeof(*s) + strlen(cat) + 1 + devicelen);
			if (!s) {
				return -1;
			}
			strcpy(s->data, cat);
			s->name = s->data;
			if (device) {
				strcpy(s->data + strlen(cat) + 1, device); /* Safe */
				s->device = s->data + strlen(cat) + 1;
			}

			s->disarm_delay = DEFAULT_DISARM_DELAY;

			for (var = ast_variable_browse(cfg, cat); var; var = var->next) {
				if (!strcasecmp(var->name, "type") || !strcasecmp(var->name, "device") || !strcasecmp(var->name, "client")) {
					continue;
				}

				if (!strcasecmp(var->name, "sensor_id") && !ast_strlen_zero(var->value)) {
					if (invalid_telenumeric_id(var->value, 0)) {
						ast_log(LOG_ERROR, "Invalid sensor ID '%s' (must contain only 0-9 and A-D)\n", var->value);
						ast_free(s);
						s = NULL;
						break;
					}
					ast_copy_string(s->sensor_id, var->value, sizeof(s->sensor_id));
				} else if (!strcasecmp(var->name, "disarm_delay") && !ast_strlen_zero(var->value)) {
					s->disarm_delay = atoi(var->value);
				} else {
					ast_log(LOG_WARNING, "Unknown keyword in section '%s': %s at line %d of %s\n", cat, var->name, var->lineno, CONFIG_FILE);
				}
			}
			/* Using a label after this block triggers warnings in old versions of gcc about label at end of compound statement,
			 * so just check the pointer: */
			if (s) {
				if (ast_strlen_zero(s->sensor_id)) {
					ast_log(LOG_ERROR, "Sensor '%s' missing sensor ID\n", cat);
					ast_free(s);
					return -1;
				}
				ast_debug(4, "Initializing alarm sensor %s\n", s->sensor_id);
				AST_RWLIST_INSERT_TAIL(&c->sensors, s, entry);
			}
		} else if (!strcasecmp(type, "keypad")) {
			struct alarm_client *c;
			const char *client = ast_variable_retrieve(cfg, cat, "client");

			if (!client) {
				ast_log(LOG_ERROR, "Ignoring keypad profile '%s', no client specified\n", cat);
				continue;
			}
			c = find_client_locked(client);
			if (!c) {
				ast_log(LOG_ERROR, "No such client '%s', unable to link keypad profile '%s' to it\n", client, cat);
				continue;
			}

			for (var = ast_variable_browse(cfg, cat); var; var = var->next) {
				if (!strcasecmp(var->name, "type") || !strcasecmp(var->name, "client")) {
					continue;
				}

				if (!strcasecmp(var->name, "keypad_device") && !ast_strlen_zero(var->value)) {
					ast_copy_string(c->keypad_device, var->value, sizeof(c->keypad_device));
				} else if (!strcasecmp(var->name, "pin") && !ast_strlen_zero(var->value)) {
					if (invalid_telenumeric_id(var->value, 1)) {
						ast_log(LOG_ERROR, "Invalid PIN '%s' (must contain only 0-9 and A-D)\n", var->value);
						return -1;
					}
					ast_copy_string(c->pin, var->value, sizeof(c->pin));
				} else if (!strcasecmp(var->name, "audio") && !ast_strlen_zero(var->value)) {
					if (c->audio) {
						ast_free(c->audio);
					}
					c->audio = ast_strdup(var->value);
				} else if (!strcasecmp(var->name, "cid_num") && !ast_strlen_zero(var->value)) {
					if (c->cid_num) {
						ast_free(c->cid_num);
					}
					c->cid_num = ast_strdup(var->value);
				} else if (!strcasecmp(var->name, "cid_name") && !ast_strlen_zero(var->value)) {
					if (c->cid_name) {
						ast_free(c->cid_name);
					}
					c->cid_name = ast_strdup(var->value);
				} else {
					ast_log(LOG_WARNING, "Unknown keyword in section '%s': %s at line %d of %s\n", cat, var->name, var->lineno, CONFIG_FILE);
				}
			}
			ast_debug(3, "Initializing keypad for alarm client %s\n", c->client_id);
		}
	}

	return 0;
}

static int alarmsensor_exec(struct ast_channel *chan, const char *data)
{
	char *argcopy;
	struct alarm_client *c;
	struct alarm_sensor *s;
	time_t breach_time;
	int is_egress;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(client);
		AST_APP_ARG(sensor);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_ERROR, "%s requires arguments\n", "AlarmSensor");
		return -1;
	}

	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, argcopy);

	if (ast_strlen_zero(args.client)) {
		ast_log(LOG_ERROR, "%s requires a client name\n", "AlarmSensor");
		return -1;
	}

	AST_RWLIST_RDLOCK(&clients);
	c = find_client_locked(args.client);
	if (!c) {
		ast_log(LOG_ERROR, "Client '%s' not found in configuration\n", args.client);
		goto cleanup;
	}

	/* Now, figure out which sensor this is. */
	if (!ast_strlen_zero(args.sensor)) {
		s = find_sensor(c, args.sensor);
		if (!s) {
			ast_log(LOG_ERROR, "No such sensor '%s'\n", args.sensor);
			goto cleanup;
		}
	} else {
		char device[64];
		ast_channel_get_device_name(chan, device, sizeof(device));
		s = find_sensor_by_device(c, device);
		if (!s) {
			ast_log(LOG_ERROR, "No sensor defined for device '%s'\n", device);
			goto cleanup;
		}
	}

	/* Okay, now we can start.
	 * Since the sensor just took the sensor loop off hook, it has been triggered. */
	s->triggered = 1;

	/* Update state from OK to TRIGGERED.
	 * From here, it can return to ALARM_STATE_OK if disarmed within s->disarm_delay time.
	 * Otherwise, it will transition to ALARM_STATE_BREACH at that time. */
	is_egress = c->last_arm > time(NULL) - c->egress_delay;
	if (is_egress) {
		ast_debug(1, "Egress is currently permitted, not triggering alarm\n");
		breach_time = 0;
	} else if (!s->disarm_delay) {
		ast_debug(1, "Sensor does not trigger alarms, no breach timer required\n");
		breach_time = 0;
	} else {
		time_t now = time(NULL);
		c->state = ALARM_STATE_TRIGGERED;
		breach_time = now + s->disarm_delay;
		ast_debug(3, "Time is currently %lu, breach will occur at %lu\n", now, breach_time);
	}

	if (breach_time) {
		char breachbuf[16];
		/* If no other sensor is currently triggered, or
		 * if this would cause us to enter the breach state sooner than existing triggered sensors,
		 * update the threshold at which we would transition. */
		if (!c->breach_time || breach_time < c->breach_time) {
			ast_debug(5, "Updating breach time from %lu to %lu\n", c->breach_time, breach_time);
			c->breach_time = breach_time;
		}
		/* Use an absolute time, not a relative number of seconds,
		 * in case we need to report this event by phone, we don't
		 * want to add an additional delay. */
		snprintf(breachbuf, sizeof(breachbuf), "%lu", c->breach_time); /* Send the breach_time for this sensor no matter what, server will ignore if not relevant */
		generate_event(c, EVENT_ALARM_SENSOR_TRIGGERED, s, breachbuf);
	} else {
		generate_event(c, EVENT_ALARM_SENSOR_TRIGGERED, s, NULL);
	}

	/* If we have a keypad device to autodial, kick that off */
	if (breach_time && !is_egress && !ast_strlen_zero(c->keypad_device)) {
		orig_app_device(c->keypad_device, NULL, "AlarmKeypad", c->name, c->cid_num, c->cid_name);
	}

	/* Now, wait for the sensor to be restored. This could be soon, it could not be. */
	while (ast_safe_sleep(chan, 60000) != -1);

	ast_debug(3, "Sensor '%s' appears to have been restored\n", s->name);
	s->triggered = 0;
	generate_event(c, EVENT_ALARM_SENSOR_RESTORED, s, NULL);

	/* The only time we get a WRLOCK on clients is when cleaning them up at module unload.
	 * Therefore, it's okay to hold the RDLOCK as long as this application is executing. */

cleanup:
	AST_RWLIST_UNLOCK(&clients);
	return -1; /* Never continue executing dialplan */
}

static int alarmeventreceiver_exec(struct ast_channel *chan, const char *data)
{
	int res;
	struct reporting_client *r;
	char clientid[32], clientpin[32];

	/* XXX Name not actually checked, since there can only be 1 server profile */
	if (!this_alarm_server) {
		ast_log(LOG_ERROR, "This server is not configured with a server profile\n");
		return -1;
	}

	/* Answer, since we need bidirectional audio */
	if (ast_answer(chan)) {
		return -1;
	}

	res = ast_dtmf_stream(chan, NULL, "*", 0, 75);
	if (res) {
		ast_log(LOG_WARNING, "Channel disappeared before ACK completed\n");
		return -1;
	}

	/* Read the client ID and client PIN */
	res = ast_app_getdata_terminator(chan, "", clientid, sizeof(clientid), 10000, "*");
	if (res != AST_GETDATA_COMPLETE) {
		ast_log(LOG_WARNING, "Failed to receive client ID\n");
		return -1;
	}
	ast_debug(3, "Client ID received is '%s'\n", clientid);
	res = ast_app_getdata_terminator(chan, "", clientpin, sizeof(clientpin), 5000, "*");
	if (res != AST_GETDATA_COMPLETE && res != AST_GETDATA_EMPTY_END_TERMINATED) {
		ast_log(LOG_WARNING, "Failed to receive client PIN\n");
		return -1;
	}

	r = authenticate_client(this_alarm_server, clientid, clientpin);
	if (!r) {
		/* Just hang up, since the client failed to authenticate */
		return -1;
	}

	ast_log(LOG_NOTICE, "Started telephone reporting session for client '%s'\n", clientid);

	/* Receive chunks of data from the client */
	for (;;) {
		char buf[MAX_PACKET_SIZE];
		char *seqno, *timestamp, *eventid, *sensorid, *data;
		int sequence_no, event;

		/* Wait up to a minute to receive data, in case some was already sent
		 * and the client is keeping the line open because it thinks it might
		 * send another event soon (eliminating the need to set up the connection again). */
		res = ast_app_getdata_terminator(chan, "", buf, 32, 60000, "#");
		if (res == AST_GETDATA_TIMEOUT) {
			ast_verb(5, "Alarm client channel %s timed out, disconnecting\n", ast_channel_name(chan));
			return -1;
		} else if (res == AST_GETDATA_COMPLETE || res == AST_GETDATA_EMPTY_END_TERMINATED) {
			/* Got an entire chunk, #-terminated */
			ast_debug(3, "Received chunk '%s'\n", buf);
		} else {
			if (ast_check_hangup(chan)) {
				ast_debug(3, "Channel %s hung up\n", ast_channel_name(chan));
			} else {
				ast_debug(3, "Disconnecting %s\n", ast_channel_name(chan));
			}
			return -1;
		}

		if (res == AST_GETDATA_EMPTY_END_TERMINATED) {
			char ack[24];
			/* Send an acknowledgment. */
			ast_debug(3, "Sending DTMF ACK %d\n", r->next_ack);
			snprintf(ack, sizeof(ack), "%u#", r->next_ack);
			res = ast_dtmf_stream(chan, NULL, ack, 150, 75);
			if (res) {
				ast_log(LOG_WARNING, "Channel disappeared before ACK completed\n");
				return -1;
			}
			continue;
		}

		/* Process this event */
		data = buf;
		seqno = strsep(&data, "*");
		timestamp = strsep(&data, "*");
		eventid = strsep(&data, "*");
		sensorid = strsep(&data, "*");

		if (ast_strlen_zero(seqno) || ast_strlen_zero(timestamp) || ast_strlen_zero(eventid)) {
			ast_log(LOG_WARNING, "Received empty data in non-empty permissible fields\n");
			return -1;
		}
		sequence_no = atoi(seqno);
		event = atoi(eventid);

		if (process_message(this_alarm_server, r, clientid, sequence_no, timestamp, event, sensorid, data) < 0) {
			return -1;
		}
	}
}

static int valid_pin(struct alarm_client *c, const char *input)
{
	int index = 0;
	char *pin, *allpins = ast_strdupa(c->pin);

	while ((pin = strsep(&allpins, ","))) {
		index++;
		if (!strcmp(pin, input)) {
			/* Return the index of the PIN so we know which one was used */
			return index;
		}
	}
	return 0;
}

static int alarmkeypad_exec(struct ast_channel *chan, const char *data)
{
	struct alarm_client *c;
	int attempts = 0;
	char digits[32];

	if (ast_strlen_zero(data)) {
		ast_log(LOG_ERROR, "%s requires arguments\n", "AlarmKeypad");
		return -1;
	}

	AST_RWLIST_RDLOCK(&clients);
	c = find_client_locked(data);
	if (!c) {
		ast_log(LOG_ERROR, "Client '%s' not found in configuration\n", data);
		goto cleanup;
	}

	/* XXX This implementation is still very primitive.
	 * For example, there is no way to totally disable the system
	 * so that sensor triggers do not trigger an alarm.
	 * The only workaround is accessing the keypad each time
	 * before the sensor trigger. */

	if (c->state == ALARM_STATE_TRIGGERED || c->state == ALARM_STATE_BREACH) {
		/* System needs to be disarmed */
		int res;

		if (!ast_strlen_zero(c->pin)) {
			if (ast_strlen_zero(c->audio)) {
				/* Just use an alert sounding tone */
				res = ast_playtones_start(chan, 0, "440/100,0/100", 0);
			}

#define NUM_ATTEMPTS 4

			/* Wait for keypad (DTMF) input */
			ast_stopstream(chan);
			while (++attempts <= NUM_ATTEMPTS) {
				ast_debug(4, "Alarm disarm attempt %d\n", attempts);
				digits[0] = '\0';
				res = ast_app_getdata_terminator(chan, c->audio, digits, sizeof(digits), 4000, NULL);
				if (res < 0) {
					break;
				}
				if (res == AST_GETDATA_COMPLETE || res == AST_GETDATA_EMPTY_END_TERMINATED || (res == AST_GETDATA_TIMEOUT && !ast_strlen_zero(digits))) {
					/* Could be comma-separated list */
					int pin_index = valid_pin(c, digits);
					if (pin_index) {
						ast_log(LOG_NOTICE, "Alarm successfully disarmed using pin %d\n", pin_index);
						generate_event(c, EVENT_ALARM_DISARMED, NULL, NULL);
						c->state = ALARM_STATE_OK;
						c->breach_time = 0; /* Reset, or it will cause an immediate breach when the sensor triggers again! */
						/* Play confirmation tone to indicate success */
						ast_playtones_start(chan, 0, "!350+440/100,!0/100,!350+440/100,!0/1000", 0);
						ast_safe_sleep(chan, 1250);
						break;
					}
					ast_log(LOG_WARNING, "Invalid PIN received\n");
				} else {
					if (res == AST_GETDATA_TIMEOUT && c->state == ALARM_STATE_OK) {
						/* Alarm must've been disarmed from another phone.
						 * If we're playing an audio file, the whole audio file will time out no matter what. */
						ast_verb(6, "Alarm was disarmed from another phone, exiting...\n");
						break;
					}
					ast_log(LOG_WARNING, "Alarm keypad timed out with no input\n");
				}
			}
			if (attempts == NUM_ATTEMPTS) {
				ast_log(LOG_WARNING, "Too many failed disarm attempts, disconnecting\n");
			}
		} else {
			ast_log(LOG_WARNING, "Missing PIN (no way to disarm alarm)\n");
		}
	} else { /* ALARM_STATE_OK */
		/* System can be temporarily disarmed, if desired.
		 * This just momentarily permits egress without triggering the alarm. */
		ast_verb(6, "Arming system, permitting egress for next %d second%s\n", c->egress_delay, ESS(c->egress_delay));
		c->last_arm = time(NULL);
		/* Play confirmation tone to indicate success */
		ast_playtones_start(chan, 0, "!350+440/100,!0/100,!350+440/100,!0/1000", 0);
		ast_safe_sleep(chan, 1250);
		generate_event(c, EVENT_ALARM_TEMP_DISARMED, NULL, NULL);
	}

cleanup:
	AST_RWLIST_UNLOCK(&clients);
	return 0;
}

static int sensor_triggered_read(struct ast_channel *chan, const char *cmd, char *parse, char *buffer, size_t buflen)
{
	char *argcopy;
	struct alarm_client *c;
	struct alarm_sensor *s;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(client);
		AST_APP_ARG(sensor);
	);

	if (ast_strlen_zero(parse)) {
		ast_log(LOG_ERROR, "Must specify client-name,sensor-name\n");
		return -1;
	}

	argcopy = ast_strdupa(parse);
	AST_STANDARD_APP_ARGS(args, argcopy);

	if (ast_strlen_zero(args.client)) {
		ast_log(LOG_ERROR, "Must specify client name\n");
		return -1;
	}

	AST_RWLIST_RDLOCK(&clients);
	c = find_client_locked(args.client);
	if (!c) {
		ast_log(LOG_ERROR, "Client '%s' not found in configuration\n", args.client);
		AST_RWLIST_UNLOCK(&clients);
		return -1;
	}
	if (!ast_strlen_zero(args.sensor)) {
		s = find_sensor(c, args.sensor);
		if (!s) {
			AST_RWLIST_UNLOCK(&clients);
			ast_log(LOG_ERROR, "No such sensor '%s'\n", args.sensor);
			return -1;
		}
	} else {
		AST_RWLIST_UNLOCK(&clients);
		ast_log(LOG_ERROR, "Must specify sensor name\n");
		return -1;
	}

	ast_copy_string(buffer, s->triggered ? "1" : "0", buflen);
	AST_RWLIST_UNLOCK(&clients);
	return 0;
}

static struct ast_custom_function acf_sensortriggered = {
	.name = "ALARMSYSTEM_SENSOR_TRIGGERED",
	.read = sensor_triggered_read,
};

static char *handle_show_sensors(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-12s %-20s %s\n"
#define FORMAT2 "%-12s %-20s %s\n"
	struct alarm_client *c;

	switch(cmd) {
	case CLI_INIT:
		e->command = "alarmsystem show sensors";
		e->usage =
			"Usage: alarmsystem show sensors\n"
			"       Lists all alarm sensors.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	ast_cli(a->fd, FORMAT, "Client", "Sensor", "State");
	AST_RWLIST_RDLOCK(&clients);
	AST_RWLIST_TRAVERSE(&clients, c, entry) {
		struct alarm_sensor *s;
		AST_LIST_TRAVERSE(&c->sensors, s, entry) {
			ast_cli(a->fd, FORMAT2, c->name, s->name, s->triggered ? "TRIGGERED" : "NORMAL");
		}
	}
	AST_RWLIST_UNLOCK(&clients);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static char *handle_show_clients(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-12s %-20s %7s %s\n"
#define FORMAT2 "%-12s %-20s %7s %s\n"
	struct alarm_client *c;

	switch(cmd) {
	case CLI_INIT:
		e->command = "alarmsystem show clients";
		e->usage =
			"Usage: alarmsystem show clients\n"
			"       Lists all alarm clients.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	ast_cli(a->fd, FORMAT, "Client ID", "Name", "IP Conn", "State");
	AST_RWLIST_RDLOCK(&clients);
	AST_RWLIST_TRAVERSE(&clients, c, entry) {
		ast_cli(a->fd, FORMAT2, c->client_id, c->name, c->ip_connected ? "ONLINE" : "OFFLINE", state2text(c->state));
	}
	AST_RWLIST_UNLOCK(&clients);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static char *handle_show_events(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-12s %8s %8s %s\n"
#define FORMAT2 "%-12s %8d %8d %s\n"
	struct alarm_client *c;

	switch(cmd) {
	case CLI_INIT:
		e->command = "alarmsystem show events";
		e->usage =
			"Usage: alarmsystem show events [<client name>]\n"
			"       Lists all unreported (in flight) alarm events, optionally filtered by client name.\n";
		return NULL;
	case CLI_GENERATE:
		if (a->pos == 3) {
			char *ret = NULL;
			int which = 0;
			size_t wlen = strlen(a->word);
			AST_RWLIST_RDLOCK(&clients);
			AST_RWLIST_TRAVERSE(&clients, c, entry) {
				if (!strncasecmp(a->word, c->name, wlen) && ++which > a->n) {
					ret = ast_strdup(c->name);
					break;
				}
			}
			AST_RWLIST_UNLOCK(&clients);
			return ret;
		}
		return NULL;
	}

	ast_cli(a->fd, FORMAT, "Client", "SeqNo", "Attempts", "Encoded Event");
	AST_RWLIST_RDLOCK(&clients);
	AST_RWLIST_TRAVERSE(&clients, c, entry) {
		struct alarm_event *e;
		if (a->argc == 4 && strcasecmp(a->argv[3], c->name)) {
			continue;
		}
		AST_LIST_TRAVERSE(&c->events, e, entry) {
			ast_cli(a->fd, FORMAT2, c->name, e->seqno, e->attempts, e->encoded);
		}
	}
	AST_RWLIST_UNLOCK(&clients);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static char *handle_show_reporters(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-12s %7s %s\n"
#define FORMAT2 "%-12s %7s %s\n"
	struct reporting_client *r;

	switch(cmd) {
	case CLI_INIT:
		e->command = "alarmsystem show reporters";
		e->usage =
			"Usage: alarmsystem show reporters\n"
			"       Lists all reporting alarm clients.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (!this_alarm_server) {
		ast_cli(a->fd, "An alarm reporting server is not configured.\n");
		return CLI_FAILURE;
	}

	ast_cli(a->fd, FORMAT, "Client ID", "IP Conn", "State");
	AST_LIST_LOCK(&this_alarm_server->clients);
	AST_LIST_TRAVERSE(&this_alarm_server->clients, r, entry) {
		ast_cli(a->fd, FORMAT2, r->client_id, r->ip_connected ? "ONLINE" : "OFFLINE", state2text(r->state));
	}
	AST_LIST_UNLOCK(&this_alarm_server->clients);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

static struct ast_cli_entry alarmsystem_cli[] = {
	AST_CLI_DEFINE(handle_show_reporters, "List all reporting alarm clients"),
	AST_CLI_DEFINE(handle_show_clients, "List all alarm clients"),
	AST_CLI_DEFINE(handle_show_sensors, "List all alarm sensors"),
	AST_CLI_DEFINE(handle_show_events, "List all unreported (in flight) alarm events"),
};

static int unload_module(void)
{
	module_shutting_down = 1;

	ast_cli_unregister_multiple(alarmsystem_cli, ARRAY_LEN(alarmsystem_cli));
	ast_custom_function_unregister(&acf_sensortriggered);
	ast_unregister_application("AlarmSensor");
	ast_unregister_application("AlarmEventReceiver");
	ast_unregister_application("AlarmKeypad");

	/* Stop server thread */
	if (io) {
		if (netthreadid != AST_PTHREADT_NULL) {
			pthread_kill(netthreadid, SIGURG);
			pthread_join(netthreadid, NULL);
			netthreadid = AST_PTHREADT_NULL;
		}
	}

	if (io) {
		io_context_destroy(io);
	}
	if (sched) {
		ast_sched_context_destroy(sched);
	}

	cleanup_clients();
	if (this_alarm_server) {
		cleanup_server(this_alarm_server);
		this_alarm_server = NULL;
	}
	return 0;
}

static int load_module(void)
{
	int res = 0;

	if (load_config()) {
		ast_log(LOG_ERROR, "Aborting load, please fix configuration\n");
		unload_module();
		return AST_MODULE_LOAD_DECLINE;
	}

	if (io && ast_pthread_create_background(&netthreadid, NULL, server_thread, NULL) < 0) {
		ast_log(LOG_ERROR, "Unable to start server thread\n");
		unload_module();
		return AST_MODULE_LOAD_DECLINE;
	}

	res |= ast_register_application_xml("AlarmSensor", alarmsensor_exec);
	res |= ast_register_application_xml("AlarmEventReceiver", alarmeventreceiver_exec);
	res |= ast_register_application_xml("AlarmKeypad", alarmkeypad_exec);
	if (res) {
		ast_log(LOG_ERROR, "Unable to register applications\n");
		unload_module();
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_custom_function_register(&acf_sensortriggered);
	ast_cli_register_multiple(alarmsystem_cli, ARRAY_LEN(alarmsystem_cli));
	return 0;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Simple Alarm System");
