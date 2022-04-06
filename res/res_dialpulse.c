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
 * \brief Dial pulse feature module
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <math.h>

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/channel.h"
#include "asterisk/app.h"
#include "asterisk/module.h"
#include "asterisk/indications.h"
#include "../channels/sig_analog.h"
#include "../channels/chan_dahdi.h"

/*** DOCUMENTATION
	<application name="DialSpeedTest" language="en_US">
		<synopsis>
			Performs a dial pulse speed test on a channel.
		</synopsis>
		<syntax>
			<parameter name="file">
				<para>Audio file to use while waiting for "0" to be dialed.</para>
				<para>By default, the system will play a dial tone if a file
				is not specified.</para>
			</parameter>
			<parameter name="timeout">
				<para>The number of seconds to wait for all digits, if greater
				than <literal>0</literal>. Can be floating point. Default
				is no timeout.</para>
			</parameter>
			<parameter name="pps">
				<para>Pulses per second. By default, the application will try
				to determine whether a 10 pps or 20 pps dial is being used.</para>
				<para>You may also specify <literal>10</literal> or <literal>20</literal>
				explicitly.</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="t">
						<para>Automatically play the appropriate tone depending on
						the outcome of the test. If the dial is slow, the caller
						will hear busy tone. If the dial is fast, the caller will
						hear reorder (fast busy) tone. If the dial is normal, the
						caller will hear continuous audible ringback tone.</para>
						<para>If this option is used, the tone will be started by
						this application. An application like <literal>Wait</literal>
						will then be needed to then play the tone for the desired amount
						of time afterwards.</para>
					</option>
					<option name="r">
						<para>Used for performing readjustment, as opposed to a test.</para>
						<para>The default is to consider a dial "normal" speed if it
						is between 8 and 11 pulses per second (pps). For readjustment, this
						tolerance is reduced to 9.5 to 10.5 pps.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Dial tone is provided to indicate the test is ready to begin.</para>
			<para>The application then waits for ten dial pulses and measures their
			speed to indicate to the user if the dial is slow, fast, or normal speed.</para>
			<para>There are two modes in which this application may be used: test and readjust.
			The default mode is test, which has a wider tolerance. The readjust mode may be
			used by specifying the <literal>r</literal> option.</para>
			<variablelist>
				<variable name="DIALPULSERESULT">
					<para>This is the status of the dial speed test.</para>
					<value name="SLOW" />
					<value name="NORMAL" />
					<value name="FAST" />
					<value name="TIMEOUT" />
					<value name="HANGUP" />
					<value name="ERROR" />
				</variable>
				<variable name="DIALPULSESPEED">
					<para>The numeric measured speed of the dial, in pulse per second (pps).</para>
					<para>Only set if the test succeeds.</para>
				</variable>
				<variable name="DIALPULSEPERCENTMAKE">
					<para>The make percentage of a dial.</para>
					<para>Only set if the test is performed on an FXS channel using DAHDI.</para>
				</variable>
				<variable name="DIALPULSEPERCENTBREAK">
					<para>The break percentage of a dial.</para>
					<para>Only set if the test is performed on an FXS channel using DAHDI.</para>
				</variable>
			</variablelist>
		</description>
	</application>
 ***/

enum read_option_flags {
	OPT_TONE = (1 << 0),
};

AST_APP_OPTIONS(dspeed_app_options, {
	AST_APP_OPTION('t', OPT_TONE),
});

static const char *dspeed_name = "DialSpeedTest";

static int dspeed_test(struct ast_channel *chan, int timeout) {
	struct ast_frame *frame = NULL;
	struct timeval start;
	int remaining_time = timeout;
	int pulses_read = 0, res = 0;
	struct timespec begin, end;

	start = ast_tvnow();

	while (timeout == 0 || remaining_time > 0) {
		if (timeout > 0) {
			remaining_time = ast_remaining_ms(start, timeout);
			if (remaining_time <= 0) {
				break;
			}
		}
		if (ast_waitfor(chan, 1000) > 0) {
			frame = ast_read(chan);
			if (!frame) {
				ast_debug(1, "Channel '%s' did not return a frame; probably hung up.\n", ast_channel_name(chan));
				res = -1;
				break;
			} else if (frame->frametype == AST_FRAME_CONTROL && frame->subclass.integer == AST_CONTROL_PULSE) {
				if (++pulses_read == 1) {
					begin = ast_tsnow(); /* start the pulse timer */
				}
				ast_debug(2, "Dial pulse speed test: pulse %d\n", pulses_read);
				if (pulses_read == 10) {
					break;
				}
			}
			ast_frfree(frame);
		} else {
			res = -1;
		}
	}
	if (pulses_read) {
		end = ast_tsnow();
		res = ((end.tv_sec * 1000 + end.tv_nsec / 1000000) - (begin.tv_sec * 1000 + begin.tv_nsec / 1000000));
	}
	return res;
}

static int dspeed_exec(struct ast_channel *chan, const char *data)
{
	double tosec;
	struct ast_flags flags = {0};
	char *file = NULL, *argcopy = NULL;
	int res, to = 0, pps = 0, tone = 0;

	AST_DECLARE_APP_ARGS(arglist,
		AST_APP_ARG(file);
		AST_APP_ARG(timeout);
		AST_APP_ARG(pps);
		AST_APP_ARG(options);
	);

	argcopy = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(arglist, argcopy);

	if (!ast_strlen_zero(arglist.options)) {
		ast_app_parse_options(dspeed_app_options, &flags, NULL, arglist.options);
		if (ast_test_flag(&flags, OPT_TONE)) {
			tone = 1;
		}
	}
	if (!ast_strlen_zero(arglist.timeout)) {
		tosec = atof(arglist.timeout);
		if (tosec <= 0) {
			ast_log(LOG_WARNING, "Timeout '%f' is invalid, ignoring.\n", tosec);
			to = 0;
		} else {
			to = tosec * 1000.0;
		}
	}
	if (!ast_strlen_zero(arglist.pps)) {
		pps = atoi(arglist.pps);
		if (pps && pps != 10 && pps != 20) {
			ast_log(LOG_WARNING, "Invalid pps setting, ignoring: %d\n", pps);
			pps = 0;
		}
	}

	ast_stopstream(chan);

	if (!ast_strlen_zero(arglist.file)) {
		if (ast_fileexists(arglist.file, NULL, ast_channel_language(chan))) {
			file = arglist.file;
		} else {
			ast_log(LOG_WARNING, "File '%s' does not exist\n", arglist.file);
		}
	}
	if (ast_strlen_zero(file)) {
		struct ast_tone_zone_sound *ts = ast_get_indication_tone(ast_channel_zone(chan), "dial");
		if (ts) {
			ast_playtones_start(chan, 0, ts->data, 0);
			ts = ast_tone_zone_sound_unref(ts);
		} else {
			ast_log(LOG_WARNING, "Couldn't start tone playback\n");
		}
	} else {
		ast_streamfile(chan, file, ast_channel_language(chan));
	}

	res = dspeed_test(chan, to);
	if (ast_strlen_zero(file)) {
		ast_playtones_stop(chan);
	} else {
		ast_stopstream(chan);
	}

	if (res == -1) {
		pbx_builtin_setvar_helper(chan, "DIALPULSERESULT", "HANGUP");
	} else if (!res) {
		pbx_builtin_setvar_helper(chan, "DIALPULSERESULT", "TIMEOUT");
	} else if (res > 0) {
#define BUFFER_LEN 8
		const char *result;
		char buf[BUFFER_LEN];
		double dialpps = 10000.0 / res; /* pps = 10000 / elapsed time */
		struct ast_tone_zone_sound *ts = NULL;

		snprintf(buf, BUFFER_LEN, "%.3f", dialpps);
		pbx_builtin_setvar_helper(chan, "DIALPULSESPEED", buf);

		if (!pps) { /* try to determine whether this is a 10 pps or 20 pps dial */
			pps = dialpps >= 15 ? 20 : 10;
		}

		/*! \todo allow playing file override? Is that even possible? StartPlayback? actually, yeah... */
		/*! \todo get the right timings/thresholds. And if over even a bit right now it's FAST - fix that. */
		/* These timings (8-11 and 9.5-10.5, for 10pps dials) are found in a number of telephone documents. */
		if (dialpps < pps - 2) {
			result = "SLOW";
			if (tone) {
				ts = ast_get_indication_tone(ast_channel_zone(chan), "busy");
			}
		} else if (dialpps > pps + 1) {
			result = "FAST";
			if (tone) {
				ts = ast_get_indication_tone(ast_channel_zone(chan), "congestion");
			}
		} else {
			result = "NORMAL";
			if (tone) {
				if (ast_playtones_start(chan, 0, "440+480/1000", 0)) { /* continuous "ring" tone */
					ast_log(LOG_WARNING, "Unable to start playtones\n");
				}
				tone = 0;
			}
		}

		ast_verb(3, "Dial speed was %.3f pps (%s) (took %d ms for %d pps test)", dialpps, result, res, pps);
		pbx_builtin_setvar_helper(chan, "DIALPULSERESULT", result);

#if HAVE_DAHDI
		if (!strcasecmp(ast_channel_tech(chan)->type, "DAHDI")) {
			struct dahdi_pvt *pvt = ast_channel_tech_pvt(chan);
			if (dahdi_analog_lib_handles(pvt->sig, 0, 0)) {
				struct analog_pvt *analog_p = pvt->sig_pvt;
				int i, j;
				int maketime = 0, breaktime = 0;

				ast_mutex_lock(&pvt->lock);
				j = analog_p->pulsemakecount < analog_p->pulsebreakcount ? analog_p->pulsemakecount : analog_p->pulsebreakcount;
				/* break = on-hook, make = off-hook
				 * % break = on -> off (break->make)
				 * % make  = off -> on (make->break)
				 *
				 * e.g. make0, break0, make1, break1, make2, break2...
				 *
				 */
				for (i = 0; i < j; i++) {
					maketime += analog_p->pulsebreaks[i] - analog_p->pulsemakes[i];
					if (i) {
						breaktime += analog_p->pulsemakes[i] - analog_p->pulsebreaks[i - 1];
					}
				}
				ast_mutex_unlock(&pvt->lock);

				if (j) {
					double makeratio, breakratio;
					char pct[4];
					int total;
					maketime = (int) (1.0 * maketime / j);
					breaktime = (int) (1.0 * breaktime / (j - 1)); /* we've got 1 less than with maketime */
					total = maketime + breaktime;
					makeratio = 100.0 * maketime / total;
					breakratio = 100.0 * breaktime / total;

					ast_verb(3, "Dial make/break ratio is %.3f%% make, %.3f%% break\n", makeratio, breakratio);

					snprintf(pct, 4, "%d", (int) round(makeratio));
					pbx_builtin_setvar_helper(chan, "DIALPULSEPERCENTMAKE", pct);
					snprintf(pct, 4, "%d", (int) round(breakratio));
					pbx_builtin_setvar_helper(chan, "DIALPULSEPERCENTBREAK", pct);
				} else {
					ast_log(LOG_WARNING, "No make/break ratio information available\n");
				}
			} else {
				ast_debug(1, "Channel is not analog?\n");
			}
		}
#endif

		if (tone) {
			if (ts) {
				ast_playtones_start(chan, 0, ts->data, 0);
				ts = ast_tone_zone_sound_unref(ts);
			} else {
				ast_log(LOG_WARNING, "Couldn't start tone playback\n");
			}
		}
	} else {
		ast_log(LOG_WARNING, "Dial pulse speed test returned '%d'\n", res);
		pbx_builtin_setvar_helper(chan, "DIALPULSERESULT", "ERROR");
	}

	return res == -1 ? -1 : 0;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(dspeed_name);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_register_application_xml(dspeed_name, dspeed_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Dial Pulse Feature Module");
