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
 * \brief Stream silent audio
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"

/*** DOCUMENTATION
	<application name="StreamSilence" language="en_US">
		<synopsis>
			Streams silence to a channel.
		</synopsis>
		<syntax>
			<parameter name="timeout">
				<para>The maximum amount of time, in seconds, this application should stream silent
				audio before dialplan execution continues automatically to the next priority.
				By default, there is no timeout.</para>
			</parameter>
		</syntax>
		<description>
			<para>Streams silent audio to a channel, for up to a provided number of seconds.</para>
			<para>This application will send silent audio to a channel, as opposed to applications
			like <literal>Wait</literal> which do not. This guarantees that audiohooks will
			function properly, even if the channel is not bridged to something that is continously
			sending frames.</para>
		</description>
		<see-also>
			<ref type="application">Wait</ref>
		</see-also>
	</application>
	<function name="STREAM_SILENCE" language="en_US">
		<synopsis>
			Stream silent audio on a channel.
		</synopsis>
		<syntax>
			<parameter name="active" required="false">
				<para><literal>1</literal> to enable or empty to disable.</para>
			</parameter>
		</syntax>
		<description>
			<para>Streams silent audio on a channel until disabled.
			This ensures that audiohooks can be processed constantly
			even if the channel receives no other frames.</para>
		</description>
		<see-also>
			<ref type="application">ChanSpy</ref>
		</see-also>
	</function>
 ***/

static char *app = "StreamSilence";

static int streamsilence_exec(struct ast_channel *chan, const char *data)
{
	struct timeval start = ast_tvnow();
	struct ast_silence_generator *g;
	int timeout_ms = 0;
	char *appdata;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(timeout);
	);

	appdata = ast_strdupa(S_OR(data, ""));
	AST_STANDARD_APP_ARGS(args, appdata);

	if (!ast_strlen_zero(args.timeout)) {
		ast_app_parse_timelen(args.timeout, &timeout_ms, TIMELEN_SECONDS);
	}
	if (timeout_ms > 0) {
		ast_debug(1, "Waiting for %d ms\n", timeout_ms);
	}
	g = ast_channel_start_silence_generator(chan);
	while (timeout_ms == 0 || ast_remaining_ms(start, timeout_ms)) {
		if (ast_safe_sleep(chan, 1000)) {
			ast_channel_stop_silence_generator(chan, g);
			return -1; /* channel hung up */
		}
	}
	ast_channel_stop_silence_generator(chan, g);
	return 0;
}

/*! \brief Static structure for datastore information */
static const struct ast_datastore_info streamsilence_datastore = {
	.type = "streamsilence",
};

/*! \internal \brief Disable silence stream on the channel */
static int remove_streamsilence(struct ast_channel *chan)
{
	struct ast_datastore *datastore = NULL;
	struct ast_silence_generator *g;
	SCOPED_CHANNELLOCK(chan_lock, chan);

	datastore = ast_channel_datastore_find(chan, &streamsilence_datastore, NULL);
	if (!datastore) {
		ast_log(AST_LOG_WARNING, "Cannot remove STREAM_SILENCE from %s: STREAM_SILENCE not currently enabled\n",
		        ast_channel_name(chan));
		return -1;
	}
	g = datastore->data;
	if (g) {
		ast_channel_stop_silence_generator(chan, g);
		/* no need to call ast_free, stop_silence_generator frees g for us */
		g = NULL;
	}
	if (ast_channel_datastore_remove(chan, datastore)) {
		ast_log(AST_LOG_WARNING, "Failed to remove STREAM_SILENCE datastore from channel %s\n",
		        ast_channel_name(chan));
		return -1;
	}
	ast_datastore_free(datastore);

	return 0;
}

static int streamsilence_write(struct ast_channel *chan, const char *cmd, char *data, const char *value)
{
	struct ast_datastore *datastore = NULL;
	struct ast_silence_generator *g;

	if (!chan) {
		ast_log(LOG_WARNING, "No channel was provided to %s function.\n", cmd);
		return -1;
	}
	if (ast_strlen_zero(value)) {
		return remove_streamsilence(chan);
	}
	if (!strchr(value, '1')) {
		ast_log(LOG_WARNING, "Invalid argument: %s\n", value);
		return -1;
	}

	ast_channel_lock(chan);
	if (!(datastore = ast_channel_datastore_find(chan, &streamsilence_datastore, NULL))) {
		/* Allocate a new datastore to hold the reference to this audiohook information */
		if (!(datastore = ast_datastore_alloc(&streamsilence_datastore, NULL))) {
			ast_channel_unlock(chan);
			return 0;
		}
		g = ast_channel_start_silence_generator(chan);
		datastore->data = g;
		ast_channel_datastore_add(chan, datastore);
	} else {
		ast_debug(1, "Silence generator already active, ignoring\n");
	}
	ast_channel_unlock(chan);

	return 0;
}

static struct ast_custom_function streamsilence_function = {
	.name = "STREAM_SILENCE",
	.write = streamsilence_write,
};

static int unload_module(void)
{
	int res;

	res = ast_custom_function_unregister(&streamsilence_function);
	res |= ast_unregister_application(app);

	return res;
}

static int load_module(void)
{
	int res;

	res = ast_custom_function_register(&streamsilence_function);
	res = ast_register_application_xml(app, streamsilence_exec);

	return res;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Streams silent audio to a channel");
