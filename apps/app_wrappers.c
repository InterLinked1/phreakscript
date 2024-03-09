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
 * \brief Trivial application wrappers
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/indications.h"
#include "asterisk/causes.h"

/*** DOCUMENTATION
	<application name="InbandDial" language="en_US">
		<synopsis>
			Dial a destination, play in-band signalling if needed, and hang up the channel
		</synopsis>
		<syntax>
			<parameter name="dialstr" required="yes">
				<para>Arguments to the Dial application</para>
			</parameter>
		</syntax>
		<description>
			<para>This application is a simple wrapper which calls Dial, plays the appropriate
			precise tone plan tones if needed, and then hangs up the channel.</para>
			<para>This is meant to simplify the very common idiom of dialing a destination,
			branching depending on the DIALSTATUS, and then hanging up.</para>
			<para>Dialplan execution will not continue after calling this application.</para>
		</description>
		<see-also>
			<ref type="application">Dial</ref>
		</see-also>
	</application>
 ***/

static char *dial_app = "InbandDial";

static int dial_exec(struct ast_channel *chan, const char *data)
{
	struct ast_tone_zone_sound *ts = NULL;
	char dialstatus[64] = "";
	char hangupcause[16] = "";
	int cause = 0;

	if (ast_strlen_zero(data)) {
		ast_log(LOG_ERROR, "%s requires arguments\n", dial_app);
		return -1;
	}

	/* Even if Dial returns -1, we want to proceed for logging purposes and to hang up directly */
	ast_pbx_exec_application(chan, "Dial", data);

	ast_channel_lock(chan);
	ast_copy_string(dialstatus, S_OR(pbx_builtin_getvar_helper(chan, "DIALSTATUS"), ""), sizeof(dialstatus));
	ast_copy_string(hangupcause, S_OR(pbx_builtin_getvar_helper(chan, "HANGUPCAUSE"), ""), sizeof(hangupcause));
	ast_channel_unlock(chan);

	if (!ast_strlen_zero(hangupcause)) {
		ast_verb(4, "Call ended, DIALSTATUS: %s (HANGUPCAUSE: %s)\n", dialstatus, hangupcause);
	} else {
		ast_verb(4, "Call ended, DIALSTATUS: %s\n", dialstatus);
	}

	if (!strcmp(dialstatus, "ANSWER")) {
		/* Normal answer and clearing */
	} else if (!strcmp(dialstatus, "NOANSWER")) {
		/* Remote end disconnected without providing answer supervision. Continue. */
	} else if (!strcmp(dialstatus, "CANCEL")) {
		/* Caller hung up before answer. Continue. */
	} else if (!strcmp(dialstatus, "BUSY")) {
		/* Out of band busy, make it in band */
		ts = ast_get_indication_tone(ast_channel_zone(chan), "busy"); /* Play busy tone */
	} else if (!strcmp(dialstatus, "CHANUNAVAIL") || !strcmp(dialstatus, "CONGESTION")) {
		ts = ast_get_indication_tone(ast_channel_zone(chan), "congestion"); /* Play reorder tone */
	} else {
		ast_log(LOG_WARNING, "Channel %s exited Dial with unexpected DIALSTATUS '%s'\n", ast_channel_name(chan), dialstatus);
	}

	if (ts) {
		int res = ast_playtones_start(chan, 0, ts->data, 0);
		ts = ast_tone_zone_sound_unref(ts);
		if (res) {
			ast_log(LOG_WARNING, "Unable to start tones on channel %s\n", ast_channel_name(chan));
		} else {
			/* Stream the tone until the caller disconnects, up to a maximum of 3 minutes.
			 * At that point, if the caller hasn't disconnected, something could be wrong,
			 * so we shouldn't wait forever for the channel to clear. */
			if (ast_safe_sleep(chan, 180 * 1000)) {
				return -1;
			}
		}
	}

	/* Based on pbx_builtin_hangup */
	ast_set_hangupsource(chan, "dialplan/dialinband", 0);

	if (!ast_strlen_zero(hangupcause)) {
		cause = ast_str2cause(hangupcause);
		if (cause <= 0) {
			if (sscanf(hangupcause, "%30d", &cause) != 1 || cause <= 0) {
				ast_log(LOG_WARNING, "Invalid hangup cause: \"%s\"\n", data);
				cause = 0;
			}
		}
	}

	ast_channel_lock(chan);
	if (cause <= 0) {
		cause = ast_channel_hangupcause(chan);
		if (cause <= 0) {
			cause = AST_CAUSE_NORMAL_CLEARING;
		}
	}
	ast_channel_hangupcause_set(chan, cause);
	ast_softhangup_nolock(chan, AST_SOFTHANGUP_EXPLICIT);
	ast_channel_unlock(chan);

	return -1;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(dial_app);

	return res;
}

static int load_module(void)
{
	return ast_register_application_xml(dial_app, dial_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Trivial Dialplan Application Wrappers");
