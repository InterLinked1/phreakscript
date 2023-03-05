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
 * \brief Per-channel DTMF tracing function
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup functions
 *
 * \note This is meant to be a simpler and more granular version
 * of what the DTMF log level does. In addition to being global,
 * the DTMF log absolutely spams the console with several lines per digit,
 * making it quite unsuitable for busy systems.
 * The output of this framehook is one line per digit, per channel.
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/framehook.h"
#include "asterisk/term.h"

/*** DOCUMENTATION
	<function name="DTMF_TRACE" language="en_US">
		<synopsis>
			View DTMF digits as they are sent or received on a channel.
		</synopsis>
		<syntax>
			<parameter name="direction" required="false">
				<para><literal>TX</literal> or <literal>RX</literal> to limit the direction.</para>
				<para>Default is both directions.</para>
			</parameter>
		</syntax>
		<description>
			<para>Subsequent function invocations will replace earlier ones.</para>
			<para>Examples:</para>
			<example title="View only received DTMF digits">
			exten => 1,1,Set(DTMF_TRACE(RX)=1)
			</example>
			<example title="Disable DTMF trace">
			exten => 1,1,Set(DTMF_TRACE(TX)=0)
			</example>
		</description>
	</function>
 ***/

struct dtmf_trace_data {
	unsigned int tx:1;
	unsigned int rx:1;
};

static void datastore_destroy_cb(void *data) {
	ast_free(data);
}

static const struct ast_datastore_info dtmf_trace_datastore = {
	.type = "dtmftrace",
	.destroy = datastore_destroy_cb
};

static void hook_destroy_cb(void *framedata)
{
	ast_free(framedata);
}

static struct ast_frame *hook_event_cb(struct ast_channel *chan, struct ast_frame *frame, enum ast_framehook_event event, void *data)
{
	struct dtmf_trace_data *framedata = data;

	if ((event != AST_FRAMEHOOK_EVENT_WRITE) && (event != AST_FRAMEHOOK_EVENT_READ)) {
		return frame;
	}
	if (frame && frame->frametype == AST_FRAME_DTMF_END) {
		if (event == AST_FRAMEHOOK_EVENT_READ && !framedata->rx) {
			ast_debug(4, "Digit %c from %s does not match filter\n", frame->subclass. integer, ast_channel_name(chan));
		} else if (event == AST_FRAMEHOOK_EVENT_WRITE && !framedata->tx) {
			ast_debug(4, "Digit %c from %s does not match filter\n", frame->subclass. integer, ast_channel_name(chan));
		} else {
			ast_verbose(COLORIZE_FMT "[%c] %s %s\n", COLORIZE(COLOR_BRGREEN, 0, "DTMF"), frame->subclass.integer, event == AST_FRAMEHOOK_EVENT_READ ? "<==" : "==>", ast_channel_name(chan));
		}
	}
	return frame;
}

static int dtmf_trace_helper(struct ast_channel *chan, const char *cmd, char *data, const char *value)
{
	struct dtmf_trace_data *framedata;
	struct ast_datastore *datastore = NULL;
	struct ast_framehook_interface interface = {
		.version = AST_FRAMEHOOK_INTERFACE_VERSION,
		.event_cb = hook_event_cb,
		.destroy_cb = hook_destroy_cb,
	};
	int i = 0;
	int enabled;
	int newtx = 0, newrx = 0;

	if (!chan) {
		ast_log(LOG_WARNING, "No channel was provided to %s function.\n", cmd);
		return -1;
	}

	if (ast_strlen_zero(value)) {
		ast_log(LOG_ERROR, "No value provided to %s\n", cmd);
		return -1;
	}

	enabled = ast_true(value) ? 1 : 0;
	if (!ast_strlen_zero(data)) {
		if (strcasestr(data, "TX")) {
			newtx = 1;
		}
		if (strcasestr(data, "RX")) {
			newrx = 1;
		}
	} else {
		newtx = newrx = 1;
	}

	if (!(framedata = ast_calloc(1, sizeof(*framedata)))) {
		return -1;
	}

	interface.data = framedata;

	ast_channel_lock(chan);
	i = ast_framehook_attach(chan, &interface);
	if (i >= 0) {
		int *id;
		if ((datastore = ast_channel_datastore_find(chan, &dtmf_trace_datastore, NULL))) {
			id = datastore->data;
			ast_framehook_detach(chan, *id);
			ast_channel_datastore_remove(chan, datastore);
			ast_datastore_free(datastore);
		}

		if (!enabled) {
			ast_framehook_detach(chan, i);
			ast_channel_unlock(chan);
			return 0;
		}

		if (!(datastore = ast_datastore_alloc(&dtmf_trace_datastore, NULL))) {
			ast_framehook_detach(chan, i);
			ast_channel_unlock(chan);
			return 0;
		}

		if (!(id = ast_calloc(1, sizeof(int)))) {
			ast_datastore_free(datastore);
			ast_framehook_detach(chan, i);
			ast_channel_unlock(chan);
			return 0;
		}

		*id = i; /* Store off the id. The channel is still locked so it is safe to access this ptr. */
		datastore->data = id;
		framedata->tx = newtx;
		framedata->rx = newrx;
		ast_debug(2, "DTMF trace now TX=%d,RX=%d\n", newtx, newrx);
		ast_channel_datastore_add(chan, datastore);
	}
	ast_channel_unlock(chan);

	return 0;
}

static struct ast_custom_function dtmf_trace_function = {
	.name = "DTMF_TRACE",
	.write = dtmf_trace_helper,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&dtmf_trace_function);
}

static int load_module(void)
{
	int res = ast_custom_function_register(&dtmf_trace_function);
	return res ? AST_MODULE_LOAD_DECLINE : AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "DTMF Trace");
