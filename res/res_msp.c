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
 * \brief Send a message via the RFC1312 Message Send Protocol (version 2)
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
#include "asterisk/module.h"
#include "asterisk/message.h"
#include "asterisk/netsock2.h"
#include "asterisk/config.h"
#include "asterisk/utils.h"

/*** DOCUMENTATION
	<info name="MessageDestinationInfo" language="en_US" tech="MSP">
		<para>Specifying a prefix of <literal>msp:</literal> will send the
		message to the specified hostname using the Message Send Protocol version 2.</para>
		<para>For example:
		<literal>same => n,Set(MESSAGE(body)=This is my message)</literal> will set the message
		and <literal>same => n,MessageSend(10.1.2.3,Asterisk,Alice)</literal> will
		send a message to host 10.1.2.3, with a sender of <literal>Asterisk</literal>
		and a recipient of <literal>Alice.</literal>.</para>
		<para>Other Message Send Protocol parameters are not currently supported.</para>
	</info>
	<info name="MessageFromInfo" language="en_US" tech="MSP">
		<para>Specifying a prefix of <literal>msp:</literal> will specify the
		name of the sender of the message, as defined by RFC 1312.</para>
	</info>
	<info name="MessageToInfo" language="en_US" tech="MSP">
		<para>The name of the recipient, as defined in the Message Send Protocol.</para>
		<para>Specifying a prefix of <literal>msp:</literal> will specify the
		name of the recipient, as defined by RFC 1312.</para>
		<para>This parameter is optional, but may be required by the MSP server.</para>
	</info>
***/

static int msp_send(const struct ast_msg *msg, const char *destination, const char *from)
{
	ssize_t res;
	int sfd;
	struct ast_sockaddr addr;
	char destbuf[128];
	char *tmp;
	const char *hostname, *recip;
	char buf[512]; /* Max size of a MSP message */
	int len;
	const char *body;

	if (ast_strlen_zero(destination)) {
		ast_log(LOG_ERROR, "Missing destination for MSP message\n");
		return -1;
	} else if (ast_strlen_zero(from)) {
		ast_log(LOG_ERROR, "Missing sender for MSP message\n");
	}

	ast_copy_string(destbuf, destination, sizeof(destbuf));
	tmp = destbuf;
	strsep(&tmp, ":"); /* Skip msp: */
	hostname = tmp;
	recip = ast_msg_get_to(msg);

	if (ast_strlen_zero(hostname)) {
		ast_log(LOG_ERROR, "Missing hostname\n");
		return -1;
	}

	body = ast_msg_get_body(msg);
	if (ast_strlen_zero(body)) {
		ast_log(LOG_ERROR, "No message body to send\n");
		return -1;
	}

	/* Construct MSP message */
	len = snprintf(buf, sizeof(buf), "%c%s%c%c%s%c%s%c%c%c%c", /* To satisfy -Werror=format-contains-nul */
		'B', /* Version 2 of the MSP protocol */
		S_OR(recip, ""),
		'\0',
		'\0',
		body,
		'\0',
		from,
		'\0',
		'\0', '\0', '\0');

	ast_debug(3, "Sending %d-byte MSP message from %s -> %s@%s: %s\n", len, S_OR(from, ""), S_OR(recip, ""), hostname, body);
	ast_debug(3, "TO: %s\n", ast_msg_get_to(msg));

	memset(&addr, 0, sizeof(addr));
	ast_parse_arg(hostname, PARSE_ADDR, &addr);
	ast_sockaddr_set_port(&addr, 18);

	/* Message Send Protocol supports delivery via both TCP and UDP.
	 * Send via UDP, since there's less overhead, and we don't care about the acknowledgment */
	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sfd < 0) {
		ast_log(LOG_ERROR, "ast_socket failed: %s\n", strerror(errno));
		return -1;
	}

	res = ast_sendto(sfd, buf, len, 0, &addr);
	if (res <= 0) {
		ast_log(LOG_ERROR, "ast_sendto(%s) failed: %s\n", hostname, strerror(errno));
	}

	close(sfd);
	return res <= 0 ? -1 : 0;
}

static const struct ast_msg_tech msg_tech = {
	.name = "msp",
	.msg_send = msp_send,
};

static int unload_module(void)
{
	return ast_msg_tech_unregister(&msg_tech);
}

static int load_module(void)
{
	return ast_msg_tech_register(&msg_tech);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Message Send Protocol");
