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
 * \brief Channel technology function
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 * \ingroup functions
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/module.h"
#include "asterisk/pbx.h"
#include "asterisk/channel.h"
#include "asterisk/utils.h"

/*** DOCUMENTATION
	<function name="TECH_EXISTS" language="en_US">
		<synopsis>
			Checks if the specified channel technology exists and is usable.
		</synopsis>
		<syntax>
			<parameter name="tech" required="true">
				<para>The name of the channel technology (e.g. DAHDI, PJSIP, IAX2, etc.).</para>
			</parameter>
		</syntax>
		<description>
			<para>Returns 1 if the channel technology <replaceable>tech</replaceable> exists, 0 if not.</para>
		</description>
	</function>
 ***/

static int func_tech_exists_read(struct ast_channel *chan, const char *function, char *data, char *buf, size_t maxlen)
{
	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "%s: Channel tech required\n", function);
		return -1;
	}
	snprintf(buf, maxlen, "%d", (ast_get_channel_tech(data) ? 1 : 0));
	return 0;
}

static struct ast_custom_function tech_exists_function = {
	.name = "TECH_EXISTS",
	.read = func_tech_exists_read,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&tech_exists_function);
}

static int load_module(void)
{
	return ast_custom_function_register(&tech_exists_function);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Channel technology function");
