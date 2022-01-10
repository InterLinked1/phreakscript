/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2021, Naveen Albert
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
 * \brief Coin detection module
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup resources
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <math.h>

#include "asterisk/module.h"
#include "asterisk/frame.h"
#include "asterisk/format_cache.h"
#include "asterisk/channel.h"
#include "asterisk/dsp.h"
#include "asterisk/pbx.h"
#include "asterisk/audiohook.h"
#include "asterisk/app.h"
#include "asterisk/indications.h"
#include "asterisk/conversions.h"
#include "asterisk/ulaw.h"
#include "asterisk/alaw.h"

/*** DOCUMENTATION
	<application name="CoinDisposition" language="en_US">
		<synopsis>
			Signals a TSPS coin disposition
		</synopsis>
		<syntax>
			<parameter name="disposition" required="true">
				<para>Coin disposition to signal to the Class 5 end office.</para>
				<para>Must be one of the following:</para>
				<enumlist>
					<enum name="return">
						<para>Coin return</para>
						<para>Returns any coins currently in the hopper
						to the caller.</para>
					</enum>
					<enum name="collect">
						<para>Coin collect</para>
						<para>Collects coins currently in the hopper into
						the coin vault.</para>
					</enum>
					<enum name="ringback">
						<para>Coin phone ringback</para>
						<para>Used by the operator to ring back a coin phone,
						typically if a caller has hung up and additional
						overtime deposits or charges need to be paid.</para>
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
					<enum name="collectreleased">
						<para>Coin Collect and Operator Released</para>
						<para>Performs a coin collect followed by Operator Released.</para>
					</enum>
				</enumlist>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="i">
						<para>Also send an audible in-band wink, in addition
						to a WINK control frame.</para>
					</option>
					<option name="m">
						<para>Use the older multiwink signaling instead
						of Expanded In-Band Signaling.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Signals a TSPS coin disposition to a Class 5 end office
			on an active coin trunk for remote coin control.</para>
			<para>This application only performs coin control signaling.
			The Class 5 office is responsible for acting on these signals
			to perform the appropriate function.</para>
			<note><para>This application should only be used by Class 4 offices (e.g. TSPS),
			to signal Class 5 offices, not directly by Class 5 offices.</para></note>
		</description>
		<see-also>
			<ref type="application">WaitForDeposit</ref>
			<ref type="function">COIN_DETECT</ref>
		</see-also>
	</application>
	<application name="WaitForDeposit" language="en_US">
		<synopsis>
			Wait for coins to be deposited
		</synopsis>
		<syntax>
			<parameter name="amount" required="false">
				<para>Minimum deposit required (subject to the provided timeout)
				before returning. Default is 5 cents. If the amount provided is
				not evenly divisible by 5, it will be rounded up to the nearest
				nickel.</para>
			</parameter>
			<parameter name="timeout" required="false">
				<para>Maximum amount of time, in seconds, to wait for specified amount.
				Default is forever.</para>
			</parameter>
			<parameter name="delay" required="false">
				<para>Amount of time, in seconds, to wait for further deposits after
				minimum deposit has been satisfied. When this delay timeout is reached,
				the application will return. This only takes effect after the number
				of times requirement has been met. Default is 0 (return immediately).</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="l">
						<para>Relax coin denomination tone detection. This option
						may be necessary for detection of tones present in bidirectional
						or non-ideal audio.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Waits for coin denomination tones to be detected before dialplan execution continues.</para>
			<note><para>Accuracy of detection may vary with environment and is not guaranteed.</para></note>
			<para>The following variables are set by this application:</para>
			<variablelist>
				<variable name="WAITFORDEPOSITSTATUS">
					<para>This indicates the result of the wait.</para>
					<para>Note that coins may have been deposited even
					if SUCCESS is not returned.</para>
					<value name="SUCCESS"/>
					<value name="ERROR"/>
					<value name="TIMEOUT"/>
					<value name="HANGUP"/>
				</variable>
				<variable name="WAITFORDEPOSITAMOUNT">
					<para>Amount, in cents, that has been deposited.</para>
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="function">COIN_DETECT</ref>
			<ref type="application">CoinDisposition</ref>
		</see-also>
	</application>
	<function name="COIN_DETECT" language="en_US">
		<synopsis>
			Asynchronously detect coin deposits
		</synopsis>
		<syntax>
			<parameter name="options">
				<optionlist>
					<option name="a">
						<para>Minimum deposit required (subject to the provided timeout)
						before going to the destination provided in the <literal>g</literal>
						or <literal>h</literal> option. Default is 5 cents.</para>
					</option>
					<option name="d">
						<para>Delay threshold to use after the condition has matched to allow for
						additional deposits to be received. Default is 0 (no delay).</para>
					</option>
					<option name="g">
						<para>Go to the specified context,exten,priority if tone is received on this channel.
						Detection will not end automatically.</para>
					</option>
					<option name="h">
						<para>Go to the specified context,exten,priority if tone is transmitted on this channel.
						Detection will not end automatically.</para>
					</option>
					<option name="l">
						<para>Relax coin denomination tone detection. This option
						may be necessary for detection of tones present in bidirectional
						or non-ideal audio.</para>
					</option>
					<option name="r">
						<para>Apply to received frames only. Default is both directions.</para>
					</option>
					<option name="t">
						<para>Apply to transmitted frames only. Default is both directions.</para>
					</option>
					<option name="x">
						<para>Destroy the detector (stop detection).</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>The COIN_DETECT function detects coin denomination tones and keeps
			track of how many times the tone has been detected.</para>
			<para>When reading this function (instead of writing), supply <literal>tx</literal>
			to get the number of times a tone has been detected in the TX direction and
			<literal>rx</literal> to get the number of times a tone has been detected in the
			RX direction.</para>
			<note><para>Accuracy of detection may vary with environment and is not guaranteed.</para></note>
			<example title="intercept2600">
			same => n,Set(COIN_DETECT(a(10)d(5)g(got-2600,s,1))=) ; wait for 10 cents, with 5 second grace period
			for overtime deposits, and redirect to got-2600,s,1 afterwards
			same => n,Wait(15) ; wait 15 seconds for deposits
			same => n,NoOp(${COIN_DETECT(rx)}) ; amount, in cents, that has been deposited
			</example>
			<example title="removedetector">
			same => n,Set(COIN_DETECT(x)=) ; remove the detector from the channel
			</example>
		</description>
		<see-also>
			<ref type="application">WaitForDeposit</ref>
			<ref type="application">CoinDisposition</ref>
		</see-also>
	</function>
 ***/

struct detect_information {
	struct ast_dsp *dsp;
	struct ast_audiohook audiohook;
	char *gototx;
	char *gotorx;
	unsigned short int tx;
	unsigned short int rx;
	int txcount;
	int rxcount;
	int hitsrequired;
	int delay;
	int actionflag;
	int debounce;
	int debouncedhits;
	struct timeval delaytimer;
};

enum td_opts {
	OPT_TX = (1 << 1),
	OPT_RX = (1 << 2),
	OPT_END_DETECTOR = (1 << 3),
	OPT_GOTO_RX = (1 << 4),
	OPT_GOTO_TX = (1 << 5),
	OPT_HITS_REQ = (1 << 6),
	OPT_DELAY = (1 << 7),
	OPT_RELAX = (1 << 8),
};

enum {
	OPT_ARG_GOTO_RX,
	OPT_ARG_GOTO_TX,
	OPT_ARG_HITS_REQ,
	OPT_ARG_DELAY,
	/* note: this entry _MUST_ be the last one in the enum */
	OPT_ARG_ARRAY_SIZE,
};

AST_APP_OPTIONS(td_opts, {
	AST_APP_OPTION_ARG('a', OPT_HITS_REQ, OPT_ARG_HITS_REQ),
	AST_APP_OPTION_ARG('d', OPT_DELAY, OPT_ARG_DELAY),
	AST_APP_OPTION_ARG('g', OPT_GOTO_RX, OPT_ARG_GOTO_RX),
	AST_APP_OPTION_ARG('h', OPT_GOTO_TX, OPT_ARG_GOTO_TX),
	AST_APP_OPTION('l', OPT_RELAX),
	AST_APP_OPTION('t', OPT_TX),
	AST_APP_OPTION('r', OPT_RX),
	AST_APP_OPTION('x', OPT_END_DETECTOR),
});

static void destroy_callback(void *data)
{
	struct detect_information *di = data;
	ast_dsp_free(di->dsp);
	if (di->gotorx) {
		ast_free(di->gotorx);
	}
	if (di->gototx) {
		ast_free(di->gototx);
	}
	ast_audiohook_lock(&di->audiohook);
	ast_audiohook_detach(&di->audiohook);
	ast_audiohook_unlock(&di->audiohook);
	ast_audiohook_destroy(&di->audiohook);
	ast_free(di);
	return;
}

static const struct ast_datastore_info detect_datastore = {
	.type = "detect",
	.destroy = destroy_callback
};

static int remove_detect(struct ast_channel *chan)
{
	struct ast_datastore *datastore = NULL;
	struct detect_information *data;
	SCOPED_CHANNELLOCK(chan_lock, chan);

	datastore = ast_channel_datastore_find(chan, &detect_datastore, NULL);
	if (!datastore) {
		ast_log(AST_LOG_WARNING, "Cannot remove COIN_DETECT from %s: COIN_DETECT not currently enabled\n",
		        ast_channel_name(chan));
		return -1;
	}
	data = datastore->data;

	if (ast_audiohook_remove(chan, &data->audiohook)) {
		ast_log(AST_LOG_WARNING, "Failed to remove COIN_DETECT audiohook from channel %s\n", ast_channel_name(chan));
		return -1;
	}

	if (ast_channel_datastore_remove(chan, datastore)) {
		ast_log(AST_LOG_WARNING, "Failed to remove COIN_DETECT datastore from channel %s\n",
		        ast_channel_name(chan));
		return -1;
	}
	ast_datastore_free(datastore);

	return 0;
}

static char* goto_parser(struct ast_channel *chan, char *loc) {
	char *exten, *pri, *context, *parse;
	char *dest;
	int size;
	parse = ast_strdupa(loc);
	context = strsep(&parse, ",");
	exten = strsep(&parse, ",");
	pri = strsep(&parse, ",");
	if (!exten) {
		pri = context;
		exten = NULL;
		context = NULL;
	} else if (!pri) {
		pri = exten;
		exten = context;
		context = NULL;
	}
	ast_channel_lock(chan);
	if (ast_strlen_zero(exten)) {
		exten = ast_strdupa(ast_channel_exten(chan));
	}
	if (ast_strlen_zero(context)) {
		context = ast_strdupa(ast_channel_context(chan));
	}
	ast_channel_unlock(chan);

	/* size + 3: for 1 null terminator + 2 commas */
	size = strlen(context) + strlen(exten) + strlen(pri) + 3;
	dest = ast_malloc(size + 1);
	if (!dest) {
		ast_log(LOG_ERROR, "Failed to parse goto: %s,%s,%s\n", context, exten, pri);
		return NULL;
	}
	snprintf(dest, size, "%s,%s,%s", context, exten, pri);
	return dest;
}

static int detect_read(struct ast_channel *chan, const char *cmd, char *data, char *buffer, size_t buflen)
{
	struct ast_datastore *datastore = NULL;
	struct detect_information *di = NULL;

	if (!chan) {
		ast_log(LOG_WARNING, "No channel was provided to %s function.\n", cmd);
		return -1;
	}

	ast_channel_lock(chan);
	if (!(datastore = ast_channel_datastore_find(chan, &detect_datastore, NULL))) {
		ast_channel_unlock(chan);
		return -1; /* function not initiated yet, so nothing to read */
	} else {
		ast_channel_unlock(chan);
		di = datastore->data;
	}

	if (strchr(data, 't')) {
		snprintf(buffer, buflen, "%d", 5 * di->txcount); /* each beep is 5 cents */
	} else if (strchr(data, 'r')) {
		snprintf(buffer, buflen, "%d", 5 * di->rxcount); /* each beep is 5 cents */
	} else {
		ast_log(LOG_WARNING, "Invalid direction: %s\n", data);
	}

	return 0;
}

static int detect_callback(struct ast_audiohook *audiohook, struct ast_channel *chan, struct ast_frame *frame, enum ast_audiohook_direction direction)
{
	struct ast_datastore *datastore = NULL;
	struct detect_information *di = NULL;
	int now, success = 0;

	/* If the audiohook is stopping it means the channel is shutting down.... but we let the datastore destroy take care of it */
	if (audiohook->status == AST_AUDIOHOOK_STATUS_DONE) {
		return 0;
	}

	/* Grab datastore which contains our gain information */
	if (!(datastore = ast_channel_datastore_find(chan, &detect_datastore, NULL))) {
		return 0;
	}

	di = datastore->data;

	if (!frame || frame->frametype != AST_FRAME_VOICE) {
		return 0;
	}

	if (!(direction == AST_AUDIOHOOK_DIRECTION_READ ? &di->rx : &di->tx)) {
		return 0;
	}

	/* So, this is a bit weird. In tests where relax is enabled and
		silence is played instead of waiting (so probably the latter, mostly likely),
		RX hits can be interpreted as TX hits (echo????). Bailing out
		early if we don't care about one direction avoids this bug */
	if (direction == AST_AUDIOHOOK_DIRECTION_READ && !di->rx) {
		return 0;
	}
	if (direction != AST_AUDIOHOOK_DIRECTION_READ && !di->tx) {
		return 0;
	}

	/* ast_dsp_process may free the frame and return a new one */
	frame = ast_frdup(frame);
	frame = ast_dsp_process(chan, di->dsp, frame);
	if (frame->frametype == AST_FRAME_DTMF) {
		char result = frame->subclass.integer;
		if (result == '$') {
			if (direction == AST_AUDIOHOOK_DIRECTION_READ) {
				di->rxcount = di->rxcount + 1;
				now = di->rxcount;
			} else {
				di->txcount = di->txcount + 1;
				now = di->txcount;
			}
			ast_debug(1, "COIN_DETECT just got a hit (#%d in %s direction, waiting for %d total)\n", now, direction == AST_AUDIOHOOK_DIRECTION_READ ? "RX" : "TX", di->hitsrequired);
			if (now >= di->hitsrequired) {
				if (di->delay > 0) {
					di->delaytimer = ast_tvnow();
					ast_debug(1, "Deposit threshold met, waiting for %d ms before we return\n", ast_remaining_ms(di->delaytimer, di->delay));
				} else {
					success = 1;
				}
				di->actionflag = 1;
			}
			di->debounce = 0;
			di->debouncedhits++;
		} else {
			ast_debug(1, "Ignoring DTMF '%c'\n", result);
		}
	} else if (di->delay > 0) {
		now = (direction == AST_AUDIOHOOK_DIRECTION_READ) ? di->rxcount : di->txcount;
		if (now >= di->hitsrequired && di->actionflag) { /* requirement has already been met, but how about the delay? */
			/* delaytimer is guaranteed to be non-NULL here */
			int remaining_delay = ast_remaining_ms(di->delaytimer, di->delay);
			if (remaining_delay <= 0) {
				ast_debug(1, "Post-match delay of %d (without additional deposits) has been exceeded\n", di->delay);
				success = 1;
			}
		}
	}
	if (di->debounce >= 0) {
		di->debounce++;
	}
	if (di->debounce > 10) { /* this is enough to debounce a single coin, e.g. a dime will show up as one 10c deposit, rather than two 5c deposits */
		now = (direction == AST_AUDIOHOOK_DIRECTION_READ) ? di->rxcount : di->txcount;
		ast_verb(3, "%d cents just deposited (%d total so far)\n", 5 * di->debouncedhits, 5 * now);
		di->debouncedhits = 0;
		di->debounce = -1;
	}
	if (success && di->actionflag) {
		now = (direction == AST_AUDIOHOOK_DIRECTION_READ) ? di->rxcount : di->txcount;
		ast_verb(3, "%d total cents deposited\n", 5 * now);
		if (di->rxcount >= di->hitsrequired && di->gotorx) {
			ast_debug(1, "Redirecting channel to %s\n", di->gotorx);
			ast_async_parseable_goto(chan, di->gotorx);
		} else if (di->txcount >= di->hitsrequired && di->gototx) {
			ast_debug(1, "Redirecting channel to %s\n", di->gototx);
			ast_async_parseable_goto(chan, di->gototx);
		} else {
			ast_log(LOG_WARNING, "Reached threshold, but missing goto location (shouldn't happen, if you see this, this is a bug...)\n"); /* should never happen */
		}
		di->actionflag = 0; /* prevent redirecting the channel multiple times on the same success */
	}
	/* this could be the duplicated frame or a new one, doesn't matter */
	ast_frfree(frame);
	return 0;
}

static int detect_write(struct ast_channel *chan, const char *cmd, char *data, const char *value)
{
	char *parse;
	struct ast_datastore *datastore = NULL;
	struct detect_information *di = NULL;
	struct ast_flags flags = { 0 };
	char *opt_args[OPT_ARG_ARRAY_SIZE];
	struct ast_dsp *dsp;
	double delayf = 0;
	int hitsrequired = 1, delay = 0;
	int features = 0;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(options);
	);

	if (!chan) {
		ast_log(LOG_WARNING, "No channel was provided to %s function.\n", cmd);
		return -1;
	}
	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(td_opts, &flags, opt_args, args.options);
	}
	if (ast_test_flag(&flags, OPT_END_DETECTOR)) {
		return remove_detect(chan);
	}
	if (ast_test_flag(&flags, OPT_HITS_REQ) && !ast_strlen_zero(opt_args[OPT_ARG_HITS_REQ])) {
		if ((ast_str_to_int(opt_args[OPT_ARG_HITS_REQ], &hitsrequired) || hitsrequired < 1)) {
			ast_log(LOG_WARNING, "Invalid minimum deposit: %s\n", opt_args[OPT_ARG_HITS_REQ]);
			return -1;
		}
		/* Each beep is 5 cents. If amount required doesn't evenly divide by 5, ALWAYS round up (ceiling) */
		hitsrequired = (hitsrequired % 5 == 0) ? hitsrequired / 5 : (hitsrequired / 5) + 1;
	}
	if (ast_test_flag(&flags, OPT_DELAY) && !ast_strlen_zero(opt_args[OPT_ARG_DELAY])) {
		if (!ast_strlen_zero(opt_args[OPT_ARG_DELAY]) && (sscanf(opt_args[OPT_ARG_DELAY], "%30lf", &delayf) != 1 || delayf < 0)) {
			ast_log(LOG_WARNING, "Invalid post-match delay: %s\n", opt_args[OPT_ARG_DELAY]);
			return -1;
		}
		delay = 1000 * delayf;
	}

	ast_channel_lock(chan);
	if (!(datastore = ast_channel_datastore_find(chan, &detect_datastore, NULL))) {
		if (!(datastore = ast_datastore_alloc(&detect_datastore, NULL))) {
			ast_channel_unlock(chan);
			return 0;
		}
		if (!(di = ast_calloc(1, sizeof(*di)))) {
			ast_datastore_free(datastore);
			ast_channel_unlock(chan);
			return 0;
		}
		ast_audiohook_init(&di->audiohook, AST_AUDIOHOOK_TYPE_MANIPULATE, "Coin Denomination Tone Detector", AST_AUDIOHOOK_MANIPULATE_ALL_RATES);
		di->audiohook.manipulate_callback = detect_callback;
		if (!(dsp = ast_dsp_new())) {
			ast_datastore_free(datastore);
			ast_channel_unlock(chan);
			ast_log(LOG_WARNING, "Unable to allocate DSP!\n");
			return -1;
		}
		di->dsp = dsp;
		di->txcount = 0;
		di->rxcount = 0;
		datastore->data = di;
		ast_channel_datastore_add(chan, datastore);
		ast_audiohook_attach(chan, &di->audiohook);
	} else {
		di = datastore->data;
		dsp = di->dsp;
	}
	if (ast_test_flag(&flags, OPT_RELAX)) {
		features |= DSP_DIGITMODE_RELAXDTMF;
	}
	ast_dsp_set_features(di->dsp, DSP_FEATURE_DIGIT_DETECT);
	ast_dsp_set_digitmode(di->dsp, DSP_DIGITMODE_DTMF | features);
	di->gotorx = NULL;
	di->gototx = NULL;
	/* resolve gotos now, in case a full context,exten,pri wasn't specified */
	if (ast_test_flag(&flags, OPT_GOTO_RX) && !ast_strlen_zero(opt_args[OPT_ARG_GOTO_RX])) {
		di->gotorx = goto_parser(chan, opt_args[OPT_ARG_GOTO_RX]);
	}
	if (ast_test_flag(&flags, OPT_GOTO_TX) && !ast_strlen_zero(opt_args[OPT_ARG_GOTO_TX])) {
		di->gototx = goto_parser(chan, opt_args[OPT_ARG_GOTO_TX]);
	}
	di->hitsrequired = hitsrequired;
	di->delay = delay;
	di->debounce = -1;
	di->debouncedhits = 0;
	di->tx = 1;
	di->rx = 1;
	ast_debug(1, "Keeping our ears open for coin denomination tones, post-match delay %d ms, %s\n", delay, ast_test_flag(&flags, OPT_RELAX) ? "relaxed" : "unrelaxed");
	if (!ast_strlen_zero(args.options) && ast_test_flag(&flags, OPT_TX)) {
		di->tx = 1;
		di->rx = 0;
	}
	if (!ast_strlen_zero(args.options) && ast_test_flag(&flags, OPT_RX)) {
		di->rx = 1;
		di->tx = 0;
	}
	ast_channel_unlock(chan);

	return 0;
}

enum {
	OPT_APP_RELAX =  (1 << 0),
};

AST_APP_OPTIONS(wait_exec_options, BEGIN_OPTIONS
	AST_APP_OPTION('l', OPT_APP_RELAX),
END_OPTIONS);

static int wait_exec(struct ast_channel *chan, const char *data)
{
	char *appdata;
	struct ast_flags flags = {0};
	double timeoutf = 0, delayf = 0;
	int timeout = 0, times = 1, delay = 0;
	struct ast_frame *frame = NULL;
	struct ast_dsp *dsp;
	struct timeval start, delaytimer;
	int remaining_time = 0, hits = 0, features = 0;
	int debounce = -1, debouncedhits = 0;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(times);
		AST_APP_ARG(timeout);
		AST_APP_ARG(delay);
		AST_APP_ARG(options);
	);

	appdata = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, appdata);

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(wait_exec_options, &flags, NULL, args.options);
	}

	if (!ast_strlen_zero(args.timeout) && (sscanf(args.timeout, "%30lf", &timeoutf) != 1 || timeoutf < 0)) {
		ast_log(LOG_WARNING, "Invalid timeout: %s\n", args.timeout);
		goto error;
	}
	timeout = 1000 * timeoutf;
	if (!ast_strlen_zero(args.delay) && (sscanf(args.delay, "%30lf", &delayf) != 1 || delayf < 0)) {
		ast_log(LOG_WARNING, "Invalid post-match delay: %s\n", args.delay);
		goto error;
	}
	delay = 1000 * delayf;
	if (!ast_strlen_zero(args.times) && (ast_str_to_int(args.times, &times) || times < 1)) {
		ast_log(LOG_WARNING, "Invalid minimum deposit: %s\n", args.times);
		goto error;
	}
	/* Each beep is 5 cents. If amount required doesn't evenly divide by 5, ALWAYS round up (ceiling) */
	times = (times % 5 == 0) ? times / 5 : (times / 5) + 1;
	if (!(dsp = ast_dsp_new())) {
		ast_log(LOG_WARNING, "Unable to allocate DSP!\n");
		goto error;
	}
	if (ast_test_flag(&flags, OPT_APP_RELAX)) {
		features |= DSP_DIGITMODE_RELAXDTMF;
	}
	ast_dsp_set_features(dsp, DSP_FEATURE_DIGIT_DETECT);
	ast_dsp_set_digitmode(dsp, DSP_DIGITMODE_DTMF | features);
	pbx_builtin_setvar_helper(chan, "WAITFORDEPOSITAMOUNT", "0");
	ast_debug(1, "Waiting for coins, %d time(s), timeout %d ms, post-match delay %d ms\n", times, timeout, delay);
	start = ast_tvnow();
	do {
		if (hits < times && timeout > 0) {
			remaining_time = ast_remaining_ms(start, timeout);
			if (remaining_time <= 0) {
				pbx_builtin_setvar_helper(chan, "WAITFORDEPOSITSTATUS", "TIMEOUT");
				break;
			}
		}
		if (ast_waitfor(chan, 1000) > 0) {
			if (!(frame = ast_read(chan))) {
				ast_debug(1, "Channel '%s' did not return a frame; probably hung up.\n", ast_channel_name(chan));
				pbx_builtin_setvar_helper(chan, "WAITFORDEPOSITSTATUS", "HANGUP");
				break;
			} else if (frame->frametype == AST_FRAME_VOICE) {
				frame = ast_dsp_process(chan, dsp, frame);
				if (frame->frametype == AST_FRAME_DTMF) {
					char result = frame->subclass.integer;
					if (result == '$') {
						hits++;
						debouncedhits++;
						debounce = 0;
						ast_debug(1, "We just detected a coin (hit #%d)\n", hits);
						if (hits >= times) {
							pbx_builtin_setvar_helper(chan, "WAITFORDEPOSITSTATUS", "SUCCESS");
							if (delay > 0) { /* allow additional deposits, beyond the requirement, if desired */
								delaytimer = ast_tvnow();
								ast_debug(1, "Deposit threshold met, waiting for %d ms before we return\n", ast_remaining_ms(delaytimer, delay));
							} else {
								ast_frfree(frame);
								pbx_builtin_setvar_helper(chan, "WAITFORDEPOSITSTATUS", "SUCCESS");
								break;
							}
						}
					} else {
						ast_debug(1, "Ignoring DTMF '%c'\n", result);
					}
				} else if (hits >= times) { /* requirement has already been met, but how about the delay? */
					/* delaytimer is guaranteed to be non-NULL here. No need to check delay > 0, that's implicitly true if hits >= times. */
					int remaining_delay = ast_remaining_ms(delaytimer, delay);
					if (remaining_delay <= 0) {
						ast_debug(1, "Post-match delay of %d ms (without additional deposits) has been exceeded (%d)\n", delay, remaining_delay);
						break;
					}
				}
				if (debounce >= 0) {
					debounce++;
				}
				if (debounce > 10) { /* this is enough to debounce a single coin, e.g. a dime will show up as one 10c deposit, rather than two 5c deposits */
					ast_verb(3, "%d cents just deposited (%d total so far)\n", 5 * debouncedhits, 5 * hits);
					debouncedhits = 0;
					debounce = -1;
				}
			}
			ast_frfree(frame);
		} else {
			pbx_builtin_setvar_helper(chan, "WAITFORDEPOSITSTATUS", "HANGUP");
		}
	} while (timeout == 0 || remaining_time > 0);
	ast_dsp_free(dsp);

	if (hits > 0) { /* even if the threshold wasn't met, some amount could've been deposited */
		char amt[12];
		int cents = 5 * hits; /* each beep is 5 cents */
		snprintf(amt, 12, "%d", cents);
		pbx_builtin_setvar_helper(chan, "WAITFORDEPOSITAMOUNT", amt);
		ast_verb(3, "%d total cents deposited\n", cents);
	} else {
		ast_verb(3, "No coins deposited\n");
	}

	return 0;
error:
	pbx_builtin_setvar_helper(chan, "WAITFORDEPOSITSTATUS", "ERROR");
	return -1;
}

enum {
	OPT_APP_MULTIWINK =	(1 << 0),
	OPT_APP_INBAND =	(1 << 1),
};

AST_APP_OPTIONS(disposition_exec_options, BEGIN_OPTIONS
	AST_APP_OPTION('i', OPT_APP_INBAND),
	AST_APP_OPTION('m', OPT_APP_MULTIWINK),
END_OPTIONS);

static int do_winks(struct ast_channel *chan, int winks)
{
	int i;

	/* SR-2275-3 6.17.3.2.1 */
	for (i = 0; i < winks; i++) {
		/* 70-130ms on-hook and 95-150ms off-hook */
		ast_indicate(chan, AST_CONTROL_WINK); /* out of band wink */
		if (ast_safe_sleep(chan, 380)) {
			return -1;
		}
	}
	if (ast_safe_sleep(chan, winks > 2 ? 300 : 120)) {
		return -1;
	}
	return 0;
}

static int disposition_exec(struct ast_channel *chan, const char *data)
{
	/* Notes on the Network (1980), Bellcore specs, PhreakNet System Practice 201-101-010 */
	int res = 0;
	char *appdata;
	struct ast_flags flags = {0};
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(sig);
		AST_APP_ARG(options);
	);

	appdata = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, appdata);

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(disposition_exec_options, &flags, NULL, args.options);
	}

	if (ast_test_flag(&flags, OPT_APP_MULTIWINK)) {
		/* use the older multiwink signaling instead */
		if (!strcasecmp(args.sig, "return")) {
			res = do_winks(chan, 4);
		} else if (!strcasecmp(args.sig, "collect")) {
			res = do_winks(chan, 3);
		} else if (!strcasecmp(args.sig, "ringback")) { /* MF "ST3" */
			res = do_winks(chan, 5);
		} else if (!strcasecmp(args.sig, "released")) {
			res = do_winks(chan, 1);
		} else if (!strcasecmp(args.sig, "attached")) {
			res = do_winks(chan, 2);
		} else {
			ast_log(LOG_WARNING, "Unknown disposition: %s\n", args.sig);
			res = -1;
		}
	} else {
		/* coin control is forward primed with a 2600 Hz wink for 325-425ms (300-450 according to Bellcore), followed by 850ms of silence */
		ast_indicate(chan, AST_CONTROL_WINK); /* out of band wink */

		if (ast_test_flag(&flags, OPT_APP_INBAND)) {
			if (ast_playtones_start(chan, 0, "!2600/0.425|!0/0.85", 0)) { /* in band wink */
				return -1;
			}
		}
		if (ast_safe_sleep(chan, 1275)) {
			return -1;
		}
		if (ast_test_flag(&flags, OPT_APP_INBAND)) {
			ast_playtones_stop(chan);
		}

		/* By this point, the Class 5 office should have detected the wink and
			unbridged audio so that the caller will not hear the MFs being sent. */

		/* Expanded In-Band Signaling for TSPS remote coin control */
		if (!strcasecmp(args.sig, "return")) { /* MF "KP" */
			res = ast_playtones_start(chan, 0, "1100+1700", 0);
		} else if (!strcasecmp(args.sig, "collect")) { /* MF "2", also another operator released */
			res = ast_playtones_start(chan, 0, "700+1100", 0);
		} else if (!strcasecmp(args.sig, "ringback")) { /* MF "ST3P" and Code 11 */
			/* allows an operator to ring back a payphone, e.g. for overtime deposits. See Notes on the Network, 1980, Sec. 5 */
			res = ast_playtones_start(chan, 0, "700+1700", 0);
		} else if (!strcasecmp(args.sig, "released")) { /* MF "8" */
			res = ast_playtones_start(chan, 0, "900+1500", 0);
		} else if (!strcasecmp(args.sig, "attached")) { /* MF "0" */
			res = ast_playtones_start(chan, 0, "1300+1500", 0);
		} else if (!strcasecmp(args.sig, "collectreleased")) { /* MF "ST" */
			res = ast_playtones_start(chan, 0, "1500+1700", 0);
		} else {
			ast_log(LOG_WARNING, "Unknown disposition: %s\n", args.sig);
			res = -1;
		}
		if (res) {
			return -1;
		}
		if (ast_safe_sleep(chan, 700)) { /* 350ms to 800ms / 480ms to 700ms Bellcore spec */
			return -1;
		}
		ast_playtones_stop(chan);
	}

	return res;
}

static char *waitapp = "WaitForDeposit";
static char *dispositionapp = "CoinDisposition";

static struct ast_custom_function detect_function = {
	.name = "COIN_DETECT",
	.read = detect_read,
	.write = detect_write,
};

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(waitapp);
	res |= ast_unregister_application(dispositionapp);
	res |= ast_custom_function_unregister(&detect_function);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_register_application_xml(waitapp, wait_exec);
	res |= ast_register_application_xml(dispositionapp, disposition_exec);
	res |= ast_custom_function_register(&detect_function);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Coin detection module");
