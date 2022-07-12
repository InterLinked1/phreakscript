/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2022, Naveen Albert <asterisk@phreaknet.org>
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
 * \brief Internet Relay Chat
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 */

/*! \li \ref res_irc.c uses the configuration file \ref irc.conf
 * \addtogroup configuration_file Configuration Files
 */

/*!
 * \page irc.conf irc.conf
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/lock.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/cli.h"
#include "asterisk/manager.h"
#include "asterisk/config.h"
#include "asterisk/app.h"
#include "asterisk/utils.h"
#include "asterisk/acl.h"

#ifdef HAVE_OPENSSL
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

/*** DOCUMENTATION
	<application name="IRCSendMessage" language="en_US">
		<synopsis>
			Sends a message to an IRC channel
		</synopsis>
		<syntax>
			<parameter name="channel" required="true">
				<para>Should start with # for IRC channels.</para>
			</parameter>
			<parameter name="message" required="true" />
		</syntax>
		<description>
			<para>Sends a message to an IRC channel.</para>
		</description>
	</application>
	<manager name="IRCSendMessage" language="en_US">
		<synopsis>
			Sends a message to an IRC channel
		</synopsis>
		<syntax>
			<xi:include xpointer="xpointer(/docs/manager[@name='Login']/syntax/parameter[@name='ActionID'])" />
			<parameter name="Channel" required="true">
				<para>The name of the IRC channel.</para>
			</parameter>
			<parameter name="Message" required="true">
				<para>The message to be sent.</para>
			</parameter>
		</syntax>
		<description>
			<para>This action sends a message to an IRC channel using the built-in IRC client.</para>
			<para>If the IRC client is not already present in the specified channel, the action will silently fail.</para>
		</description>
	</manager>
 ***/

#define IRC_DEFAULT_PORT 6667
#define IRC_BUFFER_SIZE 256
#define CONFIG_FILE "irc.conf"

/* Helpful sources:
 * https://datatracker.ietf.org/doc/html/rfc1459
 * https://datatracker.ietf.org/doc/html/rfc2812
 * https://modern.ircdocs.horse/
 */

const char *send_msg_app = "IRCSendMessage";

int irc_socket = -1;
static pthread_t irc_thread;
static int authenticated;
ast_mutex_t irc_lock;

#ifdef HAVE_OPENSSL
SSL*     ssl = NULL;
SSL_CTX* ctx = NULL;
#endif

struct irc_server {
	char *hostname;
	unsigned int port;
	char *username;
	char *password;
	char *autojoin;
	unsigned int tls:1;
	unsigned int tlsverify:1;
	unsigned int sasl:1;
	unsigned int events:1;
#if 0
	/*! \note Not currently used, but in the future if this were part of a linked list, it could be */
	AST_LIST_ENTRY(irc_server) entry;	/*!< Next record */
#endif
};

/*! \todo allow multiple servers using linked list instead of just one global server (above) */

struct irc_server *ircserver; /* THE IRC server */

#define free_if(prop) if (prop) ast_free(prop);
#define update_value(irc, prop, val) free_if(irc->prop); irc->prop = ast_strdup(val);
#define contains_space(str) (strchr(str, ' '))
#define irc_debug(...) ast_debug(__VA_ARGS__)

static void free_server(struct irc_server *server)
{
	free_if(server->hostname);
	free_if(server->username);
	free_if(server->password);
	free_if(server->autojoin);
	ast_free(server);
}

static void free_ssl_and_ctx(void)
{
	SSL_CTX_free(ctx);
	SSL_free(ssl);
	ctx = NULL;
	ssl = NULL;
}

static void irc_cleanup(void)
{
	close(irc_socket);
	irc_socket = -1;
#ifdef HAVE_OPENSSL
	if (ssl) {
		SSL_shutdown(ssl);
		SSL_free(ssl);
		ssl = NULL;
	}
	if (ctx) {
		SSL_CTX_free(ctx);
		ctx = NULL;
	}
#endif
}

static int irc_disconnect(void)
{
	if (irc_socket < 0) {
		return -1;
	}
	pthread_cancel(irc_thread);
	pthread_kill(irc_thread, SIGURG);
	pthread_join(irc_thread, NULL);
	if (irc_socket >= 0) {
		irc_debug(1, "Since we killed the IRC connection, manually cleaning up\n");
		irc_cleanup();
	}
	return 0;
}

static int __attribute__ ((format (gnu_printf, 1, 2))) irc_send(const char *fmt, ...)
{
	int res = 0, bytes = 0;
	char *buf = NULL, *fullbuf = NULL;
	int len = 0;
	va_list ap;

	if (irc_socket < 0) {
		ast_log(LOG_WARNING, "No established IRC connection\n");
		return -1;
	}

	/* Action Name and ID */
	va_start(ap, fmt);
	len = ast_vasprintf(&buf, fmt, ap);
	va_end(ap);

	if (len < 0 || !buf) {
		ast_log(LOG_WARNING, "Failed to write dynamic buffer\n");
		return -1;
	}

	ast_mutex_lock(&irc_lock); /* Acquire lock before sending. No other thread can send */

	if (len > 2 && strncmp(buf + len - 2, "\r\n", 2)) {
		/* No CR LF at the end. Add it now. */
		fullbuf = ast_malloc(len + 3); /* Enough space for \r\n at the end */
		if (!fullbuf) {
			ast_log(LOG_WARNING, "Memory allocation failed\n");
			goto cleanup;
		}
		strcpy(fullbuf, buf); /* Safe */
		strcpy(fullbuf + len, "\r\n"); /* Safe */
		len += 2;
		*(fullbuf + len) = '\0'; /* Null terminate, for printf-style debugging */
	}

	irc_debug(3, "IRC=>%s", fullbuf ? fullbuf : buf); /* Message already ends with CR LF, don't add another new line */
#ifdef HAVE_OPENSSL
	if (ircserver->tls) {
		bytes = SSL_write(ssl, fullbuf ? fullbuf : buf, len);
	} else
#endif
	{
		bytes = write(irc_socket, fullbuf ? fullbuf : buf, len);
	}

	if (bytes < 1) {
		ast_log(LOG_WARNING, "Failed to write to socket\n");
		res = -1;
	}
	if (fullbuf) {
		ast_free(fullbuf);
	}

cleanup:
	ast_mutex_unlock(&irc_lock);
	ast_free(buf);

	return res;
}

static int irc_authenticate(const char *username, const char *password, const char *realname)
{
	int res = 0;

	if (ast_strlen_zero(username) || contains_space(username)) {
		ast_log(LOG_WARNING, "IRC username %s is invalid\n", username);
		return -1;
	}

	/* PASS must be sent before both USER and JOIN, if it exists */
	if (!ast_strlen_zero(password)) {
		res |= irc_send("PASS %s", password); /* Password, if applicable (not actually used all that much) */
	}

	/* Confused about the difference between the two? See https://stackoverflow.com/questions/31666247/ */
	res |= irc_send("NICK %s", username); /* Actual IRC nickname */
	res |= irc_send("USER %s 0 * :%s", username, S_OR(realname, username)); /* User part of hostmask, mode, unused, real name for WHOIS */
	return 0;
}

static int irc_nickserv_login(const char *username, const char *password)
{
	int res = 0;

	if (ast_strlen_zero(username) || contains_space(username)) {
		ast_log(LOG_WARNING, "IRC username %s is invalid\n", username);
		return -1;
	}
	if (ast_strlen_zero(password) || contains_space(password)) {
		ast_log(LOG_WARNING, "IRC password is invalid\n");
		return -1;
	}

	/* Confused about the difference between the two? See https://stackoverflow.com/questions/31666247/ */
	res |= irc_send("PRIVMSG NickServ :IDENTIFY %s %s", username, password); /* Actual IRC nickname */
	return 0;
}

/*! \brief # is not automatic, channels should include leading # or & */
static int irc_channel_join(const char *channel)
{
	int res;
	if (ast_strlen_zero(channel)) {
		ast_log(LOG_WARNING, "Empty channel name\n");
		return -1;
	}
	res = irc_send("JOIN %s", channel);
	if (!res) {
		ast_verb(4, "Joined IRC channel %s\n", channel);
	}
	return res;
}

static int irc_autojoin(void)
{
	if (ircserver->autojoin) {
		char *channel, *channels = ast_strdupa(ircserver->autojoin);
		while ((channel = strsep(&channels, ","))) {
			irc_channel_join(channel);
		}
	}
	return 0;
}

static void logged_in_callback(void)
{
	if (!authenticated) {
		ast_verb(3, "IRC client now authenticated to IRC server\n");
		authenticated = 1;
		irc_autojoin();
	}
}

static int irc_incoming(char *raw)
{
	char *username, *action, *tmp;

	irc_debug(3, "IRC<=%s\n", raw);

	if (!strncmp(raw, "PING :", 6)) { /* Ping? Pong! */
		irc_send("PONG :Ping pong!!");
		return 0;
	}

	if (!strncmp(raw, ":-", 2)) {
		return 0; /* Some kind of comment/general server message, ignore... */
	}
	if (!strcmp(raw, "End of /NAMES list.")) {
		return 0; /* Don't care */
	}
	if (!strcmp(raw, "AUTHENTICATE +")) {
		return 0;
	}

	action = strchr(raw, ' ');
	if (!action) {
		ast_log(LOG_WARNING, "Unexpected IRC message: %s\n", raw);
		return -1;
	}
	*action++ = '\0';
	tmp = strchr(action, ' ');
	if (!tmp) {
		ast_log(LOG_WARNING, "Unexpected IRC action: %s\n", action);
		return -1;
	}
	*tmp++ = '\0';
	username = raw + 1; /* Skip leading : */
	action = ast_strdup(action);
	/* Do something with message (or not) */
	if (!strcasecmp(action, "PRIVMSG")) {
		char *channel = NULL, *excl = strchr(username, '!');
		if (excl) {
			*excl = '\0'; /* Remove host info, etc. and just retain actual nick */
		}
		irc_debug(1, "Message from %s: %s\n", username, tmp);
		if (*tmp == '#') {
			channel = tmp;
			tmp = strchr(channel, ':');
			if (*tmp) {
				*tmp++ = '\0';
			}
		}
		if (ircserver->events) { /* Only raise an event if this flag is enabled */
			/*** DOCUMENTATION
				<managerEvent language="en_US" name="IRCMessage">
					<managerEventInstance class="EVENT_FLAG_USER">
						<synopsis>Raised when a message is sent in an IRC channel.</synopsis>
						<syntax>
							<parameter name="Channel">
								<para>IRC channel name. Can be NULL.</para>
							</parameter>
							<parameter name="User">
								<para>User that sent the message.</para>
							</parameter>
							<parameter name="Message">
								<para>The message body.</para>
							</parameter>
						</syntax>
						<description>
							<note><para>The channel is only present for channel names starting with #.</para>
							</note>
						</description>
					</managerEventInstance>
				</managerEvent>
			***/
			manager_event(EVENT_FLAG_USER, "IRCMessage",
				"Channel: %s\r\n"
				"User: %s\r\n"
				"Message: %s\r\n",
				channel,
				username,
				tmp);
		}
	/*! \todo expand with more actions */
	} else if (!strcasecmp(action, "JOIN")) {
		irc_debug(1, "%s has joined %s\n", username, tmp);
	} else if (!strcasecmp(action, "PART")) {
		irc_debug(1, "%s has left %s\n", username, tmp);
	} else if (!strcasecmp(action, "QUIT")) {
		irc_debug(1, "%s has quit %s\n", username, tmp);
	} else if (!strcasecmp(action, "NICK")) {
		irc_debug(1, "%s is now known as %s\n", username, tmp);
	} else if (!strcasecmp(action, "NOTICE")) {
		irc_debug(1, "Notice from %s: %s\n", username, tmp);
		if (strstr(tmp, "now identified")) {
			/* No IRC numeric for successful login if not using SASL */
			logged_in_callback();
		}
	} else if (!strcasecmp(action, "MODE")) {
		/* MODE: Don't care */
	} else if (!strcasecmp(action, "TOPIC")) {
		/* TOPIC: Don't care */
	} else if (!strcasecmp(action, "CAP")) {
		/* CAP, e.g. CAP * LS, don't care */
	} else {
		int numeric = atoi(action);
		/*! \todo define enum for all the numerics and use those instead */
		switch (numeric) {
		case 404:
			ast_log(LOG_WARNING, "%s\n", tmp); /* Failed to send message to channel (probably not in it) */
			break;
		case 900: /* Successful log in */
			logged_in_callback();
			break;
		case 903: /* Successful SASL log in */
			ast_verb(4, "IRC SASL authentication successful\n");
			break;
		case 904: /* SASL login error */
			ast_log(LOG_WARNING, "SASL authentication failed\n");
			break;
		case 0:
			/* XXX currently lots of unhandled stuff here */
			ast_log(LOG_WARNING, "Unexpected IRC message: %s %s %s\n", username, action, tmp);
			break;
		default:
			/* Ignore other IRC numerics */
			break;
		}
	}
	ast_free(action);
	return 0;
}

static void *irc_loop(void *vargp)
{
	int res;
	char inbuf[IRC_BUFFER_SIZE];
	struct pollfd fds[2];
	char *readinbuf;
	char *message, *messages;

	if (irc_socket < 0) {
		return NULL;
	}

	fds[0].fd = irc_socket;
	fds[0].events = POLLIN;
	readinbuf = inbuf;

	/* Kind of simple, but just blindly send all the setup info at once, without really having a 2-way conversation with the server. */
	authenticated = 0;

	if (ircserver->sasl) {
		irc_send("CAP LS 302");
	}
	if (irc_authenticate(ircserver->username, NULL, NULL)) {
		ast_log(LOG_WARNING, "Failed to authenticate... IRC client now suspending\n");
		return NULL;
	}
	if (ircserver->sasl) {
		int len;
		char base64encoded[401];
		char base64decoded[256];
		irc_send("CAP REQ :multi-prefix sasl");
		/* Plain SASL: https://www.rfc-editor.org/rfc/rfc4616.html */
		/* Base64 encode authentication identity, authorization identity, password (nick, name, password, separated by NUL) */
		len = snprintf(base64decoded, sizeof(base64decoded), "%s%c%s%c%s", ircserver->username, '\0', ircserver->username, '\0', ircserver->password);
		ast_base64encode(base64encoded, (unsigned char *) base64decoded, len, sizeof(base64encoded));
		irc_send("AUTHENTICATE PLAIN");
		/* Expect: AUTHENTICATE + */
		irc_send("AUTHENTICATE %s", base64encoded);
		irc_send("CAP END");
	}
	if (ircserver->password) { /* Authenticate to NickServ if we have a password */
		if (irc_nickserv_login(ircserver->username, ircserver->password)) {
			ast_log(LOG_WARNING, "Failed to authenticate... IRC client now suspending\n");
			return NULL;
		}
	} else {
		/* If we're authenticating, wait for a successful login.
		 * A major reason for doing this is we don't want to join any channels
		 * until we're fully logged in. This ensures that if we have a cloak,
		 * it gets applied, so we don't leak our IP address.
		 */
		irc_autojoin();
	}

	for (;;) {
		res = ast_poll(fds, 1, -1);
		pthread_testcancel();
		if (res < 0) {
			if (errno == EINTR) {
				continue;
			}
			ast_log(LOG_WARNING, "poll returned error: %s\n", strerror(errno));
			break;
		}
		/* Data from IRC server? */
		if (fds[0].revents) {
#ifdef HAVE_OPENSSL
			if (ircserver->tls) {
				res = SSL_read(ssl, readinbuf, IRC_BUFFER_SIZE - 2 - (readinbuf - inbuf));
			} else
#endif
			{
				res = recv(irc_socket, readinbuf, IRC_BUFFER_SIZE - 2 - (readinbuf - inbuf), 0);
			}
			if (res < 1) {
				break;
			}
			*(readinbuf + res) = '\0';
			*(readinbuf + res + 1) = '\0';
			readinbuf = inbuf; /* Reset pointer to beginning before we dup */
			messages = ast_strdup(readinbuf); /* Don't use ast_strdupa since we're in a loop */
			while ((message = strsep(&messages, "\n"))) { /* Use only LF, so that way we can verify messages end in CR or we know they're incomplete. */
				char *end = strchr(message, '\r');
				if (ast_strlen_zero(message)) {
					continue;
				}
				if (!end) {
					/* Haven't gotten a CR LF yet, so this message isn't actually complete yet. Go round for another loop. */
					irc_debug(8, "No CR LF, message not yet finished? %s\n", message);
					strcpy(readinbuf, message); /* Safe, since this was in the buffer in the first place */
					readinbuf += strlen(message);
					ast_free(messages);
					break;
				}
				*end = '\0'; /* Don't pass CR to message processor, just the message itself */
				irc_incoming(message); /* Got a complete message. */
			}
		}
	}

	irc_debug(1, "IRC connection terminated\n");

	irc_cleanup();
	return NULL;
}

static int irc_connect(const char *hostname, int port)
{
	int fd;
	struct ast_sockaddr saddr;

	if (ast_strlen_zero(hostname)) {
		ast_log(LOG_WARNING, "No hostname specified\n");
		return -1;
	}

	if (irc_socket >= 0) {
		irc_debug(2, "IRC socket already registered as fd %d\n", irc_socket);
		return -1;
	}

	memset(&saddr, 0, sizeof(saddr));
	if (!port) {
		port = IRC_DEFAULT_PORT;
	}

	if (ast_get_ip(&saddr, hostname)) {
		/* Lookup failed. */
		ast_log(LOG_WARNING, "Unable to resolve IRC server '%s'\n", hostname);
		return -1;
	}
	ast_sockaddr_set_port(&saddr, port);

	if (ircserver->tls) {
#ifdef HAVE_OPENSSL
		OpenSSL_add_ssl_algorithms();
		SSL_load_error_strings();
		ctx = SSL_CTX_new(TLS_client_method());
		if (!ctx) {
			ast_log(LOG_ERROR, "Failed to setup new SSL context\n");
			return -1;
		}
		SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3); /* Only use TLS */
		ssl = SSL_new(ctx);
		if (!ssl) {
			ast_log(LOG_WARNING, "Failed to create new SSL\n");
			SSL_CTX_free(ctx);
			ctx = NULL;
			return -1;
		}
#else
		ast_log(LOG_WARNING, "Asterisk was compiled without OpenSSL: TLS is not available for this connection\n");
		/* Rather than falling back to plain text and sending credentials in the clear,
		 * abort if we were supposed to use TLS but can't. */
		return -1;
#endif
	}

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		ast_log(LOG_WARNING, "Unable to create socket: %s\n", strerror(errno));
#ifdef HAVE_OPENSSL
		if (ctx) {
			SSL_CTX_free(ctx);
			ctx = NULL;
		}
#endif
		return -1;
	}

	if (ast_connect(fd, &saddr)) {
		ast_log(LOG_WARNING, "Failed to connect to %s: %s\n", ast_sockaddr_stringify(&saddr), strerror(errno));
#ifdef HAVE_OPENSSL
		if (ctx) {
			SSL_CTX_free(ctx);
			ctx = NULL;
		}
#endif
		return -1;
	}

#ifdef HAVE_OPENSSL
	if (ircserver->tls) {
		X509 *server_cert;
		char *str;
		if (SSL_set_fd(ssl, fd) != 1) {
			ast_log(LOG_WARNING, "Failed to connect SSL: %s\n", ERR_error_string(ERR_get_error(), NULL));
			free_ssl_and_ctx();
			close(fd);
			return -1;
		}
		if (SSL_connect(ssl) == -1) {
			ast_log(LOG_WARNING, "Failed to connect SSL: %s\n", ERR_error_string(ERR_get_error(), NULL));
			ERR_print_errors_fp(stderr);
			free_ssl_and_ctx();
			close(fd);
			return -1;
		}
		/* Verify cert */
		server_cert = SSL_get_peer_certificate(ssl);
		if (!server_cert) {
			ast_log(LOG_WARNING, "Failed to get peer certificate\n");
			free_ssl_and_ctx();
			close(fd);
			return -1;
		}
		str = X509_NAME_oneline(X509_get_subject_name(server_cert), 0, 0);
		if (!str) {
			ast_log(LOG_WARNING, "Failed to get peer certificate\n");
			free_ssl_and_ctx();
			close(fd);
			return -1;
		}
		ast_debug(8, "TLS SN: %s\n", str);
		OPENSSL_free(str);
		str = X509_NAME_oneline(X509_get_issuer_name (server_cert), 0, 0);
		if (!str) {
			ast_log(LOG_WARNING, "Failed to get peer certificate\n");
			free_ssl_and_ctx();
			close(fd);
			return -1;
		}
		ast_debug(8, "TLS Issuer: %s\n", str);
		OPENSSL_free(str);
		X509_free(server_cert);
		if (ircserver->tlsverify) { /* If we're verifying the cert, go for it. */
			long verify_result;
			verify_result = SSL_get_verify_result(ssl);
			if (verify_result != X509_V_OK) {
				ast_log(LOG_WARNING, "SSL verify failed: %ld (%s)\n", verify_result, X509_verify_cert_error_string(verify_result));
				free_ssl_and_ctx();
				close(fd);
				return -1;
			}
		}
	}
#endif

	ast_verb(3, "Established %s IRC connection to %s\n", ircserver->tls ? "secure" : "plain text", ast_sockaddr_stringify(&saddr));
	irc_socket = fd;

	if (ast_pthread_create(&irc_thread, NULL, irc_loop, NULL)) {
		ast_log(LOG_WARNING, "Failed to create IRC thread\n");
#ifdef HAVE_OPENSSL
		if (ircserver->tls) {
			free_ssl_and_ctx();
		}
#endif
		close(fd);
		return -1;
	}

	return 0;
}

static int irc_send_user_message(const char *user, const char *message)
{
	if (ast_strlen_zero(user)) {
		ast_log(LOG_WARNING, "Empty recipient\n");
		return -1;
	}
	if (ast_strlen_zero(message)) {
		ast_log(LOG_WARNING, "Empty message\n");
		return -1;
	}
	return irc_send("PRIVMSG %s :%s", user, message);
}

static int irc_msg_exec(struct ast_channel *chan, const char *data)
{
	int res;
	char *parse;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(channel);
		AST_APP_ARG(message);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "Missing arguments (channel,message)\n");
	}
	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);

	if (chan) {
		ast_autoservice_start(chan);
	}
	res = irc_send_user_message(args.channel, args.message);
	if (chan) {
		ast_autoservice_stop(chan);
	}
	return res;
}

static int irc_msg_tx(struct mansession *s, const struct message *m)
{
	const char *channel = astman_get_header(m, "Channel");
	const char *id = astman_get_header(m, "ActionID");
	const char *message = astman_get_header(m, "Message");

	if (ast_strlen_zero(channel)) {
		astman_send_error(s, m, "No channel specified");
		return AMI_SUCCESS;
	}

	if (ast_strlen_zero(message)) {
		astman_send_error(s, m, "Message is required");
		return AMI_SUCCESS;
	}

	if (irc_send_user_message(channel, message)) {
		astman_send_error(s, m, "IRC message failed to send");
		return AMI_SUCCESS; /* Yeah, it doesn't make sense - but returning AMI_DESTROY would cause the entire AMI connection to terminate */
	}

	astman_append(s, "Response: Success\r\n");
	if (!ast_strlen_zero(id)) {
		astman_append(s, "ActionID: %s\r\n", id);
	}

	astman_append(s, "\r\n");

	return AMI_SUCCESS;
}

static char *handle_irc_sendmsg(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "irc msg";
		e->usage =
			"Usage: irc msg <channel> <message>\n"
			"       Send message to an IRC channel (if starts with #) or user.\n"
			"       If message contains spaces, it will need to be quoted.\n"
			"       This is intended for debugging so complex messages may\n"
			"       not work as expected.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4 || ast_strlen_zero(a->argv[2]) || ast_strlen_zero(a->argv[3])) {
		return CLI_SHOWUSAGE;
	}

	/* Always use irc_send_user_message, so that if it starts with #, it will go to a channel */
	if (irc_send_user_message(a->argv[2], a->argv[3])) {
		return CLI_FAILURE;
	}

	return CLI_SUCCESS;
}

static char *handle_irc_join_channel(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "irc join";
		e->usage =
			"Usage: irc join <channel>\n"
			"       Join an IRC channel. You must prefix with # or & for channels.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3 || ast_strlen_zero(a->argv[2])) {
		return CLI_SHOWUSAGE;
	}
	if (irc_channel_join(a->argv[2])) {
		return CLI_FAILURE;
	}

	return CLI_SUCCESS;
}

static int irc_leave(const char *channel)
{
	return !ast_strlen_zero(channel) ? irc_send("PART %s", channel) : irc_send("QUIT :That's all, folks!");
}

static char *handle_irc_part_channel(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "irc part";
		e->usage =
			"Usage: irc part <channel>\n"
			"       Leave an IRC channel. You must prefix with # or & for channels.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3 || ast_strlen_zero(a->argv[2])) {
		return CLI_SHOWUSAGE;
	}
	if (irc_leave(a->argv[2])) {
		return CLI_FAILURE;
	}

	return CLI_SUCCESS;
}

static char *handle_irc_login(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "irc login";
		e->usage =
			"Usage: irc login <nick> <password>\n"
			"       Log in with NickServ.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 4 || ast_strlen_zero(a->argv[2]) || ast_strlen_zero(a->argv[3])) {
		return CLI_SHOWUSAGE;
	}

	if (irc_nickserv_login(a->argv[2], a->argv[3])) {
		return CLI_FAILURE;
	}

	return CLI_SUCCESS;
}

static struct ast_cli_entry cli_irc[] = {
	AST_CLI_DEFINE(handle_irc_sendmsg, "Send message to an IRC channel"),
	AST_CLI_DEFINE(handle_irc_join_channel, "Join an IRC channel"),
	AST_CLI_DEFINE(handle_irc_part_channel, "Leave an IRC channel"),
	AST_CLI_DEFINE(handle_irc_login, "Login with NickServ on IRC server")
};

static int irc_reload(int reload)
{
	int res = 0;
	char *cat = NULL;
	struct ast_config *cfg;
	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };
	const char *tempstr;

	if (!(cfg = ast_config_load(CONFIG_FILE, config_flags))) {
		ast_log(LOG_WARNING, "Missing config file (%s)\n", CONFIG_FILE);
		return -1;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		irc_debug(1, "Config file %s unchanged, skipping\n", CONFIG_FILE);
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Config file %s is in an invalid format. Aborting.\n", CONFIG_FILE);
		return -1;
	}

	ast_mutex_lock(&irc_lock);

	if (ircserver) {
		free_server(ircserver);
	}
	if (!(ircserver = ast_calloc(1, sizeof(*ircserver)))) {
		return NULL;
	}

	/* General section */
	/* Reset Global Var Values */
	ircserver->hostname = ((tempstr = ast_variable_retrieve(cfg, "general", "hostname"))) ? ast_strdup(tempstr) : NULL;
	ircserver->port = ((tempstr = ast_variable_retrieve(cfg, "general", "port"))) ? atoi(tempstr) : IRC_DEFAULT_PORT;
	ircserver->username = ((tempstr = ast_variable_retrieve(cfg, "general", "username"))) ? ast_strdup(tempstr) : NULL;
	ircserver->password = ((tempstr = ast_variable_retrieve(cfg, "general", "password"))) ? ast_strdup(tempstr) : NULL;
	ircserver->autojoin = ((tempstr = ast_variable_retrieve(cfg, "general", "autojoin"))) ? ast_strdup(tempstr) : NULL;
	ircserver->tls = ((tempstr = ast_variable_retrieve(cfg, "general", "tls"))) ? !strcasecmp(tempstr, "yes") ? 1 : 0 : 0;
	ircserver->tlsverify = ((tempstr = ast_variable_retrieve(cfg, "general", "tlsverify"))) ? !strcasecmp(tempstr, "yes") ? 1 : 0 : 0;
	ircserver->sasl = ((tempstr = ast_variable_retrieve(cfg, "general", "sasl"))) ? !strcasecmp(tempstr, "yes") ? 1 : 0 : 0;
	ircserver->events = ((tempstr = ast_variable_retrieve(cfg, "general", "events"))) ? !strcasecmp(tempstr, "yes") ? 1 : 0 : 0;

#ifndef HAVE_OPENSSL
	if (ircserver->tls) {
		ast_log(LOG_WARNING, "Server configured with TLS, but Asterisk was not compiled with OpenSSL. This will fail.\n");
	}
#endif

	/* Remaining sections */
	while ((cat = ast_category_browse(cfg, cat))) {
		const char *type;
		if (!strcasecmp(cat, "general")) {
			continue;
		}
		if (!(type = ast_variable_retrieve(cfg, cat, "type"))) {
			ast_log(LOG_WARNING, "Invalid entry in %s: %s defined with no type!\n", CONFIG_FILE, cat);
			continue;
		} else {
			ast_log(LOG_WARNING, "Unknown type: '%s'\n", type);
		}
	}

	ast_mutex_unlock(&irc_lock);

	ast_config_destroy(cfg);

	return res;
}

static int irc_initialize(void)
{
	ast_mutex_lock(&irc_lock);

	if (ast_strlen_zero(ircserver->username)) {
		ast_log(LOG_WARNING, "No IRC username configured\n");
		return AST_MODULE_LOAD_DECLINE;
	}
	if (irc_connect(ircserver->hostname, ircserver->port)) {
		return AST_MODULE_LOAD_DECLINE;
	}
	ast_mutex_unlock(&irc_lock);

	return 0;
}

static int reload_module(void)
{
	int res;
	if (irc_socket >= 0) {
		ast_log(LOG_NOTICE, "IRC client is currently running and will not be restarted on a reload\n");
	}
	res = irc_reload(1);
	if (irc_socket < 0) {
		/* Only if we weren't already running in the first place do we try to now start the client */
		irc_initialize();
	}
	return res;
}

static int unload_module(void)
{
	int res = 0;

	ast_unregister_application(send_msg_app);
	ast_cli_unregister_multiple(cli_irc, ARRAY_LEN(cli_irc));
	res |= ast_manager_unregister("IRCSendMessage");
	if (irc_socket >= 0) {
		irc_leave(NULL); /* If we're still connected, try to send a QUIT message of our own accord before we leave. */
	}
	irc_disconnect();
	if (ircserver) {
		free_server(ircserver);
	}

#ifdef HAVE_OPENSSL
	ERR_free_strings(); /* stub */
    EVP_cleanup();
#endif

	return res;
}

static int load_module(void)
{
	int res = 0;

	ast_mutex_init(&irc_lock);
	ircserver = NULL;
	if (irc_reload(0)) {
		return AST_MODULE_LOAD_DECLINE;
	}

	res = irc_initialize();
	if (res) {
		return res;
	}

	ast_cli_register_multiple(cli_irc, ARRAY_LEN(cli_irc));
	res |= ast_register_application_xml(send_msg_app, irc_msg_exec);
	res |= ast_manager_register_xml("IRCSendMessage", EVENT_FLAG_USER, irc_msg_tx);

#ifdef HAVE_OPENSSL
	SSL_load_error_strings(); /* stub */
    SSL_library_init(); /* stub */
    OpenSSL_add_all_algorithms();
#endif

	return res;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Internet Relay Chat",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.reload = reload_module,
);
