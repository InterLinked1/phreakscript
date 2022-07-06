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
 * \brief Send email application
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <sys/stat.h>

#include "asterisk/logger.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/file.h"
#include "asterisk/utils.h"

/*** DOCUMENTATION
	<application name="SendMail" language="en_US">
		<synopsis>
			Sends an email using the system mailer. The recipient and sender info
			can be customized per invocation, and multiple attachments can be sent.
		</synopsis>
		<syntax>
			<parameter name="recipient" required="true">
				<para>Pipe-separated list of email addresses.</para>
			</parameter>
			<parameter name="subject" required="true" />
			<parameter name="body" required="true">
				<para>The name of the variable contain the body of the message.
				This should not be the variable itself, only the name of it.</para>
			</parameter>
			<parameter name="options" required="true">
				<optionlist>
				<option name="A" argsep=":">
					<argument name="filepath">
						<para>The full path to the file to attach.</para>
					</argument>
					<argument name="filename">
						<para>The name of the attached file. The default name
						is the base file name of the attached file.</para>
					</argument>
					<argument name="mimetype">
						<para>The MIME format of the attachment. There is no default
						Content-Type header for attachments so some mail clients may
						not respond well to this. You should include the MIME type if you
						know what it is (e.g. <literal>text/plain</literal>).</para>
					</argument>
					<para>Pipe-separated list of attachments, using the full path.</para>
				</option>
				<option name="d">
					<para>Delete successfully attached files once the email is sent.</para>
				</option>
				<option name="e">
					<para>Interpret escaping in the body of the email.
					This option is required to add tabs and new lines.</para>
				</option>
				<option name="f">
					<para>Email address from which the email should be sent. This option
					is required.</para>
				</option>
				<option name="F">
					<para>Name from which the email should be sent.</para>
				</option>
				<option name="R">
					<para>Name of the recipient.</para>
				</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Sends an email using the system mailer.</para>
			<para>Sets <variable>MAILSTATUS</variable> to one of the following values:</para>
			<variablelist>
				<variable name="MAILSTATUS">
					<value name="SUCCESS">
						Email was successfully sent. This does not necessarily mean delivery
						will be successful or that the email will not bounce.
					</value>
					<value name="FAILURE">
						Email was not successfully sent.
					</value>
				</variable>
			</variablelist>
		</description>
	</application>
 ***/

static char *app = "SendMail";

#define SENDMAIL "/usr/sbin/sendmail -t"
#define ENDL "\n"
#define	MAIL_FILE_MODE	0666

static int my_umask;

static void make_email_file(FILE *p, const char *subject, const char *body, const char *toaddress, const char *toname, const char *fromaddress, const char *fromname, const char *attachments, int delete, int interpret)
{
	struct ast_tm tm;
	char date[256];
	char host[MAXHOSTNAMELEN] = "";
	char who[256];
	char bound[256];
	struct ast_str *str2 = ast_str_create(16);
	char filename[256];
	char *emailsbuf, *email, *attachmentsbuf, *attachment;
	struct timeval when = ast_tvnow();

	if (!str2) {
		ast_free(str2);
		return;
	}
	gethostname(host, sizeof(host) - 1);

	if (strchr(toaddress, '@')) {
		ast_copy_string(who, toaddress, sizeof(who));
	} else {
		snprintf(who, sizeof(who), "%s@%s", toaddress, host);
	}

	ast_strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %z", ast_localtime(&when, &tm, NULL));
	fprintf(p, "Date: %s" ENDL, date);
	fprintf(p, "From: %s <%s>" ENDL, fromname, fromaddress);

	emailsbuf = ast_strdupa(toaddress);
	fprintf(p, "To:");
	while ((email = strsep(&emailsbuf, "|"))) {
		char *next = emailsbuf;
		fprintf(p, " \"%s\" <%s>%s" ENDL, toname, email, next ? "," : "");
	}
	ast_free(str2);

	fprintf(p, "Subject: %s" ENDL, subject);
	fprintf(p, "Message-ID: <Asterisk-%u-%d@%s>" ENDL, (unsigned int) ast_random(), (int) getpid(), host);
	fprintf(p, "MIME-Version: 1.0" ENDL);
	if (!ast_strlen_zero(attachments)) {
		/* Something unique. */
		snprintf(bound, sizeof(bound), "----attachment_%d%u", (int) getpid(), (unsigned int) ast_random());
		fprintf(p, "Content-Type: multipart/mixed; boundary=\"%s\"" ENDL, bound);
		fprintf(p, ENDL ENDL "This is a multi-part message in MIME format." ENDL ENDL);
		fprintf(p, "--%s" ENDL, bound);
	}
	fprintf(p, "Content-Type: text/plain; charset=%s" ENDL "Content-Transfer-Encoding: 8bit" ENDL ENDL, "ISO-8859-1");
	if (interpret) {
		body = ast_unescape_c(body);
		fwrite(body, sizeof(char), strlen(body), p);
	} else {
		fprintf(p, "%s", body);
	}
	if (ast_strlen_zero(attachments)) {
		return;
	}
	attachmentsbuf = ast_strdupa(attachments);
	while ((attachment = strsep(&attachmentsbuf, "|"))) {
		char *fullname, *friendlyname, *mimetype;
		snprintf(filename, sizeof(filename), "%s", basename(attachment));

		mimetype = attachment;
		fullname = strsep(&mimetype, ":");
		friendlyname = strsep(&mimetype, ":");

		if (ast_strlen_zero(friendlyname)) {
			friendlyname = filename; /* no specified name, so use the full base file name */
		}

		if (!ast_file_is_readable(fullname)) {
			ast_log(AST_LOG_WARNING, "Failed to open file: %s\n", fullname);
			continue;
		}
		ast_debug(5, "Creating attachment: %s (named %s)\n", fullname, friendlyname);
		fprintf(p, ENDL "--%s" ENDL, bound);

		if (!ast_strlen_zero(mimetype)) {
			/* is Content-Type name just the file name? Or maybe there's more to it than that? */
			fprintf(p, "Content-Type: %s; name=\"%s\"" ENDL, mimetype, friendlyname);
		}

		fprintf(p, "Content-Transfer-Encoding: base64" ENDL);
		fprintf(p, "Content-Description: File attachment." ENDL);
		fprintf(p, "Content-Disposition: attachment; filename=\"%s\"" ENDL ENDL, friendlyname);
		ast_base64_encode_file_path(fullname, p, ENDL);
		if (delete) {
			unlink(fullname);
		}
	}
	fprintf(p, ENDL ENDL "--%s--" ENDL "." ENDL, bound); /* After the last attachment */
}

enum {
	OPT_ATTACHMENTS =        (1 << 0),
	OPT_FROM_ADDRESS =       (1 << 1),
	OPT_FROM_NAME =          (1 << 2),
	OPT_TO_NAME =            (1 << 3),
	OPT_DELETE_ATTACHMENTS = (1 << 4),
	OPT_ESCAPE =             (1 << 5),
};

enum {
	OPT_ARG_ATTACHMENTS,
	OPT_ARG_FROM_ADDRESS,
	OPT_ARG_FROM_NAME,
	OPT_ARG_TO_NAME,
	/* note: this entry _MUST_ be the last one in the enum */
	OPT_ARG_ARRAY_SIZE,
};

AST_APP_OPTIONS(mail_exec_options, BEGIN_OPTIONS
	AST_APP_OPTION_ARG('A', OPT_ATTACHMENTS, OPT_ARG_ATTACHMENTS),
	AST_APP_OPTION('d', OPT_DELETE_ATTACHMENTS),
	AST_APP_OPTION('e', OPT_ESCAPE),
	AST_APP_OPTION_ARG('f', OPT_FROM_ADDRESS, OPT_ARG_FROM_ADDRESS),
	AST_APP_OPTION_ARG('F', OPT_FROM_NAME, OPT_ARG_FROM_NAME),
	AST_APP_OPTION_ARG('R', OPT_TO_NAME, OPT_ARG_TO_NAME),
END_OPTIONS);

AST_THREADSTORAGE(result_buf);
AST_THREADSTORAGE(tmp_buf);

static int mail_exec(struct ast_channel *chan, const char *data)
{
	char *parse;
	char *toaddress, *subject, *body;
	FILE *p = NULL;
	char tmp[80] = "/tmp/astmail-XXXXXX";
	char tmp2[256];
	char *mailcmd = SENDMAIL; /* Mail command */
	char *fromaddress = "", *toname = "";
	char *fromname = "Asterisk PBX";
	char *attachments = "";
	int delete = 0, interpret = 0;
	int res;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(recipient);
		AST_APP_ARG(subject);
		AST_APP_ARG(body);
		AST_APP_ARG(options);
	);
	struct ast_flags64 opts = { 0, };
	char *opt_args[OPT_ARG_ARRAY_SIZE];
	char *varsubst;
	struct ast_str *str = ast_str_thread_get(&result_buf, 16);

	parse = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, parse);
	if (args.argc < 3) {
		ast_log(LOG_ERROR, "Incorrect number of arguments\n");
		pbx_builtin_setvar_helper(chan, "MAILSTATUS", "FAILURE");
		return 0;
	}
	if (ast_strlen_zero(args.recipient)) {
		ast_log(LOG_NOTICE, "Mail requires a recipient\n");
		pbx_builtin_setvar_helper(chan, "MAILSTATUS", "FAILURE");
		return 0;
	}
	if (ast_strlen_zero(args.subject)) {
		ast_log(LOG_NOTICE, "Mail requires a subject\n");
		pbx_builtin_setvar_helper(chan, "MAILSTATUS", "FAILURE");
		return 0;
	}
	if (ast_strlen_zero(args.body)) {
		ast_log(LOG_NOTICE, "Mail requires a body variable\n");
		pbx_builtin_setvar_helper(chan, "MAILSTATUS", "FAILURE");
		return 0;
	}
	if (!ast_strlen_zero(args.options) &&
		ast_app_parse_options64(mail_exec_options, &opts, opt_args, args.options)) {
		ast_log(LOG_ERROR, "Invalid options: '%s'\n", args.options);
		pbx_builtin_setvar_helper(chan, "MAILSTATUS", "FAILURE");
		return 0;
	}
	toaddress = args.recipient;
	subject = args.subject;
	body = args.body;

	varsubst = ast_alloca(strlen(args.body) + 4);
	sprintf(varsubst, "${%s}", args.body);
	ast_str_substitute_variables(&str, 0, chan, varsubst);
	body = ast_str_buffer(str);

	if (ast_test_flag64(&opts, OPT_ATTACHMENTS)
		&& !ast_strlen_zero(opt_args[OPT_ARG_ATTACHMENTS])) {
		attachments = opt_args[OPT_ARG_ATTACHMENTS];
	}
	if (ast_test_flag64(&opts, OPT_FROM_ADDRESS)
		&& !ast_strlen_zero(opt_args[OPT_ARG_FROM_ADDRESS])) {
		fromaddress = opt_args[OPT_ARG_FROM_ADDRESS];
	} else {
		ast_log(LOG_ERROR, "Must specify from address\n");
		pbx_builtin_setvar_helper(chan, "MAILSTATUS", "FAILURE");
		return 0;
	}
	if (ast_test_flag64(&opts, OPT_FROM_NAME)
		&& !ast_strlen_zero(opt_args[OPT_ARG_FROM_NAME])) {
		fromname = opt_args[OPT_ARG_FROM_NAME];
	}
	if (ast_test_flag64(&opts, OPT_TO_NAME)
		&& !ast_strlen_zero(opt_args[OPT_ARG_TO_NAME])) {
		toname = opt_args[OPT_ARG_TO_NAME];
	}
	if (ast_test_flag64(&opts, OPT_DELETE_ATTACHMENTS)) {
		delete = 1;
	}
	if (ast_test_flag64(&opts, OPT_ESCAPE)) {
		interpret = 1;
	}

	/* Make a temporary file instead of piping directly to sendmail, in case the mail
	   command hangs */
	if ((p = ast_file_mkftemp(tmp, MAIL_FILE_MODE & ~my_umask)) == NULL) {
		ast_log(LOG_WARNING, "Unable to launch '%s' (can't create temporary file)\n", mailcmd);
		pbx_builtin_setvar_helper(chan, "MAILSTATUS", "FAILURE");
		return 0;
	}
	make_email_file(p, subject, body, toaddress, toname, fromaddress, fromname, attachments, delete, interpret);
	fclose(p);
	snprintf(tmp2, sizeof(tmp2), "( %s < %s ; rm -f %s ) &", mailcmd, tmp, tmp);
	res = ast_safe_system(tmp2);
	if ((res < 0) && (errno != ECHILD)) {
		ast_log(LOG_WARNING, "Unable to execute '%s'\n", (char *)data);
		pbx_builtin_setvar_helper(chan, "MAILSTATUS", "FAILURE");
	} else if (res == 127) {
		ast_log(LOG_WARNING, "Unable to execute '%s'\n", (char *)data);
		pbx_builtin_setvar_helper(chan, "MAILSTATUS", "FAILURE");
	} else {
		if (res < 0)
			res = 0;
		if (res != 0)
			pbx_builtin_setvar_helper(chan, "MAILSTATUS", "FAILURE");
		else {
			ast_debug(1, "Sent mail to %s with command '%s'\n", toaddress, mailcmd);
			pbx_builtin_setvar_helper(chan, "MAILSTATUS", "SUCCESS");
		}
	}
	ast_autoservice_stop(chan);
	return 0;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	my_umask = umask(0);
	umask(my_umask);
	return ast_register_application_xml(app, mail_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Sends an email");
