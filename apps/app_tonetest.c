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
 * \brief Tone test module
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

/*** DOCUMENTATION
	<application name="ToneSweep" language="en_US">
		<synopsis>
			Tone sweep test
		</synopsis>
		<syntax>
			<parameter name="start" required="false">
				<para>Starting frequency. Default is 100.</para>
			</parameter>
			<parameter name="end" required="false">
				<para>Ending frequency. Default is 3500.</para>
			</parameter>
			<parameter name="duration" required="false">
				<para>Total time for sweep, in seconds. Default is
				10 seconds.</para>
			</parameter>
			<parameter name="vol" required="false">
				<para>Volume reduction factor. Higher number equals
				softer tone sweep. Default is 1 (loudest).</para>
			</parameter>
		</syntax>
		<description>
			<para>Generates an ascending or descending tone sweep (chirp) between two frequencies.</para>
		</description>
		<see-also>
			<ref type="application">Milliwatt</ref>
			<ref type="application">PlayTones</ref>
		</see-also>
	</application>
 ***/

#define TONE_AMPLITUDE_MAX	0x7fff	/* Max signed linear amplitude */

static char *chirpapp = "ToneSweep";

static void tone_sample_gen(short *slin_buf, int samples, int rate, int freq, short amplitude)
{
	int idx;
	double sample_step = 2.0 * M_PI * freq / rate;/* radians per step */

	for (idx = 0; idx < samples; ++idx) {
		slin_buf[idx] = amplitude * sin(sample_step * idx);
	}
}

static int tone_freq_sweep(struct ast_channel *chan, short amplitude, int start, int end, int incr, int timeout)
{
	int res, freq, samples = 160, len = samples * 2, result = 0;
	struct timeval timerstart;
	if (ast_set_write_format(chan, ast_format_slin)) {
		ast_log(LOG_WARNING, "Unable to set '%s' to signed linear format (write)\n", ast_channel_name(chan));
		return -1;
	}
	/* Sweep frequencies loop. */
	for (freq = start; freq != end; freq += incr) {
		timerstart = ast_tvnow();
		while (ast_remaining_ms(timerstart, timeout) > 0) {
			if (ast_waitfor(chan, 1000) > 0) {
				short slin_buf[samples + AST_FRIENDLY_OFFSET];
				struct ast_frame wf = {
					.frametype = AST_FRAME_VOICE,
					.offset = AST_FRIENDLY_OFFSET,
					.subclass.format = ast_format_slin,
					.datalen = len,
					.samples = samples,
					.src = __FUNCTION__,
				};
				ast_debug(5, "%d Hz tone at amplitude %d.\n", freq, amplitude);
				tone_sample_gen(slin_buf + AST_FRIENDLY_OFFSET, samples, DEFAULT_SAMPLE_RATE, freq, amplitude);
				wf.data.ptr = slin_buf + AST_FRIENDLY_OFFSET;
				res = ast_write(chan, &wf);
				ast_frfree(&wf);
				if (res < 0) {
					return -1;
				}
			} else {
				return -1;
			}
		}
	}

	return result;
}

static int chirp_exec(struct ast_channel *chan, const char *data)
{
	char *appdata;
	int mssleeptime, dir;
	double tmpduration;
	int start = 100, end = 3500, vol = 1, duration = 10000; /* 10 seconds, default */
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(start);
		AST_APP_ARG(end);
		AST_APP_ARG(duration);
		AST_APP_ARG(vol);
	);

	appdata = ast_strdupa(S_OR(data, ""));
	AST_STANDARD_APP_ARGS(args, appdata);

	if (!ast_strlen_zero(args.start) && ast_str_to_int(args.start, &start)) {
		if (start <= 0) {
			ast_log(LOG_WARNING, "Start frequency must be positive: %s\n", args.start);
			return -1;
		}
	}
	if (!ast_strlen_zero(args.end) && ast_str_to_int(args.end, &end)) {
		if (end <= 0) {
			ast_log(LOG_WARNING, "End frequency must be positive: %s\n", args.end);
			return -1;
		}
	}
	if (!ast_strlen_zero(args.duration) && sscanf(args.duration, "%30lf", &tmpduration) == 1) {
		duration = (int) (1000 * tmpduration);
		if (duration <= 0) {
			ast_log(LOG_WARNING, "Duration must be positive: %s\n", args.duration);
			return -1;
		}
	}
	if (!ast_strlen_zero(args.vol) && ast_str_to_int(args.vol, &vol)) {
		if (vol <= 0) {
			ast_log(LOG_WARNING, "Volume reduction factor: %s\n", args.vol);
			return -1;
		}
	}

	mssleeptime = (duration / abs(end - start));

	if (end < start) {
		dir = -1;
	} else if (end > start) {
		dir = 1;
	} else {
		ast_log(LOG_WARNING, "Start and end frequencies must be different: %d\n", start);
		return -1;
	}

	ast_debug(1, "Starting tone sweep from %d to %d Hz for %d s (%d ms each Hz)\n", start, end, duration, mssleeptime);
	tone_freq_sweep(chan, TONE_AMPLITUDE_MAX / vol, start, end, 1 * dir, mssleeptime);

	return 0;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(chirpapp);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_register_application_xml(chirpapp, chirp_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Tone test module");
