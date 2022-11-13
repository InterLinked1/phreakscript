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
 * \brief Wakeup Call Scheduler
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<depend>pbx_spool</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <utime.h>
#include <sys/stat.h>

#include "asterisk/paths.h"	/* use ast_config_AST_SPOOL_DIR */
#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/say.h"
#include "asterisk/localtime.h"

/*** DOCUMENTATION
	<application name="ScheduleWakeupCall" language="en_US">
		<synopsis>
			Wakeup Call Scheduler
		</synopsis>
		<syntax>
			<parameter name="destination" required="true">
				<para>The <literal>extension@context</literal> destination to reach wakeup call user.</para>
			</parameter>
			<parameter name="userid" required="false">
				<para>A unique ID that can be used to delete schedule wakeup calls.</para>
				<para>This must contain only alphanumeric characters and hyphens. Underscores and other symbols are not allowed.</para>
			</parameter>
			<parameter name="timezone" required="false">
				<para>The time zone of the user.</para>
				<para>If provided, any time provided will be converted from the user's time zone to the system time zone.</para>
			</parameter>
			<parameter name="options" required="false">
				<optionlist>
					<option name="d">
						<para>Allow deletion of previously scheduled wakeup calls.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Wakeup Call Scheduler and Management Interface.</para>
			<para>Users can schedule one-time or recurring wakeup calls and, optionally, delete them.</para>
			<para>Recurring wakeup calls reschedule themselves when answered,
			and thus are automatically cancelled if they are unanswered.</para>
			<para>This application may require audio prompts in the Pat Fleet Asterisk sounds library that are not
			present in the default Allison Smith sounds library.</para>
		</description>
	</application>
	<application name="HandleWakeupCall" language="en_US">
		<synopsis>
			Wakeup call handler
		</synopsis>
		<syntax/>
		<description>
			<para>This application is used internally and should not be called from the dialplan.</para>
		</description>
	</application>
 ***/

static char *app = "ScheduleWakeupCall";
static char *app2 = "HandleWakeupCall";

static char qdir[255];

enum wuc_option_flags {
	OPT_ALLOW_DELETE = (1 << 0),
};

AST_APP_OPTIONS(wuc_option_flags, {
	AST_APP_OPTION('d', OPT_ALLOW_DELETE),
});

#define MAX_ATTEMPTS 3
#define TEMPLATE_NAME "app_wakeupcall_%s"

#define NO_RES_STREAM(file) \
	if (!res) { \
		res = ast_stream_and_wait(chan, file, AST_DIGIT_ANY); \
	}

enum wuc_type {
	WAKEUP_CALL_ANY,
	WAKEUP_CALL_SINGLE,
	WAKEUP_CALL_RECURRING,
};

struct wuc_data {
	char *prefix;
	char *userid;
	int prefixlength;
	int scheduled;
	char *timezone;
	char *destination;
	char *callerid;
	unsigned int delete:1;
	enum wuc_type filter;
};

static int invalid_userid(const char *s)
{
	while (*s) {
		if (!isalnum(*s) && *s != '-') {
			return -1;
		}
		s++;
	}
	return 0;
}

/*! \brief Directory iterator callback */
static int on_call_file(const char *dir_name, const char *filename, void *obj)
{
	struct wuc_data *wuc = obj;
	enum wuc_type wuctype;
	const char *tmp;

	ast_debug(4, "Found call file: %s\n", filename);
	if (strncmp(wuc->prefix, filename, wuc->prefixlength)) {
		return 0; /* Not us. */
	}

	/* Determine what type of wakeup call this is, by the filename. */
	tmp = filename + strlen("app_wakeupcall");
	if (ast_strlen_zero(tmp)) {
		return 0;
	}
	tmp = strchr(tmp, '_');
	if (!tmp++) {
		return 0;
	}
	tmp = strchr(tmp, '_');
	if (!tmp++) {
		return 0;
	}

	if (!strncmp(tmp, "single", 6)) {
		wuctype = WAKEUP_CALL_SINGLE;
	} else if (!strncmp(tmp, "rec", 3)) {
		wuctype = WAKEUP_CALL_RECURRING;
	} else {
		wuctype = WAKEUP_CALL_ANY;
		ast_debug(3, "Failed to determine wake up call type: %s\n", tmp);
	}

	if (wuc->filter != WAKEUP_CALL_ANY && wuctype != wuc->filter) {
		return 0; /* Some kind of wakeup call that we don't care about. */
	}

	ast_debug(2, "Call file match (%s): %s\n", filename,
		wuctype == WAKEUP_CALL_SINGLE ? "single" : wuctype == WAKEUP_CALL_RECURRING ? "recurring" : "(unknown)");

	if (wuc->delete) {
		char fullpath[PATH_MAX];
		snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_name, filename);
		if (unlink(fullpath)) {
			ast_log(LOG_WARNING, "Failed to delete file %s: %s\n", fullpath, strerror(errno));
			return 0;
		} else {
			ast_debug(3, "Deleted %s\n", fullpath);
			wuc->scheduled++; /* Count of relevant matching wake up calls (even if we delete them all) */
		}
	} else {
		wuc->scheduled++; /* Count of relevant matching wake up calls (even if we delete them all) */
	}
	return 1;
}

static int confirm(struct ast_channel *chan, time_t epoch, enum wuc_type type, const char *timezone)
{
	int res, attempts = 0;

	for (;;) {
		attempts++;

		res = ast_stream_and_wait(chan, "rqsted-wakeup-for", AST_DIGIT_NONE);
		/* If it's recurring, don't announce a specific date for the wakeup call (although the first one will also be at whatever ABd would say). */
		res = ast_say_date_with_format(chan, epoch, AST_DIGIT_NONE, ast_channel_language(chan), type == WAKEUP_CALL_SINGLE ? "ABd IM p" : "IM p", timezone);
#if 0
		/* "at" currently isn't a standard library prompt, even in the Fleet library.
		 * I have a copy of it, but to keep this generic we don't use it here.
		 * If we did, then we would do one call to say_date with ABd and another with IM p
		 */
		res = ast_stream_and_wait(chan, "at", AST_DIGIT_NONE);
#endif

		NO_RES_STREAM("to-confirm-wakeup");
		NO_RES_STREAM("press-1");
		NO_RES_STREAM("to-cancel-wakeup");
		NO_RES_STREAM("press-0");
		NO_RES_STREAM("silence/5");

		if (res > 0) {
			switch (res) {
			case '1':
				return 0;
			case '0':
				return 2;
			default:
				/* Invalid response. */
				break;
			}
		}

		if (attempts > MAX_ATTEMPTS) {
			return 1;
		}
		if (res < 0) {
			return res;
		}
	}

	return res;
}

static int create_call_file(struct wuc_data *wuc, enum wuc_type type, time_t epoch, int yr, int month, int day, int hr, int min, int uhr, int umin)
{
	int res = 0;
	int fd;
	FILE *fp;
	char *tmp_filename;
	char template[64] = "wakeupcallXXXXXX";
	char spool_fname[PATH_MAX];
	char timestr[32];
	struct stat finfo;
	struct utimbuf new_times;

	snprintf(timestr, sizeof(timestr), "%04d%02d%02d_%02d%02d", yr, month, day, hr, min);
	snprintf(spool_fname, sizeof(spool_fname), "%s/%s_%s_%s.call", qdir, wuc->prefix, type == WAKEUP_CALL_SINGLE ? "single" : "rec", timestr);

	fd = ast_file_fdtemp("/tmp", &tmp_filename, template);
	if (fd < 0) {
		ast_log(LOG_ERROR, "Failed to get temporary file descriptor\n");
		return -1;
	}

	fp = fdopen(fd, "wb");
	if (!fp) {
		ast_log(LOG_WARNING, "Failed to create temp file\n");
		return -1;
	}

	fprintf(fp, "Channel: %s\n", wuc->destination);
	fprintf(fp, "Application: %s\n", app2);
	fprintf(fp, "Data: %s_%s_%02d%02d\n", wuc->userid, type == WAKEUP_CALL_SINGLE ? "single" : "rec", uhr, umin);
	fprintf(fp, "CallerID: WAKEUP CALL <%s>\n", S_OR(wuc->callerid, ""));
	fprintf(fp, "MaxRetries: %d\n", 3);
	fprintf(fp, "RetryTime: %d\n", 540); /* The standard "snooze" time is 9 minutes. */
	fprintf(fp, "Setvar: wuc_destination=%s\n", wuc->destination);
	fprintf(fp, "Setvar: wuc_timezone=%s\n", wuc->timezone);
	fclose(fp);

	/* Set modified timestamp to the time of the wakeup call. */
	stat(tmp_filename, &finfo);
	new_times.actime = finfo.st_atime; /* Same atime */
	new_times.modtime = epoch; /* Set mtime to time of wakeup call. */

	if (utime(tmp_filename, &new_times)) {
		ast_log(LOG_WARNING, "Failed to adjust modification time to the future\n");
		res = -1;
	} else {
		/* Move to spool directory */
		ast_debug(3, "Renaming %s -> %s\n", tmp_filename, spool_fname);
		/* No real risk to renaming to a file that already exists either, because if the file name is the same, its contents likely are, too. */
		if (rename(tmp_filename, spool_fname)) {
			ast_log(LOG_WARNING, "Failed to move call file to spool directory: %s\n", strerror(errno));
			res = -1;
		}
	}

	ast_free(tmp_filename);
	return res;
}

static int scheduler(struct ast_channel *chan, int hr, int min, enum wuc_type type, struct wuc_data *wuc, int autoconfirm)
{
	struct timeval when;
	struct ast_tm tm;
	int tomorrow = 0;
	char tmstr[60];
	time_t epoch;
	int res;
	const char *timezone = wuc->timezone;

	/* First, fetch the time using the system time zone instead of the user's time zone.
	 * Note that in this function, by system time, we mean the system's local time, not necessarily UTC. */
	ast_localtime(&when, &tm, NULL);
	ast_debug(3, "Current time in system timezone %s: %03d-%02d-%02d %02d:%02d:%02d\n", S_OR(timezone, "(default)"),
		tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	/* Next, get the current time right now, in the caller's time zone. */
	ast_get_timeval(NULL, &when, ast_tvnow(), NULL);
	ast_localtime(&when, &tm, timezone); /* If timezone is NULL, it will be the system default. */

	ast_debug(3, "Current time in timezone %s: %03d-%02d-%02d %02d:%02d:%02d\n", S_OR(timezone, "(default)"),
		tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	/* Now, determine what the smallest year/month/day is that's in the future. */
	if (tm.tm_hour > hr || (tm.tm_hour == hr && tm.tm_min >= min)) {
		/* It's already past that time today, so schedule it for tomorrow. */
		tomorrow = 1;
	}

	/* Use the current time as our base, since today is a valid date, just update the hour and minute. */
	tm.tm_hour = hr;
	tm.tm_min = min;
	tm.tm_sec = 0; /* Don't need seconds. */

/* Months are 0-indexed */
#define MONTH_ROLLOVER(y, m, d) (d > 31 || (m == 1 && d > 29) || (m == 1 && d > 28 && (y % 4 > 0 || y % 100 == 0)) || (d > 30 && (m == 4 || m == 6 || m == 9 || m == 11)))

	/* One approach might be converting to epoch time, adding a day, and converting back.
	 * But this won't properly account for Daylight Saving Time.
	 * Also for that reason, any math we do must be on the user's time zone, not the system time zone.
	 */
	if (tomorrow) {
		tm.tm_mday += 1;
		if (MONTH_ROLLOVER(tm.tm_year, tm.tm_mon, tm.tm_mday)) {
			tm.tm_mday = 1;
			tm.tm_mon += 1;
		}
		if (tm.tm_mon >= 12) {
			tm.tm_mon = 0;
			tm.tm_year += 1;
		}
	} else if (autoconfirm) {
		/* If we're rescheduling it, the user just answered, so there's no way there can be another one today.
		 * The only exception is if the call was scheduled for one day and actually answered the next (due to snooze, etc.) */
		ast_debug(1, "Possible bug? Rescheduling existing sequence for same day?\n");
	}

	ast_debug(3, "Future time in user timezone %s: %03d-%02d-%02d %02d:%02d:%02d\n", S_OR(timezone, "(default)"),
		tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

	/* Convert the future time in user's time zone to the system time zone.
	 * Again, because of DST, we can't simply used a calculated offset.
	 * First, convert to epoch time.
	 */
	snprintf(tmstr, sizeof(tmstr), "%4d-%02d-%02d %02d:%02d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
	ast_debug(3, "Time string: %s\n", tmstr);

	if (!ast_strptime(tmstr, "%Y-%m-%d %H:%M", &tm)) {
		ast_log(LOG_WARNING, "strptime found no time specified within the string\n");
		return -1;
	}

	when = ast_mktime(&tm, timezone); /* Convert to epoch time, from the user's time zone. */
	epoch = when.tv_sec;
	ast_debug(3, "Epoch is %ld\n", epoch);

	if (time(NULL) > epoch) {
		/* We shouldn't be scheduling anything in the past... */
		ast_log(LOG_WARNING, "Epoch %ld is already past?\n", epoch);
		return -1;
	}

	/* INTERMISSION: Confirm with the user that this is actually correct. */
	if (!autoconfirm) {
		res = confirm(chan, epoch, type, timezone);
		if (res == 2) {
			/* Not correct. */
			res = ast_stream_and_wait(chan, "wakeup-call-cancelled", AST_DIGIT_NONE);
			return res;
		} else if (res) {
			return res;
		}
	}

	/* Finally, convert the epoch time back into system time. */
	snprintf(tmstr, sizeof(tmstr), "%ld", when.tv_sec);
	ast_get_timeval(tmstr, &when, ast_tvnow(), NULL);
	ast_localtime(&when, &tm, NULL);

	ast_verb(4, "%s %s wakeup call for %04d-%02d-%02d %02d:%02d\n",
		autoconfirm ? "Rescheduling" : "Scheduling",
		type == WAKEUP_CALL_SINGLE ? "one-time" : "daily",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);

	/* Actually generate the call file. */
	return create_call_file(wuc, type, epoch, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, hr, min);
}

#define WUC_ASSERT(x) if (!(x)) { ast_log(LOG_ERROR, "Assertion failed\n"); return -1; }

static int reschedule(struct ast_channel *chan, const char *data)
{
	char *userid, *type, *timestr;
	struct wuc_data wuc;
	int hr, min;
	char template_name[PATH_MAX];
	const char *tmpvar;
	char *destination, *timezone;
	char *s = ast_strdupa(data);

	/* Data format from the call file is:
	 * userid_type_HHMM */
	userid = strsep(&s, "_");
	WUC_ASSERT(userid);
	type = strsep(&s, "_");
	WUC_ASSERT(type);
	timestr = strsep(&s, "_");
	WUC_ASSERT(timestr);

	ast_debug(3, "Rescheduling parameters: %s/%s/%s\n", userid, type, timestr);
	if (strcmp(type, "rec")) {
		ast_debug(2, "Not recurring, not relevant\n");
		return 0;
	}

	min = atoi(timestr + 2);
	*(timestr + 2) = '\0';
	hr = atoi(timestr);

	memset(&wuc, 0, sizeof(wuc));
	snprintf(template_name, sizeof(template_name), TEMPLATE_NAME, userid);

	ast_channel_lock(chan);
	tmpvar = pbx_builtin_getvar_helper(chan, "wuc_destination");
	if (ast_strlen_zero(tmpvar)) {
		ast_channel_unlock(chan);
		return -1;
	}
	destination = ast_strdupa(tmpvar);
	tmpvar = pbx_builtin_getvar_helper(chan, "wuc_timezone");
	if (ast_strlen_zero(tmpvar)) {
		ast_channel_unlock(chan);
		return -1;
	}
	timezone = ast_strdupa(tmpvar);
	ast_channel_unlock(chan);

	wuc.prefix = template_name;
	wuc.userid = userid;
	wuc.prefixlength = strlen(template_name);
	wuc.destination = destination;
	/* We do need to use the user's time zone, because Daylight Saving could happen at any point.
	 * So always start from the time the user originally entered when originally scheduling the
	 * wakeup call, and do the needed conversions to system time, each time. */
	wuc.timezone = timezone;
	wuc.callerid = ast_channel_caller(chan)->id.number.str;

	/* Create the next one, no confirmation required. */
	return scheduler(chan, hr, min, WAKEUP_CALL_RECURRING, &wuc, 1);
}

static int schedule(struct ast_channel *chan, enum wuc_type type, struct wuc_data *wuc)
{
	int res;
	int iterations = 0;
	char time[5];
	char hour[3], minute[3];
	char promptfiles[PATH_MAX] = "";
	int hr, min;

	/* ast_app_getdata will return > 0 if we haven't gotten a response.
	 * However, specifying ~no timeout will cause it to return without the user being able to enter a full response.
	 * The workaround is to call ast_app_getdata only once with all the files it needs. */

	strncat(promptfiles, type == WAKEUP_CALL_SINGLE ? "wakeup-onetime" : "wakeup-daily", sizeof(promptfiles) - 1);
	if (!wuc->scheduled) {
		strncat(promptfiles, "&not-rqsted-wakeup", sizeof(promptfiles) - 1);
	}
	strncat(promptfiles, "&to-rqst-wakeup-call&enter-a-time", sizeof(promptfiles) - 1);

	for (;;) {
		iterations++;

		/* Use sizeof buffer - 1 so that it will return after 4 digits, rather than waiting for another one. */
		res = ast_app_getdata(chan, promptfiles, time, sizeof(time) - 1, 0); /* Use standard timeout */

		ast_copy_string(promptfiles, "to-rqst-wakeup-call&enter-a-time", sizeof(promptfiles)); /* Never again say the intro. */

		if (res > 0 && iterations <= MAX_ATTEMPTS) {
			continue; /* No response, prompt again. */
		} else if (!res) {
			ast_copy_string(hour, time, sizeof(hour));
			ast_copy_string(minute, time + 2, sizeof(minute));
			hr = atoi(hour);
			min = atoi(minute);

			if (hr > 23 || min > 59) {
				ast_verb(4, "User entered invalid time %02d:%02d\n", hr, min);
				res = ast_stream_and_wait(chan, "option-is-invalid", AST_DIGIT_ANY);
				continue;
			}
			ast_verb(4, "User entered %02d:%02d\n", hr, min);

			/* If ambiguous, confirm AM/PM */
			while (hr > 0 && hr < 13 && iterations <= MAX_ATTEMPTS && res >= 0) {
				NO_RES_STREAM("1-for-am-2-for-pm");
				if (res == '1') {
					if (hr == 12) {
						hr = 0; /* Conver to 24 hour time */
					}
					break;
				} else if (res == '2') {
					 /* Convert to 24 hour time */
					if (hr != 12) {
						hr += 12;
					}
					break;
				} else {
					++iterations;
					if (res) { /* Only play if we get *some* response. */
						res = ast_stream_and_wait(chan, "option-is-invalid", AST_DIGIT_ANY);
					}
				}
			}

			ast_debug(1, "24-hour time is %02d:%02d\n", hr, min);
			res = scheduler(chan, hr, min, type, wuc, 0);
			if (res > 0) {
				res = ast_stream_and_wait(chan, "option-is-invalid", AST_DIGIT_ANY);
				continue;
			} else if (!res) {
				/* XXX This is admittedly not the best prompt... but we need *something*
				 * to indicate that it worked (importantly, in the available prompt library)... */
				res = ast_stream_and_wait(chan, "thank-you", AST_DIGIT_NONE);
				break; /* Done */
			}
		}

		if (iterations > MAX_ATTEMPTS) {
			return 1;
		}

		if (res < 0) {
			break;
		}
	}

	return res;
}

static int wuc_respond_exec(struct ast_channel *chan, const char *data)
{
	int i, res;

	if (ast_strlen_zero(data)) {
		ast_log(LOG_ERROR, "%s called without arguments. Do not attempt to call this from the dialplan.\n", data);
	}

	/* Reschedule the next call, if necessary. */
	if (reschedule(chan, data)) {
		ast_log(LOG_WARNING, "Failed to reschedule wakeup call\n");
	}

	for (i = 0; i < 20; i++) {
		res = ast_stream_and_wait(chan, "this-is-yr-wakeup-call", AST_DIGIT_ANY);
		if (!res) {
			res = ast_waitstream(chan, NULL);
		}
		if (!res) {
			res = ast_safe_sleep(chan, 2000); /* Pause 2 seconds between announcements. */
		}
		/* If caller hung up, or user entered a digit, hangup.
		 * This way if the caller's line is held by this incoming call,
		 * s/he can force it to release by pressing a digit. */
		if (res) {
			break;
		}
	}

	return -1; /* Do not continue and try to do anything else. */
}

static int wuc_exec(struct ast_channel *chan, const char *data)
{
	char *tmp;
	int res = 0;
	struct ast_flags flags = {0};
	int iterations = 0;
	char template_name[PATH_MAX];
	struct wuc_data wuc;
	int allow_delete = 0;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(destination);
		AST_APP_ARG(userid);
		AST_APP_ARG(timezone);
		AST_APP_ARG(callerid);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "%s requires arguments\n", app);
		return -1;
	}

	tmp = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, tmp);

	if (ast_strlen_zero(args.destination)) {
		ast_log(LOG_WARNING, "A destination is required\n");
		return -1;
	}
	if (!strchr(args.destination, '/')) {
		ast_log(LOG_WARNING, "Destination does not contain a Technology/Resource\n");
		return -1;
	}

	memset(&wuc, 0, sizeof(wuc));

	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(wuc_option_flags, &flags, NULL, args.options);
		if (ast_test_flag(&flags, OPT_ALLOW_DELETE)) {
			allow_delete = 1;
		}
	}
	/* This must follow allow_delete potentially being set. */
	if (ast_strlen_zero(args.userid)) {
		if (allow_delete) {
			ast_log(LOG_WARNING, "Deletion requires that a unique user ID be provided.\n");
		}
		allow_delete = 0;
	} else if (invalid_userid(args.userid)) {
		/* Underscore not allowed because we use that as a field separator internally.
		 * Most other characters not allowed since they could cause issues with filenames. */
		ast_log(LOG_WARNING, "User ID '%s' contains invalid characters (use only A-Za-z0-9-)\n", args.userid);
		return -1;
	}

	/* We will append the timestamp for scheduled wakeup calls. */
	snprintf(template_name, sizeof(template_name), "app_wakeupcall_%s", S_OR(args.userid, ""));

	wuc.prefix = template_name;
	wuc.userid = args.userid;
	wuc.prefixlength = strlen(template_name);
	wuc.destination = args.destination;
	wuc.timezone = args.timezone;
	wuc.callerid = args.callerid;

	for (;;) {
		iterations++;

		/* Don't repeat this if we prompt again in a sequence. */
		if (iterations == 1 && !ast_strlen_zero(args.userid)) {
			wuc.scheduled = 0; /* Reset counter. */
			wuc.filter = WAKEUP_CALL_ANY;
			/* If we know the user's unique ID, then we can figure out if there are any wakeup calls currently scheduled. */
			ast_file_read_dir(qdir, on_call_file, &wuc);
			if (wuc.scheduled) {
				ast_debug(3, "%d match(es) found\n", wuc.scheduled);
			} else {
				ast_debug(3, "No matches found\n");
				res = ast_stream_and_wait(chan, "not-rqsted-wakeup", AST_DIGIT_ANY);
			}
		}

		/* Main Menu */
		NO_RES_STREAM("wakeup-for-one-time");
		NO_RES_STREAM("press-1");
		NO_RES_STREAM("for-a-daily-wakeup-call");
		NO_RES_STREAM("press-2");
		NO_RES_STREAM("to-cancel-wakeup");
		NO_RES_STREAM("press-0");
		NO_RES_STREAM("silence/5");

		if (res > 0) {
			switch (res) {
			case '1': /* One-time wakeup call */
				wuc.scheduled = 0; /* Reset counter. */
				wuc.filter = WAKEUP_CALL_SINGLE;
				ast_file_read_dir(qdir, on_call_file, &wuc);
				res = schedule(chan, WAKEUP_CALL_SINGLE, &wuc);
				break;
			case '2': /* Daily wakeup call */
				if (ast_strlen_zero(args.userid)) {
					ast_verb(4, "Recurring wakeup calls not allowed without user ID\n");
					break;
				}
				wuc.scheduled = 0; /* Reset counter. */
				wuc.filter = WAKEUP_CALL_RECURRING;
				ast_file_read_dir(qdir, on_call_file, &wuc);
				res = schedule(chan, WAKEUP_CALL_RECURRING, &wuc);
				break;
			/*! \todo This application is still very simplistic.
			 * We should also allow scheduling recurring calls during the week/weekend,
			 * and other more flexible options. */
			case '0':
				if (!allow_delete) {
					ast_verb(4, "Deletion is not allowed\n");
					break;
				}
				wuc.scheduled = 0; /* Reset counter. */
				wuc.filter = WAKEUP_CALL_ANY;
				wuc.delete = 1; /* Delete any matches. */
				ast_file_read_dir(qdir, on_call_file, &wuc);
				wuc.delete = 0;
				if (wuc.scheduled) {
					ast_debug(3, "%d match(es) found\n", wuc.scheduled);
					res = ast_stream_and_wait(chan, "wakeup-call-cancelled", AST_DIGIT_NONE);
				} else {
					ast_debug(3, "No matches found\n");
					/* The next loop will announce that there aren't any wakeup calls currently scheduled. */
				}
				break;
			default:
				/* Invalid option. */
				break;
			}
		} else if (!res && iterations <= MAX_ATTEMPTS) {
			continue; /* No response, prompt again. */
		}

		if (res >= 0 && iterations > MAX_ATTEMPTS) {
			ast_verb(4, "Too many failed attempts, aborting\n");
			res = ast_stream_and_wait(chan, "goodbye", AST_DIGIT_NONE);
			break;
		} else if (res > 0) {
			/* res was not set to 0, so we failed. */
			ast_debug(2, "Invalid option (iteration %d)\n", iterations);
			res = ast_stream_and_wait(chan, "option-is-invalid", AST_DIGIT_ANY);
		} else if (!res) {
			/* As long as the user behaves, allow him or her to keep changing features. */
			iterations = 0;
		}

		if (res < 0) {
			break;
		}

		/* Pause between loops on successful actions. */
		if (ast_safe_sleep(chan, 1500)) {
			return -1;
		}
	}

	return res < 0 ? -1 : 0;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(app);
	res = ast_unregister_application(app2);

	return res;
}

static int load_module(void)
{
	int res = 0;

	snprintf(qdir, sizeof(qdir), "%s/%s", ast_config_AST_SPOOL_DIR, "outgoing");

	res |= ast_register_application_xml(app, wuc_exec);
	res |= ast_register_application_xml(app2, wuc_respond_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Wakeup Call Scheduler");
