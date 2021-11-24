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
 * \brief Attempts to reclaim unused heap memory
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<defaultenabled>no</defaultenabled>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"

/*** DOCUMENTATION
	<application name="MallocTrim" language="en_US">
		<synopsis>
			Attempts to reclaim unused heap memory.
		</synopsis>
		<description>
			<para>Attempts to release free memory from the heap.</para>
			<para>This application is typically used before the
			<literal>System</literal>application or the <literal>SHELL</literal>
			function if system memory conditions prevent these from succeeding
			ordinarily.</para>
			<para>This application may be used to diagnose this memory issue and
			prevent these calls from failing until the cause of the memory issue
			is found. You should also build Asterisk with <literal>MALLOC_DEBUG</literal>
			to troubleshoot memory issues.</para>
			<variablelist>
				<variable name="MALLOCTRIMSTATUS">
					<value name="SUCCESS">
						Some memory was released back to the system.
					</value>
					<value name="FAILURE">
						No memory could be released back to the system.
					</value>
					<value name="UNSUPPORTED">
						This application is not supported on this system.
					</value>
				</variable>
			</variablelist>
		</description>
	</application>
 ***/

static char *app = "MallocTrim";

static int malloc_trim_exec(struct ast_channel *chan, const char *data)
{
	#ifdef HAVE_MALLOC_TRIM
	
	extern int malloc_trim(size_t __pad) __THROW;

	if (malloc_trim(0)) {
		pbx_builtin_setvar_helper(chan, "MALLOCTRIMSTATUS", "SUCCESS");
	} else {
		pbx_builtin_setvar_helper(chan, "MALLOCTRIMSTATUS", "FAILURE");
	}
	return 0;

	#else

	pbx_builtin_setvar_helper(chan, "MALLOCTRIMSTATUS", "UNSUPPORTED");
	return 0;

	#endif
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, malloc_trim_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Frees unused heap memory");
