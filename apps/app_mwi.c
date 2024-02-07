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
 * \brief Message Waiting Indicator Control
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/mwi.h"
#include "asterisk/cli.h"

/*** DOCUMENTATION
	<application name="SetMWI" language="en_US">
		<synopsis>
			Message Waiting Indicator management
		</synopsis>
		<syntax>
			<parameter name="target" required="false" argsep="&amp;">
				<para>A list of ampersand-separated mailboxes or devices.</para>
				<para>When mailboxes are specified, an MWI notification will be sent
				to all devices subscribed to that mailbox.</para>
				<para>Alternately, if a device is specified, an MWI notification will be sent
				specifically to that endpoint. This is only supported for PJSIP/SIP endpoints.
				This can be useful for devices that may not have mailboxes.
				Note that if you do this, your PJSIP/SIP notify config will
				need to have clear-mwi and activate-mwi sections configured.</para>
			</parameter>
			<parameter name="disposition" required="false">
				<para>If specified, truthy (1) to enable MWI and falsy (0) to disable.</para>
				<para>If not specified, MWI will be set high if there are new messages in
				any of the provided mailboxes and 0 otherwise. This is only supported
				for mailboxes, not devices.</para>
			</parameter>
		</syntax>
		<description>
			<para>This application may be used to manually control Message Waiting Indicators.</para>
			<para>This can be used for MWI troubleshooting, such as remedying MWI that has not
			properly cleared, or for manually setting MWI as part of other dialplan functionality.</para>
		</description>
	</application>
 ***/

static char *app = "SetMWI";

/*! \note XXX Copied from app_voicemail.c */

/*!
 * \internal
 * \brief Parse the given mailbox_id into mailbox and context.
 * \since 12.0.0
 *
 * \param mailbox_id The mailbox\@context string to separate.
 * \param mailbox Where the mailbox part will start.
 * \param context Where the context part will start.  ("default" if not present)
 *
 * \retval 0 on success.
 * \retval -1 on error.
 */
static int separate_mailbox(char *mailbox_id, char **mailbox, char **context)
{
	if (ast_strlen_zero(mailbox_id) || !mailbox || !context) {
		return -1;
	}
	*context = mailbox_id;
	*mailbox = strsep(context, "@");
	if (ast_strlen_zero(*mailbox)) {
		return -1;
	}
	if (ast_strlen_zero(*context)) {
		*context = "default";
	}
	return 0;
}

static int dangerous_str(const char *s)
{
	while (*s) {
		/* These are all the characters we expect to see in a device name. */
		if (!isalnum(*s) && *s != '_' && *s != '-' && *s != '/') {
			return 1;
		}
		s++;
	}
	return 0;
}

static int mwi_exec(struct ast_channel *chan, const char *data)
{
	char *tmp;
	int disposition = -1;
	char *target, *targets;
	int pipefd[2] = { -1, -1 };
	char tmpbuf[256];
	char cmd[256];
	int res = -1;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(targets);
		AST_APP_ARG(disposition);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "%s requires arguments\n", app);
		return -1;
	}

	tmp = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, tmp);

	if (ast_strlen_zero(args.targets)) {
		ast_log(LOG_WARNING, "%s requires target(s)\n", app);
		return -1;
	} else if (!ast_strlen_zero(args.disposition)) {
		disposition = ast_true(args.disposition) ? 1 : 0;
	}

	if (disposition < 0) {
		int checked = 0;
		/* We need to determine what status to use.
		 * Check if any provided mailboxes have new messages. */
		disposition = 0;
		targets = ast_strdupa(args.targets);
		while ((target = strsep(&targets, "&"))) {
			if (strchr(target, '/')) {
				/* If it has a / in it, it's a device. Otherwise, assume it's a mailbox.
				 * Note that because the [@context] part is optional, we can't positively
				 * rely on an @ being present for mailboxes. */
				continue;
			}
			checked++;
			disposition += ast_app_has_voicemail(target, NULL);
			/* Don't break on first match, check all so that way we get an actual accurate count. */
		}
		if (!checked) {
			ast_log(LOG_WARNING, "Could not determine what MWI status to use (no mailboxes available)\n");
			return -1;
		}
	}

	/* Send MWI */
	targets = ast_strdupa(args.targets);
	while ((target = strsep(&targets, "&"))) {
		if (strchr(target, '/')) {
			char *device = target;
			/* Device */
			/* This is going to be difficult to do in general, since each channel driver
			 * handles MWI internally and this is not exposed through the core, except
			 * through voicemail stuff. */

			/* This is so hacky, I don't even want to think about it...
			 * Internally execute the CLI command to send a NOTIFY.
			 * We'll need a file descriptor to run CLI commands, so make one.
			 * Reuse the pipe for all devices. */
			if (pipefd[0] == -1 && pipe(pipefd)) {
				ast_log(LOG_WARNING, "pipe failed: %s\n", strerror(errno));
				continue;
			}

			/* Ick, command injection... make sure the device name doesn't contain any nasty characters that could get us in trouble... */
			if (dangerous_str(device)) {
				ast_log(LOG_WARNING, "Device name '%s' deemed malicious, skipping\n", device);
				continue;
			}

			if (!strncmp(target, "PJSIP/", 6)) {
				device += 6;
				snprintf(cmd, sizeof(cmd), "pjsip send notify %s endpoint %s", disposition ? "activate-mwi" : "clear-mwi", device);
				ast_cli_command_full(0, 0, pipefd[1], cmd);
			} else if (!strncmp(target, "SIP/", 4)) {
				device += 4;
				snprintf(cmd, sizeof(cmd), "sip notify %s %s", disposition ? "activate-mwi" : "clear-mwi", device);
				ast_cli_command_full(0, 0, pipefd[1], cmd);
			} else {
				ast_log(LOG_WARNING, "Only PJSIP and SIP endpoints are supported if you explicitly specify a device\n");
				continue;
			}
			res = read(pipefd[0], tmpbuf, sizeof(tmpbuf) - 1);
			if (res <= 0) {
				ast_log(LOG_WARNING, "read failed: %s\n", strerror(errno));
				continue;
			}
			tmpbuf[res] = '\0'; /* Guaranteed in bounds */
			if (strstr(tmpbuf, "Unable to find notify type")) {
				/* clear-mwi is present in the Asterisk sample configs, but activate-mwi is not,
				 * so if the user didn't add one to the config, that would fail. */
				ast_log(LOG_WARNING, "Notify type %s not present in %s\n", disposition ? "activate-mwi" : "clear-mwi",
					!strncmp(target, "PJSIP/", 6) ? "pjsip_notify.conf" : "sip_notify.conf");
			} else {
				res = 0;
				ast_verb(4, "Set MWI %s for %s\n", disposition ? "ON" : "OFF", device);
			}
		} else {
			/* Mailbox */
			char *mailbox, *context;
			ast_copy_string(tmpbuf, target, sizeof(tmpbuf)); /* Back up the original name */
			if (separate_mailbox(target, &mailbox, &context)) {
				ast_log(LOG_WARNING, "Failed to parse mailbox: %s\n", tmpbuf);
				continue; /* Just skip this one. */
			}
			if (!ast_publish_mwi_state_channel(mailbox, context, disposition, 0, ast_channel_uniqueid(chan))) {
				res = 0;
				ast_verb(4, "Set MWI %s for %s@%s\n", disposition ? "ON" : "OFF", mailbox, context);
			} else {
				ast_log(LOG_WARNING, "Failed to set MWI for %s@%s\n", mailbox, context);
			}
		}
	}

	/* Close the pipe if we made one. */
	if (pipefd[0] > -1) {
		close(pipefd[0]);
		close(pipefd[1]);
	}

	/* As long as we successfully did *something* (even if we had a partial failure), return 0. */
	return res;
}

static int unload_module(void)
{
	return ast_unregister_application(app);
}

static int load_module(void)
{
	return ast_register_application_xml(app, mwi_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Message Waiting Indicators");
