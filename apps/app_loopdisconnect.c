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
 * \brief Perform a loop disconnect
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
#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"

/*** DOCUMENTATION
	<application name="LoopDisconnect" language="en_US">
		<synopsis>
			Performs a loop disconnect on an FXS channel.
		</synopsis>
		<syntax />
		<description>
			<para>Performs a loop disconnect (open switching interval) on an FXS channel.</para>
			<para>This will not hangup the channel or do anything besides the loop disconnect.</para>
			<para>Providing a loop disconnect can be used to force compatible equipment that will
			recognize a loop disconnect to release the line, such as answering machines.</para>
			<para>This application will wait for the loop disconnect to finish before continuing.</para>
		</description>
		<see-also>
			<ref type="function">POLARITY</ref>
		</see-also>
	</application>
 ***/

static char *app = "LoopDisconnect";

static inline int dahdi_wait_event(int fd)
{
	/* Avoid the silly dahdi_waitevent which ignores a bunch of events */
	int i, j = 0;
	i = DAHDI_IOMUX_SIGEVENT;
	if (ioctl(fd, DAHDI_IOMUX, &i) == -1) {
		return -1;
	}
	if (ioctl(fd, DAHDI_GETEVENT, &j) == -1) {
		return -1;
	}
	return j;
}

static int kewl_exec(struct ast_channel *chan, const char *data)
{
	int res, x;
	struct dahdi_params dahdip;

	if (strcasecmp(ast_channel_tech(chan)->type, "DAHDI")) {
		ast_log(LOG_WARNING, "%s is not a DAHDI channel\n", ast_channel_name(chan));
		return -1;
	}

	memset(&dahdip, 0, sizeof(dahdip));

	if (ioctl(ast_channel_fd(chan, 0), DAHDI_GET_PARAMS, &dahdip)) {
		ast_log(LOG_WARNING, "Unable to get parameters of %s: %s\n", ast_channel_name(chan), strerror(errno));
		return -1;
	}

	if (!(dahdip.sigtype & __DAHDI_SIG_FXO)) {
		ast_log(LOG_WARNING, "%s is not an FXS channel\n", ast_channel_name(chan));
		return -1;
	}

	x = DAHDI_KEWL;
	res = ioctl(ast_channel_fd(chan, 0), DAHDI_HOOK, &x);

	if (res && errno != EINPROGRESS) {
		ast_log(LOG_WARNING, "Unable to do loop disconnect on channel %s: %s\n", ast_channel_name(chan), strerror(errno));
		return -1;
	}

	if (res) {
		/* Wait for the event to finish */
		dahdi_wait_event(ast_channel_fd(chan, 0));
	}

	ast_verb(3, "Loop Disconnect on channel %s\n", ast_channel_name(chan));
	/* Actually wait for the loop disconnect to finish. */
	return ast_safe_sleep(chan, 600); /* Actual timing will depend on the kewl times, but these are defined in DAHDI's kernel.h */
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, kewl_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Loop Disconnect application");
