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
 * \brief Remote text querying
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
#include "asterisk/pbx.h"	/* function register/unregister */
#include "asterisk/utils.h"
#include "asterisk/app.h"
#include "asterisk/format_cache.h"
#include "asterisk/frame.h"
#include "asterisk/strings.h"
#include "asterisk/conversions.h"

/*** DOCUMENTATION
	<function name="TEXT_QUERY" language="en_US">
		<synopsis>
			Remote string querying
		</synopsis>
		<syntax>
			<parameter name="dialstr" required="true">
				<para>Dial string, such as provided to the Dial application</para>
			</parameter>
			<parameter name="timeout" required="false">
				<para>Timeout to wait, in seconds. Default is 5 seconds.</para>
			</parameter>
		</syntax>
		<description>
			<para>Initiate a call and receive a text data transfer.</para>
			<para>This function can be used to implement simple remote procedure
			calls between endpoints that support text frames. For example, you
			can use this to retrieve the results of certain dialplan functions
			on a different node as easily as if they were local.</para>
			<para>The other end should use SendText to send the data transfer.</para>
			<example title="Query device state of endpoints at the main branch office">
			[rx-node] ; Node A
			exten => rx,1,Set(remotestate=${QUERY(IAX2/mainbranch/2368@device-state-context)})
				same => n,ExecIf($[ "${remotestate}" = "NOT_INUSE" ]?Dial(IAX2/mainbranch/2368@extensions))
				same => n,Hangup()
			[extensions] ; Node B: allow other Asterisk systems to query local device states.
			exten => _2XXX,1,Dial(${HINT(${EXTEN})})
				same => n,Hangup()
			[device-state-context] ; Node B
			exten => _2XXX,1,SendText(${DEVICE_STATE(${HINT(${EXTEN})})})
				same => n,Hangup()
			</example>
		</description>
	</function>
 ***/

static int acf_query_read(struct ast_channel *chan, const char *cmd, char *data, char *buf, size_t len)
{
	struct ast_format_cap *cap;
	struct ast_channel *c;
	char *parse, *rbuf;
	char *tech, *destination;
	int timeout_sec = 0, timeout_ms = 5000;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(dialstr);
		AST_APP_ARG(timeout);
	);
	struct ast_custom_function *cdr_prop_func = ast_custom_function_find("CDR_PROP");

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "Missing arguments: dialstring\n");
		return -1;
	}

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	if (ast_strlen_zero(args.dialstr)) {
		ast_log(LOG_WARNING, "Missing arguments: dialstring\n");
		return -1;
	}
	if (!ast_strlen_zero(args.timeout)) {
		if ((ast_str_to_int(args.timeout, &timeout_sec) || timeout_sec < 0)) {
			ast_log(LOG_WARNING, "Invalid timeout: %s\n", args.timeout);
		} else {
			timeout_ms *= 1000;
		}
	}

	tech = args.dialstr;
	destination = strchr(tech, '/');
	if (destination) {
		*destination = '\0';
		destination++;
	} else {
		ast_log(LOG_WARNING, "Dial string must have technology/resource\n");
		return -1;
	}

	cap = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT);
	if (!cap) {
		return -1;
	}
	ast_format_cap_append(cap, ast_format_slin, 0);

	c = ast_request(tech, cap, NULL, chan, destination, NULL); /* null chan OK */
	ao2_cleanup(cap);

	if (!c) {
		return -1;
	}

	ast_channel_unlock(c);

	/* Disable CDR for this temporary channel. */
	if (cdr_prop_func) {
		ast_func_write(c, "CDR_PROP(disable)", "1");
	}

	/* Copy Caller ID, if we have it. */
	if (chan && ast_channel_caller(chan)->id.number.valid) {
		ast_channel_caller(c)->id.number.valid = 1;
		ast_channel_caller(c)->id.number.str = ast_strdup(ast_channel_caller(chan)->id.number.str);
		/* It's really the connected line that matters here, not the caller id, because that's what'll be the Caller ID on the channel we call. */
		ast_channel_connected(c)->id.number.valid = 1;
		ast_channel_connected(c)->id.number.str = ast_strdup(ast_channel_caller(chan)->id.number.str);
	}

	if (ast_call(c, destination, 0)) {
		ast_log(LOG_ERROR, "Unable to place outbound call to %s/%s\n", tech, destination);
		ast_hangup(c);
		return -1;
	}

	/* Wait for data transfer */
	if (chan) {
		ast_autoservice_start(chan);
	}
	ast_channel_ref(c);
	rbuf = ast_recvtext(c, timeout_ms);
	ast_channel_unref(c);
	ast_hangup(c);
	if (chan) {
		ast_autoservice_stop(chan);
	}

	if (!rbuf) {
		ast_log(LOG_WARNING, "No data received before channel hung up\n");
		return -1;
	}

	ast_copy_string(buf, rbuf, len);

	return 0;
}

static struct ast_custom_function query_function = {
	.name = "TEXT_QUERY",
	.read = acf_query_read,
};

static int unload_module(void)
{
	return ast_custom_function_unregister(&query_function);
}

static int load_module(void)
{
	return ast_custom_function_register(&query_function);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Remote text querying");
