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
 * \brief Message intercept hook
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup functions
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/frame.h"
#include "asterisk/pbx.h"
#include "asterisk/channel.h"
#include "asterisk/app.h"
#include "asterisk/module.h"
#include "asterisk/message.h"

/*** DOCUMENTATION
	<function name="MESSAGE_INTERCEPT_SUB" language="en_US">
		<synopsis>
			Add an intercept subroutine to run when text messages are received on a channel
		</synopsis>
		<syntax>
			<parameter name="context" required="true">
				<para>The dialplan location to execute as a callback (context,exten,priority).</para>
			</parameter>
		</syntax>
		<description>
			<para>Example:</para>
			<example title="Add message intercept">
			exten => 1,1,Set(MESSAGE_INTERCEPT_SUB()=on-message)

			[on-message]
			exten => _X!,1,NoOp(${MESSAGE_INTERCEPT_SUB())})
				same => n,Return()
			</example>
		</description>
	</function>
 ***/

struct message_intercept_data {
	struct ast_frame *frame;
	char context[AST_MAX_CONTEXT];
};

static void datastore_destroy_cb(void *data) {
	ast_free(data);
}

static const struct ast_datastore_info message_intercept_datastore = {
	.type = "message_intercept_sub",
	.destroy = datastore_destroy_cb
};

static void hook_destroy_cb(void *framedata)
{
	ast_free(framedata);
}

static struct ast_frame *hook_event_cb(struct ast_channel *chan, struct ast_frame *frame, enum ast_framehook_event event, void *data)
{
	int res;
	struct ast_msg_data *msg;
	struct message_intercept_data *framedata = data;

	if (!frame) {
		return frame;
	}

	if (event != AST_FRAMEHOOK_EVENT_WRITE && event != AST_FRAMEHOOK_EVENT_READ) {
		return frame;
	}

	if (frame->frametype != AST_FRAME_TEXT_DATA) {
		return frame;
	}

	msg = frame->data.ptr;
	ast_debug(3, "Received message: '%s'\n", ast_msg_data_get_attribute(msg, AST_MSG_DATA_ATTR_BODY));

	/* All this logic would be more elegant if it were in the core, in channel.c.
	 * Overall idea is not dissimilar to CONNECTED_LINE_SEND_SUB, REDIRECTING_SEND_SUB in channel.c: */

	/* ast_channel_get_intercept_mode is public API, but channel_set_intercept_mode is not,
	 * so we can't actually set it.
	 * However, this only seems to get used by res_agi, so unlikely to make much difference outside of that. */

	/* channel_set_intercept_mode(1); */
	framedata->frame = frame;
	res = ast_app_run_sub(NULL, chan, framedata->context, "", 1);
	framedata->frame = NULL;
	/* channel_set_intercept_mode(0); */

	ast_debug(3, "Message intercept sub res: %d\n", res);
	return frame;
}

struct message_intercept_datastore_data {
	int framehook_id;
	struct message_intercept_data *data;
};

static int read_helper(struct ast_channel *chan, const char *cmd, char *data, char *buf, size_t len)
{
	struct ast_msg_data *msg;
	struct ast_datastore *datastore;
	struct message_intercept_datastore_data *mdata;
	struct message_intercept_data *framedata;

	if (!(datastore = ast_channel_datastore_find(chan, &message_intercept_datastore, NULL))) {
		ast_log(LOG_WARNING, "No message intercept hook is currently active\n");
		return -1;
	}

	mdata = datastore->data;
	framedata = mdata->data;

	ast_channel_lock(chan);
	if (!framedata->frame) {
		ast_channel_unlock(chan);
		ast_log(LOG_WARNING, "No message data currently active\n");
		return -1;
	}

	/* Caller can't actually use the MESSAGE and MESSAGE_DATA functions,
	 * since there's no message on the channel,
	 * so we need to provide the data through this callback: */

	msg = framedata->frame->data.ptr;
	ast_debug(3, "Received message: '%s'\n", ast_msg_data_get_attribute(msg, AST_MSG_DATA_ATTR_BODY));

	ast_copy_string(buf, ast_msg_data_get_attribute(msg, AST_MSG_DATA_ATTR_BODY), len);
	ast_channel_unlock(chan);
	return 0;
}

static int write_helper(struct ast_channel *chan, const char *cmd, char *data, const char *value)
{
	struct message_intercept_data *framedata;
	struct ast_datastore *datastore = NULL;
	struct ast_framehook_interface interface = {
		.version = AST_FRAMEHOOK_INTERFACE_VERSION,
		.event_cb = hook_event_cb,
		.destroy_cb = hook_destroy_cb,
	};
	int i = 0;

	if (!chan) {
		ast_log(LOG_ERROR, "No channel was provided to %s function.\n", cmd);
		return -1;
	}
	if (ast_strlen_zero(value)) {
		ast_log(LOG_ERROR, "Missing context argument\n");
		return -1;
	}

	if (!(framedata = ast_calloc(1, sizeof(*framedata)))) {
		return 0;
	}

	interface.data = framedata;
	ast_copy_string(framedata->context, value, sizeof(framedata->context));

	ast_channel_lock(chan);
	i = ast_framehook_attach(chan, &interface);
	if (i >= 0) {
		int id;
		struct message_intercept_datastore_data *mdata;
		if ((datastore = ast_channel_datastore_find(chan, &message_intercept_datastore, NULL))) {
			mdata = datastore->data;
			id = mdata->framehook_id;
			ast_framehook_detach(chan, id);
			ast_channel_datastore_remove(chan, datastore);
			ast_datastore_free(datastore);
		}

		if (!(datastore = ast_datastore_alloc(&message_intercept_datastore, NULL))) {
			ast_framehook_detach(chan, i);
			ast_channel_unlock(chan);
			return 0;
		}

		if (!(mdata = ast_calloc(1, sizeof(*mdata)))) {
			ast_datastore_free(datastore);
			ast_framehook_detach(chan, i);
			ast_channel_unlock(chan);
			return 0;
		}

		mdata->framehook_id = i; /* Store off the id. The channel is still locked so it is safe to access this ptr. */
		mdata->data = framedata;
		datastore->data = mdata;
		ast_channel_datastore_add(chan, datastore);
	}
	ast_channel_unlock(chan);

	return 0;
}

static struct ast_custom_function message_intercept_function = {
	.name = "MESSAGE_INTERCEPT_SUB",
	.read = read_helper,
	.write = write_helper,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&message_intercept_function);
}

static int load_module(void)
{
	return ast_custom_function_register(&message_intercept_function);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Message data intercept");
