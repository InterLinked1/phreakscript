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
 * \brief Audichron Time and Temperature Announcement
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <sys/stat.h>
#include <dirent.h>

#include <libgen.h> /* use basename, dirname */
#include "asterisk/paths.h"	/* use ast_config_AST_DATA_DIR */

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/say.h"
#include "asterisk/conversions.h"
#include "asterisk/musiconhold.h"

/*** DOCUMENTATION
	<application name="Audichron" language="en_US">
		<synopsis>
			Announce the time and temperature in the style of an Audichron machine
		</synopsis>
		<syntax>
			<parameter name="promptdir" required="false">
				<para>A directory containing prompt files to use for time announcements.</para>
				<para>Numbers between 0 through 9 may or may not be zero-prefixed.</para>
				<para>Hours and minutes, if using separate prompt sets, may be optionally prefixed
				with <literal>tmh</literal> and <literal>tmm</literal>.</para>
				<para>As long as the filenames are reasonable for time and temperature prompts,
				this application will attempt to find the correct ones and play them,
				so no configuration is required for varying prompt sets. However, correctness
				is not guaranteed and you should test your settings before deploying them
				into production.</para>
				<para>If no prompt set is provided, the default prompt set will be used instead.</para>
			</parameter>
			<parameter name="temppromptdir" required="false">
				<para>A directory containing prompt files to use for temperature announcements.</para>
				<para>Numbers between 0 through 9 may or may not be zero-prefixed.</para>
				<para>Negative temperatures should have a <literal>n</literal> after the number in the filename.</para>
				<para>As long as the filenames are reasonable for time and temperature prompts,
				this application will attempt to find the correct ones and play them,
				so no configuration is required for varying prompt sets. However, correctness
				is not guaranteed and you should test your settings before deploying them
				into production.</para>
			</parameter>
			<parameter name="tz" required="false">
				<para>Time zone to use for announcements.</para>
				<para>Default is system time zone.</para>
			</parameter>
			<parameter name="temp" required="false">
				<para>Current temperature, in Fahrenheit.</para>
				<para>If provided, a temperature announcement will be done after the time.</para>
				<para>To update the temperature during announcements,
				this application will need to be called multiple times. If you do this,
				omit the advertisement argument on subsequent calls so those are not
				repeated between iterations.</para>
			</parameter>
			<parameter name="advertisement" required="false">
				<para>Filename(s) of optional advertisement to play when the call is answered.</para>
				<para>This corresponds to STM (Small Town Machine) operation mode, as opposed to M12.</para>
				<para>If there is more than one iteration, these ads are repeated each time.</para>
				<para>If this is the path to a directory, rather than a file,
				a random file in this directory will be played each iteration.</para>
			</parameter>
			<parameter name="iterations" required="false">
				<para>Number of iterations to run, before disconnecting.</para>
				<para>Default is indefinite.</para>
			</parameter>
			<parameter name="options" required="no">
				<optionlist>
					<option name="c">
						<para>Also announce temperature in Celsius, in addition to Fahrenheit.</para>
					</option>
					<option name="l">
						<para>Make iterations 20 seconds instead of 10 seconds.</para>
						<para>May be required if prompts take up a lot of time.</para>
					</option>
					<option name="n">
						<para>Do not answer the call when the announcement unit kicks in.</para>
						<para>By default, when the first announcement starts, the application will answer
						the call.</para>
					</option>
					<option name="s">
						<para>Announce seconds in time announcements. This corresponds to "comparator" mode.</para>
						<para>Example phrasing is "at the tone, the time will be twelve fifteen and ten seconds".</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Announce the time (and optionally, temperature) in the style of an Audichron machine.</para>
			<para>This application will automatically handle the timing required for proper announcements. However,
			some degree of fine tuning of application parameters may be required, so testing is recommended.
			Some prompts are mandatory, and if not provided, will cause the application exit immediately.</para>
			<para>All audio must be 8 KHz sampled PCM audio (ulaw or wav files), for timings to be calculated correctly.</para>
		</description>
		<see-also>
			<ref type="application">SayNumber</ref>
		</see-also>
	</application>
 ***/

static char *app = "Audichron";

enum audichron_option_flags {
	OPT_CELSIUS   = (1 << 0),
	OPT_NOANSWER  = (1 << 1),
	OPT_SECONDS   = (1 << 2),
	OPT_LONG_CYCLE = (1 << 3),
};

AST_APP_OPTIONS(audichron_app_options, {
	AST_APP_OPTION('c', OPT_CELSIUS),
	AST_APP_OPTION('l', OPT_LONG_CYCLE),
	AST_APP_OPTION('n', OPT_NOANSWER),
	AST_APP_OPTION('s', OPT_SECONDS),
});

struct audichron {
	const char *promptdir;
	const char *temppromptdir;
	const char *advertisement;
	const char *tz;
	int temp;
	int iterations;				/* Number of iterations. 0 = indefinite (default) */
	int pretime;
	unsigned int do_temp:1;
	unsigned int do_celsius:1;
	unsigned int answer:1;
	unsigned int do_seconds:1;
	unsigned int answered:1;
	unsigned int ad_is_dir:1;
	unsigned int longcycle:1;
	int rand_index;
	int num_files;
	char at_tone[PATH_MAX]; /* At the tone, the time will be... */
	char tone[PATH_MAX];
	char temperature[PATH_MAX];
	char fahrenheit[PATH_MAX];
	char celsius[PATH_MAX];
	char and_prompt[PATH_MAX];
	char seconds[PATH_MAX];
	char exactly[PATH_MAX];
	/* Temporary, reset each iteration */
	char hr[PATH_MAX];
	char min[PATH_MAX];
	char sec[PATH_MAX];
};

struct find_prompt_search {
	const char *prefix;
	char result[PATH_MAX];
	unsigned int exact:1;
};

#define NO_WAY_TO_DETERMINE_EXTENSION_FROM_FILESTREAM

static int handle_find_prompt(const char *dir_name, const char *filename, void *obj)
{
	size_t flen;
	struct find_prompt_search *s = obj;
	char *ext;
	const char *prefix = s->prefix;

	if (strncmp(prefix, filename, strlen(prefix))) {
		return 0; /* Skip */
	}

	ext = strrchr(filename, '.');
	if (!ext) {
		return 0;
	}
	flen = ext - filename;

	if (s->exact && strncmp(prefix, filename, flen)) {
		return 0; /* Skip */
	}

	ext++;
	if (!ast_get_format_for_file_ext(ext)) {
		/* Try to get another version if we can */
		ast_debug(3, "Skipping '%s', no format loaded for extension '%s'\n", filename, ext);
		return 0;
	}

	/* Use this one */
	ast_copy_string(s->result, filename, sizeof(s->result));
#ifndef NO_WAY_TO_DETERMINE_EXTENSION_FROM_FILESTREAM
	ext = strrchr(s->result, '.');
	if (ext) {
		*ext++ = '\0'; /* Ditch the extension */
	}
#endif
#ifdef EXTRA_DEBUG
	ast_debug(6, "Found match: %s\n", s->result);
#endif
	return 1;
}

static int get_audio_length(struct ast_channel *chan, const char *filename, struct ast_format *fmt)
{
	int length;
	long max_length;
	int sample_rate;
	struct ast_filestream *fs;

	if (!ast_fileexists(filename, NULL, ast_channel_language(chan))) {
		ast_log(LOG_WARNING, "No files for '%s' exist\n", filename);
		return -1;
	}

	fs = ast_openstream_full(chan, filename, ast_channel_language(chan), 1); /* Not playing now, just checking it out */
	if (!fs) {
		ast_log(LOG_ERROR, "Failed to open stream '%s'\n", filename);
		return -1;
	}

	ast_seekstream(fs, 0, SEEK_END);
	max_length = ast_tellstream(fs);
	sample_rate = ast_ratestream(fs);
	if (ast_channel_stream(chan)) {
		ast_closestream(ast_channel_stream(chan));
		ast_channel_stream_set(chan, NULL);
	}

	length = ast_format_determine_length(fmt, max_length);
#ifdef EXTRA_DEBUG
	ast_debug(6, "max length %lu, sample rate %d, raw length %d\n", max_length, sample_rate, length);
#endif
	length /= ((sample_rate + 999) / 1000); /* Keep in ms, and round up */
	ast_debug(4, "Length of %s is %d ms\n", filename, length);
	return length;
}

static int find_prompt(struct ast_channel *chan, struct audichron *restrict a, const char *promptdir, int *restrict pretime, char *restrict buf, size_t len,
	const char *prefix, const char *opt_prefix, const char *default_prompt, int exact)
{
	int length;
	struct ast_format *fmt;
	char *ext;

	if (ast_strlen_zero(promptdir)) {
#ifdef NO_WAY_TO_DETERMINE_EXTENSION_FROM_FILESTREAM
#pragma GCC diagnostic ignored "-Wformat-truncation"
		/* This code is really disgusting on so many levels.
		 * Since there is no API in file.c to get the extension of the format,
		 * we have to do all this work instead... fix this!!!
		 * It also doesn't really work reliably if there are files
		 * that exist with multiple extensions.
		 *
		 * This nonsense could ALL be avoided if there was an ast_filestream_get_fmt function.
		 */
		int res;
		char parentdir[PATH_MAX];
		char dirnamebuf[PATH_MAX];
		char basenamebuf[PATH_MAX];
		char *parent;
		const char *base;
		struct find_prompt_search search = {
			.result = "",
			.exact = 1,
		};
		ast_copy_string(dirnamebuf, default_prompt, sizeof(dirnamebuf));
		ast_copy_string(basenamebuf, default_prompt, sizeof(basenamebuf));
		parent = dirname(dirnamebuf);
		if (!ast_strlen_zero(parent) && !strcmp(parent, ".")) {
			parent = "";
		}
		base = basename(basenamebuf);
		search.prefix = base;
		if (default_prompt[0] == '/') {
			ast_copy_string(parentdir, parent, sizeof(parentdir));
		} else {
			snprintf(parentdir, sizeof(parentdir), "%s/sounds/%s%s%s", ast_config_AST_DATA_DIR, ast_channel_language(chan), *parent != '\0' ? "/" : "", parent);
		}
		ast_debug(3, "basename = %s, dirname = %s\n", base, parentdir);
		res = ast_file_read_dir(parentdir, handle_find_prompt, &search);
		if (res < 0) {
			ast_log(LOG_ERROR, "Failed to scan directory '%s'\n", parentdir);
			return -1;
		}
		if (!search.result[0]) {
			ast_log(LOG_WARNING, "Failed to find suitable filename '%s' in %s\n", default_prompt, parentdir);
			return -1;
		}
		snprintf(buf, len, "%s/%s", parentdir, search.result); /* Filename, without prefix */
#else
		ast_copy_string(buf, default_prompt, len);
#endif
	} else {
		/* Try to determine an appropriate one */
		struct find_prompt_search search = {
			.prefix = prefix,
			.result = "",
			.exact = exact,
		};
		int res = ast_file_read_dir(promptdir, handle_find_prompt, &search);
		if (res < 0) {
			ast_log(LOG_ERROR, "Failed to scan directory '%s'\n", promptdir);
			return -1;
		}
		/* If it's a number, try without zero prefix */
		if (!search.result[0] && prefix[0] == '0') {
			search.prefix = prefix + 1;
			res = ast_file_read_dir(promptdir, handle_find_prompt, &search);
			if (res < 0) {
				return -1;
			}
		}
		/* Another prefix for this filename? */
		if (!search.result[0] && opt_prefix) {
			char fullprefix[PATH_MAX];
			snprintf(fullprefix, sizeof(fullprefix), "%s%s", opt_prefix, prefix);
			search.prefix = fullprefix;
			res = ast_file_read_dir(promptdir, handle_find_prompt, &search);
			if (res < 0) {
				return -1;
			}
			if (!search.result[0] && prefix[0] == '0') {
				snprintf(fullprefix, sizeof(fullprefix), "%s%s", opt_prefix, prefix + 1);
				search.prefix = fullprefix;
				res = ast_file_read_dir(promptdir, handle_find_prompt, &search);
				if (res < 0) {
					return -1;
				}
			}
		}
		if (!search.result[0]) {
			/* Some are optional so don't make this a warning */
			ast_debug(1, "No filename with prefix '%s' in %s\n", prefix, promptdir);
			return -1;
		}
		snprintf(buf, len, "%s/%s", promptdir, search.result); /* Filename, without prefix */
	}

#ifdef NO_WAY_TO_DETERMINE_EXTENSION_FROM_FILESTREAM
	ext = strrchr(buf, '.');
	if (!ext) {
		ast_log(LOG_WARNING, "File '%s' has no extension?\n", buf);
		return -1;
	}
	*ext++ = '\0';
	fmt = ast_get_format_for_file_ext(ext);
	if (!fmt) {
		ast_log(LOG_ERROR, "No format for extension '%s'\n", ext);
		return -1;
	}
#endif

	length = get_audio_length(chan, buf, fmt);

	*pretime += length;
	return 0;
}

#define FIND_PROMPT(var, prefix, default_prompt) find_prompt(chan, a, a->promptdir, &pretime, a->var, sizeof(a->var), prefix, NULL, default_prompt, 0)
#define FIND_PROMPT_EXACT(var, prefix, opt_prefix, default_prompt) find_prompt(chan, a, a->promptdir, &pretime, a->var, sizeof(a->var), prefix, opt_prefix, default_prompt, 1)
#define FIND_TEMP_PROMPT(var, prefix) find_prompt(chan, a, a->temppromptdir, &pretime, var, sizeof(var), prefix, NULL, NULL, 1)

/*! \brief Round up to nearest multiple */
static inline time_t round_up(time_t orig, int multiple)
{
	time_t remainder = orig % multiple;
	time_t add = multiple - remainder;
	return orig + add;
}

#define PLAY_PROMPT(chan, prompt) \
	if (ast_streamfile(chan, prompt, ast_channel_language(chan))) { \
		return -1; \
	} else if (ast_waitstream(chan, NULL)) { \
		return -1; \
	}

#define ABS(n) (n < 0 ? -n : n)

static int play_temperature(struct ast_channel *chan, struct audichron *a, int num)
{
	if (a->promptdir) {
		int pretime = 0; /* Result unused */
		int res;
		char temps[16];
		char filename[256] = "";
		snprintf(temps, sizeof(temps), "%d%s", ABS(num), num < 0 ? "n" : "");
		res = FIND_TEMP_PROMPT(filename, temps);
		if (!filename[0] && !res && num < 10) { /* Try 0-prefixing if no match at first */
			snprintf(temps, sizeof(temps), "%02d%s", ABS(num), num < 0 ? "n" : "");
			res = FIND_TEMP_PROMPT(filename, temps);
		}
		if (res) {
			return -1;
		}
		PLAY_PROMPT(chan, filename);
	} else {
		return ast_say_number(chan, num, "", ast_channel_language(chan), NULL);
	}
	return 0;
}

static int get_rand_file(struct audichron *a, char *buf, size_t len, const char *directory)
{
	struct dirent *entry, **entries;
	int files, fno;
	unsigned int c = 0;
	char *ext;
	int found_file = 0;

	/* use scandir instead of opendir/readdir, so the listing is ordered */
	files = scandir(directory, &entries, NULL, alphasort);
	if (files < 0) {
		ast_log(LOG_ERROR, "scandir(%s) failed: %s\n", directory, strerror(errno));
		return -1;
	}
	if (!a->num_files) {
		/* 1-indexed, so not initialized yet */
		fno = 0;
		while (fno < files && (entry = entries[fno++])) {
			if (entry->d_type != DT_REG || !strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
				continue;
			}
			ext = strrchr(entry->d_name, '.');
			if (ext++ && ast_get_format_for_file_ext(ext)) {
				c++;
			}
		}
		a->num_files = c;
		a->rand_index = rand() % a->num_files;
	} else {
		if (++a->rand_index >= a->num_files) {
			a->rand_index = 0;
		}
	}
	fno = 0;
	c = 0;
	while (fno < files && (entry = entries[fno++])) {
		if (entry->d_type != DT_REG || !strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
			ast_std_free(entry);
			continue;
		}
		ext = strrchr(entry->d_name, '.');
		if (!ext++ || !ast_get_format_for_file_ext(ext)) {
			ast_std_free(entry);
			continue;
		}
		if (c++ == a->rand_index) {
			snprintf(buf, len, "%s/%s", directory, entry->d_name);
			found_file = 1;
		}
		ast_std_free(entry);
	}
	ast_std_free(entries);
	if (!a->num_files) {
		ast_log(LOG_WARNING, "Directory %s is empty or does not contain any playable files\n", directory);
		return -1;
	}
	ast_assert_return(found_file, -1);
	return 0;
}

static int audichron_loop(struct ast_channel *chan, struct audichron *a)
{
	time_t now;
	time_t waketime;
	time_t tonetime;
	int sleepsec;
	int res = 0;
	int orig_pretime;
	int pretime = a->pretime;
	struct timeval when;
	struct ast_tm tm;
	int longcycle;
	int exact;
	char adfile[PATH_MAX];
	char hr[6], min[6], sec[6];

	if (!a->answered) {
		/* First loop only */
		a->answered = 1;
		/* Calculate how long it will take to play everything prior to the tone */
		if (a->do_seconds) {
			res |= FIND_PROMPT(at_tone, "at", "at-tone-time-exactly");
		} else {
			FIND_PROMPT(at_tone, "time", "current-time-is");
		}
		if (a->promptdir) {
			FIND_PROMPT(and_prompt, "and", NULL);
			FIND_PROMPT(seconds, "s", NULL);
			FIND_PROMPT(exactly, "exactly", NULL);
		}
		a->pretime = pretime;
		if (a->do_seconds) {
			res |= FIND_PROMPT(tone, "tone", "beep"); /* Not computed in length */
		}
		if (a->do_temp) {
			if (a->do_celsius) {
				res |= FIND_PROMPT(fahrenheit, "f", "fahrenheit");
				res |= FIND_PROMPT(celsius, "c", "celsius");
			} else {
				FIND_PROMPT(temperature, "temp", "temperature");
			}
		}
		if (res) {
			return -1;
		}
		pretime = a->pretime; /* Reset */
	}

	if (!a->promptdir) {
		pretime += a->do_seconds ? 4000 : 1500; /* Conservative estimate of how long it could take to announce the time itself, accounting for the default using SayUnixTime */
	}
	if (a->answer) {
		pretime += 500; /* Pause after answering so media isn't cut off */
	}

	/* If we're playing an ad, get that ready */
	if (!ast_strlen_zero(a->advertisement)) {
		int length;
		if (a->ad_is_dir) {
			char *ext;
			if (get_rand_file(a, adfile, sizeof(adfile), a->advertisement)) {
				return -1;
			}
			ext = strrchr(adfile, '.');
			if (!ext) {
				ast_log(LOG_ERROR, "File %s has no extension\n", adfile);
				return -1;
			}
			*ext++ = '\0'; /* Remove extension */
			length = get_audio_length(chan, adfile, ast_get_format_for_file_ext(ext));
			pretime += length;
		} else {
			char testname[PATH_MAX];
#ifdef NO_WAY_TO_DETERMINE_EXTENSION_FROM_FILESTREAM
			struct stat st;
			struct ast_format *fmt;
			snprintf(testname, sizeof(testname), "%s.ulaw", a->advertisement);
			if (!stat(testname, &st)) {
				fmt = ast_get_format_for_file_ext("ulaw");
			} else {
				fmt = ast_get_format_for_file_ext("wav");
			}
#endif
			length = get_audio_length(chan, a->advertisement, fmt);
			pretime += length;
		}
	}

	/* pretime is the minimum amount of time needed between now and when the tone will be played */
	now = time(NULL);
	/* If using default prompts, and we're announcing Celsius, there isn't enough time to do it every 10 seconds, so force it to be on % 20, to include the minute */
	longcycle = a->longcycle || (!a->promptdir && a->do_seconds && a->do_celsius);
	tonetime = round_up(now + pretime / 1000, longcycle ? 20 : 10); /* If there's not enough time before the next multiple of 10, we'll have to do the next one instead */
	orig_pretime = pretime;

tryagain:
	if (!a->longcycle && pretime > 8500) {
		ast_log(LOG_WARNING, "Too long for short iteration, assuming long cycle - setting the 'l' option explicitly is recommended\n");
		longcycle = 1;
	}

	exact = tonetime % 60 == 0;
	if (1 || !exact) { /* Sounds better if we do it all the time, and have silence after on the minute */
		pretime += 1500; /* Add more time if it's in the middle of a minute */
	}

	if (a->promptdir) {
		when.tv_sec = tonetime;
		when.tv_usec = 0;
		ast_localtime(&when, &tm, a->tz);
		res = ast_strftime(hr, sizeof(hr), "%l", &tm);
		if (hr[0] == ' ') {
			hr[0] = '0';
		}
		if (res < 0) {
			ast_log(LOG_WARNING, "ast_strftime failed: %s\n", strerror(errno));
			return -1;
		}
		res = ast_strftime(min, sizeof(min), "%M", &tm);
		if (res < 0) {
			ast_log(LOG_WARNING, "ast_strftime failed: %s\n", strerror(errno));
			return -1;
		}
		res = ast_strftime(sec, sizeof(sec), "%S", &tm);
		if (res < 0) {
			ast_log(LOG_WARNING, "ast_strftime failed: %s\n", strerror(errno));
			return -1;
		}
		/* This is also a common prompt set naming convention for separate hours/minutes: */
		res = FIND_PROMPT_EXACT(hr, hr, "tmh", NULL);
		res |= FIND_PROMPT_EXACT(min, min, "tmm", NULL);
		if (a->do_seconds) {
			res |= FIND_PROMPT_EXACT(sec, sec, NULL, NULL); /* Internally tries with and without zero padding */
		}
		if (res) {
			return -1;
		}
	}

	waketime = tonetime - pretime / 1000;
	sleepsec = waketime - now;

	if (sleepsec < 0) {
		/* Not enough time before the time we were going to announce, so reschedule for the next one */
		ast_debug(3, "longcycle=%d, tonetime=%lu, pretime=%d, waketime=%lu, now=%lu\n", longcycle, tonetime, pretime, waketime, time(NULL));
		tonetime += longcycle ? 20 : 10;
		pretime = orig_pretime;
		goto tryagain;
	}

	ast_debug(5, "Currently %lu, tone at %lu, pretime is %d ms, sleeping for %d sec\n", now, tonetime, pretime, sleepsec);
	if (ast_safe_sleep(chan, sleepsec * 1000)) {
		return -1;
	}

	/* Answer, unless told not to */
	if (a->answer) {
		a->answer = 0;
		/* Avoid ast_auto_answer, since that uses ast_answer instead of ast_raw_answer,
		 * which could block for up to 500 ms. */
		if (ast_channel_state(chan) != AST_STATE_UP) {
			if (ast_raw_answer(chan)) {
				return -1;
			}
		} else {
			ast_log(LOG_WARNING, "Channel %s was already answered\n", ast_channel_name(chan));
		}
		/* Trip the ring. Stop any music on hold, ringing, etc. */
		ast_stopstream(chan);
		ast_moh_stop(chan);
		if (ast_safe_sleep(chan, 500)) {
			return -1;
		}
	}

	if (!ast_strlen_zero(a->advertisement)) {
		if (a->ad_is_dir) {
			PLAY_PROMPT(chan, adfile);
		} else {
			PLAY_PROMPT(chan, a->advertisement);
		}
	}

	if (!ast_strlen_zero(a->at_tone)) {
		PLAY_PROMPT(chan, a->at_tone);
	}

	if (a->promptdir) {
		PLAY_PROMPT(chan, a->hr);
		PLAY_PROMPT(chan, a->min);
		if (a->do_seconds) {
			if (exact) {
				if (!ast_strlen_zero(a->exactly)) {
					PLAY_PROMPT(chan, a->exactly);
				}
			} else {
				if (!ast_strlen_zero(a->and_prompt)) {
					PLAY_PROMPT(chan, a->and_prompt);
				}
				PLAY_PROMPT(chan, a->sec);
				if (!ast_strlen_zero(a->seconds)) {
					PLAY_PROMPT(chan, a->seconds);
				}
			}
		}
	} else {
		/* Use defaults */
		char app_args[256];
#define DEFAULT_TIME_SHORT_FMT "IM p"
#define DEFAULT_TIME_LONG_FMT "IM 'vm-and' S 'seconds'"
#define DEFAULT_TIME_LONG_FMT_MINUTE "IM"
		if (a->do_seconds) {
			snprintf(app_args, sizeof(app_args), "%lu,%s,%s", tonetime, S_OR(a->tz, ""), a->do_seconds ? !exact ? DEFAULT_TIME_LONG_FMT : DEFAULT_TIME_LONG_FMT_MINUTE : DEFAULT_TIME_SHORT_FMT);
		} else {
			snprintf(app_args, sizeof(app_args), ",%s,%s", S_OR(a->tz, ""), a->do_seconds ? DEFAULT_TIME_LONG_FMT : DEFAULT_TIME_SHORT_FMT);
		}
		res = ast_pbx_exec_application(chan, "SayUnixTime", app_args);
		if (res) {
			return res;
		}
	}

	/* Wait for the tone */
	ast_assert_return(time(NULL) <= tonetime, -1); /* We had one job... this can't be screwed up */
	if (a->do_seconds) {
		struct timeval tv;
		int ms;
		/* Use progressively shorter polling intervals as we get closer to the anticipated second. */
		while (time(NULL) < tonetime - 1) {
			if (ast_safe_sleep(chan, 1000)) {
				return -1;
			}
		}
		gettimeofday(&tv, NULL);
		ms = tv.tv_usec / 1000;
		/* 1000 - ms gives us number of milliseconds until the next second starts. */
		ms = 1000 - ms;
		if (ms > 5) {
			ms -= 3;
		}
		if (ast_safe_sleep(chan, ms)) {
			return -1;
		}
		while (time(NULL) != tonetime) {
			/* Make the tone accurate to within ~1 ms */
			if (ast_safe_sleep(chan, 1)) {
				return -1;
			}
		}
		PLAY_PROMPT(chan, a->tone);
	}

	if (a->do_temp) {
		if (ast_safe_sleep(chan, 100)) {
			return -1;
		}
		if (a->do_celsius) {
			int celsius_temp = (int) (((a->temp - 32) * 5) * 1.0 / 9);
			res = play_temperature(chan, a, a->temp);
			if (res < 0) {
				return -1;
			}
			PLAY_PROMPT(chan, a->fahrenheit);
			res = play_temperature(chan, a, celsius_temp);
			if (res < 0) {
				return -1;
			}
			PLAY_PROMPT(chan, a->celsius);
		} else {
			if (!ast_strlen_zero(a->temperature)) {
				PLAY_PROMPT(chan, a->temperature);
			}
			res = play_temperature(chan, a, a->temp);
			if (res < 0) {
				return -1;
			}
		}
	}

	return 0;
}

static int run_audichron(struct ast_channel *chan, struct audichron *a)
{
	int res;

	do {
		res = audichron_loop(chan, a); /* Single loop */
	} while (!res && (!a->iterations || --a->iterations));
	return res;
}

static int audichron_exec(struct ast_channel *chan, const char *data)
{
	struct ast_flags flags = {0};
	struct audichron a;
	char *tmp;
	char full_promptdir[PATH_MAX] = "";
	char full_temppromptdir[PATH_MAX] = "";
	char full_ad[PATH_MAX] = "";
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(promptdir);
		AST_APP_ARG(temppromptdir);
		AST_APP_ARG(tz);
		AST_APP_ARG(temperature);
		AST_APP_ARG(advertisement);
		AST_APP_ARG(iterations);
		AST_APP_ARG(options);
	);

	memset(&a, 0, sizeof(a));

	tmp = ast_strdupa(S_OR(data, ""));
	AST_STANDARD_APP_ARGS(args, tmp);

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(audichron_app_options, &flags, NULL, args.options);
		a.do_celsius = ast_test_flag(&flags, OPT_CELSIUS) ? 1 : 0;
		a.answer = ast_test_flag(&flags, OPT_NOANSWER) ? 0 : 1;
		a.do_seconds = ast_test_flag(&flags, OPT_SECONDS) ? 1 : 0;
		a.longcycle = ast_test_flag(&flags, OPT_LONG_CYCLE) ? 1 : 0;
	}

	if (!ast_strlen_zero(args.promptdir)) {
		if (args.promptdir[0] == '/') {
			a.promptdir = args.promptdir;
		} else {
			snprintf(full_promptdir, sizeof(full_promptdir), "%s/sounds/%s/%s", ast_config_AST_DATA_DIR, ast_channel_language(chan), args.promptdir);
			a.promptdir = full_promptdir;
		}
	}
	if (!ast_strlen_zero(args.temppromptdir)) {
		if (args.temppromptdir[0] == '/') {
			a.temppromptdir = args.temppromptdir;
		} else {
			snprintf(full_temppromptdir, sizeof(full_temppromptdir), "%s/sounds/%s/%s", ast_config_AST_DATA_DIR, ast_channel_language(chan), args.temppromptdir);
			a.temppromptdir = full_temppromptdir;
		}
	}
	if (!ast_strlen_zero(args.advertisement)) {
		struct stat st;
		int stat_res;
		if (args.advertisement[0] == '/') {
			a.advertisement = args.advertisement;
		} else {
			snprintf(full_ad, sizeof(full_ad), "%s/sounds/%s/%s", ast_config_AST_DATA_DIR, ast_channel_language(chan), args.advertisement);
			a.advertisement = full_ad;
		}
		/* To avoid needing to know file extension */
		stat_res = stat(a.advertisement, &st);
		a.ad_is_dir = !stat_res && S_ISDIR(st.st_mode);
		if (!a.ad_is_dir && !ast_fileexists(a.advertisement, NULL, ast_channel_language(chan))) {
			ast_log(LOG_WARNING, "No such file or directory: %s\n", a.advertisement);
			return -1;
		}
	}

	a.tz = S_OR(args.tz, NULL);
	if (!ast_strlen_zero(args.temperature)) {
		if (ast_str_to_int(args.temperature, &a.temp)) {
			ast_log(LOG_ERROR, "Invalid temperature: %s\n", args.temperature);
			return -1;
		}
		a.do_temp = 1;
	}
	if (!ast_strlen_zero(args.iterations)) {
		if (ast_str_to_int(args.iterations, &a.iterations) || a.iterations < 1) {
			ast_log(LOG_ERROR, "Invalid iterations: %s\n", args.iterations);
			return -1;
		}
	}
	if (a.promptdir && a.do_temp && !a.temppromptdir) {
		ast_log(LOG_ERROR, "Missing temppromptdir\n");
		return -1;
	}

	return run_audichron(chan, &a);
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, audichron_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Audichron Time and Temperature Announcements");
