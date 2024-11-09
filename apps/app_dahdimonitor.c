/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2023, Naveen Albert
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
 * \brief Monitor a DAHDI channel
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<depend>dahdi</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <dahdi/user.h>

#include "asterisk/app.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/format_cache.h"

/*** DOCUMENTATION
	<application name="DAHDIMonitor" language="en_US">
		<synopsis>
			Monitor a DAHDI channel
		</synopsis>
		<syntax>
			<parameter name="channel" required="yes">
				<para>DAHDI channel number, as it appears in <literal>system.conf</literal>.</para>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="l">
						<para>Stop reading after a specified number of bytes.</para>
					</option>
					<option name="p">
						<para>Monitor pre-echocancelled audio.</para>
					</option>
					<option name="r">
						<para>Monitor received audio only.</para>
					</option>
					<option name="t">
						<para>Monitor transmitted audio only.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>This monitors a DAHDI channel, similar to the dahdi_monitor utility provided with DAHDI Tools.</para>
			<para>This can be used to monitor DAHDI channels using Asterisk. DAHDI channels correspond to channels
			configured in DAHDI's system.conf, NOT Asterisk channels. If you want to monitor chan_dahdi channels
			in Asterisk, use <literal>DAHDISpy</literal> instead.</para>
			<para>This is essentially the audio equivalent of a lineman's test set for DAHDI channels, in software,
			allowing you to monitor DAHDI channels remotely using Asterisk (unlike dahdi_monitor, which requires a sound card),
			or using a physical test set connected to a trunk.</para>
			<para>Some (but not all) of the options for dahdi_monitor are also available with this application.
			If you just want to save monitored audio to a file, you should use the dahdi_monitor tool directly.</para>
			<para>To exit, you may press any DTMF digit or hang up.</para>
		</description>
		<see-also>
			<ref type="application">DAHDISpy</ref>
		</see-also>
	</application>
 ***/

static char *app = "DAHDIMonitor";

enum dahdi_monitor_opts {
	OPT_LIMIT = (1 << 0),
	OPT_PREECHOCANCEL = (1 << 1),
	OPT_RX = (1 << 2),
	OPT_TX = (1 << 3),
};

enum {
	OPT_ARG_LIMIT,
	OPT_ARG_ARRAY_SIZE,
};

AST_APP_OPTIONS(app_opts, {
	AST_APP_OPTION_ARG('l', OPT_LIMIT, OPT_ARG_LIMIT),
	AST_APP_OPTION('p', OPT_PREECHOCANCEL),
	AST_APP_OPTION('r', OPT_RX),
	AST_APP_OPTION('t', OPT_TX),
});

/*! Chunk size to read -- we use 20ms chunks to make things happy. */
#define READ_SIZE 160
#define LINEAR_READ_SIZE READ_SIZE * 2
#define CONF_SIZE 320

/*! each buffer is 20ms, so this is 640ms total */
#define DEFAULT_AUDIO_BUFFERS  32

static int pseudo_open(void)
{
	int fd;
	struct dahdi_bufferinfo bi;
	int x = 1;

	fd = open("/dev/dahdi/pseudo", O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		ast_log(LOG_ERROR, "Unable to open pseudo channel: %s\n", strerror(errno));
		return -1;
	}

	/* Set up buffering information */
	memset(&bi, 0, sizeof(bi));
	bi.bufsize = CONF_SIZE / 2;
	bi.txbufpolicy = DAHDI_POLICY_IMMEDIATE;
	bi.rxbufpolicy = DAHDI_POLICY_IMMEDIATE;
	bi.numbufs = DEFAULT_AUDIO_BUFFERS;
	if (ioctl(fd, DAHDI_SET_BUFINFO, &bi)) {
		ast_log(LOG_WARNING, "Unable to set buffering information: %s\n", strerror(errno));
		close(fd);
		return -1;
	}

	if (ioctl(fd, DAHDI_SETLINEAR, &x)) {
		ast_log(LOG_ERROR, "Unable to set linear mode: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	x = READ_SIZE;
	if (ioctl(fd, DAHDI_SET_BLOCKSIZE, &x)) {
		ast_log(LOG_ERROR, "Unable to set sane block size: %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	return fd;
}

static int dahdi_monitor_exec(struct ast_channel *chan, const char *data)
{
	int res = -1;
	struct ast_flags flags;
	char *opt_args[OPT_ARG_ARRAY_SIZE];
	char *parse;
	int sofar = 0, maxbytes = 0;
	int dahdichan;
	int dfd = -1;
	int skip_dahdi = 0;
	struct dahdi_confinfo dahdic;
	char buf[CONF_SIZE + AST_FRIENDLY_OFFSET];
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(chan);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_ERROR, "DAHDIMonitor requires arguments\n");
		return -1;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	if (ast_strlen_zero(args.chan)) {
		ast_log(LOG_ERROR, "DAHDIMonitor requires a channel number\n");
		return -1;
	}
	dahdichan = atoi(args.chan);
	memset(&flags, 0, sizeof(flags));
	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(app_opts, &flags, opt_args, args.options);
	}
	if (ast_test_flag(&flags, OPT_LIMIT) && !ast_strlen_zero(opt_args[OPT_ARG_LIMIT])) {
		maxbytes = atoi(opt_args[OPT_ARG_LIMIT]);
	}

	if (ast_test_flag(&flags, OPT_TX) && ast_test_flag(&flags, OPT_RX)) {
		ast_log(LOG_ERROR, "The TX and RX options are mutually exclusive. Omit both to monitor both directions simultaneously.\n");
		return -1;
	}

	ast_channel_set_writeformat(chan, ast_format_slin);
	ast_channel_set_rawwriteformat(chan, ast_format_slin);
	ast_channel_set_readformat(chan, ast_format_slin);
	ast_channel_set_rawreadformat(chan, ast_format_slin);
	if (ast_format_cmp(ast_channel_rawreadformat(chan), ast_format_slin) != AST_FORMAT_CMP_EQUAL) {
		ast_log(LOG_WARNING, "Channel %s not in slinear mode\n", ast_channel_name(chan));
		return -1;
	}

	/* Open psuedo device */
	dfd = pseudo_open();
	if (dfd < 0) {
		return -1;
	}

	memset(&dahdic, 0, sizeof(dahdic));
	dahdic.chan = 0;
	dahdic.confno = dahdichan;

	if (ast_test_flag(&flags, OPT_TX) || ast_test_flag(&flags, OPT_RX)) {
		/* One or the other */
		dahdic.confmode = ast_test_flag(&flags, OPT_PREECHOCANCEL) ? ast_test_flag(&flags, OPT_TX) ? DAHDI_CONF_MONITOR_TX_PREECHO : DAHDI_CONF_MONITOR_RX_PREECHO :
			ast_test_flag(&flags, OPT_TX) ? DAHDI_CONF_MONITORTX : DAHDI_CONF_MONITOR;
	} else {
		/* Monitor both */
		dahdic.confmode = ast_test_flag(&flags, OPT_PREECHOCANCEL) ? DAHDI_CONF_MONITORBOTH_PREECHO : DAHDI_CONF_MONITORBOTH;
	}

	if (ioctl(dfd, DAHDI_SETCONF, &dahdic) < 0) {
		ast_log(LOG_ERROR, "Unable to monitor channel %d: %s\n", dahdichan, strerror(errno));
		close(dfd);
		return -1;
	}

	ast_debug(2, "Monitoring DAHDI channel %d, pre-echo: %d, tx: %d, rx: %d, limit: %d\n", dahdichan,
		ast_test_flag(&flags, OPT_PREECHOCANCEL), ast_test_flag(&flags, OPT_TX), ast_test_flag(&flags, OPT_RX), maxbytes);

	/* Copy from pseudo to Asterisk channel until we get a hangup or DTMF */
	for (;;) {
		struct ast_channel *c;
		struct ast_frame *f, fr;
		int outfd, ms;

		/* Perform a hangup check here since ast_waitfor_nandfds will not always be able to get a channel after a hangup has occurred */
		if (ast_check_hangup(chan)) {
			break;
		}

		if (skip_dahdi) {
			int res = ast_waitfor(chan, 50); /* The Asterisk channel should have activity every 20 ms */
			if (res < 0) {
				ast_debug(1, "ast_waitfor failed on channel %s\n", ast_channel_name(chan));
				res = -1;
				break;
			}
			c = chan;
		} else {
			c = ast_waitfor_nandfds(&chan, 1, &dfd, 1, NULL, &outfd, &ms);
		}
		skip_dahdi = 0;
		if (c) {
			f = ast_read(chan);
			if (!f) {
				break; /* Probably hung up */
			}
			if (f->frametype == AST_FRAME_DTMF) {
				ast_frfree(f);
				res = 0;
				break;
			}
			ast_frfree(f);
		} else {
			res = read(dfd, buf, CONF_SIZE);
			if (res < 1) {
				if (errno == EAGAIN) {
					/* Sometimes DAHDI resources like to be temporarily unavailable.
					 * DAHDI_POLICY_IMMEDIATE also makes this more likely
					 * (see https://support.digium.com/s/article/How-to-configure-DAHDI-buffer-policies)
					 * This is normal and expected.
					 * Don't abort if this happens, just keep trying
					 * in case the channel becomes available again soon.
					 * To avoid having a super tight loop (dozens per millisecond),
					 * skip reading from DAHDI the next iteration,
					 * and just use the Asterisk channel for timing, which will temper CPU usage.
					 */
					skip_dahdi = 1;
					continue;
				}
				ast_log(LOG_ERROR, "Failed to read from DAHDI channel %d: %s\n", dahdichan, strerror(errno));
				res = -1;
				break;
			}

			if (res != LINEAR_READ_SIZE) {
				ast_log(LOG_WARNING, "Short read (%d/%d)\n", res, LINEAR_READ_SIZE);
			}

			memset(&fr, 0, sizeof(fr));
			fr.frametype = AST_FRAME_VOICE;
			fr.subclass.format = ast_format_slin;
			fr.samples = res / 2;
			fr.mallocd = 0;
			fr.offset = AST_FRIENDLY_OFFSET;
			fr.datalen = res;
			fr.data.ptr = buf;

			if (ast_write(chan, &fr) < 0) {
				ast_log(LOG_WARNING, "Unable to write frame to channel %s\n", ast_channel_name(chan));
				break;
			}
			sofar += res;
			if (maxbytes && sofar >= maxbytes) {
				break;
			}
		}
	}

	close(dfd);
	return res < 0 ? -1 : 0;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, dahdi_monitor_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "DAHDI channel monitor");
