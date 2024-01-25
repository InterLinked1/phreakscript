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
 * \brief Automated Coin Toll System
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup resources
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
	<application name="ACTS" language="en_US">
		<synopsis>
			Automated Coin Toll System
		</synopsis>
		<syntax>
			<parameter name="dialstr" required="true">
				<para>Tech/resource to dial for the outgoing call.</para>
				<para>If you need more complexity than this offers, you can dial a Local
				channel that dials the actual destination.</para>
			</parameter>
			<parameter name="audiodir" required="true">
				<para>Asterisk sounds directory containing the A.C.T.S. prompts</para>
				<para>This application does look for a certain set of prompts; please make sure they all exist.</para>
			</parameter>
			<parameter name="options" required="false">
				<optionlist>
					<option name="a">
						<para>Call is already Operator Attached when calling this application.</para>
						<para>Typically, the Class 5 switch puts the phone in Operator Attached mode
						before sending the call to the Class 4 switch for processing. If this is the case,
						you should specify this option to avoid a duplicate Operator Attached, and to ensure
						that Operator Released is sent, if necessary.</para>
					</option>
					<option name="f">
						<para>Allow the caller to hook flash for operator assistance.</para>
						<para>This option specifies the channel to call for an operator.</para>
					</option>
					<option name="i">
						<para>Initial deposit, for the first 3 minutes, in cents.</para>
					</option>
					<option name="o">
						<para>Overtime deposit, per-minute, in cents.</para>
					</option>
					<option name="p">
						<para>Initial period (e.g. if other than 3 minutes), in seconds.</para>
						<para>To allow the initial period to be unlimited, instead set the overtime deposit to 0.</para>
					</option>
					<option name="r">
						<para>Relaxed tone detection. May result in more false positives but fewer false negatives.</para>
					</option>
					<option name="s">
						<para>Detect single-frequency (2200 Hz) only coin denomination tones, rather than standard 1700 + 2200 Hz coin denomination tones.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Dialplan application for Class 4 switches to process toll calls from coin stations.</para>
			<para>A.C.T.S uses Expanded In-Band Signaling to communicate with the Class 5 end office
			that is connected to the pay station being controlled by A.C.T.S.</para>
			<para>It is assumed that no money is in the hopper when this application is called.
			If you are using this with coin first phones, you should call <literal>CoinDisposition(return)</literal>
			first.</para>
			<variablelist>
				<variable name="ACTS_RESULT">
					<para>This is the result of the call.</para>
					<value name="CALLER_HANGUP">
						Caller hung up
					</value>
					<value name="CALLEE_HANGUP">
						Callee hung up
					</value>
					<value name="NOANSWER">
						Callee hung up without far end supervision
					</value>
					<value name="INSUFFICIENT_INITIAL">
						Insufficient deposit for initial period
					</value>
					<value name="OVERTIME_EXPIRED">
						Call terminated due to insufficient funds to continue overtime
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
				<variable name="ACTS_COLLECTED">
					<para>The total amount of money, in cents, collected into the coin vault for this call.</para>
					<para>This variable is only set if <literal>ACTS_RESULT</literal> is not <literal>INVALID</literal>.</para>
				</variable>
				<variable name="ACTS_CREDIT_REQUIRED">
					<para>This variable is only set on the operator channel, when dialed.</para>
					<para>This contains the amount of money required to continue the call, in cents.</para>
				</variable>
				<variable name="ACTS_IN_OVERTIME">
					<para>This variable is only set on the operator channel, when dialed.</para>
					<para>This is 1 if the call is in overtime and 0 if not.</para>
				</variable>
				<variable name="ACTS_INITIAL_PERIOD">
					<para>This variable is only set on the operator channel, when dialed.</para>
					<para>This contains the duration of the initial period of the call, in seconds.</para>
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">CoinDisposition</ref>
			<ref type="application">WaitForDeposit</ref>
			<ref type="function">COIN_DETECT</ref>
			<ref type="function">COIN_EIS</ref>
		</see-also>
	</application>
 ***/

/*! \brief Information about an A.C.T.S. call */
struct acts_call {
	/* Channels */
	struct ast_channel *chan;	/* Calling channel */
	struct ast_channel *ochan;	/* Outgoing channel */
	struct ast_channel *achan;	/* Announcement channel */
	struct ast_channel *opchan;	/* Operator channel */
	struct ast_bridge *bridge;
	/* Call configuration */
	const char *audiodir;
	const char *tech;
	const char *resource;
	const char *optech;
	const char *opresource;
	int initialperiod;
	int initialdeposit;
	int overtimedeposit;
	/* Call properties */
	int hopper;
	int collected;
	int credit;
	int overtime_index;
	int idle_intervals;	/* Number of overtime callback intervals during which nothing has been deposited, in a row */
	time_t start;
	time_t answertime;
	time_t expiretime;	/* When acts->chan will next be kicked from the bridge */
	/* Bit fields */
	unsigned int relax:1;
	unsigned int sf:1;	/* Single-frequency coin denomination tones */
	unsigned int detecting:1;	/* Detection currently enabled */
	unsigned int attached:1;	/* Currently operator attached */
	unsigned int arrivedattached:1;	/* Call arrived operator attached */
	unsigned int overtime:1;	/* Currently in overtime */
	unsigned int timeoutexpired:1;	/* Timeout has expired */
	unsigned int callerdisconnected:1;	/* Caller has hung up */
	unsigned int calleedisconnected:1;	/* Callee has hung up */
	unsigned int ignorehangup:1; /* Ignore soft hangup */
	unsigned int operatorpending:1;	/* Summoned an operator */
	const char *result;
	pthread_t opthread;
	ast_mutex_t lock;
	AST_RWLIST_ENTRY(acts_call) entry;
};

/*! \brief Linked list of all A.C.T.S. calls */
static AST_RWLIST_HEAD_STATIC(calls, acts_call);

enum {
	OPT_INITIAL_DEPOSIT = (1 << 0),
	OPT_INITIAL_PERIOD = (1 << 1),
	OPT_OVERTIME_DEPOSIT = (1 << 2),
	OPT_FLASH_FOR_OPERATOR = (1 << 3),
	OPT_ALREADY_ATTACHED = (1 << 4),
	OPT_RELAX = (1 << 5),
	OPT_SF = (1 << 6),
};

enum {
	OPT_ARG_INITIAL_DEPOSIT,
	OPT_ARG_INITIAL_PERIOD,
	OPT_ARG_OVERTIME_DEPOSIT,
	OPT_ARG_FLASH_FOR_OPERATOR,
	/* note: this entry _MUST_ be the last one in the enum */
	OPT_ARG_ARRAY_SIZE,
};

AST_APP_OPTIONS(app_opts,{
	AST_APP_OPTION('a', OPT_ALREADY_ATTACHED),
	AST_APP_OPTION_ARG('f', OPT_FLASH_FOR_OPERATOR, OPT_ARG_FLASH_FOR_OPERATOR),
	AST_APP_OPTION_ARG('i', OPT_INITIAL_DEPOSIT, OPT_ARG_INITIAL_DEPOSIT),
	AST_APP_OPTION_ARG('o', OPT_OVERTIME_DEPOSIT, OPT_ARG_OVERTIME_DEPOSIT),
	AST_APP_OPTION_ARG('p', OPT_INITIAL_PERIOD, OPT_ARG_INITIAL_PERIOD),
	AST_APP_OPTION('r', OPT_RELAX),
	AST_APP_OPTION('s', OPT_SF),
});

static const char *acts_app = "ACTS";

enum coin_disposition {
	COIN_COLLECT,
	COIN_RETURN,
	OPERATOR_ATTACHED,
	OPERATOR_RELEASED,
};

static int coin_disposition(struct acts_call *acts, enum coin_disposition disp)
{
	switch (disp) {
	case COIN_COLLECT:
		acts->collected += acts->hopper;
		acts->hopper = 0;
		return ast_pbx_exec_application(acts->chan, "CoinDisposition", "collect");
	case COIN_RETURN:
		acts->hopper = 0;
		return ast_pbx_exec_application(acts->chan, "CoinDisposition", "return");
	case OPERATOR_ATTACHED:
		ast_assert(!acts->attached);
		acts->attached = 1;
		return ast_pbx_exec_application(acts->chan, "CoinDisposition", "attached");
	case OPERATOR_RELEASED:
		ast_assert(acts->attached);
		acts->attached = 0;
		return ast_pbx_exec_application(acts->chan, "CoinDisposition", "released");
	}
	__builtin_unreachable();
}

/*!
 * \brief Begin coin detection on the channel
 * \param acts
 * \param required The minimum amount required to continue
 * \param overtime Whether overtime deposited are allowed
 * \retval 0 on success, -1 on failure
 */
static int start_coin_detect(struct acts_call *acts, int required, int overtime)
{
	char args[96];
	if (overtime) {
		snprintf(args, sizeof(args), "COIN_DETECT("
			"r" /* Receive direction only */
			"%s%s)",
			acts->relax ? "l" : "",
			"d(4)" /* Delay completion after min. deposit satisfied if overtime deposits allowed */
		);
	} else {
		snprintf(args, sizeof(args), "COIN_DETECT("
			"r" /* Receive direction only */
			"a(%d)" /* Amount required */
			"%s%s)",
			required,
			acts->relax ? "l" : "",
			"g(NONEXISTENT_CONTEXT,s,1)" /* Dummy label to trigger channel hangup when satisfied */
		);
	}

	ast_debug(3, "Enabling coin detect: %s\n", args);
	if (ast_func_write(acts->chan, args, "")) {
		return -1;
	}
	ast_assert(!acts->detecting);
	acts->detecting = 1;
	return 0;
}

static int stop_coin_detect(struct acts_call *acts)
{
	ast_assert(acts->detecting);
	acts->detecting = 0;
	ast_debug(3, "Disabling coin detection\n");
	return ast_func_write(acts->chan, "COIN_DETECT(x)", "");
}

/*! \note This SHOULD only be called by the thread that owns acts->chan */
static int get_current_deposit(struct acts_call *acts)
{
	char buf[256] = "";
	int res;

	ast_assert(acts->detecting);
	res = ast_func_read(acts->chan, "COIN_DETECT(rx)", buf, sizeof(buf));
	if (res) {
		return -1;
	}
	acts->hopper = atoi(buf);
	return acts->hopper;
}

static int acts_play_prompt(struct acts_call *acts, const char *file)
{
	char filename[512];
	snprintf(filename, sizeof(filename), "%s/%s", acts->audiodir, file);
	/* If we're in overtime, prompts play on achan.
	 * For initial deposit, directly on chan. */
	return ast_stream_and_wait(acts->answertime ? acts->achan : acts->chan, filename, "");
}

static int acts_say_money(struct acts_call *acts, int amount)
{
	char filename[256];
	int res = 0;

	if (amount >= 100) {
		int dollars = amount / 100;
		snprintf(filename, sizeof(filename), "%d", dollars);
		res = acts_play_prompt(acts, filename);
		if (!res) {
			res = acts_play_prompt(acts, amount >= 200 ? "dollars" : "dollar");
		}
		amount -= (100 * dollars);
		if (!res && amount) {
			res = acts_play_prompt(acts, "and");
		}
	}

	if (!amount || res) {
		return res;
	}

	if (amount >= 20) {
		int tens = 10 * (amount / 10);
		snprintf(filename, sizeof(filename), "%d", tens);
		res = acts_play_prompt(acts, filename);
		amount -= tens;
	}

	if (!res && amount) {
		snprintf(filename, sizeof(filename), "%d", amount);
		res = acts_play_prompt(acts, filename);
	}

	if (!res) {
		res = acts_play_prompt(acts, "cents");
	}

	return res;
}

static int play_prompts(struct acts_call *acts, int required, int overtime, int i)
{
	int res = 0;
	int left;

	left = required - acts->hopper;

	if (left <= 0) {
		/* If not in overtime, no further deposits are allowed, so break immediately.
		 * If overtime, we want to give the caller the chance to deposit more than is necessary,
		 * so just go right to sleeping for a bit. */
		if (!overtime) {
			return res;
		}
	} else {
		int left2;
		/* Only play alert tone for overtime,
		 * and only the first time we prompt */
		if (!i && overtime) {
			res = acts_play_prompt(acts, "tone");
		}
		if (!res) {
			res = acts_play_prompt(acts, "please-deposit");
		}
		if (res) {
			return res;
		}

		/* Right before we actually ask for an amount,
		 * query the amount in case money was depositing
		 * while we were saying "please deposit".
		 *
		 * However, if it's less than 5 cents (zero, or negative),
		 * prompt for the stale amount anyways. */
		left2 = required - get_current_deposit(acts);
		if (left2 > 0) {
			if (left2 != left) {
				ast_debug(3, "Sneaky! Caller deposited money during the intro prompts...\n");
			}
			left = left2;
		}

		res = acts_say_money(acts, left);
		if (res) {
			return res;
		}

		/* If we're really all paid up,
		 * don't continue with these warning prompts. */
		if (acts->overtimedeposit && left2 > 0) {
			if (overtime) {
				res = acts_play_prompt(acts, "for-the-next");
				if (!res) {
					res = acts_play_prompt(acts, "minute");
				}
				if (i >= 2) { /* Only warn once caller starts running out of time */
					if (!res) {
						res = acts_play_prompt(acts, "or-your-call");
					}
					if (!res) {
						res = acts_play_prompt(acts, "will-be-disconnected");
					}
				}
			} else {
				res = acts_play_prompt(acts, "for-the-first");
				if (!res) {
					/* Initial deposit interval is 3 minutes */
					res = acts_play_prompt(acts, "3");
				}
				if (!res) {
					res = acts_play_prompt(acts, "minutes");
				}
			}
		}
	}

	if (!res) {
		res = ast_safe_sleep(acts->answertime ? acts->achan : acts->chan, 5000);
	}

	return res;
}

static int play_outro(struct acts_call *acts, int required, int overtime)
{
	int res = acts_play_prompt(acts, "thank-you");
	if (!res && overtime && acts->hopper > required) {
		int credit = acts->hopper - required;
		res = acts_play_prompt(acts, "you-have");
		if (!res) {
			res = acts_say_money(acts, credit);
		}
		if (!res) {
			res = acts_play_prompt(acts, "credit-toward-overtime");
		}
	}
	return res;
}

#define INTERNALLY_REDIRECTED() ((ast_channel_softhangup_internal_flag(acts->chan) & AST_SOFTHANGUP_ASYNCGOTO) && !strcmp(ast_channel_context(acts->chan), "NONEXISTENT_CONTEXT"))

static int play_prompts_helper(struct acts_call *acts, int required, int overtime)
{
	int res, i = 0;
	/* Must be less than a minute, for overtime deposits */
#define ABSOLUTE_OVERTIME_CALLBACK_MAX_ITERATIONS 5
	int num_iterations = 4;
	int deposit_delta = 0;

	ast_debug(1, "Round %d before: %d/%d cents deposited\n", i, 0, required);
	for (; i < num_iterations; i++) {
		int amt_start, amt_end;
		int prompt_this_round = 1;

		/* Note that get_current_deposit should only be called "normally"
		 * in this function and in play_prompts if !overtime,
		 * since then we're running in the thread that owns acts->chan.
		 * During overtime, we're running in a separate thread.
		 *
		 * In practice, it's not an issue calling it from a separate thread,
		 * and the channel isn't going to disappear since the dialplan thread
		 * isn't going to execute without joining this thread first,
		 * and all we're doing is reading audiohook data.
		 *
		 * The alternative approach is setting up a bridge periodic hook
		 * on the channel directly that periodically queries the hopper balance
		 * and updates acts->hopper for us, but that isn't necessarily in sync
		 * with the prompts, which can lead to asking for stale amounts.
		 */

		amt_start = get_current_deposit(acts);
		/* If the caller is actively depositing coins, which is indicated
		 * by having received a deposit in the last prompt interval,
		 * then don't repeat the prompts again just yet. Instead,
		 * just wait silently.
		 *
		 * After some time passes and no further deposits have been made,
		 * if we are still owed money, then reprompt.
		 */
		if (deposit_delta) {
			if (overtime) {
				/* We have less than a minute total when in overtime,
				 * no matter how much needs to be deposited, or how
				 * slowly the caller is depositing.
				 * We can relax a little if the caller starts depositing,
				 * but only to a certain extent. */
				if (num_iterations < ABSOLUTE_OVERTIME_CALLBACK_MAX_ITERATIONS) {
					num_iterations++;
					prompt_this_round = 0;
				} else {
					prompt_this_round = 1;
				}
			} else {
				/* Call hasn't started yet, so reset the timer, as
				 * the caller is free to take a while to deposit the entire
				 * initial deposit. */
				i = 0;
				prompt_this_round = 0;
			}
		}

		if (prompt_this_round) {
			res = play_prompts(acts, required, overtime, i);
		} else {
			res = ast_safe_sleep(acts->answertime ? acts->achan : acts->chan, 5000);
		}

		amt_end = get_current_deposit(acts);
		deposit_delta = amt_end - amt_start;
		ast_debug(1, "Round %d/%d after: %d/%d cents deposited (delta: %d)\n", i, num_iterations, amt_end, required, deposit_delta);

		/* If this is for the initial period,
		 * we're done as soon as we get the required deposit.
		 * In overtime, it's more complicated since the caller
		 * can prepay beyond the requirement.
		 */
		if (amt_end >= required) {
			if (!overtime) {
				/* If this is for the initial period,
				 * we're done as soon as we get the required deposit. */
				break;
			} else {
				/* If overtime, break if we've been idle for an iteration. */
				if (!deposit_delta) {
					break;
				}
			}
		}
	}
	return res;
}

static int get_initial_deposit(struct acts_call *acts, int required)
{
	int res = -1;
	char context[AST_MAX_CONTEXT], exten[AST_MAX_EXTENSION];
	int pri;
	int fake_goto = 0;
	struct ast_silence_generator *silgen = NULL;

	if (!acts->arrivedattached) {
		/* Put the phone in Operator Attached mode. */
		if (coin_disposition(acts, OPERATOR_ATTACHED)) {
			return -1;
		}

		if (ast_safe_sleep(acts->chan, 1000)) {
			goto cleanup;
		}
	}

	/* Preserve our original context, exten, pri,
	 * since ast_explicit_goto will be called by the coin detector,
	 * and once we clear the soft hangup, we need to reset the location
	 * to what it was originally. */

	ast_channel_lock(acts->chan);
	ast_copy_string(context, ast_channel_context(acts->chan), sizeof(context));
	ast_copy_string(exten, ast_channel_exten(acts->chan), sizeof(exten));
	pri = ast_channel_priority(acts->chan);
	ast_channel_unlock(acts->chan);

	/* Set up coin detection on the channel */
	res = start_coin_detect(acts, required, 0);
	if (res) {
		goto cleanup;
	}

	/* The coin detector is an audiohook,
	 * and audiohooks need constant media to function,
	 * so don't use ast_safe_sleep here without a silence generator. */
	silgen = ast_channel_start_silence_generator(acts->chan);

	/* Run the deposit announcements synchronously until we get interrupted. */
	res = play_prompts_helper(acts, required, 0);

	if (INTERNALLY_REDIRECTED()) {
		/* If async goto happened due to meeting deposit requirement, that's not a real hangup */
		res = 0;
		fake_goto = 1;
	}

	if (silgen) {
		ast_channel_stop_silence_generator(acts->chan, silgen);
	}

	/* Record and store how much money is in the hopper right now,
	 * before we destroy the detector */
	if (get_current_deposit(acts) < required) {
		/* Couldn't have been INTERNALLY_REDIRECTED() if this is the case, so need to run that logic on this path */
		ast_verb(4, "Insufficient deposit, disconnecting...\n");
		res = 1;
		acts->result = "INSUFFICIENT_INITIAL";
	}

	/* Destroy coin tone detector */
	stop_coin_detect(acts);

	/* Check if the channel really hung up or if it was just redirected internally to NONEXISTENT_LABEL.
	 * This just means deposit is satisfied. Clear the flag, and proceed as normal.
	 * It is important that we do all this AFTER disabling the detector since we could be redirected multiple times, otherwise. */
	if (fake_goto || INTERNALLY_REDIRECTED()) {
		ast_debug(2, "Clearing internal softhangup on %s\n", ast_channel_name(acts->chan));
		ast_channel_clear_softhangup(acts->chan, AST_SOFTHANGUP_ASYNCGOTO);
		res = 0;
	}

	/* Reset location, to undo the effects of any async goto */
	ast_channel_context_set(acts->chan, context);
	ast_channel_exten_set(acts->chan, exten);
	ast_channel_priority_set(acts->chan, pri + 1); /* Have to add one or that'll go back to us next */

	if (ast_channel_softhangup_internal_flag(acts->chan)) {
		ast_debug(4, "Channel %s really has a softhangup?\n", ast_channel_name(acts->chan));
		if (ast_channel_softhangup_internal_flag(acts->chan) & AST_SOFTHANGUP_ASYNCGOTO) {
			ast_debug(3, "Is an async goto\n");
			ast_debug(3, "Context: %s\n", ast_channel_context(acts->chan));
		}
	}

	if (!res) {
		res = play_outro(acts, required, 0);
		if (res) {
			ast_debug(5, "play_outro returned %d\n", res);
		}
	}

	/* Detach */
cleanup:
	res |= coin_disposition(acts, OPERATOR_RELEASED);
	return res;
}

static void *async_announcements(void *varg)
{
	int res = 0;
	struct acts_call *acts = varg;

	/* Play the prompts on this channel,
	 * and they'll go over the unreal chan pair into the bridge. */
	res = play_prompts_helper(acts, -acts->credit, 1);

	if (!res) {
		if (acts->hopper >= -acts->credit) {
			/* If we got enough funds, acknowledge that.
			 * If we fell short, the call is being disconnect now,
			 * so don't play anything further. */
			res = play_outro(acts, -acts->credit, 1);
		}
	}

	/* Now, kick chan from the bridge.
	 * We do it this way, rather than from within overtime_timeout_cb,
	 * to ensure that the prompts are audible to the caller,
	 * as the caller should not be out of the bridge for more than the time
	 * required to detach properly. */
	acts->ignorehangup = 1;

	/* XXX ast_bridge_remove(acts->bridge, acts->chan)
	 * completely disconnects the caller, rather than just kicking from the bridge.
	 * This requires checking for ignorehangup and ignoring the hangup,
	 * which is a bit messy.
	 * Ideally, we would do a "pure" bridge kick without triggering a hangup. */

	/* XXX Should ref the channels before doing this? */
	ast_debug(3, "Kicking %s from bridge momentarily to signal overtime collection is over\n", ast_channel_name(acts->chan));
	ast_bridge_remove(acts->bridge, acts->chan);
	return NULL;
}

static int set_gains(struct acts_call *acts, int lopsided)
{
	/* XXX Should probably get/ref the channel rather than using directly */
	ast_channel_lock(acts->ochan);
	ast_func_write(acts->ochan, "VOLUME(RX)", lopsided ? "-3" : "0");
	ast_channel_unlock(acts->ochan);
	return ast_func_write(acts->chan, "VOLUME(RX)", lopsided ? "2" : "0");
}

static struct ast_channel *alloc_playback_chan(struct acts_call *acts)
{
	struct ast_channel *chan;
	struct ast_format_cap *cap;

	cap = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT);
	if (!cap) {
		return NULL;
	}

	ast_format_cap_append(cap, ast_format_slin, 0);
	chan = ast_request("Announcer", cap, NULL, NULL, "ACTS", NULL);
	ao2_ref(cap, -1);
	return chan;
}

/* Forward declaration */
static int bridge_with_timeout(struct acts_call *acts, struct ast_bridge *bridge, int timeout, int overtime);

static int get_overtime_deposit(struct acts_call *acts)
{
	int res;
	struct ast_channel *achan;
	pthread_t prompt_thread;

	res = coin_disposition(acts, OPERATOR_ATTACHED);
	if (res) {
		return res;
	}

	/* Allow the conversation to continue while collecting overtime deposits,
	 * but reduce the volume of the callee and make us louder to the detector. */
	set_gains(acts, 1);

	res = start_coin_detect(acts, -acts->credit, 1);
	if (res) {
		goto cleanup3;
	}

	/* Since we're still bridged, we can't run the prompts
	 * directly on the channel. Instead, we need to
	 * play the prompts into the bridge using another channel. */
	achan = alloc_playback_chan(acts);
	if (!achan) {
		ast_log(LOG_ERROR, "Failed to allocate announcement channel\n");
		goto cleanup2;
	}
	if (ast_unreal_channel_push_to_bridge(achan, acts->bridge, AST_BRIDGE_CHANNEL_FLAG_IMMOVABLE | AST_BRIDGE_CHANNEL_FLAG_LONELY)) {
		ast_log(LOG_ERROR, "Failed to push announcement channel into bridge\n");
		res = -1;
		goto cleanup;
	}

	/* Start prompts on the announcement channel */
	acts->overtime_index = 0;
	acts->idle_intervals = 0;
	acts->achan = achan;
	if (ast_pthread_create(&prompt_thread, NULL, async_announcements, acts)) {
		ast_log(LOG_ERROR, "Failed to create announcement thread.\n");
		res = -1;
		goto cleanup;
	}

	/* Because overtime allows for the possibility of overtime deposits, beyond
	 * what the caller is required to pay for the next minute,
	 * unlike when paying for the initial 3 minutes,
	 * we don't need to be able to immediately detect "required deposits satisfied".
	 * In fact, checking every few seconds is completely fine,
	 * we don't need to know how much is in the hopper in real time.
	 * To that end, we can leave the coin detector running in the background
	 * and run the bridge synchronously on the channel,
	 * with a periodic callback to check if we're good or not. */

	/* If we're not done within 55 seconds, we're disconnecting the call */
	res = bridge_with_timeout(acts, acts->bridge, 55, 1); /* Wait just shy of one minute, max */
	if (!ast_check_hangup(acts->chan) && acts->hopper > -acts->credit) {
		/* We got enough money, and we were simply kicked from the bridge. */
		ast_debug(3, "Minimum required overtime deposit satisfied\n");
		res = 0;
	}
	pthread_join(prompt_thread, NULL); /* Wait for prompts to finish before continuing */

	/* Clean up and return to the call */
	ast_debug(3, "Finished this overtime deposit session (res = %d)\n", res);

cleanup:
	ast_hangup(achan);
cleanup2:
	acts->achan = NULL;
	get_current_deposit(acts); /* Get final value, to catch any straggling deposits towards the end */
	stop_coin_detect(acts);
	set_gains(acts, 0); /* Back to normal */
cleanup3:
	res |= coin_disposition(acts, OPERATOR_RELEASED);
	return res;
}

#define DISCONNECT_FAR_END() \
	if (acts->ochan) { \
		ast_hangup(acts->ochan); \
		acts->ochan = NULL; \
	} else { \
		ast_log(LOG_ERROR, "No far end channel exists!\n"); \
	}

#define CAN_EARLY_BRIDGE(chan,peer) ( \
	!ast_channel_audiohooks(chan) && !ast_channel_audiohooks(peer) && \
	ast_framehook_list_is_empty(ast_channel_framehooks(chan)) && ast_framehook_list_is_empty(ast_channel_framehooks(peer)))

static int wait_for_answer(struct acts_call *acts)
{
	int res = -1;
	struct ast_channel *watchers[2];
	int sent_progress = 0, sent_proceeding = 0, sent_ringing = 0;

	/* Set up early media, a la wait_for_answer */
	ast_deactivate_generator(acts->chan);
	if (ast_channel_make_compatible(acts->chan, acts->ochan) < 0) {
		ast_log(LOG_ERROR, "Failed to make %s and %s compatible\n", ast_channel_name(acts->chan), ast_channel_name(acts->ochan));
		DISCONNECT_FAR_END();
		return 1;
	}

	while (res < 0) {
		struct ast_channel *winner;
		int to = 1000;

		watchers[0] = acts->chan;
		watchers[1] = acts->ochan;

		winner = ast_waitfor_n(watchers, 2, &to);
		if (winner == acts->ochan) {
			char frametype[64];
			char subclass[64];
			struct ast_frame *f = ast_read(acts->ochan);
			if (!f) {
				DISCONNECT_FAR_END();
				ast_verb(4, "Outgoing channel disconnected before answer\n");
				acts->result = "NOANSWER";
				return 1;
			}
			switch (f->frametype) {
			case AST_FRAME_CONTROL:
				switch (f->subclass.integer) {
				case AST_CONTROL_ANSWER:
					ast_verb(3, "%s answered %s\n", ast_channel_name(acts->ochan), ast_channel_name(acts->chan));
					ast_channel_hangupcause_set(acts->chan, AST_CAUSE_NORMAL_CLEARING);
					ast_channel_hangupcause_set(acts->ochan, AST_CAUSE_NORMAL_CLEARING);
					res = 0;
					break;
				case AST_CONTROL_BUSY:
					ast_verb(3, "%s is busy\n", ast_channel_name(acts->ochan));
					DISCONNECT_FAR_END();
					res = 1;
					acts->result = "BUSY";
					break;
				case AST_CONTROL_CONGESTION:
					ast_verb(3, "%s is circuit-busy\n", ast_channel_name(acts->ochan));
					DISCONNECT_FAR_END();
					res = 1;
					acts->result = "CONGESTION";
					break;
				case AST_CONTROL_PROGRESS:
				case AST_CONTROL_PROCEEDING:
				case AST_CONTROL_RINGING:
					ast_frame_subclass2str(f, subclass, sizeof(subclass), NULL, 0);
					ast_verb(3, "Got %s on %s, passing it to %s\n", subclass, ast_channel_name(acts->ochan), ast_channel_name(acts->chan));
					if (CAN_EARLY_BRIDGE(acts->chan, acts->ochan)) {
						ast_debug(3, "Setting up early bridge\n");
						ast_channel_early_bridge(acts->chan, acts->ochan);
					}
					if (f->subclass.integer == AST_CONTROL_PROGRESS && !sent_progress) {
						sent_progress = 1;
						ast_indicate(acts->chan, f->subclass.integer);
					} else if (f->subclass.integer == AST_CONTROL_PROCEEDING && !sent_proceeding) {
						sent_proceeding = 1;
						ast_indicate(acts->chan, f->subclass.integer);
					} else if (f->subclass.integer == AST_CONTROL_RINGING && !sent_ringing) {
						sent_ringing = 1;
						ast_indicate(acts->chan, f->subclass.integer);
					}
					break;
				case AST_CONTROL_VIDUPDATE:
				case AST_CONTROL_SRCUPDATE:
				case AST_CONTROL_SRCCHANGE:
					ast_verb(3, "%s requested media update control %d, passing it to %s\n",
						ast_channel_name(acts->ochan), f->subclass.integer, ast_channel_name(acts->chan));
					ast_indicate(acts->chan, f->subclass.integer);
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
				if (ast_write(acts->chan, f)) {
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
		} else if (winner == acts->chan) {
			struct ast_frame *f = ast_read(acts->chan);
			if (!f || (f->frametype == AST_FRAME_CONTROL && f->subclass.integer == AST_CONTROL_HANGUP)) {
				/* Caller cancelled call */
				ast_verb(4, "Caller %s hung up\n", ast_channel_name(acts->chan));
				acts->result = "CALLER_HANGUP";
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
				if (ast_write(acts->ochan, f)) {
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

static int wait_for_op_answer(struct acts_call *acts)
{
	int res = -1;

	while (res < 0) {
		struct ast_channel *winner;
		int to = 1000;

		winner = ast_waitfor_n(&acts->opchan, 1, &to);
		if (winner == acts->opchan) {
			char frametype[64];
			char subclass[64];
			struct ast_frame *f = ast_read(acts->opchan);
			if (!f) {
				ast_hangup(acts->opchan);
				acts->opchan = NULL;
				ast_verb(4, "Operator channel disconnected before answer\n");
				return 1;
			}
			switch (f->frametype) {
			case AST_FRAME_CONTROL:
				switch (f->subclass.integer) {
				case AST_CONTROL_ANSWER:
					ast_verb(3, "Operator channel %s answered\n", ast_channel_name(acts->opchan));
					ast_channel_hangupcause_set(acts->opchan, AST_CAUSE_NORMAL_CLEARING);
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

static struct ast_channel *new_channel(struct acts_call *acts, const char *tech, const char *resource)
{
	/* Much of this function is a very simplified
	 * minimal single-out channel dialing version of what app_dial does */
	struct ast_stream_topology *topology;
	struct ast_party_caller caller;
	int cause;
	struct ast_channel *chan;

	ast_channel_lock(acts->chan);
	topology = ast_stream_topology_clone(ast_channel_get_stream_topology(acts->chan));
	ast_channel_unlock(acts->chan);

	chan = ast_request_with_stream_topology(tech, topology, NULL, acts->chan, resource, &cause);
	if (!chan) {
		ast_verb(4, "Unable to create channel %s/%s (cause %d - %s)\n", tech, resource, cause, ast_cause2str(cause));
		if (!acts->ochan) {
			/* If this is the outgoing call, set the hangup cause.
			 * Don't do this for the operator call.
			 * This happens when !acts->ochan since that is the call being currently set up.
			 * If this were the op call, acts->ochan would already be set. */
			ast_channel_hangupcause_set(acts->chan, cause);
		}
		return NULL;
	}

	ast_channel_lock_both(chan, acts->chan);

	if (CAN_EARLY_BRIDGE(chan, acts->chan)) {
		ast_debug(3, "Making RTP early bridge compatible\n");
		ast_rtp_instance_early_bridge_make_compatible(chan, acts->chan);
	}

	/* Inherit specially named variables from parent channel */
	ast_channel_inherit_variables(acts->chan, chan);
	ast_channel_datastore_inherit(acts->chan, chan);
	ast_max_forwards_decrement(chan);

	ast_channel_appl_set(chan, "AppACTS");
	ast_channel_data_set(chan, "(Outgoing Line)");

	memset(ast_channel_whentohangup(chan), 0, sizeof(*ast_channel_whentohangup(chan)));

	ast_party_caller_set_init(&caller, ast_channel_caller(chan));
	ast_connected_line_copy_from_caller(ast_channel_connected(chan), ast_channel_caller(acts->chan));
	ast_party_redirecting_copy(ast_channel_redirecting(chan), ast_channel_redirecting(acts->chan));
	ast_channel_dialed(chan)->transit_network_select = ast_channel_dialed(acts->chan)->transit_network_select;
	ast_channel_req_accountcodes(chan, acts->chan, AST_CHANNEL_REQUESTOR_BRIDGE_PEER);
	ast_channel_adsicpe_set(chan, ast_channel_adsicpe(acts->chan));
	ast_channel_transfercapability_set(chan, ast_channel_transfercapability(acts->chan));

	ast_channel_dialcontext_set(chan, ast_channel_context(acts->chan));
	ast_channel_exten_set(chan, ast_channel_exten(acts->chan));

	ast_channel_unlock(chan);
	ast_channel_unlock(acts->chan);

	return chan;
}

static int setup_outgoing_call(struct acts_call *acts)
{
	int res;

	acts->ochan = new_channel(acts, acts->tech, acts->resource);
	if (!acts->ochan) {
		return 1;
	}

	/* Place the call, but don't wait on the answer */
	res = ast_call(acts->ochan, acts->resource, 0);

	ast_channel_lock(acts->chan);
	if (res) {
		ast_log(LOG_WARNING, "Failed to call on %s\n", ast_channel_name(acts->ochan));
		ast_channel_unlock(acts->chan);
		DISCONNECT_FAR_END();
		return 1;
	}

	ast_channel_publish_dial(acts->chan, acts->ochan, acts->resource, NULL);
	ast_channel_unlock(acts->chan);

	ast_verb(3, "Called %s/%s\n", acts->tech, acts->resource);
	res = wait_for_answer(acts);
	if (res) {
		return res;
	}

	res = ast_channel_make_compatible(acts->chan, acts->ochan);
	if (res < 0) {
		ast_log(LOG_ERROR, "%s and %s are not compatible\n", ast_channel_name(acts->chan), ast_channel_name(acts->ochan));
		DISCONNECT_FAR_END();
	}

	/* It is imperative that winks from the called party not be allowed to propagate
	 * towards the Class 5 end office, so the callee could spoof coin trunk controls (e.g. "green boxing").
	 * i.e. we don't want winks to propagate through the bridge towards the caller.
	 * I'm not positive if bridging will actually pass the winks through in our scenario,
	 * but we've no reason to allow them to either way, so completely block them, to be safe. */
	if (ast_func_write(acts->ochan, "FRAME_DROP(RX)", "WINK") || ast_func_write(acts->ochan, "FRAME_DROP(TX)", "WINK")) {
		ast_log(LOG_WARNING, "Failed to inhibit wink frames from the called party\n");
	}

	return res;
}

static int callee_leave_cb(struct ast_bridge_channel *bridge_channel, void *data)
{
	struct acts_call *acts = data;

	ast_mutex_lock(&acts->lock);
	if (!acts->callerdisconnected) {
		ast_verb(5, "Callee has disconnected\n"); /* Only log this if the callee hung up first */
		acts->result = "CALLEE_HANGUP";
		/* Disconnect the caller from the bridge if we hung up first */
		ast_assert(acts->chan != NULL);
		ast_bridge_remove(acts->bridge, acts->chan);
	} else {
		ast_debug(2, "Callee disconnected due to caller hangup or bridge termination\n");
	}
	acts->calleedisconnected = 1;
	ast_mutex_unlock(&acts->lock);
	return -1;
}

static int timeout_cb(struct ast_bridge_channel *bridge_channel, void *data)
{
	struct acts_call *acts = data;
	ast_debug(1, "Timeout has expired for caller\n");
	ast_mutex_lock(&acts->lock);
	acts->timeoutexpired = 1;
	ast_bridge_channel_leave_bridge(bridge_channel, BRIDGE_CHANNEL_STATE_END, 0);
	ast_mutex_unlock(&acts->lock);
	return -1;
}

static int set_timeout(struct ast_bridge_features *features, struct acts_call *acts, int timeout)
{
	ast_debug(1, "Setting bridge timeout to %d seconds\n", timeout);
	acts->timeoutexpired = 0;
	return ast_bridge_interval_hook(features, 0, timeout * 1000, timeout_cb, acts, NULL, AST_BRIDGE_HOOK_REMOVE_ON_PULL);
}

#define ABS(x) (x < 0 ? -x : x)

static int bridge_with_timeout(struct acts_call *acts, struct ast_bridge *bridge, int timeout, int overtime)
{
	int res;
	struct ast_bridge_features features;

	/* Safe guard in case we fail to catch a hangup somewhere else, bail out now */
	if (acts->callerdisconnected) {
		ast_debug(1, "Caller has disconnected, declining to bridge\n");
		return -1;
	} else if (acts->calleedisconnected) {
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
	if (timeout && set_timeout(&features, acts, timeout)) {
		ast_log(LOG_ERROR, "Failed to set bridge timeout\n");
		ast_bridge_features_cleanup(&features);
		return -1;
	}
	timeout = ABS(timeout);

	if (timeout) {
		ast_verb(4, "ACTS call %s with %d:%02d credit remaining\n", overtime ? "resumed" : "answered", timeout / 60, timeout % 60);
	} else {
		ast_verb(4, "ACTS call %s with no hard time limit\n", overtime ? "resumed" : "answered");
	}
	res = ast_bridge_join(bridge, acts->chan, NULL, &features, NULL, 0);
	ast_bridge_features_cleanup(&features);
	if (res) {
		ast_log(LOG_ERROR, "Caller %s failed to join bridge\n", ast_channel_name(acts->ochan));
		return -1;
	}

	/* At this point, we expect that either
	 * the first 3 minutes have expired
	 * or one of the two parties has hung up. */
	if (acts->ignorehangup) {
		acts->ignorehangup = 0;
	} else if (ast_check_hangup(acts->chan)) {
		acts->callerdisconnected = 1;
		/* Wait for the callee to be disconnected,
		 * since callee_leave_cb has acts as user data,
		 * and we can't go away while it could still be using that.
		 * If it were reference counted, we could clean this bit up here.
		 */
		ast_verb(4, "Caller has hung up\n");
		acts->result = "CALLER_HANGUP";
	}
	return res;
}

static int setup_op_call(struct acts_call *acts)
{
	int res;
	char buf[20];

	acts->opchan = new_channel(acts, acts->optech, acts->opresource);
	if (!acts->opchan) {
		return 1;
	}

	/* Set key variables that the operator channel may use for assisting the caller. */
	snprintf(buf, sizeof(buf), "%d", acts->credit >= 0 ? -acts->credit : 0);
	pbx_builtin_setvar_helper(acts->opchan, "ACTS_CREDIT_REQUIRED", buf);
	pbx_builtin_setvar_helper(acts->opchan, "ACTS_IN_OVERTIME", acts->overtime ? "1" : "0");
	snprintf(buf, sizeof(buf), "%d", acts->initialperiod);
	pbx_builtin_setvar_helper(acts->opchan, "ACTS_INITIAL_PERIOD", buf);

	/* Place the call, but don't wait on the answer */
	res = ast_call(acts->opchan, acts->opresource, 0);

	ast_channel_lock(acts->chan);
	if (res) {
		ast_log(LOG_WARNING, "Failed to call on %s\n", ast_channel_name(acts->opchan));
		ast_channel_unlock(acts->chan);
		ast_hangup(acts->opchan);
		acts->opchan = NULL;
		return 1;
	}
	ast_channel_unlock(acts->chan);

	ast_verb(3, "Called %s/%s\n", acts->optech, acts->opresource);
	res = wait_for_op_answer(acts);
	if (res) {
		ast_hangup(acts->opchan);
		acts->opchan = NULL;
		return res;
	}

	res = ast_channel_make_compatible(acts->chan, acts->opchan);
	if (res < 0) {
		ast_log(LOG_ERROR, "%s and %s are not compatible\n", ast_channel_name(acts->chan), ast_channel_name(acts->opchan));
		ast_hangup(acts->opchan);
		acts->opchan = NULL;
	}

	return res;
}

static void *signal_operator(void *varg)
{
	int res;
	struct ast_bridge_features features;
	struct acts_call *acts = varg;

	if (ast_bridge_features_init(&features)) {
		ast_log(LOG_ERROR, "Failed to init bridge features\n");
		acts->operatorpending = 0;
		return NULL;
	}
	features.dtmf_passthrough = 1;

	if (!setup_op_call(acts)) {
		/* We join rather than impart, so that this is a blocking call,
		 * to ensure that acts->operatorpending is true
		 * as long as we're really in the bridge. */
		res = ast_bridge_join(acts->bridge, acts->opchan, NULL, &features, NULL, AST_BRIDGE_JOIN_INHIBIT_JOIN_COLP);
		ast_bridge_features_cleanup(&features);
		if (res) {
			ast_log(LOG_ERROR, "Operator %s failed to join bridge\n", ast_channel_name(acts->opchan));
		}
	}

	ast_debug(3, "Operator thread is exiting\n");

	ast_mutex_lock(&acts->lock);
	acts->operatorpending = 0;
	ast_hangup(acts->opchan);
	acts->opchan = NULL;
	ast_mutex_unlock(&acts->lock);

	return NULL;
}

static struct ast_frame *caller_flash_cb(struct ast_channel *chan, struct ast_frame *frame, enum ast_framehook_event event, void *data)
{
	struct acts_call *acts = data;

	if (!frame || frame->frametype != AST_FRAME_CONTROL || frame->subclass.integer != AST_CONTROL_FLASH) {
		return frame;
	} else if (event != AST_FRAMEHOOK_EVENT_READ) {
		/* Unusual for a hook flash to propagate all the way to us in this direction, but could happen... */
		ast_debug(1, "Hmm... got hook flash from called party?\n");
		return frame;
	}

	ast_mutex_lock(&acts->lock);
	if (acts->operatorpending) {
		ast_verb(5, "Operator already attached, ignoring hook flash...\n");
	} else {
		ast_verb(4, "Caller signalled hook flash towards A.C.T.S., summoning operator\n");
		/* Summon an operator.
		 * If we already have in the past and that's expired, join the thread first. */
		if (acts->opthread) {
			ast_debug(2, "Cleaning up old operator thread before spawning a new one\n");
			pthread_join(acts->opthread, NULL);
			acts->opthread = 0;
		}
		acts->operatorpending = 1;
		if (ast_pthread_create(&acts->opthread, NULL, signal_operator, acts)) {
			ast_log(LOG_ERROR, "Failed to summon operator\n");
		}
	}
	ast_mutex_unlock(&acts->lock);

	/* Sure, let the flash propagate onwards... */
	return frame;
}

static void cleanup_bridge(struct acts_call *acts)
{
	ast_debug(3, "Destroying ACTS call bridge %p\n", acts->bridge);
	if (ast_bridge_destroy(acts->bridge, 0)) {
		ast_log(LOG_ERROR, "Failed to destroy ACTS bridge %p?\n", acts->bridge);
	}

	ast_mutex_lock(&acts->lock);
	acts->bridge = NULL;
	acts->ochan = NULL; /* Since this channel was imparted independently, we don't hang it up, it should get destroyed by the ast_bridge_destroy call above. */
	ast_debug(3, "Finished cleaning up ACTS bridge\n");
	ast_mutex_unlock(&acts->lock);
}

static int acts_run(struct acts_call *acts)
{
	int res;
	struct ast_bridge_features *callee_features;
	int framehook_id = -1;
	struct ast_framehook_interface interface = {
		.version = AST_FRAMEHOOK_INTERFACE_VERSION,
		.event_cb = caller_flash_cb,
		.data = acts,
	};

	/* We assume a dial tone first line if A.C.T.S. is being used,
	 * since it's not suitable for postpay (or semi-postpay) lines.
	 * Technically compatible with coin first, if the initial deposit
	 * is returned before proceeding. */

	/* This refers to initial deposit for ACTS, not for the phone itself */
	if (acts->initialdeposit) {
		res = get_initial_deposit(acts, acts->initialdeposit);
		ast_debug(5, "get_initial_deposit returned %d\n", res);
		if (res) {
			return res;
		}
		/* acts->hopper contains how much money was deposited and is
		 * now sitting in the hopper */
		ast_verb(4, "Initial deposit has been satisfied\n");
	} else {
		/* Strange that the call was routed to A.C.T.S in the first place,
		* but just let it go through... */
		ast_verb(4, "No initial deposit required\n");
		if (acts->arrivedattached) {
			/* In this case, we need to release. */
			res = coin_disposition(acts, OPERATOR_RELEASED);
			if (res) {
				return res;
			}
		}
	}

	/* Now, we can begin the call.
	 * Rather than calling the Dial() application,
	 * because we want to manipulate the bridging afterwards,
	 * it's easier to do the dialing and bridging ourselves manually. */
	res = setup_outgoing_call(acts);
	if (res) {
		if (!ast_check_hangup(acts->chan) && !acts->ochan) {
			/* Call never answered and has already ended.
			 * Refund the caller and exit. */
			if (coin_disposition(acts, COIN_RETURN)) {
				return -1;
			}
		}
		return res;
	}

	/* Call has now answered. Actually bridge the channels.
	 * The 3-minute initial period starts now. */
	acts->answertime = time(NULL);
	if (ast_channel_state(acts->chan) == AST_STATE_UP) {
		/* Accurate supervision is important.
		 * The channel should not already be answered prior to now. */
		ast_log(LOG_WARNING, "Channel %s was already answered?\n", ast_channel_name(acts->chan));
	} else {
		ast_raw_answer(acts->chan);
	}

	acts->bridge = ast_bridge_base_new(AST_BRIDGE_CAPABILITY_MULTIMIX, AST_BRIDGE_FLAG_TRANSFER_PROHIBITED, acts_app, NULL, NULL);
	if (!acts->bridge) {
		ast_log(LOG_ERROR, "Failed to create bridge for ACTS call\n");
		return -1;
	}
	ast_bridge_set_internal_sample_rate(acts->bridge, 8000); /* 8 KHz */
	ast_bridge_set_maximum_sample_rate(acts->bridge, 8000);
	ast_bridge_set_mixing_interval(acts->bridge, 20); /* 20 ms */

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
	res = ast_bridge_leave_hook(callee_features, callee_leave_cb, acts, NULL, 0);
	if (res) {
		ast_log(LOG_ERROR, "Failed to add leave hook to bridge features\n");
		DISCONNECT_FAR_END();
		goto cleanup;
	}
	callee_features->dtmf_passthrough = 1;
	res = ast_bridge_impart(acts->bridge, acts->ochan, NULL, callee_features, AST_BRIDGE_IMPART_CHAN_INDEPENDENT | AST_BRIDGE_IMPART_INHIBIT_JOIN_COLP);
	if (res) {
		ast_log(LOG_ERROR, "Callee %s failed to join bridge\n", ast_channel_name(acts->ochan));
		DISCONNECT_FAR_END();
		goto cleanup;
	}

	if (acts->optech) {
		/* After the initial deposit,
		 * the caller is always bridge expect when Expanded In-Band Signaling
		 * is taking place. Therefore, set up a hook for hook flash,
		 * so the caller can flash for operator assistance.
		 * There aren't any bridge hooks that are generic enough for control frames,
		 * so we just use a regular framehook. */
		framehook_id = ast_framehook_attach(acts->chan, &interface);
		if (framehook_id < 0) {
			ast_log(LOG_WARNING, "Failed to attach flash framehook: %s\n", ast_channel_name(acts->chan));
		}
	}

#define SECS_COLLECT_BEFORE_PROMPTING 7

	/* First 3 minutes (or an unlimited length call)
	 * Subtract 7 since we collect the contents of the hopper 7 seconds before time is up.
	 * The sound of the hopper collecting into the vault also signals to the caller that their time is almost up,
	 * and they should get more coins ready to continue the call. */
	if (acts->overtimedeposit) {
		acts->expiretime = time(NULL) + acts->initialperiod;
	}
	res = bridge_with_timeout(acts, acts->bridge, acts->overtimedeposit ? acts->initialperiod - SECS_COLLECT_BEFORE_PROMPTING : 0, 0);
	if (res || acts->calleedisconnected || !acts->overtimedeposit) {
		goto cleanup;
	}

	/* Overtime loop */
	acts->credit = 0; /* This is zero-initialized, but just to be clear, no credit carries over from the initial period. */
	acts->overtime = 1;
	for (;;) {
		int newtimeout;
		time_t now, overtime_start;

		/* Calculate amount required. */
		acts->credit -= acts->overtimedeposit;
		/* Not enough funds to continue without requiring another deposit */
		ast_assert(acts->credit < 0);

		/* Collect whatever's in the hopper. */
		res = coin_disposition(acts, COIN_COLLECT);
		if (res) {
			break;
		}
		/* Rejoin the bridge for 7 seconds.
		 * However, coin collect itself takes a couple seconds,
		 * so subtract off two from that since those seconds already passed. */
		res = bridge_with_timeout(acts, acts->bridge, SECS_COLLECT_BEFORE_PROMPTING - 2, 1);
		if (res) {
			break;
		}

		/* The next minute of overtime starts NOW,
		 * not whenever the caller finishes paying for it. */
		overtime_start = time(NULL);
		res = get_overtime_deposit(acts);
		if (res) {
			break;
		}

		ast_mutex_lock(&acts->lock);
		acts->credit += acts->hopper;
		if (acts->credit < 0) {
			/* If we're still underpaid,
			 * disconnect the call. */
			ast_verb(3, "Call automatically disconnected due to insufficient overtime funds\n");
			acts->callerdisconnected = 1;
			ast_mutex_unlock(&acts->lock);
			acts->result = "OVERTIME_EXPIRED";
			break;
		}
		ast_mutex_unlock(&acts->lock);

		now = time(NULL);
		newtimeout = 60 - (now - overtime_start); /* Subtract out how long the caller took to deposit from the time left */

		/* If more was deposited than necessary,
		 * specifically enough that we could go for at least 2 minutes on credit,
		 * rather than exiting the bridge every 60 seconds to subtract
		 * the overtime per minute and re-enter the bridge,
		 * do some math now to simplify bridging operations. */
		ast_debug(4, "Credit remaining: %d c, time remaining: %d s (hopper contains %d cents)\n", acts->credit, newtimeout, acts->hopper);
		while (acts->credit >= acts->overtimedeposit) {
			acts->credit -= acts->overtimedeposit;
			newtimeout += 60;
			ast_debug(4, "Credit remaining: %d c, time remaining: %d s\n", acts->credit, newtimeout);
		}

		if (newtimeout < SECS_COLLECT_BEFORE_PROMPTING + 1) {
			/* Min timeout is 1. We really shouldn't allow this to happen since this would be fradulent.
			 * Basically we'll just enter the bridge for a second, collect everything, and repeat the loop. */
			ast_log(LOG_WARNING, "Caller took too long to deposit, changing timeout from %d to %d\n", newtimeout, SECS_COLLECT_BEFORE_PROMPTING + 1);
			newtimeout = SECS_COLLECT_BEFORE_PROMPTING + 1;
		}
		acts->expiretime = now + newtimeout;

		res = bridge_with_timeout(acts, acts->bridge, newtimeout - SECS_COLLECT_BEFORE_PROMPTING, 1);
	}

	if (acts->callerdisconnected && !acts->result && res < 0) {
		acts->result = "CALLER_HANGUP";
	}

cleanup:
	ast_mutex_lock(&acts->lock);
	if (framehook_id >= 0 && ast_framehook_detach(acts->chan, framehook_id)) {
		ast_log(LOG_WARNING, "Failed to remove framehook from channel %s\n", ast_channel_name(acts->chan));
	}
	acts->callerdisconnected = 1; /* We're not in the bridge anymore */
	ast_mutex_unlock(&acts->lock);

	if (acts->opthread) {
		if (acts->opchan) {
			ast_debug(2, "Kicking operator %s from bridge\n", ast_channel_name(acts->opchan));
			/* Use ast_bridge_remove rather than ast_bridge_kick,
			 * so as not to incidentally dissolve the bridge; we will do that ourselves. */
			ast_bridge_remove(acts->bridge, acts->opchan);
		}
		pthread_join(acts->opthread, NULL);
	}

	cleanup_bridge(acts);

	/* ast_bridge_destroy doesn't block synchronously
	 * until all callbacks have finished, so we need
	 * to effectively busy wait until we can proceed. */
	if (!acts->calleedisconnected) {
		int c = 0;

		/* Wait for callee_leave_cb to finish executing,
		 * since its callback data is acts, and that is
		 * stack allocated in this thread, so we can't go
		 * away until after it does. */
		while (!acts->calleedisconnected) {
			/* Channel has already hung up,
			 * so ast_safe_sleep might fail?
			 * Ignore its return value. */
			ast_safe_sleep(acts->chan, 10);
			if (c == 100) {
				ast_log(LOG_WARNING, "Callee channel has still not been disconnected?\n");
			}
		}
		ast_debug(2, "Callee has now disconnected and callee_leave_cb is done with callback data\n");
	}

	/* At this point, opchan should be completely done with the bridge */
	ast_debug(3, "Finished cleaning up ACTS call\n");
	return res;
}

static int acts_exec(struct ast_channel *chan, const char *data)
{
	struct acts_call acts;
	struct ast_flags flags = {0};
	char databuf[32];
	char *opt_args[OPT_ARG_ARRAY_SIZE];
	char *appdata;
	char *dialstr, *opdialstr;
	int res;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(dialstr);
		AST_APP_ARG(audiodir);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_ERROR, "%s requires arguments\n", acts_app);
		goto invalid;
	}

	appdata = ast_strdupa(S_OR(data, ""));
	AST_STANDARD_APP_ARGS(args, appdata);

	memset(&acts, 0, sizeof(acts));
	ast_mutex_init(&acts.lock);

	if (ast_strlen_zero(args.dialstr)) {
		ast_log(LOG_ERROR, "%s requires a dial string\n", acts_app);
		goto invalid;
	} else if (ast_strlen_zero(args.audiodir)) {
		ast_log(LOG_ERROR, "%s requires an audio directory\n", acts_app);
		goto invalid;
	}
	acts.audiodir = args.audiodir;
	dialstr = args.dialstr;
	acts.tech = strsep(&dialstr, "/");
	acts.resource = dialstr;
	if (ast_strlen_zero(acts.tech) || ast_strlen_zero(acts.resource)) {
		ast_log(LOG_ERROR, "%s requires a valid dial string (tech/resource)\n", acts_app);
		goto invalid;
	}

	acts.initialperiod = 180; /* Default initial paid period is 3 minutes */

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(app_opts, &flags, opt_args, args.options);
		if (ast_test_flag(&flags, OPT_INITIAL_DEPOSIT) && !ast_strlen_zero(opt_args[OPT_ARG_INITIAL_DEPOSIT])) {
			acts.initialdeposit = atoi(opt_args[OPT_ARG_INITIAL_DEPOSIT]);
			if (acts.initialdeposit < 0) {
				ast_log(LOG_ERROR, "Invalid initial deposit: %d cents\n", acts.initialdeposit);
				goto invalid;
			}
		}
		if (ast_test_flag(&flags, OPT_OVERTIME_DEPOSIT) && !ast_strlen_zero(opt_args[OPT_ARG_OVERTIME_DEPOSIT])) {
			acts.overtimedeposit = atoi(opt_args[OPT_ARG_OVERTIME_DEPOSIT]);
			if (acts.overtimedeposit < 0) {
				ast_log(LOG_ERROR, "Invalid overtime deposit: %d cents\n", acts.overtimedeposit);
				goto invalid;
			}
		}
		if (ast_test_flag(&flags, OPT_INITIAL_PERIOD) && !ast_strlen_zero(opt_args[OPT_ARG_INITIAL_PERIOD])) {
			acts.initialperiod = atoi(opt_args[OPT_ARG_INITIAL_PERIOD]);
			if (acts.initialperiod < 10) {
				ast_log(LOG_ERROR, "Invalid initial period: %d seconds\n", acts.initialperiod);
				goto invalid;
			}
		}
		if (ast_test_flag(&flags, OPT_FLASH_FOR_OPERATOR) && !ast_strlen_zero(opt_args[OPT_ARG_FLASH_FOR_OPERATOR])) {
			opdialstr = opt_args[OPT_ARG_FLASH_FOR_OPERATOR];
			acts.optech = strsep(&opdialstr, "/");
			acts.opresource = opdialstr;
			if (ast_strlen_zero(acts.optech) || ast_strlen_zero(acts.opresource)) {
				ast_log(LOG_WARNING, "Operator dial string is invalid (must be tech/resource)\n");
				acts.optech = acts.opresource = NULL;
			}
		}
		acts.arrivedattached = ast_test_flag(&flags, OPT_ALREADY_ATTACHED) ? 1 : 0;
		acts.attached = acts.arrivedattached;
		acts.relax = ast_test_flag(&flags, OPT_RELAX) ? 1 : 0;
		acts.sf = ast_test_flag(&flags, OPT_SF) ? 1 : 0;
	}

	acts.chan = chan;
	acts.start = time(NULL);

	/* Keep the call in the linked list while they're active
	 * so we can dump all ACTS calls from the CLI. */
	AST_RWLIST_WRLOCK(&calls);
	AST_RWLIST_INSERT_TAIL(&calls, &acts, entry);
	AST_RWLIST_UNLOCK(&calls);

	res = acts_run(&acts);

	AST_RWLIST_WRLOCK(&calls);
	AST_RWLIST_REMOVE(&calls, &acts, entry);
	AST_RWLIST_UNLOCK(&calls);

	/* If ochan still exists, nix it */
	if (acts.ochan) {
		/* We assume here that ochan is not bridged,
		 * or that will trigger an assertion. */
		ast_debug(3, "Cleaning up %s since it wasn't bridged\n", ast_channel_name(acts.ochan));
		ast_assert(!ast_channel_bridge_peer(acts.ochan));
		ast_hangup(acts.ochan);
		acts.ochan = NULL;
	}

	if (acts.result) {
		pbx_builtin_setvar_helper(chan, "ACTS_RESULT", acts.result);
	} else {
		ast_log(LOG_WARNING, "Missing internal result disposition... assuming failure\n");
		pbx_builtin_setvar_helper(chan, "ACTS_RESULT", "FAILURE");
	}

	if (res > 0) {
		/* Positive return means the call was forcibly disconnect,
		 * but chan isn't hung up, so keep going */
		res = 0;
	}

	/* Finish calculating correctly.
	 * If the call answered and there is money still in the hopper, collect it
	 * XXX That makes sense for during overtime itself, but if the call
	 * ended while prompting for further deposits, maybe that should be returned? */
	if (acts.answertime && acts.hopper) {
		if (res >= 0) {
			res |= coin_disposition(&acts, COIN_COLLECT);
		} else {
			/* Channel hung up, so we can't actually
			 * collect from here, the Class 5 end office
			 * will handle that.
			 * However, we still want to take this amount
			 * into account for our calculations. */
			acts.collected += acts.hopper;
			acts.hopper = 0;
		}
	}

	snprintf(databuf, sizeof(databuf), "%d", acts.collected);
	pbx_builtin_setvar_helper(chan, "ACTS_COLLECTED", databuf);

	ast_mutex_destroy(&acts.lock);
	return res;

invalid:
	pbx_builtin_setvar_helper(chan, "ACTS_RESULT", "INVALID");
	return -1;
}

#define MONEY_FMT "%s$%3d.%02d" /* 8 characters */
#define MONEY_ARGS(x) x < 0 ? "-" : " ", ABS(x) / 100, (ABS(x) % 100)

static char *handle_show_calls(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	int total = 0;
	time_t now;
	struct acts_call *acts;

	switch(cmd) {
	case CLI_INIT:
		e->command = "acts show calls";
		e->usage =
			"Usage: acts show calls\n"
			"       Lists status of all ACTS calls\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc < 3) {
		return CLI_SHOWUSAGE;
	}

	now = time(NULL);

	AST_RWLIST_RDLOCK(&calls);
	AST_RWLIST_TRAVERSE(&calls, acts, entry) {
		int diff, hr, min, sec;
		int ans_hr = 0, ans_min = 0, ans_sec = 0;
		int exp_min = 0, exp_sec = 0;

		ast_mutex_lock(&acts->lock); /* Not strictly needed, but ensure nothing changes */

		/* Print heading if first call */
		if (!total++) {
			ast_cli(a->fd, "%8s %8s %9s %8s %8s %8s %8s %8s %9s %s\n",
				"Duration", "Ans Dur.","Time Left", "OpAttach", "Initial", "Overtime", "Credit", "Hopper", "Collected", "Called Channel / Operator Channel");
		}

		/* Calculate duration */
		diff = now - acts->start;
		hr = diff / 3600;
		min = (diff % 3600) / 60;
		sec = diff % 60;

		if (acts->answertime) {
			diff = now - acts->answertime;
			ans_hr = diff / 3600;
			ans_min = (diff % 3600) / 60;
			ans_sec = diff % 60;
		}

		if (acts->expiretime > now) {
			diff = acts->expiretime - now;
			exp_min = diff / 60;
			exp_sec = diff % 60;
		}

		ast_cli(a->fd, "%02d:%02d:%02d %02d:%02d:%02d %6d:%02d %8s " MONEY_FMT " " MONEY_FMT " " MONEY_FMT " " MONEY_FMT "  " MONEY_FMT " %s / %s\n",
				hr, min, sec,
				ans_hr, ans_min, ans_sec,
				exp_min, exp_sec,
				acts->attached ? "Yes" : "No",
				MONEY_ARGS(acts->initialdeposit), MONEY_ARGS(acts->overtimedeposit),
				MONEY_ARGS(acts->credit), MONEY_ARGS(acts->hopper), MONEY_ARGS(acts->collected),
				acts->ochan ? ast_channel_name(acts->ochan) : "", /* XXX Technically want locking for ochan */
				acts->opchan ? ast_channel_name(acts->opchan) : ""
			);
		ast_mutex_unlock(&acts->lock);
	}
	AST_RWLIST_UNLOCK(&calls);

	if (!total) {
		ast_cli(a->fd, "No active A.C.T.S. calls\n");
	}

	return CLI_SUCCESS;
}

static struct ast_cli_entry acts_cli[] = {
	AST_CLI_DEFINE(handle_show_calls, "Displays information about all active A.C.T.S. calls"),
};

static int unload_module(void)
{
	/* We won't be able to unload if there are any active calls,
	 * so if we get here, the calls linked list must be empty. */

	ast_cli_unregister_multiple(acts_cli, ARRAY_LEN(acts_cli));
	return ast_unregister_application(acts_app);
}

static int load_module(void)
{
	int res = ast_register_application_xml(acts_app, acts_exec);
	if (!res) {
		ast_cli_register_multiple(acts_cli, ARRAY_LEN(acts_cli));
	}
	return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Automated Coin Toll System",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.requires = "res_coindetect,chan_bridge_media",
);
