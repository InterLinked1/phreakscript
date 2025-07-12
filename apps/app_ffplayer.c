/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2025, Naveen Albert
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
 * \brief Application to play audio streams using ffmpeg
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \note Based off app_mp3, by Mark Spencer <markster@digium.com>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>

#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/frame.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/translate.h"
#include "asterisk/app.h"
#include "asterisk/format_cache.h"

#define FFMPEG "/usr/bin/ffmpeg"

/*** DOCUMENTATION
	<application name="FFPlayer" language="en_US">
		<synopsis>
			Play a file or stream using ffmpeg.
		</synopsis>
		<syntax>
			<parameter name="Location" required="true">
				<para>Location of the file to be played.
				(argument passed to ffmpeg)</para>
			</parameter>
		</syntax>
		<description>
			<para>Executes ffmpeg to play the given location.</para>
			<para>User can exit by pressing any key on the dialpad, or by hanging up.</para>
			<example title="Play an FF playlist">
			exten => 1234,1,FFPlayer(/var/lib/asterisk/playlist.m3u)
			</example>
			<para>This application does not automatically answer and should be preceded by an
			application such as Answer() or Progress().</para>
		</description>
	</application>

 ***/
static char *app = "FFPlayer";

static int ffplay(const char *filename, unsigned int sampling_rate, int fd)
{
	int res;
	char sampling_rate_str[8];

	res = ast_safe_fork(0);
	if (res < 0)
		ast_log(LOG_WARNING, "Fork failed\n");
	if (res) {
		return res;
	}
	if (ast_opt_high_priority)
		ast_set_priority(0);

	dup2(fd, STDOUT_FILENO);
	ast_close_fds_above_n(STDERR_FILENO);

	snprintf(sampling_rate_str, 8, "%u", sampling_rate);

	execl(FFMPEG, "ffmpeg", "-i", filename, "-ar", sampling_rate_str, "-ac", "1", "-acodec", "pcm_s16le", "-f", "s16le", "-bufsize", "64k", "pipe:1", (char *) NULL);

	/* Can't use ast_log since FD's are closed */
	fprintf(stderr, "Execute of ffmpeg failed\n");
	_exit(0);
}

static int timed_read(int fd, void *data, int datalen, int timeout, int pid)
{
	int res;
	int i;
	struct pollfd fds[1];
	fds[0].fd = fd;
	fds[0].events = POLLIN;
	for (i = 0; i < timeout; i++) {
		res = ast_poll(fds, 1, 1000);
		if (res > 0) {
			break;
		} else if (res == 0) {
			ast_log(LOG_WARNING, "No activity returned from stream\n");
			/* is ffmpeg still running? */
			kill(pid, 0);
			if (errno == ESRCH) {
				return -1;
			}
		} else {
			ast_log(LOG_NOTICE, "error polling ffmpeg: %s\n", strerror(errno));
			return -1;
		}
	}

	if (i == timeout) {
		ast_log(LOG_NOTICE, "Poll timed out.\n");
		return -1;
	}

	return read(fd, data, datalen);

}

static int ff_exec(struct ast_channel *chan, const char *data)
{
	int res=0;
	int fds[2];
	int ms = -1;
	int pid = -1;
	RAII_VAR(struct ast_format *, owriteformat, NULL, ao2_cleanup);
	int timeout = 2;
	int startedff = 0;
	struct timeval next;
	struct ast_frame *f;
	struct myframe {
		struct ast_frame f;
		char offset[AST_FRIENDLY_OFFSET];
		short frdata[160];
	} myf = {
		.f = { 0, },
	};
	struct ast_format * native_format;
	unsigned int sampling_rate;
	struct ast_format * write_format;

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "FFPlayer requires an argument (filename)\n");
		return -1;
	}

	if (pipe(fds)) {
		ast_log(LOG_WARNING, "Unable to create pipe\n");
		return -1;
	}

	ast_stopstream(chan);

	native_format = ast_format_cap_get_format(ast_channel_nativeformats(chan), 0);
	sampling_rate = ast_format_get_sample_rate(native_format);
	write_format = ast_format_cache_get_slin_by_rate(sampling_rate);

	owriteformat = ao2_bump(ast_channel_writeformat(chan));
	res = ast_set_write_format(chan, write_format);
	if (res < 0) {
		ast_log(LOG_WARNING, "Unable to set write format to signed linear\n");
		return -1;
	}

	myf.f.frametype = AST_FRAME_VOICE;
	myf.f.subclass.format = write_format;
	myf.f.mallocd = 0;
	myf.f.offset = AST_FRIENDLY_OFFSET;
	myf.f.src = __PRETTY_FUNCTION__;
	myf.f.delivery.tv_sec = 0;
	myf.f.delivery.tv_usec = 0;
	myf.f.data.ptr = myf.frdata;

	res = ffplay(data, sampling_rate, fds[1]);
	if (!strncasecmp(data, "http://", 7)) {
		timeout = 10;
	}
	/* Wait 1000 ms first */
	next = ast_tvnow();
	next.tv_sec += 1;
	if (res >= 0) {
		pid = res;
		/* Order is important -- there's almost always going to be ff...  we want to prioritize the
		   user */
		for (;;) {
			ms = ast_tvdiff_ms(next, ast_tvnow());
			if (ms <= 0) {
				res = timed_read(fds[0], myf.frdata, sizeof(myf.frdata), timeout, pid);
				if (res > 0) {
					myf.f.datalen = res;
					myf.f.samples = res / 2;
					startedff = 1;
					if (ast_write(chan, &myf.f) < 0) {
						res = -1;
						break;
					}
				} else {
					ast_debug(1, "No more ffmpeg\n");
					if (!startedff) { /* we couldn't do anything, which means this stream doesn't work */
						if (!strncasecmp(data, "https://", 8)) {
							ast_log(LOG_WARNING, "%s() does not support HTTPS streams. Use HTTP instead.\n", app);
						}
						ast_log(LOG_WARNING, "ffmpeg stream '%s' is broken or nonexistent\n", data);
					}
					res = 0;
					break;
				}
				next = ast_tvadd(next, ast_samp2tv(myf.f.samples, sampling_rate));
			} else {
				ms = ast_waitfor(chan, ms);
				if (ms < 0) {
					ast_debug(1, "Hangup detected\n");
					res = -1;
					break;
				}
				if (ms) {
					f = ast_read(chan);
					if (!f) {
						ast_debug(1, "Null frame == hangup() detected\n");
						res = -1;
						break;
					}
					if (f->frametype == AST_FRAME_DTMF) {
						ast_debug(1, "User pressed a key\n");
						ast_frfree(f);
						res = 0;
						break;
					}
					ast_frfree(f);
				}
			}
		}
	}
	close(fds[0]);
	close(fds[1]);

	if (pid > -1)
		kill(pid, SIGKILL);
	if (!res && owriteformat)
		ast_set_write_format(chan, owriteformat);

	ast_frfree(&myf.f);

	return res;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, ff_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "FFmpeg player application");
