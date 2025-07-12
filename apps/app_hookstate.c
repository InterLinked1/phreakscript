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
 * \brief DAHDI channel supervision applications
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

#include "asterisk/lock.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"

/*** DOCUMENTATION
	<application name="OnHook" language="en_US">
		<synopsis>
			Go on-hook on a DAHDI channel
		</synopsis>
		<syntax />
		<description>
			<para>Go on-hook on a DAHDI channel.</para>
			<para>Only works on FXS-signaled channels (FXO ports).</para>
		</description>
		<see-also>
			<ref type="application">OffHook</ref>
			<ref type="application">SupervisionTest</ref>
		</see-also>
	</application>
	<application name="OffHook" language="en_US">
		<synopsis>
			Go off-hook on a DAHDI channel
		</synopsis>
		<syntax />
		<description>
			<para>Go off-hook on a DAHDI channel.</para>
			<para>Only works on FXS-signaled channels (FXO ports).</para>
		</description>
		<see-also>
			<ref type="application">OnHook</ref>
			<ref type="application">SupervisionTest</ref>
		</see-also>
	</application>
	<application name="SupervisionTest" language="en_US">
		<synopsis>
			Perform a supervision test
		</synopsis>
		<syntax />
		<description>
			<para>Run a supervision test on the channel. This will make the channel go on and off-hook alternately.</para>
			<para>The supervision test will run indefinitely, until the caller hangs up.</para>
			<para>Only works on FXS-signaled channels (FXO ports). To set up an access line, you will want to
			tie this to an FXS port (FXO-signaled) and use that as the test number.</para>
			<para>The FXS port should be configured with usecallerid=no, calledsubscriberheld=yes, threewaycalling=no, dialmode=none.
			The FXO port just needs to call this application.</para>
		</description>
		<see-also>
			<ref type="application">OnHook</ref>
			<ref type="application">OffHook</ref>
		</see-also>
	</application>
 ***/

static char *app_onhook = "OnHook";
static char *app_offhook = "OffHook";
static char *app_supetest = "SupervisionTest";

static inline int dahdi_wait_event(int fd)
{
	/* Avoid the silly dahdi_waitevent which ignores a bunch of events */
	int i,j=0;
	i = DAHDI_IOMUX_SIGEVENT;
	if (ioctl(fd, DAHDI_IOMUX, &i) == -1) return -1;
	if (ioctl(fd, DAHDI_GETEVENT, &j) == -1) return -1;
	return j;
}

static int set_hook(struct ast_channel *chan, int event)
{
	int res = -1;
	struct dahdi_params dahdip;

	memset(&dahdip, 0, sizeof(dahdip));
	res = ioctl(ast_channel_fd(chan, 0), DAHDI_GET_PARAMS, &dahdip);
	if (!res) {
		if (dahdip.sigtype & __DAHDI_SIG_FXS) {
			res = ioctl(ast_channel_fd(chan, 0), DAHDI_HOOK, &event);
			if (!res || (errno == EINPROGRESS)) {
				if (res) {
					/* Wait for the event to finish */
					dahdi_wait_event(ast_channel_fd(chan, 0));
				}
				ast_verb(5, "Went %s-hook on %s\n", event == DAHDI_ONHOOK ? "on" : "off", ast_channel_name(chan));
			} else {
				ast_log(LOG_WARNING, "Unable to go %s-hook on %s: %s\n", event == DAHDI_ONHOOK ? "on" : "off", ast_channel_name(chan), strerror(errno));
			}
		} else {
			ast_log(LOG_WARNING, "%s is not an FXO Channel\n", ast_channel_name(chan));
		}
	} else {
		ast_log(LOG_WARNING, "Unable to get parameters of %s: %s\n", ast_channel_name(chan), strerror(errno));
	}

	return res;
}

static int onhook_exec(struct ast_channel *chan, const char *data)
{
	if (strcasecmp(ast_channel_tech(chan)->type, "DAHDI")) {
		ast_log(LOG_WARNING, "%s is not a DAHDI channel\n", ast_channel_name(chan));
		return -1;
	}
	return set_hook(chan, DAHDI_ONHOOK);
}

static int offhook_exec(struct ast_channel *chan, const char *data)
{
	if (strcasecmp(ast_channel_tech(chan)->type, "DAHDI")) {
		ast_log(LOG_WARNING, "%s is not a DAHDI channel\n", ast_channel_name(chan));
		return -1;
	}
	return set_hook(chan, DAHDI_OFFHOOK);
}

static int supetest_exec(struct ast_channel *chan, const char *data)
{
	if (strcasecmp(ast_channel_tech(chan)->type, "DAHDI")) {
		ast_log(LOG_WARNING, "%s is not a DAHDI channel\n", ast_channel_name(chan));
		return -1;
	}

#define CHECK_RES(x) if ((x)) { return -1; }

#define SUPE_SWAP_SINGLE(chan, onhook, offhook) \
	CHECK_RES(set_hook(chan, DAHDI_ONHOOK)); \
	CHECK_RES(ast_safe_sleep(chan, onhook)); \
	CHECK_RES(set_hook(chan, DAHDI_OFFHOOK)); \
	CHECK_RES(ast_safe_sleep(chan, offhook)); \

	CHECK_RES(ast_raw_answer(chan)); /* First, go off hook */
	CHECK_RES(ast_safe_sleep(chan, 1000)); /* Pause a second before starting */

	SUPE_SWAP_SINGLE(chan, 50, 1000); /* Momentary quick supervision blip, then pause a second */

	/* Three supervision blips */
	SUPE_SWAP_SINGLE(chan, 170, 270);
	SUPE_SWAP_SINGLE(chan, 170, 270);
	SUPE_SWAP_SINGLE(chan, 170, 270);

	CHECK_RES(ast_safe_sleep(chan, 1000)); /* Pause a second */

	/* Momentary on-hooks, indefinitely. These blips are longer than the three before */
	for (;;) {
		SUPE_SWAP_SINGLE(chan, 200, 300);
	}
	return -1;
}

static int unload_module(void)
{
	ast_unregister_application(app_onhook);
	ast_unregister_application(app_offhook);
	ast_unregister_application(app_supetest);
	return 0;
}

static int load_module(void)
{
	int res = 0;
	res |= ast_register_application_xml(app_onhook, onhook_exec);
	res |= ast_register_application_xml(app_offhook, offhook_exec);
	res |= ast_register_application_xml(app_supetest, supetest_exec);
	return res;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Channel supervision application");
