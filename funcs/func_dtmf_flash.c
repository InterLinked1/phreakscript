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
 * \brief Function to intercept long DTMF digits and spawn hook flash
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup functions
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/module.h"
#include "asterisk/channel.h"
#include "asterisk/app.h"
#include "asterisk/pbx.h"
#include "asterisk/framehook.h"

/*** DOCUMENTATION
	<function name="DTMF_FLASH" language="en_US">
		<synopsis>
			Intercepts long DTMF frames on a channel and treats them as a hook flash instead
		</synopsis>
		<syntax>
			<parameter name="direction" required="true">
				<para><literal>RX</literal> or <literal>TX</literal></para>
			</parameter>
			<parameter name="digit" required="false">
				<para>Digit to intercept. Default is * (star).</para>
			</parameter>
			<parameter name="ms" required="false">
				<para>Required length (in milliseconds) of digit to intercept. Default is 750.</para>
			</parameter>
		</syntax>
		<description>
			<para>Listens for long DTMF digits and treats a special digit of a certain duration as a hook flash on the channel instead.</para>
			<para>This can be useful for IP devices that do not have the capability of sending a hook flash signal natively.</para>
			<note><para>This functionality requires that DTMF emulation be used for the device. This means that a DTMF START event is processed when a user begins holding down a DTMF key and the DTMF END event is not processed until the user has let go of the key. If fixed durations are used for DTMF, this functionality will likely not work for your endpoint.</para></note>
			<example title="Intercept star key for greater than 750ms as hook flash">
			same => n,Set(LONG_DTMF_INTERCEPT(RX,*,750)=) ; provide hook flash capability via long "*" on IP phones
			</example>
		</description>
	</function>
 ***/
 
enum direction {
    TX = 0,
    RX,
};

struct dtmf2flash_data {
	enum direction fdirection;
	char digit;
	unsigned int duration;
};

static void datastore_destroy_cb(void *data) {
	ast_free(data);
}

static const struct ast_datastore_info dtmf2flash_datastore = {
	.type = "dmtfflash",
	.destroy = datastore_destroy_cb
};

static void hook_destroy_cb(void *framedata)
{
	ast_free(framedata);
}

static struct ast_frame *hook_event_cb(struct ast_channel *chan, struct ast_frame *frame, enum ast_framehook_event event, void *data)
{
	char digit;
	long int len;
	struct dtmf2flash_data *framedata = data;

	if (!frame) {
		return frame;
	}
	if (!((event == AST_FRAMEHOOK_EVENT_WRITE && framedata->fdirection == TX) ||
		(event == AST_FRAMEHOOK_EVENT_READ && framedata->fdirection == RX))) {
		return frame;
	}
	if (frame->frametype != AST_FRAME_DTMF_END) {
		return frame;
	}

	len = frame->len;
	digit = frame->subclass.integer;

	if (digit != framedata->digit) {
		return frame; /* not the right digit */
	}
	if (len < framedata->duration) {
		return frame; /* too short to matter */
	}

	ast_verb(3, "Got long DTMF digit '%c' (%ld ms), processing hook flash for %s\n", digit, len, ast_channel_name(chan));

	ast_frfree(frame);
	frame = &ast_null_frame;
	{
		struct ast_frame f = { AST_FRAME_CONTROL, { AST_CONTROL_FLASH, } };
		ast_queue_frame(chan, &f);
	}
	return frame;
}

static int dtmf2flash_helper(struct ast_channel *chan, const char *cmd, char *data, const char *value)
{
	struct dtmf2flash_data *framedata;
	struct ast_datastore *datastore = NULL;
	struct ast_framehook_interface interface = {
		.version = AST_FRAMEHOOK_INTERFACE_VERSION,
		.event_cb = hook_event_cb,
		.destroy_cb = hook_destroy_cb,
	};
	int i = 0;
	char *parse;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(direction);
		AST_APP_ARG(digit);
		AST_APP_ARG(duration);
	);

	parse = ast_strdupa(S_OR(data, ""));
	AST_STANDARD_APP_ARGS(args, parse);

	if (!(framedata = ast_calloc(1, sizeof(*framedata)))) {
		return 0;
	}

	interface.data = framedata;

	if (!strcasecmp(args.direction, "TX")) {
		framedata->fdirection = TX;
	} else {
		framedata->fdirection = RX;
	}
	framedata->digit = ast_strlen_zero(args.digit) ? '*' : args.digit[0];
	framedata->duration = ast_strlen_zero(args.duration) ? 750 : atoi(args.duration);

	ast_channel_lock(chan);
	i = ast_framehook_attach(chan, &interface);
	if (i >= 0) {
		int *id;
		if ((datastore = ast_channel_datastore_find(chan, &dtmf2flash_datastore, NULL))) {
			id = datastore->data;
			ast_framehook_detach(chan, *id);
			ast_channel_datastore_remove(chan, datastore);
			ast_datastore_free(datastore);
		}

		if (!(datastore = ast_datastore_alloc(&dtmf2flash_datastore, NULL))) {
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
		ast_channel_datastore_add(chan, datastore);
		ast_debug(1, "Set up interception of long DTMF of digit '%c' with duration %u\n", framedata->digit, framedata->duration);
	}
	ast_channel_unlock(chan);

	return 0;
}

static struct ast_custom_function dtmf2flash_function = {
	.name = "DTMF_FLASH",
	.write = dtmf2flash_helper,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&dtmf2flash_function);
}

static int load_module(void)
{
	int res = ast_custom_function_register(&dtmf2flash_function);
	return res ? AST_MODULE_LOAD_DECLINE : AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Function to intercept long DTMF digits and spawn hook flash");
