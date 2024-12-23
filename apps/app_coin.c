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
 * \brief Local coin control applications
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<depend>res_coindetect</depend>
	<depend>chan_bridge_media</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <math.h>

#include "asterisk/module.h"
#include "asterisk/frame.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/app.h"
#include "asterisk/cli.h"
#include "asterisk/indications.h"
#include "asterisk/bridge.h"
#include "asterisk/causes.h"
#include "asterisk/format_cache.h"
#include "asterisk/core_unreal.h"
#include "asterisk/stream.h" /* use ast_stream_topology_clone */
#include "asterisk/rtp_engine.h" /* use ast_rtp_instance_early_bridge_make_compatible */
#include "asterisk/max_forwards.h" /* use ast_max_forwards_decrement */
#include "asterisk/stasis_channels.h" /* use ast_channel_publish_dial */

/*** DOCUMENTATION
	<application name="LocalCoinDisposition" language="en_US">
		<synopsis>
			Manually perform a local coin control action
		</synopsis>
		<syntax>
			<parameter name="action" required="true">
				<para>Action to perform.</para>
				<enumlist>
					<enum name="collect">
						<para>Coin collect</para>
						<para>Collects coins currently in the hopper into
						the coin vault.</para>
					</enum>
					<enum name="return">
						<para>Coin return</para>
						<para>Returns any coins currently in the hopper
						to the caller.</para>
					</enum>
					<enum name="groundtest">
						<para>Perform a initial deposit (ground) test.</para>
						<para>The variable <literal>COIN_GROUND_TEST_RESULT</literal>
						should be set via AMI in response.</para>
					</enum>
					<enum name="stuckcointest">
						<para>Perform a stuck coin (coin present) test.</para>
						<para>The variable <literal>COIN_PRESENT_TEST_RESULT</literal>
						should be set via AMI in response.</para>
					</enum>
					<enum name="released">
						<para>Operator released</para>
						<para>Used with dial tone first phones to apply negative
						battery to the coin line in order to change the mode of the
						coin totalizer to the local mode, so that readout does not
						occur in realtime but only when the initial deposit is
						reached. The telephone keypad is also reenabled, and the keypad
						has precedence over the totalizer in this mode.</para>
					</enum>
					<enum name="attached">
						<para>Operator attached</para>
						<para>Used with dial tone first coin phones to apply positive
						battery to the coin line in order to change the mode of the
						coin totalizer to the toll mode, so that deposits of any
						denomination result in an immediate readout by the totalizer
						for ACTS or the operator. The telephone keypad is
						also disabled, and the totalizer has precedence over the
						keypad in this mode.</para>
						<para>Refer to Bellcore SR-2275-3 6.17.3.4.1 for more information
						about operator attached and released and the keypad and totalizer.</para>
					</enum>
				</enumlist>
			</parameter>
		</syntax>
		<description>
			<para>Handle coin control for a call on a dial tone first coin line.</para>
			<para>This application can be used to manually send coin control
			signals on a Class 5 switch. It is generally recommended
			that you use the <literal>CoinCall</literal> application;
			however, there are certain scenarios where you may want to manually
			send a control signal, such as performing a stuck coin test when
			a phone goes off hook.</para>
			<para>This application will emit corresponding AMI events that will
			need to be handled by a program listening via AMI interfaced with
			your coin controller.</para>
			<variablelist>
				<variable name="COIN_GROUND_TEST_RESULT">
					<para>This variable is expected to be set by the user in response
					to a ground test (e.g. via AMI). It should be <literal>0</literal>
					if the initial deposit is not satisfied and <literal>1</literal>
					if the initial deposit is satisfied.</para>
				</variable>
				<variable name="COIN_PRESENT_TEST_RESULT">
					<para>This variable is expected to be set by the user in response
					to a stuck coin test (e.g. via AMI). It should be <literal>0</literal>
					if there are no coins in the hopper and <literal>1</literal>
					if there are coins in the hopper.</para>
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">CoinCall</ref>
			<ref type="application">CoinDisposition</ref>
			<ref type="application">ACTS</ref>
			<ref type="function">COIN_DETECT</ref>
			<ref type="function">COIN_EIS</ref>
		</see-also>
	</application>
	<application name="CoinCall" language="en_US">
		<synopsis>
			Local coin call handler
		</synopsis>
		<syntax>
			<parameter name="dialstr" required="true">
				<para>Tech/resource to dial for the outgoing call.</para>
				<para>If you need more complexity than this offers, you can dial a Local
				channel that dials the actual destination.</para>
			</parameter>
			<parameter name="initialperiod" required="false" default="0">
				<para>Duration, in seconds, of initial period.</para>
				<para>Default is 0 (unlimited initial period).</para>
			</parameter>
			<parameter name="initialdeposit" required="false" default="0">
				<para>Amount of initial deposit.</para>
				<para>If non-zero, a ground test will be performed to
				ensure that the initial deposit has been satisfied.
				Currently, this application does not use the amount provided here,
				only whether it is nonzero or not. This value should match
				the setting on the pay station.</para>
			</parameter>
			<parameter name="overtimeperiod" required="false" default="0">
				<para>Duration, in seconds, of local overtime period.</para>
				<para>Default is 0 (only one local overtime charge required).</para>
				<para>This option has no effect unless initialperiod is nonzero.</para>
				<para>The local overtime charge is 5 cents per overtime period
				and cannot be modified.</para>
			</parameter>
			<parameter name="announcement" required="false">
				<para>Announcement to play to inform caller local overtime deposit is required.</para>
				<para>This announcement is required if initialperiod is not 0.</para>
				<para>This recording MUST be less than 30 seconds long.</para>
			</parameter>
			<parameter name="options" required="false">
				<optionlist>
					<option name="a">
						<para>Put the phone in Operator Attached Mode before beginning the call,
						useful if the call is being sent to a toll switch for long distance handling.</para>
					</option>
					<option name="c">
						<para>Enable Feature Group C trunk mode for the call.</para>
						<para>This is intended when the call is sent to a Class 4 switch
						using a Feature Group C trunk with Expanded In-Band Signaling.
						This will ensure that corresponding coin disposition AMI events
						are emitted for directives from the Class 4 switch.</para>
					</option>
					<option name="r">
						<para>If initial deposit is satisfied, return any coins deposited
						before initiating the call.</para>
						<para>This option is intended for toll calls where deposits are
						handled by the toll switch.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Dialplan application for Class 5 switches to process calls from coin stations.</para>
			<para>This application will automatically collect and return coins as needed,
			perform a ground test to verify presence of initial deposit, and perform
			a stuck coin test for local coin overtime, if desired.</para>
			<para>If further deposits will be required, coins will be collected 30 seconds
			prior to a local overtime announcement being played.</para>
			<para>This application does not do any detection of coin denomination tones.
			A compatible coin controller interface is required.</para>
			<variablelist>
				<variable name="COIN_GROUND_TEST_RESULT">
					<para>This variable is expected to be set by the user in response
					to a ground test (e.g. via AMI). It should be <literal>0</literal>
					if the initial deposit is not satisfied and <literal>1</literal>
					if the initial deposit is satisfied.</para>
				</variable>
				<variable name="COIN_PRESENT_TEST_RESULT">
					<para>This variable is expected to be set by the user in response
					to a stuck coin test (e.g. via AMI). It should be <literal>0</literal>
					if there are no coins in the hopper and <literal>1</literal>
					if there are coins in the hopper.</para>
				</variable>
				<variable name="COIN_RESULT">
					<para>This is the result of the call.</para>
					<value name="CALLER_HANGUP">
						Call was answered and caller hung up
					</value>
					<value name="CALLER_ABORT">
						Caller hung up before call was answered
					</value>
					<value name="CALLEE_HANGUP">
						Call was answered and callee hung up
					</value>
					<value name="NOANSWER">
						Callee hung up without far end supervision
					</value>
					<value name="INITIAL_DEPOSIT_REQUIRED">
						Call rejected, initial deposit not satisfied
					</value>
					<value name="OVERTIME_DEPOSIT_REQUIRED">
						Call terminated due to lack of deposit for local call overtime
					</value>
					<value name="BUSY">
						Out of band busy
					</value>
					<value name="CONGESTION">
						Out of band congestion
					</value>
					<value name="FAILURE">
						General failure
					</value>
					<value name="INVALID">
						Invalid arguments.
					</value>
				</variable>
				<variable name="COIN_FINAL_DISPOSITION">
					<para>This indicates whether any coins in the hopper at call completition were collected or returned.</para>
					<value name="COLLECT">
						Any coins in the hopper were collected
					</value>
					<value name="RETURN">
						Any coins in the hopper were returned
					</value>
					<value name="NONE">
						No coins were in the hopper, no action taken
					</value>
				</variable>
				<variable name="COIN_AMOUNT_COLLECTED">
					<para>The total amount of money, in cents, collected into the coin vault for this call.</para>
					<para>This variable is only set if <literal>COIN_RESULT</literal> is not <literal>INVALID</literal>.</para>
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">LocalCoinDisposition</ref>
			<ref type="application">CoinDisposition</ref>
			<ref type="application">ACTS</ref>
			<ref type="function">COIN_DETECT</ref>
			<ref type="function">COIN_EIS</ref>
		</see-also>
	</application>
 ***/

/*! \brief Information about an coin call */
struct coin_call {
	/* Channels */
	struct ast_channel *chan;	/* Calling channel */
	struct ast_channel *ochan;	/* Outgoing channel */
	struct ast_channel *achan;	/* Announcement channel */
	struct ast_bridge *bridge;
	/* Call configuration */
	const char *audiodir;
	const char *tech;
	const char *resource;
	int initialperiod;
	int initialdeposit;
	int overtimedeposit;
	int overtimeperiod;
	const char *announcement;
	/* Call properties */
	int hopper;
	int collected;
	time_t start;
	time_t answertime;
	time_t expiretime;	/* When coin->chan will next be kicked from the bridge */
	/* Bit fields */
	unsigned int callerdisconnected:1;
	unsigned int calleedisconnected:1;
	unsigned int attached:1;	/*!< Currently operator attached? */
	unsigned int attachfirst:1;
	unsigned int fgc:1;
	unsigned int returnfirst:1;
	const char *result;
	const char *finaldisp;
	ast_mutex_t lock;
	AST_RWLIST_ENTRY(coin_call) entry;
};

/*! \brief Linked list of all coin calls */
static AST_RWLIST_HEAD_STATIC(calls, coin_call);

enum {
	OPT_ATTACH_FIRST = (1 << 0),
	OPT_FEATURE_GROUP_C = (1 << 1),
	OPT_RETURN_FIRST = (1 << 2),
};

AST_APP_OPTIONS(app_opts,{
	AST_APP_OPTION('a', OPT_ATTACH_FIRST),
	AST_APP_OPTION('c', OPT_FEATURE_GROUP_C),
	AST_APP_OPTION('r', OPT_RETURN_FIRST),
});

static const char *disp_app = "LocalCoinDisposition";
static const char *coin_app = "CoinCall";

enum coin_disposition {
	COIN_COLLECT,
	COIN_RETURN,
	COIN_GROUND_TEST,
	COIN_STUCK_COIN_TEST,
	OPERATOR_ATTACHED,
	OPERATOR_RELEASED,
};

static int send_ami_event(struct ast_channel *chan, const char *signal)
{
	/*** DOCUMENTATION
		<managerEvent language="en_US" name="CoinControl">
			<managerEventInstance class="EVENT_FLAG_CALL">
				<synopsis>Raised when a local coin control signal is requested.</synopsis>
					<syntax>
						<channel_snapshot/>
						<parameter name="Disposition">
							<para>The coin control signal the controller needs to perform.</para>
							<para>Will be one of the following:</para>
							<enumlist>
								<enum name="CoinReturn"/>
								<enum name="CoinCollect"/>
								<enum name="OperatorRingback"/>
								<enum name="OperatorReleased"/>
								<enum name="OperatorAttached"/>
								<enum name="GroundTest"/>
								<enum name="StuckCoinTest"/>
							</enumlist>
							<para>For ground test and stuck coin test, the controller
							also needs to set the variable <literal>COIN_GROUND_TEST_RESULT</literal>
							or <literal>COIN_PRESENT_TEST_RESULT</literal> on the channel as appropriate.
							See the documentation for <literal>CoinCall</literal> for more details.</para>
						</parameter>
					</syntax>
			</managerEventInstance>
		</managerEvent>
	***/
	manager_event(EVENT_FLAG_CALL, "CoinControl",
		"Channel: %s\r\n"
		"ChannelState: %d\r\n"
		"ChannelStateDesc: %s\r\n"
		"CallerIDNum: %s\r\n"
		"CallerIDName: %s\r\n"
		"ConnectedLineNum: %s\r\n"
		"ConnectedLineName: %s\r\n"
		"Language: %s\r\n"
		"AccountCode: %s\r\n"
		"Context: %s\r\n"
		"Exten: %s\r\n"
		"Priority: %d\r\n"
		"Uniqueid: %s\r\n"
		"Linkedid: %s\r\n"
		"Disposition: %s\r\n", /* Use same key name as CoinDisposition */
		ast_channel_name(chan),
		ast_channel_state(chan),
		ast_state2str(ast_channel_state(chan)),
		ast_channel_caller(chan)->id.number.str,
		ast_channel_caller(chan)->id.name.str,
		ast_channel_connected(chan)->id.number.str,
		ast_channel_connected(chan)->id.name.str,
		ast_channel_language(chan),
		ast_channel_accountcode(chan),
		ast_channel_context(chan),
		ast_channel_exten(chan),
		ast_channel_priority(chan),
		ast_channel_uniqueid(chan),
		ast_channel_linkedid(chan),
		signal);
	return 0;
}

static int local_coin_disposition(struct coin_call *coin, struct ast_channel *chan, enum coin_disposition disp)
{
	ast_assert(chan != NULL);

	/* Note: It is certainly possible that things outside of this application
	 * may send AMI events to the controller at the same time, e.g. COIN_EIS,
	 * in which case our view of the call is perturbed slightly,
	 * but from our perspective, we don't know about those,
	 * and any assumptions made explicit here should still hold. */

	switch (disp) {
	case COIN_COLLECT:
		ast_verb(5, "Performing coin collect on %s\n", ast_channel_name(chan));
		if (coin) {
			coin->collected += coin->hopper;
			coin->hopper = 0;
		}
		return send_ami_event(chan, "CoinCollect");
	case COIN_RETURN:
		ast_verb(5, "Performing coin return on %s\n", ast_channel_name(chan));
		if (coin) {
			coin->hopper = 0;
		}
		return send_ami_event(chan, "CoinReturn");
	case COIN_GROUND_TEST:
		ast_verb(5, "Performing ground test on %s\n", ast_channel_name(chan));
		if (coin) {
			coin->hopper = 0;
		}
		return send_ami_event(chan, "GroundTest");
	case COIN_STUCK_COIN_TEST:
		ast_verb(5, "Performing stuck coin test on %s\n", ast_channel_name(chan));
		if (coin) {
			coin->hopper = 0;
		}
		return send_ami_event(chan, "StuckCoinTest");
	case OPERATOR_ATTACHED:
		if (coin) {
			ast_assert(!coin->attached);
			coin->attached = 1;
		}
		ast_debug(3, "Putting %s into operator attached mode\n", ast_channel_name(chan));
		return send_ami_event(chan, "OperatorAttached");
	case OPERATOR_RELEASED:
		if (coin) {
			ast_assert(coin->attached);
			coin->attached = 0;
		}
		ast_debug(3, "Putting %s into operator released mode\n", ast_channel_name(chan));
		return send_ami_event(chan, "OperatorReleased");
	}
	__builtin_unreachable();
}

static int local_coin_disposition_exec(struct ast_channel *chan, const char *data)
{
	if (ast_strlen_zero(data)) {
		ast_log(LOG_ERROR, "%s requires an argument\n", disp_app);
		return -1;
	}
	if (!strcasecmp(data, "collect")) {
		return local_coin_disposition(NULL, chan, COIN_COLLECT);
	} else if (!strcasecmp(data, "return")) {
		return local_coin_disposition(NULL, chan, COIN_RETURN);
	} else if (!strcasecmp(data, "groundtest")) {
		return local_coin_disposition(NULL, chan, COIN_GROUND_TEST);
	} else if (!strcasecmp(data, "stuckcointest")) {
		return local_coin_disposition(NULL, chan, COIN_STUCK_COIN_TEST);
	} else if (!strcasecmp(data, "attached")) {
		return local_coin_disposition(NULL, chan, OPERATOR_ATTACHED);
	} else if (!strcasecmp(data, "released")) {
		return local_coin_disposition(NULL, chan, OPERATOR_RELEASED);
	} else {
		ast_log(LOG_ERROR, "Unknown disposition: %s\n", data);
		return -1;
	}
}

/*!
 * \brief Read a numeric variable from a channel
 * \param chan
 * \param varname Name of variable to read
 * \retval -1 Variable not present on channel
 * \return Numeric value of variable (presumably zero or positive)
 */
static int read_variable_value(struct ast_channel *chan, const char *varname)
{
	int result = -1;
	const char *val;

	ast_channel_lock(chan);
	val = pbx_builtin_getvar_helper(chan, varname);
	if (val) {
		result = atoi(val);
		ast_debug(3, "Channel variable set on %s: %s = %s\n", ast_channel_name(chan), varname, val);
	}
	ast_channel_unlock(chan);

	return result;
}

/*! \brief Send a coin control signal and wait up to a second for its response */
static int send_signal_and_read_response(struct coin_call *coin, enum coin_disposition disp, const char *varname)
{
	int i, res;

	/* Clear existing variable, so we don't read a stale value */
	pbx_builtin_setvar_helper(coin->chan, varname, NULL);

	res = local_coin_disposition(coin, coin->chan, disp);
	if (res < 0) {
		return res;
	}

	/* Wait for the results.
	 * Could stream a short audio file, or just sleep for a moment. */

	/* Different controllers may take slightly different amounts of time
	 * for the controller logic, so keep polling for up to a second
	 * until we get a response. */
	for (i = 0; i < 10; i++) {
		if (ast_safe_sleep(coin->chan, 100)) {
			return -1;
		}
		res = read_variable_value(coin->chan, varname);
		if (res >= 0) {
			return res;
		}
	}

	/* This might seem drastic, but if the controller hasn't provided us an answer,
	 * there is no reasonable course of action besides disconnecting the call. */
	ast_log(LOG_ERROR, "Failed to receive response from controller: '%s' variable not set after 1 second\n", varname);
	return -1;
}

/*!
 * \brief Determine if initial deposit is satisfied by performing a ground test and reading the response
 * \param coin
 * \retval -1 failure
 * \retval 0 Deposit not satisfied
 * \retval 1 Deposit satisfied
 */
static int check_initial_deposit(struct coin_call *coin)
{
	return send_signal_and_read_response(coin, COIN_GROUND_TEST, "COIN_GROUND_TEST_RESULT");
}

/*!
 * \brief Perform a stuck coin test
 * \param coin
 * \retval -1 failure
 * \retval 0 No coin(s) present in hopper
 * \retval 1 Coin(s) present in hopper
 */
static int do_stuck_coin_test(struct coin_call *coin)
{
	return send_signal_and_read_response(coin, COIN_STUCK_COIN_TEST, "COIN_PRESENT_TEST_RESULT");
}

#ifdef ADJUST_GAINS
static int set_gains(struct coin_call *coin, int lopsided)
{
	ast_channel_lock(coin->ochan);
	ast_func_write(coin->ochan, "VOLUME(RX)", lopsided ? "-3" : "0");
	ast_channel_unlock(coin->ochan);
	return ast_func_write(coin->chan, "VOLUME(RX)", lopsided ? "2" : "0");
}
#endif

static void *async_announcements(void *varg)
{
	struct coin_call *coin = varg;

#ifdef ADJUST_GAINS
	/* Allow the conversation to continue during local coin overtime announcement,
	 * but reduce the volume of the callee to make the announcement stand out. */
	set_gains(coin, 1);
#endif
	if (ast_stream_and_wait(coin->achan, coin->announcement, "")) {
		/* If somebody hangs up, that will kick us from the bridge,
		 * and we shouldn't emit a warning about that. */
		if (!coin->callerdisconnected && !coin->calleedisconnected) {
			ast_log(LOG_WARNING, "Failed to play %s\n", coin->announcement);
		}
	}
#ifdef ADJUST_GAINS
	set_gains(coin, 0); /* Restore normal levels */
#endif
	/* Don't kick main channel when done.
	 * This announcement should be less than 30 seconds,
	 * and we'll manually test after that.
	 * However, we still need to service ourselves until we get hung up,
	 * which will be no MORE than 30 seconds from now. */
	ast_safe_sleep(coin->achan, 30000);
	ast_debug(5, "Announcement thread is exiting\n");
	return NULL;
}

static struct ast_channel *alloc_playback_chan(struct coin_call *coin)
{
	struct ast_channel *chan;
	struct ast_format_cap *cap;

	cap = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT);
	if (!cap) {
		return NULL;
	}

	ast_format_cap_append(cap, ast_format_slin, 0);
	chan = ast_request("Announcer", cap, NULL, NULL, "CoinCall", NULL);
	ao2_ref(cap, -1);
	return chan;
}

static int reverse_battery(struct coin_call *coin)
{
	int res;

	/* Reverse the line for 600 ms by putting it in operator attached
	 * and then back to operator released.
	 * On a 1C2 in local mode, this causes the totalizer to step back to home. */
	res = local_coin_disposition(coin, coin->chan, OPERATOR_ATTACHED);
	if (!res) {
		res = ast_safe_sleep(coin->chan, 600);
	}
	if (!res) {
		res = local_coin_disposition(coin, coin->chan, OPERATOR_RELEASED);
	}
	return res;
}

/* Forward declaration */
static int bridge_with_timeout(struct coin_call *coin, struct ast_bridge *bridge, int timeout, int overtime);

#define MINIMUM_PERIOD 60
#define SECS_COLLECT_BEFORE_PROMPTING 30

/*!
 * \brief Obtain overtime deposit
 * \param coin
 * \retval -1 failure
 * \retval 0 Deposit not received, disconnect call
 * \retval 1 Deposit received, continue call
 */
static int get_overtime_deposit(struct coin_call *coin)
{
	int res;
	struct ast_channel *achan;
	pthread_t prompt_thread;

	/*
	 * First, collect any coins in the hopper.
	 * Then, after 30 seconds, reverse battery (operator attached/released),
	 * and then check if a coin is present in the hopper.
	 * If so, deposit is satisfied and we continue normally.
	 *
	 * If not, play the local overtime announcement.
	 * Then, 30 seconds after the recording started,
	 * check again if a coin is present.
	 * If not, terminate the call. Otherwise, continue normally.
	 */

	res = local_coin_disposition(coin, coin->chan, COIN_COLLECT);
	if (res < 0) {
		return -1;
	}
	res = bridge_with_timeout(coin, coin->bridge, SECS_COLLECT_BEFORE_PROMPTING, 1);
	if (res < 0) {
		return -1;
	}

	res = reverse_battery(coin);
	if (res < 0) {
		return res;
	}
	res = do_stuck_coin_test(coin);
	if (res < 0) {
		return res;
	} else if (res > 0) {
		/* Overtime deposit received */
		coin->hopper += 5;
		return 1;
	}

	/* Since we're still bridged, we can't run the prompts
	 * directly on the channel. Instead, we need to
	 * play the prompts into the bridge using another channel. */
	achan = alloc_playback_chan(coin);
	if (!achan) {
		ast_log(LOG_ERROR, "Failed to allocate announcement channel\n");
		goto cleanup2;
	}
	if (ast_unreal_channel_push_to_bridge(achan, coin->bridge, AST_BRIDGE_CHANNEL_FLAG_IMMOVABLE | AST_BRIDGE_CHANNEL_FLAG_LONELY)) {
		ast_log(LOG_ERROR, "Failed to push announcement channel into bridge\n");
		res = -1;
		goto cleanup;
	}

	/* Play announcement in call */
	coin->achan = achan;
	if (ast_pthread_create(&prompt_thread, NULL, async_announcements, coin)) {
		ast_log(LOG_ERROR, "Failed to create announcement thread.\n");
		res = -1;
		goto cleanup;
	}

	/* Wait 30 seconds and then check for deposit again */
	res = bridge_with_timeout(coin, coin->bridge, SECS_COLLECT_BEFORE_PROMPTING, 1);
	if (res >= 0) {
		res = reverse_battery(coin);
		if (!res) {
			res = do_stuck_coin_test(coin);
			if (res > 0) {
				coin->hopper += 5;
			}
		}
	}
	ast_debug(5, "Signaling %s to exit\n", ast_channel_name(coin->achan));
	ast_softhangup(coin->achan, AST_SOFTHANGUP_EXPLICIT); /* Make the announcement thread exit */
	ast_bridge_remove(coin->bridge, coin->achan);
	pthread_join(prompt_thread, NULL);

	/* Clean up and return to the call */
	ast_debug(3, "Finished this overtime deposit session (res = %d)\n", res);

cleanup:
	ast_hangup(achan);
cleanup2:
	coin->achan = NULL;
	return res;
}

#define DISCONNECT_FAR_END() \
	if (coin->ochan) { \
		ast_hangup(coin->ochan); \
		coin->ochan = NULL; \
	} else { \
		ast_log(LOG_ERROR, "No far end channel exists!\n"); \
	}

#define CAN_EARLY_BRIDGE(chan,peer) ( \
	!ast_channel_audiohooks(chan) && !ast_channel_audiohooks(peer) && \
	ast_framehook_list_is_empty(ast_channel_framehooks(chan)) && ast_framehook_list_is_empty(ast_channel_framehooks(peer)))

static int wait_for_answer(struct coin_call *coin)
{
	int res = -1;
	struct ast_channel *watchers[2];
	int sent_progress = 0, sent_proceeding = 0, sent_ringing = 0;

	/* Set up early media, a la wait_for_answer */
	ast_deactivate_generator(coin->chan);
	if (ast_channel_make_compatible(coin->chan, coin->ochan) < 0) {
		ast_log(LOG_ERROR, "Failed to make %s and %s compatible\n", ast_channel_name(coin->chan), ast_channel_name(coin->ochan));
		DISCONNECT_FAR_END();
		return 1;
	}

	while (res < 0) {
		struct ast_channel *winner;
		int to = 1000;

		watchers[0] = coin->chan;
		watchers[1] = coin->ochan;

		winner = ast_waitfor_n(watchers, 2, &to);
		if (winner == coin->ochan) {
			char frametype[64];
			char subclass[64];
			struct ast_frame *f = ast_read(coin->ochan);
			if (!f) {
				DISCONNECT_FAR_END();
				ast_verb(4, "Outgoing channel disconnected before answer\n");
				coin->result = "NOANSWER";
				coin->finaldisp = "RETURN";
				return 1;
			}
			switch (f->frametype) {
			case AST_FRAME_CONTROL:
				switch (f->subclass.integer) {
				case AST_CONTROL_ANSWER:
					ast_verb(3, "%s answered %s\n", ast_channel_name(coin->ochan), ast_channel_name(coin->chan));
					ast_channel_hangupcause_set(coin->chan, AST_CAUSE_NORMAL_CLEARING);
					ast_channel_hangupcause_set(coin->ochan, AST_CAUSE_NORMAL_CLEARING);
					res = 0;
					break;
				case AST_CONTROL_BUSY:
					ast_verb(3, "%s is busy\n", ast_channel_name(coin->ochan));
					DISCONNECT_FAR_END();
					res = 1;
					coin->result = "BUSY";
					coin->finaldisp = "RETURN";
					break;
				case AST_CONTROL_CONGESTION:
					ast_verb(3, "%s is circuit-busy\n", ast_channel_name(coin->ochan));
					DISCONNECT_FAR_END();
					res = 1;
					coin->result = "CONGESTION";
					coin->finaldisp = "RETURN";
					break;
				case AST_CONTROL_PROGRESS:
				case AST_CONTROL_PROCEEDING:
				case AST_CONTROL_RINGING:
					ast_frame_subclass2str(f, subclass, sizeof(subclass), NULL, 0);
					ast_verb(3, "Got %s on %s, passing it to %s\n", subclass, ast_channel_name(coin->ochan), ast_channel_name(coin->chan));
					if (CAN_EARLY_BRIDGE(coin->chan, coin->ochan)) {
						ast_debug(3, "Setting up early bridge\n");
						ast_channel_early_bridge(coin->chan, coin->ochan);
					}
					if (f->subclass.integer == AST_CONTROL_PROGRESS && !sent_progress) {
						sent_progress = 1;
						ast_indicate(coin->chan, f->subclass.integer);
					} else if (f->subclass.integer == AST_CONTROL_PROCEEDING && !sent_proceeding) {
						sent_proceeding = 1;
						ast_indicate(coin->chan, f->subclass.integer);
					} else if (f->subclass.integer == AST_CONTROL_RINGING && !sent_ringing) {
						sent_ringing = 1;
						ast_indicate(coin->chan, f->subclass.integer);
					}
					break;
				case AST_CONTROL_VIDUPDATE:
				case AST_CONTROL_SRCUPDATE:
				case AST_CONTROL_SRCCHANGE:
					ast_verb(3, "%s requested media update control %d, passing it to %s\n",
						ast_channel_name(coin->ochan), f->subclass.integer, ast_channel_name(coin->chan));
					ast_indicate(coin->chan, f->subclass.integer);
					break;
				default:
					/* Ignore everything else */
					ast_frame_subclass2str(f, subclass, sizeof(subclass), NULL, 0);
					ast_debug(1, "Ignoring control frame %s\n", subclass);
					break;
				}
				break;
			case AST_FRAME_DTMF_BEGIN:
			case AST_FRAME_DTMF_END:
			case AST_FRAME_VOICE:
			case AST_FRAME_VIDEO:
			case AST_FRAME_IMAGE:
			case AST_FRAME_TEXT:
				if (ast_write(coin->chan, f)) {
					ast_log(LOG_WARNING, "Unable to write frametype %u\n", f->frametype);
				}
				break;
			case AST_FRAME_NULL:
				break; /* Ignore */
			default:
				ast_frame_type2str(f->frametype, frametype, sizeof(frametype));
				ast_debug(1, "Ignoring callee frame type %u (%s)\n", f->frametype, frametype);
				break;
			}
			ast_frfree(f);
		} else if (winner == coin->chan) {
			struct ast_frame *f = ast_read(coin->chan);
			if (!f || (f->frametype == AST_FRAME_CONTROL && f->subclass.integer == AST_CONTROL_HANGUP)) {
				/* Caller cancelled call */
				ast_verb(4, "Caller %s hung up prior to answer\n", ast_channel_name(coin->chan));
				coin->result = "CALLER_ABORT";
				coin->finaldisp = "RETURN";
				if (f) {
					ast_frfree(f);
				}
				DISCONNECT_FAR_END();
				return -1;
			}
			/* Relay from caller to callee */
			switch (f->frametype) {
			case AST_FRAME_DTMF_BEGIN:
			case AST_FRAME_DTMF_END:
			case AST_FRAME_VOICE:
			case AST_FRAME_VIDEO:
			case AST_FRAME_IMAGE:
			case AST_FRAME_TEXT:
				if (ast_write(coin->ochan, f)) {
					ast_log(LOG_WARNING, "Unable to forward frametype: %u\n", f->frametype);
				}
				break;
			default:
				ast_debug(1, "Ignoring caller frame type %u\n", f->frametype);
				break;
			}
			ast_frfree(f);
		}
	}

	return res;
}

static struct ast_channel *new_channel(struct coin_call *coin, const char *tech, const char *resource)
{
	/* Much of this function is a very simplified
	 * minimal single-out channel dialing version of what app_dial does */
	struct ast_stream_topology *topology;
	struct ast_party_caller caller;
	int cause;
	struct ast_channel *chan;

	ast_channel_lock(coin->chan);
	topology = ast_stream_topology_clone(ast_channel_get_stream_topology(coin->chan));
	ast_channel_unlock(coin->chan);

	chan = ast_request_with_stream_topology(tech, topology, NULL, coin->chan, resource, &cause);
	if (!chan) {
		ast_verb(4, "Unable to create channel %s/%s (cause %d - %s)\n", tech, resource, cause, ast_cause2str(cause));
		if (!coin->ochan) {
			/* If this is the outgoing call, set the hangup cause.
			 * This happens when !coin->ochan since that is the call being currently set up.
			 * If this were the op call, coin->ochan would already be set. */
			ast_channel_hangupcause_set(coin->chan, cause);
		}
		return NULL;
	}

	ast_channel_lock_both(chan, coin->chan);

	if (CAN_EARLY_BRIDGE(chan, coin->chan)) {
		ast_debug(3, "Making RTP early bridge compatible\n");
		ast_rtp_instance_early_bridge_make_compatible(chan, coin->chan);
	}

	/* Inherit specially named variables from parent channel */
	ast_channel_inherit_variables(coin->chan, chan);
	ast_channel_datastore_inherit(coin->chan, chan);
	ast_max_forwards_decrement(chan);

	ast_channel_appl_set(chan, "AppCoinCall");
	ast_channel_data_set(chan, "(Outgoing Line)");

	memset(ast_channel_whentohangup(chan), 0, sizeof(*ast_channel_whentohangup(chan)));

	ast_party_caller_set_init(&caller, ast_channel_caller(chan));
	ast_connected_line_copy_from_caller(ast_channel_connected(chan), ast_channel_caller(coin->chan));
	ast_party_redirecting_copy(ast_channel_redirecting(chan), ast_channel_redirecting(coin->chan));
	ast_channel_dialed(chan)->transit_network_select = ast_channel_dialed(coin->chan)->transit_network_select;
	ast_channel_req_accountcodes(chan, coin->chan, AST_CHANNEL_REQUESTOR_BRIDGE_PEER);
	ast_channel_adsicpe_set(chan, ast_channel_adsicpe(coin->chan));
	ast_channel_transfercapability_set(chan, ast_channel_transfercapability(coin->chan));

	ast_channel_dialcontext_set(chan, ast_channel_context(coin->chan));
	ast_channel_exten_set(chan, ast_channel_exten(coin->chan));

	ast_channel_unlock(chan);
	ast_channel_unlock(coin->chan);

	return chan;
}

static int setup_outgoing_call(struct coin_call *coin)
{
	int res;

	coin->ochan = new_channel(coin, coin->tech, coin->resource);
	if (!coin->ochan) {
		return 1;
	}

	/* Place the call, but don't wait on the answer */
	res = ast_call(coin->ochan, coin->resource, 0);

	ast_channel_lock(coin->chan);
	if (res) {
		ast_log(LOG_WARNING, "Failed to call on %s\n", ast_channel_name(coin->ochan));
		ast_channel_unlock(coin->chan);
		DISCONNECT_FAR_END();
		return 1;
	}

	ast_channel_publish_dial(coin->chan, coin->ochan, coin->resource, NULL);
	ast_channel_unlock(coin->chan);

	ast_verb(3, "Called %s/%s\n", coin->tech, coin->resource);
	res = wait_for_answer(coin);
	if (res) {
		return res;
	}

	res = ast_channel_make_compatible(coin->chan, coin->ochan);
	if (res < 0) {
		ast_log(LOG_ERROR, "%s and %s are not compatible\n", ast_channel_name(coin->chan), ast_channel_name(coin->ochan));
		DISCONNECT_FAR_END();
	}

	return res;
}

static int callee_leave_cb(struct ast_bridge_channel *bridge_channel, void *data)
{
	struct coin_call *coin = data;

	ast_mutex_lock(&coin->lock);
	if (!coin->callerdisconnected) {
		ast_verb(5, "Callee has disconnected\n"); /* Only log this if the callee hung up first */
		coin->result = "CALLEE_HANGUP";
		/* Disconnect the caller from the bridge if we hung up first */
		ast_assert(coin->chan != NULL);
		ast_bridge_remove(coin->bridge, coin->chan);
	} else {
		ast_debug(2, "Callee disconnected due to caller hangup or bridge termination\n");
	}
	coin->calleedisconnected = 1;
	ast_mutex_unlock(&coin->lock);
	return -1;
}

static int timeout_cb(struct ast_bridge_channel *bridge_channel, void *data)
{
	struct coin_call *coin = data;
	ast_debug(1, "Timeout has expired for caller\n");

	ast_mutex_lock(&coin->lock);
	ast_bridge_channel_leave_bridge(bridge_channel, BRIDGE_CHANNEL_STATE_END, 0);
	ast_mutex_unlock(&coin->lock);

	return -1;
}

static int set_timeout(struct ast_bridge_features *features, struct coin_call *coin, int timeout)
{
	ast_debug(1, "Setting bridge timeout to %d seconds\n", timeout);
	return ast_bridge_interval_hook(features, 0, timeout * 1000, timeout_cb, coin, NULL, AST_BRIDGE_HOOK_REMOVE_ON_PULL);
}

static int bridge_with_timeout(struct coin_call *coin, struct ast_bridge *bridge, int timeout, int overtime)
{
	int res;
	struct ast_bridge_features features;

	/* Safe guard in case we fail to catch a hangup somewhere else, bail out now */
	if (coin->callerdisconnected) {
		ast_debug(1, "Caller has disconnected, declining to bridge\n");
		return -1;
	} else if (coin->calleedisconnected) {
		ast_debug(1, "Callee has disconnected, declining to bridge\n");
		return 1;
	}

	/* Instead of using ast_bridge_call like app_dial,
	 * we create the bridge, set features, and then impart the channels,
	 * since ast_bridge_call takes an ast_bridge_config, which doesn't matter to us;
	 * we want to set the bridge features instead. */
	if (ast_bridge_features_init(&features)) {
		ast_log(LOG_ERROR, "Failed to init bridge features\n");
		return -1;
	}
	features.dtmf_passthrough = 1;

	/* Join the calling channel to the bridge (blocking) */
	if (timeout && set_timeout(&features, coin, timeout)) {
		ast_log(LOG_ERROR, "Failed to set bridge timeout\n");
		ast_bridge_features_cleanup(&features);
		return -1;
	}

	if (timeout) {
		ast_verb(4, "Coin call %s with %d:%02d remaining\n", overtime ? "resumed" : "answered", timeout / 60, timeout % 60);
	} else {
		ast_verb(4, "Coin call %s with no time limit\n", overtime ? "resumed" : "answered");
	}

	res = ast_bridge_join(bridge, coin->chan, NULL, &features, NULL, 0);
	ast_bridge_features_cleanup(&features);
	if (res) {
		ast_log(LOG_ERROR, "Caller %s failed to join bridge\n", ast_channel_name(coin->ochan));
		return -1;
	}

	/* At this point, we expect that
	 * the call was disconnected due to
	 * failing to pay an overtime deposit or that
	 * one of the two parties has hung up. */
	if (ast_check_hangup(coin->chan)) {
		coin->callerdisconnected = 1;
		ast_verb(4, "Caller has hung up\n");
		coin->result = "CALLER_HANGUP";
	}
	return res;
}

static void cleanup_bridge(struct coin_call *coin)
{
	ast_debug(3, "Destroying coin call bridge %p\n", coin->bridge);
	if (ast_bridge_destroy(coin->bridge, 0)) {
		ast_log(LOG_ERROR, "Failed to destroy coin bridge %p?\n", coin->bridge);
	}
	ast_mutex_lock(&coin->lock);
	coin->bridge = NULL;
	coin->ochan = NULL; /* Since this channel was imparted independently, we don't hang it up, it should get destroyed by the ast_bridge_destroy call above. */
	ast_debug(3, "Finished cleaning up coin bridge\n");
	ast_mutex_unlock(&coin->lock);
}

static int coin_run(struct coin_call *coin)
{
	int res;
	struct ast_bridge_features *callee_features;

	/* Currently, this application assumes dial tone first lines. */

	/* This refers to initial deposit for ACTS, not for the phone itself */
	if (coin->initialdeposit) {
		res = check_initial_deposit(coin);
		if (res < 0) {
			return -1;
		} else if (!res) {
			ast_verb(4, "Initial deposit not satisfied\n");
			coin->result = "INITIAL_DEPOSIT_REQUIRED";
			coin->finaldisp = "RETURN";
			return 0;
		}
		ast_verb(4, "Initial deposit satisfied\n");
		coin->hopper += coin->initialdeposit;
	}

	if (coin->returnfirst) {
		/* Return any coins that might have been deposited */
		res = local_coin_disposition(coin, coin->chan, COIN_RETURN);
		if (res < 0) {
			return -1;
		}
	}
	if (coin->attachfirst) {
		res = local_coin_disposition(coin, coin->chan, OPERATOR_ATTACHED);
		if (res < 0) {
			return -1;
		}
	}

	/* Now, we can begin the call.
	 * Rather than calling the Dial() application,
	 * because we want to manipulate the bridging afterwards,
	 * it's easier to do the dialing and bridging ourselves manually. */
	res = setup_outgoing_call(coin);
	if (res) {
		if (!ast_check_hangup(coin->chan) && !coin->ochan) {
			/* Call never answered and has already ended.
			 * Refund the caller and exit. */
			if (local_coin_disposition(coin, coin->chan, COIN_RETURN)) {
				return -1;
			}
		}
		return res;
	}

	/* Call has now answered. Actually bridge the channels.
	 * The 3-minute initial period starts now. */
	coin->answertime = time(NULL);
	if (ast_channel_state(coin->chan) == AST_STATE_UP) {
		/* Accurate supervision is important.
		 * The channel should not already be answered prior to now. */
		ast_log(LOG_WARNING, "Channel %s was already answered?\n", ast_channel_name(coin->chan));
	} else {
		ast_raw_answer(coin->chan);
	}

	coin->bridge = ast_bridge_base_new(AST_BRIDGE_CAPABILITY_MULTIMIX, AST_BRIDGE_FLAG_TRANSFER_PROHIBITED, coin_app, NULL, NULL);
	if (!coin->bridge) {
		ast_log(LOG_ERROR, "Failed to create bridge for ACTS call\n");
		return -1;
	}
	ast_bridge_set_internal_sample_rate(coin->bridge, 8000); /* 8 KHz */
	ast_bridge_set_maximum_sample_rate(coin->bridge, 8000);
	ast_bridge_set_mixing_interval(coin->bridge, 20); /* 20 ms */

	/* Impart (non-blocking) the called channel */
	callee_features = ast_bridge_features_new();
	if (!callee_features) {
		ast_log(LOG_ERROR, "Failed to create bridge features for callee\n");
		res = -1;
		DISCONNECT_FAR_END();
		goto cleanup;
	}
	/* Since the callee will be running in another thread,
	 * we need a callback to know if it leaves the bridge,
	 * so we can disconnect the caller. */
	res = ast_bridge_leave_hook(callee_features, callee_leave_cb, coin, NULL, 0);
	if (res) {
		ast_log(LOG_ERROR, "Failed to add leave hook to bridge features\n");
		DISCONNECT_FAR_END();
		goto cleanup;
	}
	callee_features->dtmf_passthrough = 1;
	res = ast_bridge_impart(coin->bridge, coin->ochan, NULL, callee_features, AST_BRIDGE_IMPART_CHAN_INDEPENDENT | AST_BRIDGE_IMPART_INHIBIT_JOIN_COLP);
	if (res) {
		ast_log(LOG_ERROR, "Callee %s failed to join bridge\n", ast_channel_name(coin->ochan));
		DISCONNECT_FAR_END();
		goto cleanup;
	}

	/* First 3 minutes (or an unlimited length call)
	 * Subtract 7 since we collect the contents of the hopper 7 seconds before time is up.
	 * The sound of the hopper collecting into the vault also signals to the caller that their time is almost up,
	 * and they should get more coins ready to continue the call. */
	if (coin->initialperiod) {
		coin->expiretime = time(NULL) + coin->initialperiod;
	}
	res = bridge_with_timeout(coin, coin->bridge, coin->initialperiod ? coin->initialperiod - SECS_COLLECT_BEFORE_PROMPTING : 0, 0);
	if (coin->callerdisconnected || coin->calleedisconnected) {
		goto cleanup;
	}

	if (!coin->initialperiod) {
		/* Hmm, how'd we get here? If initial period is unlimited,
		 * we should stay in the bridge until call ends. */
		ast_log(LOG_WARNING, "Bridge terminated unexpectedtly, ending call\n");
		DISCONNECT_FAR_END();
		goto cleanup;
	}

	for (;;) {
		res = get_overtime_deposit(coin);
		if (res < 0) {
			break;
		} else if (!res) {
			ast_verb(4, "Call disconnected, deposit not satisfied for local overtime\n");
			coin->result = "OVERTIME_DEPOSIT_REQUIRED";
			coin->result = "NONE"; /* If no deposit was made, the hopper is empty */
			DISCONNECT_FAR_END();
			break;
		}
		ast_verb(4, "Coin present in hopper, local overtime deposit satisfied\n");
		coin->hopper += 5; /* 5c (at least) were deposited */
		res = bridge_with_timeout(coin, coin->bridge, coin->overtimeperiod ? coin->overtimeperiod - SECS_COLLECT_BEFORE_PROMPTING : 0, 1);
		if (coin->callerdisconnected || coin->calleedisconnected) {
			goto cleanup;
		}
	}

	if (coin->callerdisconnected && !coin->result && res < 0) {
		coin->result = "CALLER_HANGUP";
	}

cleanup:
	if (!coin->finaldisp) {
		/* Defaults, if not overridden.
		 * If time is up, then we already collected what we're owed,
		 * and more time hasn't been granted yet, so return any partial deposit.
		 * Otherwise, collect, before we release (or instruct the Class 5 office to). */
		if (time(NULL) >= coin->expiretime) {
			coin->finaldisp = "RETURN";
		} else {
			coin->finaldisp = "COLLECT";
		}
	}
	ast_mutex_lock(&coin->lock);
	coin->callerdisconnected = 1; /* We're not in the bridge anymore */
	ast_mutex_unlock(&coin->lock);

	cleanup_bridge(coin);

	/* ast_bridge_destroy doesn't block synchronously
	 * until all callbacks have finished, so we need
	 * to effectively busy wait until we can proceed. */
	if (!coin->calleedisconnected) {
		int c = 0;

		/* Wait for callee_leave_cb to finish executing,
		 * since its callback data is coin, and that is
		 * stack allocated in this thread, so we can't go
		 * away until after it does. */
		while (!coin->calleedisconnected) {
			/* Channel has already hung up,
			 * so ast_safe_sleep might fail?
			 * Ignore its return value. */
			ast_safe_sleep(coin->chan, 10);
			if (c == 100) {
				ast_log(LOG_WARNING, "Callee channel has still not been disconnected?\n");
			}
		}
		ast_debug(2, "Callee has now disconnected and callee_leave_cb is done with callback data\n");
	}

	/* At this point, opchan should be completely done with the bridge */
	ast_debug(3, "Finished cleaning up coin call\n");

	return res;
}

static int coin_exec(struct ast_channel *chan, const char *data)
{
	struct coin_call coin;
	struct ast_flags flags = {0};
	char databuf[32];
	char *appdata;
	char *dialstr;
	int res;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(dialstr);
		AST_APP_ARG(initialperiod);
		AST_APP_ARG(initialdeposit);
		AST_APP_ARG(overtimeperiod);
		AST_APP_ARG(announcement);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_ERROR, "%s requires arguments\n", coin_app);
		goto invalid;
	}

	appdata = ast_strdupa(S_OR(data, ""));
	AST_STANDARD_APP_ARGS(args, appdata);

	memset(&coin, 0, sizeof(coin));
	ast_mutex_init(&coin.lock);

	if (ast_strlen_zero(args.dialstr)) {
		ast_log(LOG_ERROR, "%s requires a dial string\n", coin_app);
		goto invalid;
	}

	dialstr = args.dialstr;
	coin.tech = strsep(&dialstr, "/");
	coin.resource = dialstr;
	if (ast_strlen_zero(coin.tech) || ast_strlen_zero(coin.resource)) {
		ast_log(LOG_ERROR, "%s requires a valid dial string (tech/resource)\n", coin_app);
		goto invalid;
	}

	coin.initialperiod = atoi(args.initialperiod);
	coin.initialdeposit = atoi(args.initialdeposit);
	coin.overtimeperiod = atoi(args.overtimeperiod);
	coin.announcement = args.announcement;

	if (coin.initialperiod < 0) {
		ast_log(LOG_ERROR, "Invalid initial period: %s\n", args.initialperiod);
		goto invalid;
	} else if (coin.initialperiod && coin.initialperiod < MINIMUM_PERIOD) {
		ast_log(LOG_ERROR, "Invalid initial period (must be 0 or at least %d): %s\n", MINIMUM_PERIOD, args.initialperiod);
		goto invalid;
	} else if (coin.initialdeposit < 0) {
		ast_log(LOG_ERROR, "Invalid initial deposit: %s\n", args.initialdeposit);
		goto invalid;
	} else if (coin.overtimeperiod < 0) {
		ast_log(LOG_ERROR, "Invalid overtime period: %s\n", args.overtimeperiod);
		goto invalid;
	} else if (coin.overtimeperiod && coin.overtimeperiod < MINIMUM_PERIOD) {
		ast_log(LOG_ERROR, "Invalid overtime period (must be 0 or at least %d): %s\n", MINIMUM_PERIOD, args.overtimeperiod);
		goto invalid;
	}

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(app_opts, &flags, NULL, args.options);
		coin.attachfirst = ast_test_flag(&flags, OPT_ATTACH_FIRST) ? 1 : 0;
		coin.fgc = ast_test_flag(&flags, OPT_FEATURE_GROUP_C) ? 1 : 0;
		coin.returnfirst = ast_test_flag(&flags, OPT_RETURN_FIRST) ? 1 : 0;
	}

	coin.chan = chan;
	coin.start = time(NULL);

	if (coin.fgc && ast_func_write(coin.chan, "COIN_EIS()", "TX")) {
		ast_log(LOG_ERROR, "Failed to enable Feature Group C support\n");
		return -1;
	}

	/* Keep the call in the linked list while they're active
	 * so we can dump all ongoing coin calls from the CLI. */
	AST_RWLIST_WRLOCK(&calls);
	AST_RWLIST_INSERT_TAIL(&calls, &coin, entry);
	AST_RWLIST_UNLOCK(&calls);

	res = coin_run(&coin);

	AST_RWLIST_WRLOCK(&calls);
	AST_RWLIST_REMOVE(&calls, &coin, entry);
	AST_RWLIST_UNLOCK(&calls);

	if (coin.fgc && ast_func_write(coin.chan, "COIN_EIS()", "remove")) {
		ast_log(LOG_WARNING, "Failed to disable Feature Group C support\n");
	}

	/* If ochan still exists, nix it */
	if (coin.ochan) {
		/* We assume here that ochan is not bridged,
		 * or that will trigger an assertion. */
		ast_debug(3, "Cleaning up %s since it wasn't bridged\n", ast_channel_name(coin.ochan));
		ast_assert(!ast_channel_bridge_peer(coin.ochan));
		ast_hangup(coin.ochan);
		coin.ochan = NULL;
	}

	if (coin.result) {
		pbx_builtin_setvar_helper(chan, "COIN_RESULT", coin.result);
		pbx_builtin_setvar_helper(chan, "COIN_FINAL_DISPOSITION", coin.finaldisp);
	} else {
		ast_log(LOG_WARNING, "Missing internal result disposition... assuming failure\n");
		pbx_builtin_setvar_helper(chan, "COIN_RESULT", "FAILURE");
		pbx_builtin_setvar_helper(chan, "COIN_FINAL_DISPOSITION", "RETURN"); /* Fail safe to return, but this shouldn't happen */
	}

	if (res > 0) {
		/* Positive return means the call was forcibly disconnect,
		 * but chan isn't hung up, so keep going */
		res = 0;
	}

	/* Finish calculating correctly.
	 * If the call answered and there is money still in the hopper, collect it. */
	if (coin.answertime && coin.hopper) {
		if (res >= 0) {
			res |= local_coin_disposition(&coin, coin.chan, COIN_COLLECT);
		} else {
			/* Channel hung up, so we can't actually
			 * collect from here, the Class 5 end office
			 * will handle that.
			 * However, we still want to take this amount
			 * into account for our calculations. */
			coin.collected += coin.hopper;
			coin.hopper = 0;
		}
	}

	snprintf(databuf, sizeof(databuf), "%d", coin.collected);
	pbx_builtin_setvar_helper(chan, "COIN_AMOUNT_COLLECTED", databuf);

	ast_mutex_destroy(&coin.lock);
	return res;

invalid:
	pbx_builtin_setvar_helper(chan, "COIN_RESULT", "INVALID");
	return -1;
}

#define MONEY_FMT "%s$%3d.%02d" /* 8 characters */
#define ABS(x) (x < 0 ? -x : x)
#define MONEY_ARGS(x) x < 0 ? "-" : " ", ABS(x) / 100, (ABS(x) % 100)

static char *handle_show_calls(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	int total = 0;
	time_t now;
	struct coin_call *coin;

	switch(cmd) {
	case CLI_INIT:
		e->command = "coin show calls";
		e->usage =
			"Usage: coin show calls\n"
			"       Lists status of all coin calls\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc < 3) {
		return CLI_SHOWUSAGE;
	}

	now = time(NULL);

	AST_RWLIST_RDLOCK(&calls);
	AST_RWLIST_TRAVERSE(&calls, coin, entry) {
		int diff, hr, min, sec;
		int ans_hr = 0, ans_min = 0, ans_sec = 0;
		int exp_min = 0, exp_sec = 0;

		ast_mutex_lock(&coin->lock); /* Not strictly needed, but ensure nothing changes */

		/* Print heading if first call */
		if (!total++) {
			ast_cli(a->fd, "%8s %8s %9s %8s %8s %8s %9s %s\n",
				"Duration", "Ans Dur.","Time Left", "Initial", "Overtime", "Hopper", "Collected", "Called Channel");
		}

		/* Calculate duration */
		diff = now - coin->start;
		hr = diff / 3600;
		min = (diff % 3600) / 60;
		sec = diff % 60;

		if (coin->answertime) {
			diff = now - coin->answertime;
			ans_hr = diff / 3600;
			ans_min = (diff % 3600) / 60;
			ans_sec = diff % 60;
		}

		if (coin->expiretime > now) {
			diff = coin->expiretime - now;
			exp_min = diff / 60;
			exp_sec = diff % 60;
		}

		ast_cli(a->fd, "%02d:%02d:%02d %02d:%02d:%02d %6d:%02d " MONEY_FMT " " MONEY_FMT " " MONEY_FMT "  " MONEY_FMT " %s\n",
				hr, min, sec,
				ans_hr, ans_min, ans_sec,
				exp_min, exp_sec,
				MONEY_ARGS(coin->initialdeposit), MONEY_ARGS(coin->overtimedeposit),
				MONEY_ARGS(coin->hopper), MONEY_ARGS(coin->collected),
				coin->ochan ? ast_channel_name(coin->ochan) : "" /* XXX Technically want locking for ochan */
			);
		ast_mutex_unlock(&coin->lock);
	}
	AST_RWLIST_UNLOCK(&calls);

	if (!total) {
		ast_cli(a->fd, "No active coin calls\n");
	}

	return CLI_SUCCESS;
}

static struct ast_cli_entry coin_cli[] = {
	AST_CLI_DEFINE(handle_show_calls, "Displays information about all coin calls"),
};

static int unload_module(void)
{
	/* We won't be able to unload if there are any active calls,
	 * so if we get here, the calls linked list must be empty. */
	ast_cli_unregister_multiple(coin_cli, ARRAY_LEN(coin_cli));
	ast_unregister_application(coin_app);
	ast_unregister_application(disp_app);
	return 0;
}

static int load_module(void)
{
	int res = ast_register_application_xml(coin_app, coin_exec);
	if (!res) {
		res = ast_register_application_xml(disp_app, local_coin_disposition_exec);
	}
	if (!res) {
		ast_cli_register_multiple(coin_cli, ARRAY_LEN(coin_cli));
	}
	return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Local Coin Control",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.requires = "res_coindetect,chan_bridge_media",
);
