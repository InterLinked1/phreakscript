/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2022, Naveen Albert
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
 * \brief George, the Interactive Answering Machine
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*! \li \ref app_george.c uses the configuration file \ref app_george.conf
 * \addtogroup configuration_file Configuration Files
 */

/*!
 * \page app_george.conf app_george.conf
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/channel.h"
#include "asterisk/app.h"
#include "asterisk/module.h"
#include "asterisk/frame.h"
#include "asterisk/format_cache.h"
#include "asterisk/dsp.h"
#include "asterisk/callerid.h"
#include "asterisk/conversions.h"

/*** DOCUMENTATION
	<application name="George" language="en_US">
		<synopsis>
			George, the Interactive answering machine
		</synopsis>
		<syntax>
			<parameter name="mailbox">
				<para>Voicemail mailbox (mbox[@context]) into which to dump messages.</para>
			</parameter>
			<parameter name="waitsec">
				<para>Number of seconds to wait before answering</para>
				<para>Default is 0.</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="c">
						<para>Support Call Waiting.</para>
						<para>This should only be done for FXS lines with Call Waiting enabled.</para>
						<para>Note that all calls during a single session will be combined into
						a single recording.</para>
					</option>
					<option name="i">
						<para>Support Call Waiting Caller ID.</para>
						<para>Support received CWCID spill if the FXS line has Call Waiting Caller ID available.</para>
						<para>Note that nothing is currently done with this information. The first caller's information
						is what is used as the calling number for the entire recording session.</para>
						<note><para>This functionality does not currently work and should not be used.</para></note>
					</option>
					<option name="j">
						<para>Allow jumping out to the <literal>*</literal> extension if it exists in the current context.</para>
						<para>The user can press * at any point, for example to manage the mailbox.</para>
					</option>
					<option name="t">
						<para>Enable toll saver.</para>
						<para>By default, the call will be answered in half as much time if messages exist.</para>
						<para>You can specify a time to use for toll saver explicitly.</para>
						<para>If Toll Saver is handled elsewhere, you should not use this option.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>This is George, the Interactive Answering Machine, a virtual telephone answering device,
			i.e. "soft" answering machine implementation.</para>
			<para>It is based off Evan Doorbell's "George, the Interactive Answering Machine".</para>
			<para>It is intended to be used to answer an FXO channel that may be connected
			to a CO line or other FXS channel.</para>
			<para>If Call Waiting is enabled, this application will detect the Call Waiting Subscriber Alert Signal
			on the channel and automatically try to handle the call waiting. This allows the answering machine
			to simultaneously service two incoming callers. A single recorded message will be used
			for the original call and any call waitings that are handled.</para>
			<para>This application does not automatically adjust the gain or receive volume on the channel.
			You may wish to do this to make detection of talking easier or harder.</para>
			<para>To use this application, you must provide all of the audio prompts in the
			<literal>app_george.conf</literal> config file.</para>
		</description>
	</application>
	<configInfo name="app_george" language="en_US">
		<synopsis>George, the Interactive Answering Machine</synopsis>
		<configFile name="app_george.conf">
			<configObject name="general">
				<synopsis>Audio files to use for George. All audio prompts must be specified.</synopsis>
				<configOption name="hello">
					<synopsis>Hello?</synopsis>
				</configOption>
				<configOption name="whos_calling">
					<synopsis>This is the answering machine, who's calling please?</synopsis>
				</configOption>
				<configOption name="recording">
					<synopsis>This is the answering machine. I'm recording your message, go ahead.</synopsis>
				</configOption>
				<configOption name="get_number">
					<synopsis>All right, what's your number?</synopsis>
				</configOption>
				<configOption name="go_ahead">
					<synopsis>Go ahead.</synopsis>
				</configOption>
				<configOption name="thanks_anything_else">
					<synopsis>Thanks, anything else?</synopsis>
				</configOption>
				<configOption name="okay_thanks">
					<synopsis>Okay, thanks.</synopsis>
				</configOption>
				<configOption name="bye">
					<synopsis>Bye.</synopsis>
				</configOption>
				<configOption name="cw_prompt">
					<synopsis>I have another call, can you hold please?</synopsis>
				</configOption>
				<configOption name="prompt_anything_else">
					<synopsis>Anything else?</synopsis>
				</configOption>
				<configOption name="connect_sound">
					<synopsis>Answering sound</synopsis>
				</configOption>
				<configOption name="hangup_sound">
					<synopsis>Hangup sound</synopsis>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
 ***/

enum tad_option_flags {
	OPT_CALLWAIT = (1 << 0),
	OPT_CWCID = (1 << 1),
	OPT_TOLLSAVER = (1 << 2),
	OPT_JUMP = (1 << 3),
};

enum {
	OPT_ARG_TOLLSAVER,
	/* note: this entry _MUST_ be the last one in the enum */
	OPT_ARG_ARRAY_SIZE,
};

AST_APP_OPTIONS(tad_option_flags, {
	AST_APP_OPTION('c', OPT_CALLWAIT),
	AST_APP_OPTION('i', OPT_CWCID),
	AST_APP_OPTION('j', OPT_JUMP),
	AST_APP_OPTION_ARG('t', OPT_TOLLSAVER, OPT_ARG_TOLLSAVER),
});

static const char *app = "George";

#define CONFIG_FILE "app_george.conf"

/* Enough time that we think the caller has stopped talking, and not merely paused. */
#define SILENCE_TIME 1500
#define SILENCE_TIME_LONG 3500

struct call_state {
	int our_turn;
	int lastfinish;
	int prompt_number;
	int talkthreshold;
	unsigned int active:1;
	unsigned int disconnect:1;
	unsigned int longwindedmessage:1;
};

struct call_info {
	struct call_state calls[2];
	int active_call;
	unsigned int sas_pending:1;
};

#define NUM_PROMPTS 8

static char *promptfiles[NUM_PROMPTS] = {
	NULL,	/* Hello? */
	NULL,	/* This is the answering machine, who's calling please? */
	NULL,	/* This is the answering machine. I'm recording your message, go ahead. */
	NULL,	/* All right, what's your number? */
	NULL,	/* Go ahead. */
	NULL,	/* Thanks, anything else? */
	NULL,	/* Okay, thanks. */
	NULL,	/* Bye. */
};
static char *cw_prompt = NULL;
static char *prompt_anything_else = NULL;
static char *connect_sound = NULL;
static char *hangup_sound = NULL;

#define LOAD_GENERAL_STR(field) { \
	const char *var = NULL; \
	if (!ast_strlen_zero(var = ast_variable_retrieve(cfg, "general", #field))) { \
		field = ast_strdup(var); \
	} \
	if (ast_strlen_zero(var)) { \
		ast_log(LOG_NOTICE, "%s was not specified\n", #field); \
		ast_config_destroy(cfg); \
		return -1; \
	} else if (!ast_fileexists((const char*) var, NULL, NULL)) { \
		ast_log(LOG_WARNING, "%s file does not exist: %s\n", #field, var); \
		ast_config_destroy(cfg); \
		return -1; \
	} \
}

#define LOAD_INDEX_PROMPT(index, field) { \
	const char *var = NULL; \
	if (!ast_strlen_zero(var = ast_variable_retrieve(cfg, "general", #field))) { \
		promptfiles[index] = ast_strdup(var); \
	} \
	if (ast_strlen_zero(var)) { \
		ast_log(LOG_NOTICE, "%s was not specified\n", #field); \
		ast_config_destroy(cfg); \
		return -1; \
	} else if (!ast_fileexists((const char*) var, NULL, NULL)) { \
		ast_log(LOG_WARNING, "%s file does not exist: %s\n", #field, var); \
		ast_config_destroy(cfg); \
		return -1; \
	} \
}

/*! \note This module doesn't currently support reloads, so we don't need to worry about locking here.
 * In theory that could be added (and locked would need to be added), but now that we can refresh
 * modules easily, I'm lazy and don't currently feel it's worth the effort for a module that
 * few other people will use besides me... */
static int load_config(void)
{
	struct ast_config *cfg;
	struct ast_flags config_flags = { 0 };

	if (!(cfg = ast_config_load(CONFIG_FILE, config_flags))) {
		ast_log(LOG_WARNING, "Config file %s not found, declining to load\n", CONFIG_FILE);
		return -1;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Config file %s is in an invalid format. Aborting.\n", CONFIG_FILE);
		return -1;
	}

	/* Only care about the general section */
	/* If anything fails (file not specified or not found), abort, since the application just won't work right. */
	LOAD_INDEX_PROMPT(0, hello);
	LOAD_INDEX_PROMPT(1, whos_calling);
	LOAD_INDEX_PROMPT(2, recording);
	LOAD_INDEX_PROMPT(3, get_number);
	LOAD_INDEX_PROMPT(4, go_ahead);
	LOAD_INDEX_PROMPT(5, thanks_anything_else);
	LOAD_INDEX_PROMPT(6, okay_thanks);
	LOAD_INDEX_PROMPT(7, bye);
	LOAD_GENERAL_STR(cw_prompt);
	LOAD_GENERAL_STR(prompt_anything_else);
	LOAD_GENERAL_STR(connect_sound);
	LOAD_GENERAL_STR(hangup_sound);

	ast_config_destroy(cfg);
	return 0;
}

#define free_if_exists(m) if (m) { ast_free(m); m = NULL; }

static void unload_config(void)
{
	int i;

	free_if_exists(cw_prompt);
	free_if_exists(prompt_anything_else);
	free_if_exists(connect_sound);
	free_if_exists(hangup_sound);
	for (i = 0; i < NUM_PROMPTS; i++) {
		free_if_exists(promptfiles[i]);
	}
}

#define RESPONSE_FRAMES_REQUIRED_HIGH 40
#define RESPONSE_FRAMES_REQUIRED 10
#define RESPONSE_FRAMES_REQUIRED_BYE 4
#define RESPONSE_FRAMES_REQUIRED_CW 8

/* Assume 50 ms per frame. */
#define FRAMES_PER_SECOND 50
#define MAXIMUM_NORMAL_INTRO_RESPONSE_SECS 10
#define MINIMUM_PHONE_NUMBER_START_FRAMES 12

#define skip_prompt() { \
	cs->prompt_number += 1; \
	ast_debug(3, "Skipping prompt %d\n", cs->prompt_number); \
	cs->prompt_number += 1; \
}

#define OTHER_CALL(x) (x == 0 ? 1 : 0)

static void update_prompt(struct call_state *cs)
{
	int lastfinish = cs->lastfinish;
	int now = time(NULL);

	if (lastfinish) {
		ast_debug(4, "Caller's response was %d seconds long\n", (now - lastfinish));
	}

	if (cs->prompt_number == 0 && cs->longwindedmessage) {
		/* If the caller initially jabbers on for a while, use prompt 2 next instead of prompt 1. */
		ast_debug(1, "Caller's introduction was %d seconds long, somebody's a long winded rambler\n", (now - lastfinish));
		skip_prompt();
	} else if (cs->prompt_number >= 1 && cs->prompt_number <= 3) {
		/* "Who's calling?" or "I'm recording" but never both. Same for "What's your number?" and "Go ahead" */
		skip_prompt();
	} else {
		cs->prompt_number += 1; /* Just advance 1. */
	}

	if (cs->prompt_number == NUM_PROMPTS - 1) {
		/* If we're going to do the last one but caller took a while last time, repeat a previous prompt. */
		if ((now - lastfinish) > 12) {
			cs->prompt_number -= 2;
			ast_debug(3, "Repeating prompt %d\n", cs->prompt_number);
		} else if ((now - lastfinish) > 8) {
			cs->prompt_number -= 1;
			ast_debug(3, "Repeating prompt %d\n", cs->prompt_number);
		}
	}

	cs->lastfinish = now;

	if (cs->prompt_number >= NUM_PROMPTS - 2) {
		cs->talkthreshold = RESPONSE_FRAMES_REQUIRED_BYE;
		ast_debug(3, "Reducing talk threshold required to %d\n", cs->talkthreshold);
	} else if (cs->prompt_number > 4 && cs->talkthreshold != RESPONSE_FRAMES_REQUIRED) {
		/* If we were using a longer threshold temporarily, crank it back down now. */
		cs->talkthreshold = RESPONSE_FRAMES_REQUIRED;
		ast_debug(3, "Reducing talk threshold required to %d\n", cs->talkthreshold);
	} else if (cs->prompt_number == 3) {
		/* Initially, use a long threshold for getting the number. We'll reduce it after we get sufficient noise. */
		cs->talkthreshold = RESPONSE_FRAMES_REQUIRED_HIGH;
	}
}

static int start_and_swap(struct ast_channel *chan, struct call_info *cinfo, const char *file)
{
	cinfo->calls[cinfo->active_call].our_turn = cinfo->calls[cinfo->active_call].our_turn ? 0 : 1; /* Swap! */
	ast_channel_set_flag(chan, AST_FLAG_END_DTMF_ONLY);
	if (ast_streamfile(chan, file, ast_channel_language(chan))) {
		ast_log(LOG_WARNING, "Failed to stream file: %s\n", file);
		return -1;
	}
	return 0;
}

static int do_nag(struct ast_channel *chan, struct call_info *cinfo, int hangup)
{
	int fileno;

	if (hangup) {
		fileno = NUM_PROMPTS - 1;
	} else if (cinfo->calls[cinfo->active_call].prompt_number <= 3) {
		fileno = 0;
	} else {
		return start_and_swap(chan, cinfo, prompt_anything_else);
	}

	return start_and_swap(chan, cinfo, promptfiles[fileno]);
}

static int do_cwprompt(struct ast_channel *chan, struct call_info *cinfo)
{
	return start_and_swap(chan, cinfo, cw_prompt);
}

static int swap_turns(struct ast_channel *chan, struct call_info *cinfo)
{
	int res = 0;

	cinfo->calls[cinfo->active_call].our_turn = cinfo->calls[cinfo->active_call].our_turn ? 0 : 1; /* Swap! */
	if (cinfo->calls[cinfo->active_call].our_turn) {
		const char *file;

		update_prompt(&cinfo->calls[cinfo->active_call]);

		file = promptfiles[cinfo->calls[cinfo->active_call].prompt_number];
		if (cinfo->sas_pending) {
			ast_debug(2, "A call waiting is pending on %s\n", ast_channel_name(chan));
			/*! \todo add audio file */
		}
		if (ast_strlen_zero(file)) {
			ast_log(LOG_WARNING, "Prompt %d is empty or not defined\n", cinfo->calls[cinfo->active_call].prompt_number);
			return -1;
		}
		ast_debug(1, "It is now our turn, playing prompt %d: %s to caller %d\n", cinfo->calls[cinfo->active_call].prompt_number, file, cinfo->active_call);
		ast_channel_set_flag(chan, AST_FLAG_END_DTMF_ONLY);
		res = ast_streamfile(chan, file, ast_channel_language(chan));
		if (res) {
			ast_log(LOG_WARNING, "Failed to stream file: %s\n", file);
		}
	} else {
		ast_debug(3, "Playback finished for prompt %d (caller %d)\n", cinfo->calls[cinfo->active_call].prompt_number, cinfo->active_call);
		ast_channel_clear_flag(chan, AST_FLAG_END_DTMF_ONLY);
		if (cinfo->calls[cinfo->active_call].prompt_number >= NUM_PROMPTS - 1) {
			/* That was the last prompt. */
			ast_debug(1, "Nothing left to do, hanging up...\n");
			cinfo->calls[cinfo->active_call].disconnect = 1;
			cinfo->calls[cinfo->active_call].our_turn = 1; /* Make it our turn so we process the disconnect immediately. */
			return 0; /* Hang up, we're done. */
		}
		ast_debug(1, "It is now caller %d's turn to respond\n", cinfo->active_call);
	}

	return res;
}

static int switch_calls(struct ast_channel *chan, struct call_info *cinfo)
{
	struct ast_frame f = { AST_FRAME_CONTROL, { AST_CONTROL_FLASH, } };
	int res = ast_write(chan, &f);
	/* XXX This always seems to emit this for the FXO channel we flash (should be fixed in sig_analog):
	 * WARNING[161258][C-0000011f]: sig_analog.c:3196 __analog_handle_event: Ring/Off-hook in strange state 6 */
	if (res) {
		ast_log(LOG_WARNING, "Failed to queue hook flash\n");
		return -1;
	}
	cinfo->active_call = OTHER_CALL(cinfo->active_call); /* Swap which call is indicated as the active one. */
	/* If we're switching to this call, we meant to, so it's definitely active. */
	cinfo->calls[cinfo->active_call].active = 1;
	ast_verb(4, "Switching from caller %d to %d\n", OTHER_CALL(cinfo->active_call), cinfo->active_call);
	cinfo->calls[cinfo->active_call].our_turn = 0;
	/* Wait half a second whenever we hook flash, to give the other party time to adjust to us being there,
	 * and so our first word or two doesn't get cut off. */
	if (ast_safe_sleep(chan, 500)) {
		return -1;
	}
	return swap_turns(chan, cinfo);
}

static int ack_cw(struct ast_channel *chan)
{
	ast_debug(4, "Sending DTMF acknowledgment to %s\n", ast_channel_name(chan));
	if (ast_senddigit(chan, 'D', 200)) {
		ast_log(LOG_WARNING, "Failed to send DTMF acknowledgment to %s\n", ast_channel_name(chan));
		return -1;
	}
	ast_debug(3, "Sent DTMF acknowledgment to %s\n", ast_channel_name(chan));
	return 0;
}

static void reset_call(struct call_state *cs)
{
	cs->our_turn = 0;
	cs->disconnect = 0;
	cs->talkthreshold = RESPONSE_FRAMES_REQUIRED;
	cs->prompt_number = -1; /* So when we swap_turns initially, we start at 0. */
}

static int disconnect_current(struct ast_channel *chan, struct call_info *cinfo)
{
	int other_call;
	/* Done handling this call. */
	ast_debug(3, "Done with caller %d, disconnecting\n", cinfo->active_call);

	/* Play hangup sound to caller. */
	if (!ast_streamfile(chan, hangup_sound, ast_channel_language(chan))) {
		ast_waitstream(chan, NULL);
	}

	reset_call(&cinfo->calls[cinfo->active_call]);
	cinfo->calls[cinfo->active_call].active = 0;
	other_call = OTHER_CALL(cinfo->active_call);
	if (!cinfo->calls[other_call].active) {
		/* If we don't have any callers left, we're done. */
		ast_debug(1, "No active calls remaining, going on-hook\n");
		/* We can terminate the loop and go on-hook. */
		return -1;
	}
	ast_verb(4, "Caller %d is still on hold, swapping\n", other_call);

	/* XXX Ideally we would go on hook and let the other call ring back, to forcibly disconnect the current caller.
	 * That is probably the better way to do it, as flashing leaves the other guy in limbo, and hopefully he hangs up?
	 * If we do that, we could actually just hang up then on the FXO port. The actual line will ring back with the
	 * other call, we'll detect a ring and answer the FXO channel, and just begin executing this application all over
	 * again. So we could/should actually just hang up, without any regard to whether this another call or not...
	 * Except that assumes we haven't started the other call, and if we have, we state for that call we don't want
	 * to lose, we don't want to stop the recording and dispatch it, so for now we flash, and if wanted to do what
	 * I said above we would need to actually do DAHDI ON HOOK and DAHDI OFF HOOK manually.
	 *
	 * TL;DR We can't actually force disconnect a caller, and if a caller we "hang up" on doesn't actually hang up,
	 * that will hold us up and when we disconnect, we'll just end up swapping back to them later.
	 */
	return switch_calls(chan, cinfo);
}

#define CWCID_DEBUG
#ifdef CWCID_DEBUG
#include "asterisk/fskmodem.h"
struct callerid_state {
	fsk_data fskd;
	char rawdata[256];
	short oldstuff[160];
	int oldlen;
	int pos;
	int type;
	int cksum;
	char name[64];
	char number[64];
	int flags;
	int sawflag;
	int len;

	int skipflag;
	unsigned short crc;
};
#endif

static int run_tad(struct ast_channel *chan, int callwait, int receive_cwcid, int allow_jump)
{
	int dres, res = -1;
	struct ast_dsp *dsp = NULL;
	struct ast_frame *frame = NULL;

	int totalsilence;
	int got_response = 0;
	int noisy_frames = 0;
	int silent_frames = 0;
	struct call_info cinfo;
	int no_response_count = 0;
	int sas_asked_about = 0;
	int awaiting_cwcid = 0;

	int silent_too_long;
	int elapsed_frames = 0;

	struct callerid_state *cwcid = NULL;

	if (!(dsp = ast_dsp_new())) {
		ast_log(LOG_WARNING, "Unable to allocate DSP!\n");
		return -1;
	}
	ast_dsp_set_threshold(dsp, ast_dsp_get_threshold_from_settings(THRESHOLD_SILENCE));

	/* Keep our ears open for Call Waiting SAS, if needed. */
	if (callwait) {
		ast_dsp_set_features(dsp, DSP_FEATURE_FREQ_DETECT);
		ast_dsp_set_freqmode(dsp, 440, 275, 16, 0); /* We need 300ms of 440 Hz (high tone), so settle for 275ms of it. */
	}

	memset(&cinfo, 0, sizeof(cinfo));
	reset_call(&cinfo.calls[0]);
	reset_call(&cinfo.calls[1]);

	cinfo.active_call = 0;
	cinfo.calls[0].active = 1;
	cinfo.calls[1].active = 0;
	cinfo.sas_pending = 0;

	ast_stopstream(chan);

	/* Play answer sound. */
	if (!ast_streamfile(chan, connect_sound, ast_channel_language(chan))) {
		ast_waitstream(chan, NULL);
	}

	/* Our turn */
	if (swap_turns(chan, &cinfo)) {
		goto cleanup;
	}

	/* Main loop that handles the whole answering machine.
	 * Pretty much everything gets done in here directly, meaning we do all the channel servicing,
	 * as opposed to using functions that service the channel.
	 * For example, we don't use ast_waitstream, we play one audio frame each loop as needed.
	 */
	for (;;) {
		if (ast_waitfor(chan, 1000) <= 0) {
			ast_debug(1, "Channel %s hung up?\n", ast_channel_name(chan)); /* No audio after 1 second, probably gone. */
			break;
		}
		frame = ast_read(chan);
		if (!frame) {
			ast_debug(1, "Channel '%s' did not return a frame; probably hung up.\n", ast_channel_name(chan));
			break;
		}

		/* Support escaping to management interface. */
		if (frame->frametype == AST_FRAME_DTMF && frame->subclass.integer == '*') {
			/* Escape to management interface. */
			if (!allow_jump) {
				ast_debug(1, "Jumping to * extension is disabled\n");
			} else if (ast_exists_extension(chan, ast_channel_context(chan), "*", 1, NULL)) {
				ast_explicit_goto(chan, NULL, "*", 1);
				res = 0;
				ast_verb(4, "Escaping to management interface\n");
				ast_frfree(frame);
				break;
			} else {
				ast_debug(1, "%s,%s,%d does not exist, not breaking out\n", ast_channel_context(chan), "*", 1);
			}
		}

		/* Always keep an ear out for Call Waiting tone, if needed. */
		if (callwait && frame->frametype == AST_FRAME_VOICE) {
			frame = ast_dsp_process(chan, dsp, frame);
			if (frame->frametype == AST_FRAME_DTMF && frame->subclass.integer == 'q') {
				ast_debug(4, "Call Waiting tone detected\n");
				/* Note that this will also count as talking when we run silence detection,
				 * but that works out since we'll want to interrupt the caller anyways,
				 * even if s/he never said anything. */
				if (!cinfo.sas_pending) {
					ast_verb(4, "Call waiting just arrived on %s\n", ast_channel_name(chan));
					cinfo.sas_pending = 1; /* Just detected SAS (Call Waiting Tone) */
					sas_asked_about = 0;
					cinfo.calls[cinfo.active_call].talkthreshold = RESPONSE_FRAMES_REQUIRED_CW;
					if (receive_cwcid) {
						awaiting_cwcid = 2.4 * FRAMES_PER_SECOND; /* We're expecting a CWCID FSK spill soon, within this number of frames, max. */
						if (cwcid) {
							callerid_free(cwcid);
							cwcid = NULL;
						}
						/* We can't just send the ACK immediately here because we only just detected the SAS, no CAS yet.
						 * Also, yes, I know in the real Caller ID protocol, we should only be listening for the CAS
						 * in the first place, for CWCID, but we need the SAS to detect a call waiting anyways, and
						 * Asterisk as of now doesn't support generic multifrequency tone detection so we can't actually detect the CAS.
						 * Just assume it'll be present not long after the SAS. */
					}
				} else {
					ast_verb(4, "Call waiting still pending on %s\n", ast_channel_name(chan));
				}
			}
		}

		/* Decode Call Waiting Caller ID */
		if (awaiting_cwcid && frame->frametype == AST_FRAME_VOICE) {
			--awaiting_cwcid;
			if (cwcid) {
				int cres = callerid_feed(cwcid, frame->data.ptr, frame->datalen, ast_format_ulaw);
#ifdef CWCID_DEBUG
				/*! \todo BUGBUG We do successfully send the ACK and get the FSK spill if available,
				 * but we're not currently using callerid_feed properly, and don't get anything we can decode.
				 * Once this works then CWCID_DEBUG should be removed.
				 */
				ast_debug(4, "cres: %d, pos: %d, len: %d, oldlen: %d, sawflag: %d, skipflag: %d\n", cres, cwcid->pos, cwcid->len, cwcid->oldlen, cwcid->sawflag, cwcid->skipflag);
#endif
				if (cres < 0) {
					ast_log(LOG_WARNING, "Failed to decode Caller ID\n");
				}
				if (cres == 1) {
					char numbuf[300], namebuf[300];
					char *name, *num;
					int flags;
					callerid_get(cwcid, &name, &num, &flags);
					ast_copy_string(namebuf, S_OR(name, ""), 16); /* Max CNAM length is 15 anyways... */
					ast_copy_string(numbuf, S_OR(num, ""), 16);
					ast_debug(1, "Number: %s, Name: %s\n", numbuf, namebuf);
					if (ast_strlen_zero(num)) {
						ast_log(LOG_WARNING, "Empty Caller ID decoded?\n"); /* Possible checksum failure */
					}
					awaiting_cwcid = 0; /* Done decoding, whether we succeeded or not. */
				}
				if (!awaiting_cwcid) {
					if (!cres) {
						ast_log(LOG_WARNING, "No CWCID received in time\n");
					}
					callerid_free(cwcid);
					cwcid = NULL;
				}
			} else if (awaiting_cwcid <= (2 * FRAMES_PER_SECOND)) {
				/* Wait a moment (~400ms from SAS), assume CAS was sent, then send ACK */
				ack_cw(chan);
				cwcid = callerid_new(CID_SIG_BELL); /* Listen for Bell 202 FSK spill. */
				/* XXX Ugly hack (but it works). For some reason, callerid_feed never gets the 'U', so manually advance to get it started and then it'll proceed. */
				/* BUGBUG callerid_feed will get to the end with this modification, but the checksum always fails, so it's still not right... */
				cwcid->sawflag = 2;
			}
		}

		/* If it's our turn, we're streaming audio to the caller. */
		if (cinfo.calls[cinfo.active_call].our_turn) {
			int ms;

			ast_frfree(frame); /* If we're streaming audio, we don't care what the caller is doing. */

			/* This is what ensures that ast_channel_stream becomes NULL once the stream has finished. */
			ms = ast_sched_wait(ast_channel_sched(chan));
			if (ms < 0 && !ast_channel_timingfunc(chan)) {
				ast_stopstream(chan);
			}

			/* We need to play a prompt to the caller. */
			/* Don't simply use ast_waitstream, because we need to fully service the channel, esp. for CW Tone */
			if (!ast_channel_stream(chan)) {
				/* Stream finished. */
				if (cinfo.calls[cinfo.active_call].disconnect) {
					/* Current call needs to be disconnected. */
					if (disconnect_current(chan, &cinfo)) {
						break;
					}
				} else {
					/* Now wait for a response from the caller. */
					if (swap_turns(chan, &cinfo)) {
						break;
					}
				}
			} else { /* else, stream is still playing. */
				ast_sched_runq(ast_channel_sched(chan));
			}
		/* Else, we're waiting for a response from the caller. */
		} else if (frame->frametype == AST_FRAME_VOICE) {
			/* Waiting for response from caller. */
			dres = ast_dsp_silence(dsp, frame, &totalsilence);

			/* The MixMonitor audiohook is what's recording everything, beyond distinguishing talking and silence, we're not ourselves concerned with anything much. */
			ast_frfree(frame); /* Free the frame as soon as possible so we don't have ast_frfree wherever it's possible to break from the loop. */

			elapsed_frames++;

			if (!dres) {
				/* Frame is not silent, so reset. */
				noisy_frames++;
				if (noisy_frames == cinfo.calls[cinfo.active_call].talkthreshold) {
					ast_debug(3, "User has begun a satisfactorily long response (%d)\n", noisy_frames);
					silent_frames = 0;
					got_response = 1;
					no_response_count = 0;
				}
				/* Note: Long winded talkers aren't just pure noise, so noisy_frames won't actually increment as fast as FRAMES_PER_SECOND per second. */
				if (cinfo.calls[cinfo.active_call].prompt_number == 0 && noisy_frames >= (MAXIMUM_NORMAL_INTRO_RESPONSE_SECS * 1000) / FRAMES_PER_SECOND) {
					/* If the introductory response is longer than ~15 seconds, then assume it's some kind of long message
					 * or someone messing around as opposed to an actual person trying to talk to somebody. */
					if (!cinfo.calls[cinfo.active_call].longwindedmessage) { /* Only do once. */
						cinfo.calls[cinfo.active_call].longwindedmessage = 1;
						ast_verb(4, "Caller %d appears to be a long-winded talker...\n", cinfo.active_call);
						cinfo.calls[cinfo.active_call].talkthreshold = RESPONSE_FRAMES_REQUIRED_HIGH; /* Require more silence than usual to end the response. */
					}
				} else if (cinfo.calls[cinfo.active_call].prompt_number == 3 && noisy_frames > MINIMUM_PHONE_NUMBER_START_FRAMES) {
					/* Adaptive threshold for getting phone number from caller.
					 * Use a long threshold at first, and reduce it after we probably got a few digits (the exchange code) */
					if (cinfo.calls[cinfo.active_call].talkthreshold == RESPONSE_FRAMES_REQUIRED_HIGH) {
						ast_debug(3, "Reducing talk threshold from %d to %d\n", RESPONSE_FRAMES_REQUIRED_HIGH, RESPONSE_FRAMES_REQUIRED);
						cinfo.calls[cinfo.active_call].talkthreshold = RESPONSE_FRAMES_REQUIRED;
						noisy_frames = 0; /* However, do reset this to 0, so we distinctly need 2 chunks of audio. */
						silent_frames = 0;
					}
				}
			} else {
				/* Got a silent frame. */
				silent_frames++; /* XXX I'm not really sure if this variable serves much purpose, given it more or less constantly accumulates. */
				if (silent_frames % 10 == 0) {
					/* Only periodically print the silence count for debugging. */
					ast_debug(5, "noisy frames: %d/%d, silent frames: %d, total silence: %d\n",
						noisy_frames, cinfo.calls[cinfo.active_call].talkthreshold, silent_frames, totalsilence);
				}
			}

			/* Wait min 3 seconds */
			silent_too_long = !got_response && silent_frames > 3 * FRAMES_PER_SECOND && totalsilence > (cinfo.calls[cinfo.active_call].prompt_number == 0 ? 3000 : 5000);
			if (silent_too_long && noisy_frames > (int) (0.6 * cinfo.calls[cinfo.active_call].talkthreshold)) {
				/* If the response length was most of the way there, let it slide rather than considering it a nonresponse. */
				ast_debug(3, "I suppose that response was long enough...\n");
				noisy_frames = cinfo.calls[cinfo.active_call].talkthreshold; /* Pretend that the caller actually said enough. */
				dres = 1;
				totalsilence = SILENCE_TIME_LONG;
				silent_too_long = 0;
			} else if (noisy_frames > 1000 && silent_frames < 20) {
				ast_log(LOG_WARNING, "Silence/noise ratio is unnatural\n");
				/* Just hang up. */
				cinfo.calls[cinfo.active_call].our_turn = 1;
				cinfo.calls[cinfo.active_call].disconnect = 1;
			} else if (elapsed_frames > 60 * FRAMES_PER_SECOND) { /* Even the longest talker is not going to yabber for more than a minute. */
				/* Interestingly this case often happens if the caller simply hangs up, as for some reason totalsilence doesn't increase. */
				/* Safeguard to prevent ourselves from being tied up indefinitely.
				 * Ideally, however, we should really just get cut off by loop current disconnect. */
				ast_log(LOG_WARNING, "Too much audio received without any action\n");
				/* Just hang up. */
				cinfo.calls[cinfo.active_call].our_turn = 1;
				cinfo.calls[cinfo.active_call].disconnect = 1;
			}

			/* Caller never responded. */
			if (silent_too_long) {
				/* Caller silent too long and never said enough for us to have considered having received a response. */
				ast_debug(3, "got_response: %d, noisy frames: %d, silent_frames: %d, totalsilence: %d, no_response_count: %d\n",
					got_response, noisy_frames, silent_frames, totalsilence, no_response_count);
				/* We said something and didn't get a response within 3 to 5 seconds. Prompt once again. */
				silent_frames = 0;
				noisy_frames = 0;
				if (++no_response_count < 2) {
					ast_debug(3, "Didn't get a timely response, greeting again\n");
					do_nag(chan, &cinfo, 0);
				} else {
					ast_verb(4, "No timely response from caller, disconnecting\n");
					/* Hang up on this caller. */
					if (cinfo.calls[cinfo.active_call].prompt_number > 0) {
						/* Only say bye if we even got a response, ever, from the caller. Otherwise, most people would just hang up without saying anything. */
						do_nag(chan, &cinfo, 1);
					} else {
						cinfo.calls[cinfo.active_call].our_turn = 1; /* Still need to swap to our turn, so we handle the disconnect now. */
					}
					cinfo.calls[cinfo.active_call].disconnect = 1;
				}
			}

			/* Caller finished a response: process it (totalsilence is what tells us how much silence has really elapsed recently) */
			if (noisy_frames >= cinfo.calls[cinfo.active_call].talkthreshold && dres && (totalsilence >= (cinfo.calls[cinfo.active_call].longwindedmessage ? SILENCE_TIME_LONG : SILENCE_TIME))) {
				/* If we got a response and caller hasn't said anything for a bit. It's our turn now. */
				ast_debug(4, "sas_pending: %d, sas_asked_about: %d\n", cinfo.sas_pending, sas_asked_about);
				elapsed_frames = 0;
				if (cinfo.sas_pending) {
					if (!sas_asked_about) {
						/* First, ask. */
						ast_debug(3, "Call waiting to handle, interrupting regularly scheduled program\n");
						do_cwprompt(chan, &cinfo);
						sas_asked_about = 1;
					} else {
						/* Second, regardless of the actual reaction, switch calls. */
						ast_verb(4, "Switching to call waiting\n");
						cinfo.sas_pending = 0;
						sas_asked_about = 0;
						/* XXX If the call waiting caller hung up before we swap to it, this can cause the original caller to get disconnected with chan_dahdi */
						if (switch_calls(chan, &cinfo)) {
							break;
						}
						silent_frames = 0;
					}
				} else if (swap_turns(chan, &cinfo)) {
					break;
				}
				noisy_frames = 0;
				silent_frames = 0;
				got_response = 0;
			}
		}
	}

cleanup:
	if (cwcid) {
		callerid_free(cwcid);
	}
	ast_dsp_free(dsp);
	return res;
}

static int george_exec(struct ast_channel *chan, const char *data)
{
	char *argcopy = NULL;
	struct ast_flags flags = {0};
	char *opt_args[OPT_ARG_ARRAY_SIZE];
	int res = 0;
	int callwait = 0, cwcid = 0, tollsaver = 0, jump = 0;
	int waitsec = 0;
	int actual_waitms = 0;
	char mixmonargs[256];

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(mbox);
		AST_APP_ARG(waitsec);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "Must specify a mbox[@context]\n");
		return -1;
	}

	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, argcopy);

	if (ast_strlen_zero(args.mbox)) {
		ast_log(LOG_WARNING, "Must specify a mbox[@context]\n");
		return -1;
	}

	if (!ast_strlen_zero(args.waitsec) && (ast_str_to_int(args.waitsec, &waitsec) || waitsec < 0)) {
		ast_log(LOG_WARNING, "Invalid number wait time: %s\n", args.waitsec);
		return -1;
	}
	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(tad_option_flags, &flags, opt_args, args.options);
		if (ast_test_flag(&flags, OPT_CALLWAIT)) {
			callwait = 1;
		}
		if (ast_test_flag(&flags, OPT_CWCID)) {
			cwcid = 1;
		}
		if (ast_test_flag(&flags, OPT_TOLLSAVER)) {
			tollsaver = -1;
			if (!ast_strlen_zero(args.waitsec) && (ast_str_to_int(opt_args[OPT_ARG_TOLLSAVER], &tollsaver) || tollsaver < 0)) {
				ast_log(LOG_WARNING, "Invalid toll saver time: %s\n", opt_args[OPT_ARG_TOLLSAVER]);
				return -1;
			}
		}
		if (ast_test_flag(&flags, OPT_JUMP)) {
			jump = 1;
		}
	}

	if ((res = ast_set_read_format(chan, ast_format_ulaw)) < 0) {
		ast_log(LOG_WARNING, "Unable to set channel format, giving up\n");
		return -1;
	}

	/* Wait until we should answer the channel. */
	actual_waitms = waitsec * 1000;
	if (tollsaver < 0) {
		actual_waitms /= 2; /* Answer twice as fast. */
	} else if (tollsaver > 0) {
		if (tollsaver >= waitsec) {
			ast_log(LOG_WARNING, "Toll saver time (%d) is not smaller than regular answer time (%d)\n", tollsaver, waitsec);
			/* Proceed, but this doesn't really make sense, and it's probably not what was desired. */
		}
		actual_waitms = tollsaver * 1000;
	}
	if (ast_safe_sleep(chan, waitsec * 1000)) {
		return -1;
	}

	/* Answer the call. */
	if (ast_channel_state(chan) != AST_STATE_UP) {
		if (ast_answer(chan)) {
			return -1;
		}
	} else {
		ast_log(LOG_WARNING, "Channel %s was already answered?\n", ast_channel_name(chan));
	}

	/* Start the recording. When done, dispatch to mailbox, using the channel's real Caller ID, and then delete it. */
	snprintf(mixmonargs, sizeof(mixmonargs), "tad-%s.wav,cdm(%s)", ast_channel_uniqueid(chan), args.mbox);
	/* XXX The emitted AMI event (through app_voicemail) has a NULL Caller ID (correct in email). Known about but not fixed in ASTERISK-30283. */
	ast_debug(4, "MixMonitor arguments: %s\n", mixmonargs);
	if (ast_pbx_exec_application(chan, "MixMonitor", mixmonargs)) {
		return -1;
	}

	res = run_tad(chan, callwait, cwcid, jump); /* Run the TAD (telephone answering device) state machine */

	/* Stop the recording. */
	if (ast_pbx_exec_application(chan, "StopMixMonitor", NULL)) {
		return -1;
	}

	/* No need to restore the previous read format, we're done (caller probably hung up, unless escaping to management interface). */
	return res;
}

static int unload_module(void)
{
	int res = ast_unregister_application(app);
	unload_config();
	return res;
}

static int load_module(void)
{
	if (load_config()) {
		unload_config();
		return AST_MODULE_LOAD_DECLINE;
	}
	return ast_register_application_xml(app, george_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Interactive Answering Machine");
