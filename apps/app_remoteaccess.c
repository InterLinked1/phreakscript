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
 * \brief Remote Access
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<use type="module">app_saytelnumber</use>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/say.h"

/*** DOCUMENTATION
	<application name="RemoteAccess" language="en_US">
		<synopsis>
			Remote Access
		</synopsis>
		<syntax>
			<parameter name="vmcontext">
				<para>Voicemail context containing users.</para>
				<para>PINs for Remote Access will be the voicemail passwords.</para>
			</parameter>
			<parameter name="directory" required="true">
				<para>Directory containing the prompt set to be used.</para>
				<para>Do NOT terminate with a trailing slash.</para>
			</parameter>
			<parameter name="dbkey" required="true">
				<para>Name of AstDB family/key to use.</para>
				<para>Use ^{RAF_NUMBER} in place of the translated Remote Access number.</para>
			</parameter>
			<parameter name="sub_available">
				<para>Gosub context to check if Remote Access feature is available.</para>
				<para>The <literal>EXTEN</literal> will be the Remote Access number entered. The voicemail context provided to the application will be provided as <literal>ARG1</literal>.</para>
				<para>The subroutine should return the number to use for DB queries if allowed and empty otherwise.
				For example, this might return a full 10-digit number instead of an extension.</para>
				<para>If this argument is not provided, it is assumed the feature is available for any valid extension, and that the number is the extension entered.</para>
			</parameter>
			<parameter name="sub_valid">
				<para>Gosub context to check if a number can be used with Remote Access Forwarding.</para>
				<para><literal>ARG1</literal> will be the translated Remote Access number.</para>
				<para>The subroutine should return 1 if valid and 0 otherwise.</para>
				<para>If not provided, it is assumed that any number may be used.</para>
			</parameter>
			<parameter name="saytelnum_dir">
				<para>Directory for SayTelephoneNumber</para>
			</parameter>
			<parameter name="saytelnum_args">
				<para>Arguments to SayTelephoneNumber</para>
			</parameter>
			<parameter name="maxnumlen">
				<para>Maximum number length for extensions in this voicemail context.</para>
			</parameter>
			<parameter name="options" required="no">
				<optionlist>
					<option name="f">
						<para>Do not provide built-in Remote Access Forwarding</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>This is an implementation of Remote Access, as used with AT&amp;T/5ESS.</para>
			<para>The audio prompts referenced in this module are required for this module to work.</para>
			<para>Available features should be defined as extensions (by feature code) in the same extension.
			A special priority of 1000 is used to Return() the a filename announcing the name of the feature.</para>
			<para>These extensions will be invoked using Gosub. Therefore, execution should terminate with a Return.</para>
			<para>If the value 0 is returned, this will be handled as an error condition and Remote Access will disconnect.
			Otherwise, the user is allowed to select another feature.</para>
			<variablelist>
				<variable name="RAF_NUMBER">
					<para>The remote access forwarding number, if successfully authenticated.</para>
					<para>Note this is the number as translated, not entered originally.</para>
				</variable>
			</variablelist>
		</description>
		<see-also>
			<ref type="application">SelectiveFeature</ref>
			<ref type="application">SayTelephoneNumber</ref>
		</see-also>
	</application>
 ***/

static char *app = "RemoteAccess";

#define MAX_ATTEMPTS 3

struct raf_opts {
	char *directory; /* Directory containing prompts */
	char *vmcontext;
	char *dbkey;
	char *subavailable;
	char *subvalid;
	char *saytelnum_dir;
	char *saytelnum_args;
	unsigned long number;
	int maxnumlen;
	unsigned int builtinforward:1;
};

static int raf_play(struct ast_channel *chan, struct raf_opts *rafopts, const char *file)
{
	char prompt[PATH_MAX];
	snprintf(prompt, sizeof(prompt), "%s/%s", rafopts->directory, file);
	return ast_stream_and_wait(chan, prompt, AST_DIGIT_ANY);
}

/*! \brief Say a telephone number (not a number!) */
static int say_telephone_number(struct ast_channel *chan, struct raf_opts *rafopts, char *number)
{
	const char *digit;
	char args[256];
	int res;
	if (ast_strlen_zero(number)) {
		ast_debug(1, "No telephone number specified?\n");
		return 0;
	}
	if (ast_strlen_zero(rafopts->saytelnum_dir) || ast_strlen_zero(rafopts->saytelnum_args)) {
		ast_debug(1, "Using SayDigits instead of SayTelephoneNumber\n");
		/* allow any digit to interrupt */
		res = ast_say_digit_str(chan, number, "", ast_channel_language(chan));
		if (res > 0) {
			return res;
		}
		return res;
	}

	/* these strings are known to be non-NULL at this point */
	/* Add the option to allow DTMF interrupt. For that, we need to know whether %s ends with options */
	if (strchr(rafopts->saytelnum_args, ',')) {
		snprintf(args, sizeof(args), "%s,%s,%si", rafopts->saytelnum_dir, number, rafopts->saytelnum_args);
	} else {
		snprintf(args, sizeof(args), "%s,%s,%s,i", rafopts->saytelnum_dir, number, rafopts->saytelnum_args);
	}

	pbx_builtin_setvar_helper(chan, "SAYTELNUM_DIGIT", NULL);
	ast_debug(1, "Calling SayTelephoneNumber with args: %s\n", args);
	res = ast_pbx_exec_application(chan, "SayTelephoneNumber", args);

	if (res < 0) {
		return res; /* user hung up */
	}

	/* see if we got interrupted. If SayTelephoneNumber were in this same module, or a core API, this wouldn't be necessary. */
	ast_channel_lock(chan);
	digit = pbx_builtin_getvar_helper(chan, "SAYTELNUM_DIGIT");

	if (!ast_strlen_zero(digit)) {
		res = *digit;
		ast_channel_unlock(chan);
		return res;
	}

	ast_channel_unlock(chan);

	return 0;
}

static long raf_available(struct ast_channel *chan, struct raf_opts *rafopts, const char *num)
{
	char location[256];
	const char *retval;
	long number = 0;
	int res;

	if (ast_strlen_zero(rafopts->subavailable)) {
		return atol(num); /* No way to check, so assume yes, it's available. */
	}
	snprintf(location, sizeof(location), "%s,%s,%d(%s)", rafopts->subavailable, num, 1, rafopts->vmcontext);
	ast_debug(3, "Executing: Gosub(%s)\n", location);
	res = ast_app_exec_sub(NULL, chan, location, 0);
	if (res < 0) {
		return -1;
	}

	ast_channel_lock(chan);
	retval = pbx_builtin_getvar_helper(chan, "GOSUB_RETVAL");
	if (!ast_strlen_zero(retval)) {
		number = atol(retval);
	}
	ast_channel_unlock(chan);

	return number;
}

static int raf_getnum(struct ast_channel *chan, struct raf_opts *rafopts, char *buf, size_t len)
{
	int res, attempts = 0;
	char prompt[PATH_MAX];
	char pin[32];
	char realpin[32];
	char translated[AST_MAX_EXTENSION];
	char *actual_pin = realpin;
	long number;

	for (; attempts < MAX_ATTEMPTS; attempts++) {
		char optbuf[3];
		for (; attempts < MAX_ATTEMPTS; attempts++) {
			ast_debug(3, "Attempt to read RAF number: %d/%d\n", attempts + 1, MAX_ATTEMPTS);
			/* ast_app_getdata doesn't stop at any particular number of digits, so hack the buffer length for that */
			/* In reality, AT&T's prompt always has a timeout, it doesn't wait for any particular number of digits. */
			snprintf(prompt, sizeof(prompt), "%s/%s", rafopts->directory, "remote-welcome");
			res = ast_app_getdata(chan, prompt, buf, MIN(len, rafopts->maxnumlen), 0); /* Use standard timeout */
			if (res < 0) {
				return -1;
			}
			if (!res) {
				break;
			}
		}

		pin[0] = '\0';

		/* XXX This prompt will not stop if DTMF is received. In reality, most users won't notice this as it's short enough. */
		res = raf_play(chan, rafopts, "the-num-you-have-dialed-is");

		/* Read back number, allowing user to interrupt announcement. */
		if (!res) {
			res = say_telephone_number(chan, rafopts, buf);
			if (res > 0) {
				pin[0] = res;
			}
		} else if (res > 0) {
			pin[0] = res;
		}
		if (res < 0) {
			return -1;
		}
		if (!pin[0]) {
			/* If number was not interrupted... */
			snprintf(prompt, sizeof(prompt), "%s/%s", rafopts->directory, "if-num-correct-dial-pin");
			res = ast_app_getdata(chan, prompt, pin, sizeof(pin), 0); /* Use standard timeout */
			if (res < 0) {
				return -1;
			}
		} else {
			/* We already read one digit. */
			res = ast_app_getdata(chan, "", pin + 1, sizeof(pin) - 1, 0); /* Use standard timeout */
			if (res < 0) {
				return -1;
			}
		}
		if (res < 0) {
			return -1;
		}
		/* If number is correct, user dial PINs.
		 * If not, user dials 0. */
		if (*buf == '0') {
			ast_debug(1, "User indicated number is incorrect, reading again\n");
			continue;
		}
		if (!*buf) {
			ast_debug(1, "User did not enter a PIN, reading again\n");
			continue;
		}
		/* We got a PIN, check if it's correct.
		 * Also, check if the number is legitimate, we don't do that until this point. */
		snprintf(prompt, sizeof(prompt), "VM_INFO(%s@%s,exists)", buf, S_OR(rafopts->vmcontext, "default"));
		ast_func_read(chan, prompt, optbuf, sizeof(optbuf));
		if (strcmp(optbuf, "1")) {
			ast_verb(4, "Number '%s' does not exist\n", buf);
			continue;
		}
		/* Number exists. Check the PIN. */
		snprintf(prompt, sizeof(prompt), "VM_INFO(%s@%s,password)", buf, S_OR(rafopts->vmcontext, "default"));
		ast_func_read(chan, prompt, realpin, sizeof(realpin));
		actual_pin = realpin;
		if (*actual_pin == '-') {
			actual_pin++; /* If PIN is not user changeable, skip the first character so we have the real PIN. */
		}
		if (!*actual_pin || strcmp(pin, actual_pin)) {
			ast_verb(4, "Incorrect PIN for number '%s'\n", buf);
			continue;
		}

		/* PIN is correct. Finally, confirm that the number is allowed to use RAF, using a user level callback. */
		number = raf_available(chan, rafopts, buf);
		if (number < 0) {
			return -1;
		} else if (!number) {
			ast_verb(4, "Remote Access not available for '%s'\n", buf);
			continue;
		}
		rafopts->number = (unsigned long) number;

		ast_verb(4, "User successfully accessed Remote Access for '%lu'\n", rafopts->number);
		snprintf(translated, sizeof(translated), "%lu", rafopts->number);
		/* Variable provided so user subroutines can use it. */
		pbx_builtin_setvar_helper(chan, "RAF_NUMBER", translated); /* This is the translated number. */
		break;
	}

	if (attempts >= MAX_ATTEMPTS) {
		ast_verb(4, "Caller %s exceeded max attempts to obtain Remote Access Forwarding number\n",
			ast_channel_caller(chan)->id.number.str);
		raf_play(chan, rafopts, "ccad-hangup");
		return 1; /* Disconnect */
	}

	return 0;
}

static int raf_featcode(struct ast_channel *chan, struct raf_opts *rafopts, int skipintro, char *featcode, size_t len)
{
	char buf[3];
	char extendata[PATH_MAX];
	char introprompt[PATH_MAX] = "";
	char prompt[PATH_MAX];
	int res, attempts;
	int code = 0;

start:
	if (!skipintro) {
		snprintf(introprompt, sizeof(introprompt), "%s/%s", rafopts->directory, "please-dial-a-feature-code-now");
	}

	for (attempts = 0; attempts < MAX_ATTEMPTS; attempts++) {
		ast_debug(3, "Attempt to read feature code: %d/%d\n", attempts + 1, MAX_ATTEMPTS);

		/* Read a 2-digit feature code. Again, this is an optimization we have made, to only accept 2 digits. */
		res = ast_app_getdata(chan, introprompt, buf, 2, 0); /* Use standard timeout */
		if (res < 0) {
			return -1;
		}
		if (res) {
			continue;
		}

		/* Past this point, if we loop again, we should NOT repeat the intro prompt. */
		introprompt[0] = '\0';

		code = atoi(buf);
		/* Check if feature code is valid. */
		if (rafopts->builtinforward && (code == 72 || code == 73)) {
			ast_debug(3, "Skipping check for built in\n");
		} else if (code <= 0 || ast_get_extension_data(extendata, sizeof(extendata), chan, ast_channel_context(chan), buf, 1000)) {
			/* Extension does not exist in current context. */
			res = raf_play(chan, rafopts, "sorry-num-dialed");
			if (!res) {
				res = say_telephone_number(chan, rafopts, buf);
			}
			if (!res) {
				res = raf_play(chan, rafopts, "invalid-feature-code");
			}
			if (res < 0) {
				return -1;
			}
			continue;
		}
		break;
	}

	ast_assert(code);

	for (attempts = 0; attempts < MAX_ATTEMPTS; attempts++) {
		/* Okay, definitely got a valid remote feature code. Confirm with user. */
		res = raf_play(chan, rafopts, "you-have-accessed-the");
		if (!res) {
			if (rafopts->builtinforward && (code == 72 || code == 73)) {
				res = raf_play(chan, rafopts, code == 72 ? "call-forwarding-activation" : "call-forwarding-deactivation");
			} else {
				res = ast_stream_and_wait(chan, extendata, AST_DIGIT_ANY);
			}
		}
		if (!res) {
			res = raf_play(chan, rafopts, "feature");
		}
		if (!res) {
			snprintf(prompt, sizeof(prompt), "%s/%s", rafopts->directory, "to-confirm-feature-dial-1");
			res = ast_stream_and_wait(chan, prompt, AST_DIGIT_ANY);
		}
		if (res < 0) {
			return -1;
		}
		if (res > 0) {
			ast_debug(3, "Got response '%c'\n", res);
		}
		if (res == '1') {
			ast_verb(4, "User confirmed remote feature '%s'\n", buf);
			ast_copy_string(featcode, buf, len);
			return 0;
		} else if (res == '0') {
			goto start; /* Not really needed, but a double loop feels overbearing here. */
		}
		ast_debug(2, "Didn't get confirmation\n");
	}

	ast_verb(4, "Caller %s exceeded max attempts to dial a remote feature code\n",
		ast_channel_caller(chan)->id.number.str);
	raf_play(chan, rafopts, "sorry-temporary-service-problems");
	return 1; /* Disconnect */
}

static long raf_get_current(struct ast_channel *chan, const char *keyname, char *numbuf, size_t len)
{
	char buf[260] = "";
	char substituted[270];
	char result[3];

	snprintf(buf, sizeof(buf), "DB(%s)", keyname);
	pbx_substitute_variables_helper(chan, buf, substituted, sizeof(substituted));
	if (ast_func_read(chan, substituted, result, sizeof(result))) {
		return -1;
	}
	if (ast_strlen_zero(result)) {
		return -1;
	}
	ast_copy_string(numbuf, result, len);
	ast_debug(3, "Current RAF value: %s\n", result);
	return 0;
}

static int raf_set(struct ast_channel *chan, const char *keyname, const char *value)
{
	char buf[270] = "";
	char substituted[270];

	snprintf(buf, sizeof(buf), "%s(%s)", value ? "DB" : "DB_DELETE", keyname);
	pbx_substitute_variables_helper(chan, buf, substituted, sizeof(substituted));
	ast_debug(3, "Function execution: %s = %s\n", substituted, S_OR(value, ""));
	return ast_func_write(chan, substituted, S_OR(value, ""));
}

static int raf_build_dbkey(struct raf_opts *rafopts, char *buf, size_t len)
{
	char *s = buf;

	*buf = '\0';
	ast_copy_string(buf, rafopts->dbkey, len);

	while (*s) {
		if (*s == '^') {
			*s = '$';
		}
		s++;
	}

	return 0;
}

static int raf_raf(struct ast_channel *chan, struct raf_opts *rafopts, int forward)
{
	int res, existing, attempts = 0;
	char dbkey[256];
	char raf_target[AST_MAX_EXTENSION];
	char prompt[PATH_MAX];

	ast_debug(3, "Using built-in RAF routine\n");

	raf_build_dbkey(rafopts, dbkey, sizeof(dbkey));
	existing = raf_get_current(chan, dbkey, raf_target, sizeof(raf_target)) ? 0 : 1;

	ast_debug(3, "RAF is currently %s\n", existing ? "enabled" : "disabled");

	res = raf_play(chan, rafopts, "this-is-call-forwarding-service");
	if (res < 0) {
		return -1;
	}

	if (forward) {
		if (existing) {
			/* Trying to forward calls, but forwarding already enabled */
			res = raf_play(chan, rafopts, "your-calls-currently-forwarded-to");
			if (!res) {
				res = say_telephone_number(chan, rafopts, raf_target);
			}
			if (!res) {
				res = raf_play(chan, rafopts, "you-may-not-forward-another-until-deactivated");
			}
			return 0;
		} else {
			/* Get forwarding number */
			char newfwd[AST_MAX_EXTENSION];
			char location[256];
			const char *retval;
			int valid;

			snprintf(prompt, sizeof(prompt), "%s/%s", rafopts->directory, "please-dial-num-to-which-to-fwd");
			do {
				attempts++;
				newfwd[0] = '\0';
				res = ast_app_getdata(chan, prompt, newfwd, sizeof(newfwd), 0); /* Use standard timeout */
				if (res < 0) {
					return -1;
				} else if (ast_strlen_zero(newfwd)) { /* Timeout could be acceptable, so don't use res */
					ast_debug(3, "No input, prompting for number again\n");
					snprintf(prompt, sizeof(prompt), "%s/%s", rafopts->directory, "please-dial-num-to-which-to-fwd");
					continue;
				}
				/* Got a number. Is it any good? */
				ast_debug(2, "User entered '%s'\n", newfwd);
				if (!rafopts->subvalid) {
					ast_debug(3, "No validator subroutine available, assuming number is valid\n");
					break; /* No way to test, assume it is valid. */
				}
				snprintf(location, sizeof(location), "%s,%s,%d(%lu)", rafopts->subvalid, newfwd, 1, rafopts->number);
				/* Execute remote feature callback. */
				res = ast_app_exec_sub(NULL, chan, location, 0);
				if (res < 0) {
					return -1;
				}

				ast_channel_lock(chan);
				retval = pbx_builtin_getvar_helper(chan, "GOSUB_RETVAL");
				valid = !ast_strlen_zero(retval) && !strcmp(retval, "1");
				ast_channel_unlock(chan);

				if (!valid) {
					res = raf_play(chan, rafopts, "sorry-digits-dialed");
					if (!res) {
						res = say_telephone_number(chan, rafopts, newfwd);
					}
					if (res < 0) {
						return -1;
					} else if (res) {
						/* XXX This is not elegant right now as we'll only be able to read digits starting with the next prompt.
						 * Here we got input but don't process it. This should really be fixed.
						 * It's not super elegant to, because you need to pass in buf + 1 to ast_app_getdata and set buf[0] here.
						 */
					}
					snprintf(prompt, sizeof(prompt), "%s/%s", rafopts->directory, "are-not-a-valid-telephone-number");
					continue;
				}
				break;
			} while (attempts <= MAX_ATTEMPTS);

			/* Number is okay. Ask for confirmation. */
			do {
				attempts++;
				/* Got a valid number. */
				res = say_telephone_number(chan, rafopts, newfwd);
				if (!res) {
					res = raf_play(chan, rafopts, "if-num-correct-dial-1");
				}
				if (res < 0) {
					return -1;
				} else if (res == '1') {
					/* Confirmed! */
					if (raf_set(chan, dbkey, newfwd)) {
						ast_log(LOG_WARNING, "Failed to set DB(%s) = %s\n", dbkey, newfwd);
						continue;
					}
					ast_verb(4, "Remote Access Forwarding enabled for %lu => %s\n", rafopts->number, newfwd);
					raf_play(chan, rafopts, "calls-now-forwarded");
					return 0;
				}
			} while (attempts <= MAX_ATTEMPTS);

			res = raf_play(chan, rafopts, "call-forwarding-too-many-errors");
			if (res >= 0) {
				ast_safe_sleep(chan, 5000);
			}
			return -1; /* Disconnect */
		}
	} else { /* else Disable forwarding. */
		if (!existing) {
			/* No calls currently being forwarded. */
			res = raf_play(chan, rafopts, "your-calls-not-currently-fwd");
		} else {
			/* Disable existing forwarding. */
			raf_set(chan, dbkey, NULL);
			ast_verb(3, "Remote Access Forwarding disabled for %lu\n", rafopts->number);
			res = raf_play(chan, rafopts, "your-calls-no-longer-be-fwded");
		}
		return res < 0 ? -1 : 0;
	}
}

static int raf_exec(struct ast_channel *chan, const char *data)
{
	char *tmp;
	int res;
	char number[AST_MAX_EXTENSION];
	char featcode[3];
	struct raf_opts rafopts_main = {
		.vmcontext = NULL,
		.directory = "custom",
		.dbkey = NULL,
		.subavailable = NULL,
		.saytelnum_dir = NULL,
		.saytelnum_args = NULL,
		.maxnumlen = 10,
		.builtinforward = 1,
	};
	int iterations = -1;
	struct raf_opts *rafopts = &rafopts_main;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(vmcontext);
		AST_APP_ARG(directory);
		AST_APP_ARG(dbkey);
		AST_APP_ARG(subavailable);
		AST_APP_ARG(subvalid);
		AST_APP_ARG(saytelnum_dir);
		AST_APP_ARG(saytelnum_args);
		AST_APP_ARG(maxnumlen);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "%s requires arguments\n", app);
		return -1;
	}

	tmp = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, tmp);

	if (!ast_strlen_zero(args.vmcontext)) {
		rafopts_main.vmcontext = args.vmcontext;
	}
	if (!ast_strlen_zero(args.directory)) {
		rafopts_main.directory = args.directory;
	}
	if (!ast_strlen_zero(args.dbkey)) {
		rafopts_main.dbkey = args.dbkey;
	} else {
		ast_log(LOG_WARNING, "DB family/key required\n");
		return -1;
	}
	if (!ast_strlen_zero(args.subavailable)) {
		rafopts_main.subavailable = args.subavailable;
	}
	if (!ast_strlen_zero(args.subvalid)) {
		rafopts_main.subvalid = args.subvalid;
	}
	if (!ast_strlen_zero(args.saytelnum_dir)) {
		rafopts_main.saytelnum_dir = args.saytelnum_dir;
	}
	if (!ast_strlen_zero(args.saytelnum_args)) {
		rafopts_main.saytelnum_args = args.saytelnum_args;
	}
	if (!ast_strlen_zero(args.maxnumlen)) {
		rafopts_main.maxnumlen = atoi(args.maxnumlen);
		if (rafopts_main.maxnumlen < 1) {
			ast_log(LOG_WARNING, "Invalid max number length: %s\n", args.maxnumlen);
			return -1;
		}
	}
	if (!ast_strlen_zero(args.options)) {
		rafopts_main.builtinforward = strchr(args.options, 'f') ? 0 : 1;
	}

	if (ast_safe_sleep(chan, 250)) {
		return -1;
	}
	ast_answer(chan); /* Answer the channel, because this is how it actually works. This is not a free call. */
	if (ast_safe_sleep(chan, 250)) { /* Wait 250 ms after answer. */
		return -1;
	}

	/* Get the remote number to control */
	res = raf_getnum(chan, rafopts, number, sizeof(number));
	if (res) {
		return -1;
	}

	/* User has authenticated. */
	for (;;) {
		int do_hangup = 0;

		iterations++; /* So that it's now 0 for the first iteration. */
		res = raf_featcode(chan, rafopts, iterations > 0 ? 1 : 0, featcode, sizeof(featcode));
		if (res) {
			break;
		}

		if (rafopts->builtinforward && (!strcmp(featcode, "72") || !strcmp(featcode, "73"))) {
			/* Built in Remote Access Forwarding */
			res = raf_raf(chan, rafopts, !strcmp(featcode, "72") ? 1 : 0);
		} else {
			char location[256];
			snprintf(location, sizeof(location), "%s,%s,%d", ast_channel_context(chan), featcode, 1);
			/* Execute remote feature callback. */
			/* XXX For some reason ast_pbx_exec_application doesn't seem to work (just returns immediately).
			 * Problem with ast_app_exec_sub is it throws a warning on hangup during subroutine. Can be ignored but not elegant.
			 */
			ast_debug(3, "Executing: %s(%s)\n", "Gosub", location);
			res = ast_app_exec_sub(NULL, chan, location, 0);
			ast_debug(3, "Gosub returned %d\n", res);
			if (!res && ast_check_hangup(chan)) {
				res = -1; /* User hung up during the Gosub */
				/* XXX There will also be a WARNING about abnormal Gosub exit... this can be ignored */
			}
			if (!res) {
				const char *retval;
				ast_channel_lock(chan);
				retval = pbx_builtin_getvar_helper(chan, "GOSUB_RETVAL");
				if (!ast_strlen_zero(retval) && !strcmp(retval, "0")) {
					do_hangup = 1;
				}
				ast_channel_unlock(chan);
				if (do_hangup) {
					res = -1;
				}
			}
		}

		/* As long as the user behaves, allow him or her to keep changing features. */
		if (res) {
			break;
		}
		raf_play(chan, rafopts, "you-may-hangup-or-another-feature");
	}

	return res;
}

static int unload_module(void)
{
	int res;

	res = ast_unregister_application(app);

	return res;
}

static int load_module(void)
{
	return ast_register_application_xml(app, raf_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Remote Access");
