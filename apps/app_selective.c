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
 */

/*! \file
 *
 * \brief Selective feature application
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*! \li \ref app_selective.c uses the configuration file \ref selective.conf
 * \addtogroup configuration_file Configuration Files
 */

/*!
 * \page selective.conf selective.conf
 */

/*** MODULEINFO
	<depend>app_stack</depend>
	<depend>app_db</depend>
	<depend>func_db</depend>
	<use type="module">app_saytelnumber</use>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/cli.h"
#include "asterisk/term.h"
#include "asterisk/config.h"
#include "asterisk/app.h"
#include "asterisk/say.h"
#include "asterisk/utils.h"
#include "asterisk/strings.h"
#include "asterisk/astdb.h"

/*** DOCUMENTATION
	<application name="SelectiveFeature" language="en_US">
		<synopsis>
			Interactive auto-attendant to control selective calling features
		</synopsis>
		<syntax>
			<parameter name="profile">
				<para>Name of profile in <literal>selective.conf</literal> to
				use for providing the requested selective calling feature.</para>
			</parameter>
			<parameter name="options" required="no">
				<optionlist>
					<option name="p">
						<para>Use pulse dialing announcements. Default is tone
						dialing announcements.</para>
						<para>This should be used when the caller has dialed
						11 instead of * to reach the feature, regardless of
						whether pulse or tone dialing is being used.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>This provides the interactive auto-attendant used for common
			selective calling features (e.g. Selective Call Rejection or Call Block,
			Priority Call, Selective or Preferred Call Forwarding, etc.) as implemented
			on traditional TDM switches (e.g. Lucent 5ESS).</para>
			<para>This application allows you to provide the appropriate selective
			calling feature management interface simply by configuring a feature
			profile in the configuration file.</para>
			<para>Typically, the prompts vary by region, generally by Regional Bell
			Operating Company at time of Divestiture. You will need to supply your own
			prompt set to use this application.</para>
			<para>Multiple prompts may be specified for a prompt config,
			ampersand-delimited. Many of the prompts are shared between features
			and may be specified in a template. You can then easily add new selective
			calling features by overriding only the feature-specific prompts and
			specifying the feature-specific configuration info.</para>
			<para>This application relies internally on AstDB.</para>
		</description>
	</application>
	<configInfo name="app_selective" language="en_US">
		<synopsis>Module to provide user management of selective calling features</synopsis>
		<configFile name="selective.conf">
			<configObject name="general">
				<synopsis>Options that apply globally to app_selective</synopsis>
				<configOption name="brief_wait" default="silence/1">
					<synopsis>Audio file containing a 1 second pause.</synopsis>
				</configOption>
				<configOption name="short_wait" default="silence/3">
					<synopsis>Audio file containing a 3 second pause.</synopsis>
				</configOption>
				<configOption name="main_wait" default="silence/4">
					<synopsis>Audio file containing a 4 second pause.</synopsis>
				</configOption>
				<configOption name="med_wait" default="silence/7">
					<synopsis>Audio file containing a 7 second pause.</synopsis>
				</configOption>
				<configOption name="long_wait" default="silence/7&amp;silence/8">
					<synopsis>Audio file containing a 15 second pause.</synopsis>
				</configOption>
			</configObject>
			<configObject name="profile">
				<synopsis>Defined profiles for app_selective to use.</synopsis>
				<configOption name="empty_list_allowed" default="no">
					<synopsis>Whether or not this feature may be active if the list is empty.</synopsis>
				</configOption>
				<configOption name="astdb_active">
					<synopsis>The AstDB family/key path that indicates with a 1 or 0 whether or not the feature is active.</synopsis>
					<description>
						<para>Dialplan expression that evaluates to a truthy value when this feature is active and a falsy value otherwise.</para>
						<para>May contain expressions, functions, and variables.</para>
					</description>
				</configOption>
				<configOption name="astdb_entries">
					<synopsis>The AstDB prefix used for managing entries on this list</synopsis>
					<description>
						<para>The AstDB prefix used for managing entries on this list.</para>
						<para>Each line must have its own list.</para>
						<para>May contain expressions, functions, and variables.</para>
					</description>
				</configOption>
				<configOption name="astdb_featnum">
					<synopsis>If specified, the AstDB location of a number to use when feature is invoked.</synopsis>
					<description>
						<para>The feature number is an optional special number used in conjunction with the feature, e.g. with Selective Call Forwarding, for instance, where the feature number is the forward-to number for numbers on the list.</para>
						<para>Do not specify this attribute if a feature activation number is not required for a selective feature.</para>
						<para>If this option is configured, additional prompts are required.</para>
					</description>
				</configOption>
				<configOption name="sub_numvalid">
					<synopsis>Dialplan subroutine to execute as a callback to check if a user-entered number is valid</synopsis>
					<description>
						<para>Return an empty or zero value for numbers that are not valid, and a postive number for numbers that are valid.</para>
					</description>
				</configOption>
				<configOption name="sub_numaddable">
					<synopsis>Dialplan subroutine to execute as a callback to check if a user-entered number may be added to the feature list as an entry</synopsis>
					<description>
						<para>Return an empty or zero value for numbers that should not be allowed to be added to the list, and a postive number for numbers that may be added.</para>
					</description>
				</configOption>
				<configOption name="sub_featnumcheck">
					<synopsis>Dialplan subroutine to execute as a callback to check if a user-entered number may be used as the special feature number</synopsis>
					<description>
						<para>Return an empty or zero value for numbers that should not be allowed to be used as the special feature number, and a postive number for numbers that may be used.</para>
					</description>
				</configOption>
				<configOption name="last_caller_num">
					<synopsis>Expression for CALLERID(num) of last incoming call</synopsis>
				</configOption>
				<configOption name="last_caller_pres">
					<synopsis>Expression for CALLERID(pres) of last incoming call</synopsis>
				</configOption>
				<configOption name="saytelnum_dir">
					<synopsis>Directory containing prompts for SayTelephoneNumber announcements</synopsis>
				</configOption>
				<configOption name="saytelnum_args">
					<synopsis>Arguments for SayTelephoneNumber application, following the number</synopsis>
				</configOption>
				<configOption name="prompt_on">
					<synopsis>Prompt to indicate the feature is currently or newly enabled</synopsis>
				</configOption>
				<configOption name="prompt_off">
					<synopsis>Prompt to indicate the feature is currently or newly disabled</synopsis>
				</configOption>
				<configOption name="prompt_dial_during_instr">
					<synopsis>Prompt to say "You may dial during the instructions for faster service".</synopsis>
				</configOption>
				<configOption name="prompt_to_reject_last">
					<synopsis>Prompt to say "To reject the last calling party, dial 12, and then, dial 01".</synopsis>
				</configOption>
				<configOption name="prompt_to_reject_last_dtmf">
					<synopsis>Prompt to say "To reject the last calling party, press the number sign key, dial 01, and then, press the number sign key, again".</synopsis>
				</configOption>
				<configOption name="prompt_to_turn_off">
					<synopsis>Prompt to say "To turn off this service, dial 3".</synopsis>
				</configOption>
				<configOption name="prompt_to_turn_on">
					<synopsis>Prompt to say "To turn on this service, dial 3".</synopsis>
				</configOption>
				<configOption name="prompt_add_delete">
					<synopsis>Prompt to for adding or deleting entries</synopsis>
				</configOption>
				<configOption name="prompt_instructions_1">
					<synopsis>Main instructions part 1</synopsis>
				</configOption>
				<configOption name="prompt_instructions_1_dtmf">
					<synopsis>Main instructions part 1 (DTMF version)</synopsis>
				</configOption>
				<configOption name="prompt_instructions_2_on">
					<synopsis>Main instructions part 2 (service on)</synopsis>
				</configOption>
				<configOption name="prompt_instructions_2_off">
					<synopsis>Main instructions part 2 (service off)</synopsis>
				</configOption>
				<configOption name="prompt_instructions_3">
					<synopsis>Main instructions part 3</synopsis>
				</configOption>
				<configOption name="prompt_there_is">
					<synopsis>Prompt to say "there is"</synopsis>
				</configOption>
				<configOption name="prompt_there_are">
					<synopsis>Prompt to say "there are"</synopsis>
				</configOption>
				<configOption name="prompt_no_entries">
					<synopsis>Prompt to say "There are no entries on your list"</synopsis>
				</configOption>
				<configOption name="prompt_on_your_list">
					<synopsis>Prompt to say "on your list"</synopsis>
				</configOption>
				<configOption name="prompt_one">
					<synopsis>Prompt to say "1"</synopsis>
				</configOption>
				<configOption name="prompt_ten">
					<synopsis>Prompt to say "10"</synopsis>
				</configOption>
				<configOption name="prompt_sorry_no_entries">
					<synopsis>Prompt to say "Sorry, there are no entries on your list."</synopsis>
				</configOption>
				<configOption name="prompt_no_more_private">
					<synopsis>Prompt to say "There are no more private entries on your list."</synopsis>
				</configOption>
				<configOption name="prompt_no_more">
					<synopsis>Prompt to say "There are no more entries on your list."</synopsis>
				</configOption>
				<configOption name="prompt_private_entries">
					<synopsis>Prompt to say "private entries"</synopsis>
				</configOption>
				<configOption name="prompt_one_private_entry">
					<synopsis>Prompt to say "one private entry"</synopsis>
				</configOption>
				<configOption name="prompt_one_private_entry_down">
					<synopsis>Prompt to say "one private entry", ending in a down pitch</synopsis>
				</configOption>
				<configOption name="prompt_private_entries_down">
					<synopsis>Prompt to say "private entries", ending in a down pitch</synopsis>
				</configOption>
				<configOption name="prompt_one_entry">
					<synopsis>Prompt to say "one entry"</synopsis>
				</configOption>
				<configOption name="prompt_entries_on_your_list">
					<synopsis>Prompt to say "entries on your list"</synopsis>
				</configOption>
				<configOption name="prompt_including">
					<synopsis>Prompt to say "including"</synopsis>
				</configOption>
				<configOption name="prompt_entry_required">
					<synopsis>Prompt if an entry is required to activate the feature</synopsis>
				</configOption>
				<configOption name="prompt_entry_required_dtmf">
					<synopsis>Prompt if an entry is required to activate the feature (DTMF version)</synopsis>
				</configOption>
				<configOption name="prompt_beep">
					<synopsis>Beep tone</synopsis>
				</configOption>
				<configOption name="prompt_dial_number_to_add">
					<synopsis>Prompt to dial number to be added</synopsis>
				</configOption>
				<configOption name="prompt_dial_number_to_add_dtmf">
					<synopsis>Prompt to dial number to be added (DTMF version)</synopsis>
				</configOption>
				<configOption name="prompt_dial_remove">
					<synopsis>Prompt to dial number to be removed</synopsis>
				</configOption>
				<configOption name="prompt_must_dial_number">
					<synopsis>Prompt to inform caller he or she must dial a number</synopsis>
				</configOption>
				<configOption name="prompt_start_again">
					<synopsis>Prompt to inform caller to start again</synopsis>
				</configOption>
				<configOption name="prompt_number_dialed">
					<synopsis>Prompt to say "The number you have dialed"</synopsis>
				</configOption>
				<configOption name="prompt_not_permitted">
					<synopsis>Prompt for number not permitted</synopsis>
				</configOption>
				<configOption name="prompt_too_few_digits_start_again">
					<synopsis>Prompt for "You have dialed too few digits. Please start again."</synopsis>
				</configOption>
				<configOption name="prompt_too_many_digits_start_again">
					<synopsis>Prompt for "You have dialed too many digits. Please start again."</synopsis>
				</configOption>
				<configOption name="prompt_last_caller_not_available">
					<synopsis>Prompt for last caller is not available</synopsis>
				</configOption>
				<configOption name="prompt_number_incorrect">
					<synopsis>Prompt for number dialed is incorrect</synopsis>
				</configOption>
				<configOption name="prompt_number_not_available_svc">
					<synopsis>Prompt for number dialed is not available with this service</synopsis>
				</configOption>
				<configOption name="prompt_already_on_list">
					<synopsis>Prompt for number dialed already on list</synopsis>
				</configOption>
				<configOption name="prompt_as_a_private_entry">
					<synopsis>Prompt for "as a private entry"</synopsis>
				</configOption>
				<configOption name="prompt_list_full">
					<synopsis>Prompt for list full</synopsis>
				</configOption>
				<configOption name="prompt_number_to_remove_not_on_list">
					<synopsis>Prompt for number to remove not on list</synopsis>
				</configOption>
				<configOption name="prompt_confirm_entry">
					<synopsis>Prompt to confirm entry</synopsis>
				</configOption>
				<configOption name="prompt_dial_featnum">
					<synopsis>Prompt to enter a feature number for enabling the service. Only applies when feature number required</synopsis>
				</configOption>
				<configOption name="prompt_active_featnum">
					<synopsis>Prompt to hear current feature number.</synopsis>
				</configOption>
				<configOption name="prompt_number_added_is">
					<synopsis>Prompt to hear number added</synopsis>
				</configOption>
				<configOption name="prompt_number_removed_is">
					<synopsis>Prompt to hear number removed</synopsis>
				</configOption>
				<configOption name="prompt_a_private_entry">
					<synopsis>Prompt for "a private entry"</synopsis>
				</configOption>
				<configOption name="prompt_please_continue">
					<synopsis>Prompt to continue</synopsis>
				</configOption>
				<configOption name="prompt_try_again_later">
					<synopsis>Prompt to try again later</synopsis>
				</configOption>
				<configOption name="prompt_hangup">
					<synopsis>Prompt to hangup and consult instructions</synopsis>
				</configOption>
				<configOption name="prompt_invalid_command">
					<synopsis>Prompt for invalid command</synopsis>
				</configOption>
				<configOption name="prompt_to_delete_dial_07">
					<synopsis>Prompt for "To delete an entry, dial 07 as soon as you hear it"</synopsis>
				</configOption>
				<configOption name="prompt_first_entry_is">
					<synopsis>Prompt for "The first entry on your list is"</synopsis>
				</configOption>
				<configOption name="prompt_next">
					<synopsis>Prompt for "Next..."</synopsis>
				</configOption>
				<configOption name="prompt_next">
					<synopsis>Prompt for "This is the end of your list"</synopsis>
				</configOption>
			</configObject>
		</configFile>
	</configInfo>
 ***/

enum selective_flags {
	OPT_PULSE    = (1 << 0),
};

AST_APP_OPTIONS(selective_app_options, {
	AST_APP_OPTION('p', OPT_PULSE),
});

#define CONFIG_FILE "selective.conf"

static char *app = "SelectiveFeature";

#define MAX_YN_STRING		4

/*! \brief Data structure for feature */
struct selective_proc {
	ast_mutex_t lock;
	char name[AST_MAX_CONTEXT];			/*!< Name */

	/* Internal */
	int total;							/*!< Total number of uses */
	int pending_toggle;					/*!< Not used for permanent members, only temporary copies */
	int empty_list_allowed;				/*!< Whether or not the feature can be active with no entries */

	/* General & Misc. */
	char astdb_active[PATH_MAX];		/*!< AstDB key for if feature is active */
	char astdb_entries[PATH_MAX];		/*!< AstDB prefix containing entries */
	char astdb_featnum[PATH_MAX];		/*!< AstDB key for optional special feature number */
	char sub_numvalid[AST_MAX_CONTEXT];	/*!< Subroutine to check number validity */
	char sub_numaddable[AST_MAX_CONTEXT]; /*!< Subroutine to check number addability */
	char sub_featnumcheck[AST_MAX_CONTEXT]; /*!< Subroutine to check special feature number usability */
	char last_caller_num[PATH_MAX];		/*!< Expression for last caller number */
	char last_caller_pres[PATH_MAX];	/*!< Expression for last caller pres */
	char saytelnum_dir[PATH_MAX];		/*!< Directory containing prompts */
	char saytelnum_args[PATH_MAX];		/*!< Dialplan arguments to SayTelephoneNumber() */

	/* Prompts: Status */
	char prompt_on[PATH_MAX];			/*!< Prompt for feature on */
	char prompt_off[PATH_MAX];			/*!< Prompt for feature off */

	/* What follows is, admittedly, an arbitrary hack. Some of these are singular prompts,
	 * others of these are "containers" for multiple prompts combined. These still allow
	 * the user to specify multiple prompts together for any of these, if the actual
	 * singular files are being used as opposed to being combined as presented below.
	 * This just arbitrarily corresponds to how I happened to have the audio spliced
	 * up at the time. In the future, it may be worth breaking these down into the
	 * individual singular prompts. However, the way it is here is likely to be more
	 * compatible with how different people may have spliced them up, depending on
	 * how they obtained their prompt sets.
	 */

	/* Prompts: Main Menu */
	char prompt_dial_during_instr[PATH_MAX];
	char prompt_to_reject_last[PATH_MAX];
	char prompt_to_reject_last_dtmf[PATH_MAX];
	char prompt_to_turn_off[PATH_MAX];
	char prompt_to_turn_on[PATH_MAX];
	char prompt_add_delete[PATH_MAX];
	char prompt_instructions_1[PATH_MAX];
	char prompt_instructions_1_dtmf[PATH_MAX];
	char prompt_instructions_2_on[PATH_MAX];
	char prompt_instructions_2_off[PATH_MAX];
	char prompt_instructions_3[PATH_MAX];

	/* Prompts: List Contents */
	char prompt_there_is[PATH_MAX];
	char prompt_there_are[PATH_MAX];
	char prompt_no_entries[PATH_MAX];
	char prompt_on_your_list[PATH_MAX];
	char prompt_one[PATH_MAX];
	char prompt_ten[PATH_MAX];
	char prompt_sorry_no_entries[PATH_MAX];
	char prompt_no_more_private[PATH_MAX];
	char prompt_no_more[PATH_MAX];
	char prompt_private_entries[PATH_MAX];
	char prompt_one_private_entry[PATH_MAX];
	char prompt_one_private_entry_down[PATH_MAX];
	char prompt_private_entries_down[PATH_MAX];
	char prompt_one_entry[PATH_MAX];
	char prompt_entries_on_your_list[PATH_MAX];
	char prompt_including[PATH_MAX];

	/* Prompts: Activation Caveats */
	char prompt_entry_required[PATH_MAX];
	char prompt_entry_required_dtmf[PATH_MAX];

	/* Prompts: List Management */
	char prompt_beep[PATH_MAX];
	char prompt_dial_number_to_add[PATH_MAX];
	char prompt_dial_number_to_add_dtmf[PATH_MAX];
	char prompt_dial_remove[PATH_MAX]; /*! \todo is there a DTMF version? */
	char prompt_must_dial_number[PATH_MAX];
	char prompt_start_again[PATH_MAX];
	char prompt_number_dialed[PATH_MAX];
	char prompt_not_permitted[PATH_MAX];
	char prompt_too_few_digits_start_again[PATH_MAX];
	char prompt_too_many_digits_start_again[PATH_MAX];
	char prompt_last_caller_not_available[PATH_MAX];
	char prompt_number_incorrect[PATH_MAX];
	char prompt_number_not_available_svc[PATH_MAX];
	char prompt_already_on_list[PATH_MAX];
	char prompt_as_a_private_entry[PATH_MAX];
	char prompt_list_full[PATH_MAX];
	char prompt_number_to_remove_not_on_list[PATH_MAX];
	char prompt_confirm_entry[PATH_MAX];

	/* Prompts: Feature Number */
	char prompt_dial_featnum[PATH_MAX];
	char prompt_active_featnum[PATH_MAX];

	/* Prompts: List Management Egress */
	char prompt_number_added_is[PATH_MAX];
	char prompt_number_removed_is[PATH_MAX];
	char prompt_a_private_entry[PATH_MAX];
	char prompt_please_continue[PATH_MAX];
	char prompt_try_again_later[PATH_MAX];
	char prompt_hangup[PATH_MAX];
	char prompt_invalid_command[PATH_MAX];

	/* Prompts: Hear & Manage Entries */
	char prompt_to_delete_dial_07[PATH_MAX];
	char prompt_first_entry_is[PATH_MAX];
	char prompt_next[PATH_MAX];
	char prompt_end_of_list[PATH_MAX];

	AST_LIST_ENTRY(selective_proc) entry;	/*!< Next Feature record */
};

static AST_RWLIST_HEAD_STATIC(features, selective_proc);

static char brief_wait[PATH_MAX];			/*!< Brief wait (1s) */
static char short_wait[PATH_MAX];			/*!< Short wait (3s) */
static char main_wait[PATH_MAX];			/*!< Main wait (4s) */
static char med_wait[PATH_MAX];				/*!< Medium wait (7s) */
static char long_wait[PATH_MAX];			/*!< Long wait (15s) */

/*! \brief Allocate and initialize feature processing profile */
static struct selective_proc *alloc_profile(const char *fname)
{
	struct selective_proc *f;

	if (!(f = ast_calloc(1, sizeof(*f)))) {
		return NULL;
	}

	ast_mutex_init(&f->lock);
	ast_copy_string(f->name, fname, sizeof(f->name));

	/* initialize these only on allocation, so they're not reset during a reload */
	f->total = 0;

	return f;
}

#define LOAD_STR_PARAM(field, cond) \
if (!strcasecmp(param, #field)) { \
	if (!(cond)) { \
		ast_log(LOG_WARNING, "Invalid value for %s, ignoring: '%s'\n", param, val); \
		return; \
	} \
	ast_copy_string(f->field, val, sizeof(f->field)); \
	return; \
}

#define LOAD_INT_PARAM(field, cond) \
if (!strcasecmp(param, #field)) { \
	if (!(cond)) { \
		ast_log(LOG_WARNING, "Invalid value for %s, ignoring: '%s'\n", param, val); \
		return; \
	} \
	f->field = !strcasecmp(val, "yes") ? 1 : 0; \
	return; \
}

#define LOAD_FLOAT_PARAM(field, cond) \
if (!strcasecmp(param, #field)) { \
	if (!(cond)) { \
		ast_log(LOG_WARNING, "Invalid value for %s, ignoring: '%s'\n", param, val); \
		return; \
	} \
	if (sscanf(val, "%30f", &f->field) != 1) { \
		ast_log(LOG_WARNING, "Invalid floating point value for %s, ignoring: '%s'\n", param, val); \
	} \
	return; \
}

static int contains_whitespace(const char *str)
{
	char *s = (char*) str;
	while (*s) {
		if (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') {
			return 1;
		}
		s++;
	}
	return 0;
}

/*! \brief Set parameter in profile from configuration file */
static void profile_set_param(struct selective_proc *f, const char *param, const char *val, int linenum, int failunknown)
{
	/* val[0] = simple mandatory attribute, 1 = can be empty */
	LOAD_INT_PARAM(empty_list_allowed, !strcasecmp(val, "yes") || !strcasecmp(val, "no"));
	LOAD_STR_PARAM(astdb_active, val[0]);
	LOAD_STR_PARAM(astdb_entries, val[0]);
	LOAD_STR_PARAM(astdb_featnum, 1);
	LOAD_STR_PARAM(sub_numvalid, val[0]);
	LOAD_STR_PARAM(sub_numaddable, val[0]);
	LOAD_STR_PARAM(sub_featnumcheck, val[0]);
	LOAD_STR_PARAM(last_caller_num, val[0]);
	LOAD_STR_PARAM(last_caller_pres, val[0]);
	LOAD_STR_PARAM(saytelnum_dir, val[0]);
	LOAD_STR_PARAM(saytelnum_args, val[0]);

	/* Prompts: Status */
	LOAD_STR_PARAM(prompt_on, val[0]);
	LOAD_STR_PARAM(prompt_off, val[0]);

	/* Prompts: Main Menu */
	LOAD_STR_PARAM(prompt_dial_during_instr, val[0]);
	LOAD_STR_PARAM(prompt_to_reject_last, val[0]);
	LOAD_STR_PARAM(prompt_to_reject_last_dtmf, val[0]);
	LOAD_STR_PARAM(prompt_to_turn_off, val[0]);
	LOAD_STR_PARAM(prompt_to_turn_on, val[0]);
	LOAD_STR_PARAM(prompt_add_delete, val[0]);
	LOAD_STR_PARAM(prompt_instructions_1, val[0]);
	LOAD_STR_PARAM(prompt_instructions_1_dtmf, val[0]);
	LOAD_STR_PARAM(prompt_instructions_2_on, val[0]);
	LOAD_STR_PARAM(prompt_instructions_2_off, val[0]);
	LOAD_STR_PARAM(prompt_instructions_3, val[0]);

	/* Prompts: List Contents */
	LOAD_STR_PARAM(prompt_there_is, val[0]);
	LOAD_STR_PARAM(prompt_there_are, val[0]);
	LOAD_STR_PARAM(prompt_no_entries, val[0]);
	LOAD_STR_PARAM(prompt_on_your_list, val[0]);
	LOAD_STR_PARAM(prompt_one, val[0]);
	LOAD_STR_PARAM(prompt_ten, val[0]);
	LOAD_STR_PARAM(prompt_dial_remove, val[0]);
	LOAD_STR_PARAM(prompt_sorry_no_entries, val[0]);
	LOAD_STR_PARAM(prompt_no_more_private, val[0]);
	LOAD_STR_PARAM(prompt_no_more, val[0]);
	LOAD_STR_PARAM(prompt_private_entries, val[0]);
	LOAD_STR_PARAM(prompt_one_private_entry, val[0]);
	LOAD_STR_PARAM(prompt_one_private_entry_down, val[0]);
	LOAD_STR_PARAM(prompt_private_entries_down, val[0]);
	LOAD_STR_PARAM(prompt_one_entry, val[0]);
	LOAD_STR_PARAM(prompt_entries_on_your_list, val[0]);
	LOAD_STR_PARAM(prompt_including, val[0]);

	/* Prompts: Activation Caveats */
	LOAD_STR_PARAM(prompt_entry_required, val[0]);
	LOAD_STR_PARAM(prompt_entry_required_dtmf, val[0]);

	/* Prompts: List Management */
	LOAD_STR_PARAM(prompt_beep, val[0]);
	LOAD_STR_PARAM(prompt_dial_number_to_add, val[0]);
	LOAD_STR_PARAM(prompt_dial_number_to_add_dtmf, val[0]);
	LOAD_STR_PARAM(prompt_dial_remove, val[0]);
	LOAD_STR_PARAM(prompt_must_dial_number, val[0]);
	LOAD_STR_PARAM(prompt_start_again, val[0]);
	LOAD_STR_PARAM(prompt_number_dialed, val[0]);
	LOAD_STR_PARAM(prompt_not_permitted, val[0]);
	LOAD_STR_PARAM(prompt_too_few_digits_start_again, val[0]);
	LOAD_STR_PARAM(prompt_too_many_digits_start_again, val[0]);
	LOAD_STR_PARAM(prompt_last_caller_not_available, val[0]);
	LOAD_STR_PARAM(prompt_number_incorrect, val[0]);
	LOAD_STR_PARAM(prompt_number_not_available_svc, val[0]);
	LOAD_STR_PARAM(prompt_already_on_list, val[0]);
	LOAD_STR_PARAM(prompt_as_a_private_entry, val[0]);
	LOAD_STR_PARAM(prompt_list_full, val[0]);
	LOAD_STR_PARAM(prompt_number_to_remove_not_on_list, val[0]);
	LOAD_STR_PARAM(prompt_confirm_entry, val[0]);

	/* Prompts: Feature Number */
	LOAD_STR_PARAM(prompt_dial_featnum, 1);
	LOAD_STR_PARAM(prompt_active_featnum, 1);

	/* Prompts: List Management Egress */
	LOAD_STR_PARAM(prompt_number_added_is, val[0]);
	LOAD_STR_PARAM(prompt_number_removed_is, val[0]);
	LOAD_STR_PARAM(prompt_a_private_entry, val[0]);
	LOAD_STR_PARAM(prompt_please_continue, val[0]);
	LOAD_STR_PARAM(prompt_try_again_later, val[0]);
	LOAD_STR_PARAM(prompt_hangup, val[0]);
	LOAD_STR_PARAM(prompt_invalid_command, val[0]);

	/* Prompts: Hear & Manage Entries */
	LOAD_STR_PARAM(prompt_to_delete_dial_07, val[0]);
	LOAD_STR_PARAM(prompt_first_entry_is, val[0]);
	LOAD_STR_PARAM(prompt_next, val[0]);
	LOAD_STR_PARAM(prompt_end_of_list, val[0]);

	if (0) {
		if (contains_whitespace(val))
			exit(1);
	}

	if (failunknown) {
		if (linenum >= 0) {
			ast_log(LOG_WARNING, "Unknown keyword in profile '%s': %s at line %d of %s\n", f->name, param, linenum, CONFIG_FILE);
		} else {
			ast_log(LOG_WARNING, "Unknown keyword in profile '%s': %s\n", f->name, param);
		}
	}
}

#define LOAD_GENERAL_STR(field, default) { \
	const char *var = ast_variable_retrieve(cfg, "general", #field); \
	if (!ast_strlen_zero(var)) { \
		ast_copy_string(field, var, sizeof(field)); \
	} else { \
		ast_log(LOG_NOTICE, "%s was not specified: defaulting to %s\n", #field, default); \
		ast_copy_string(field, default, sizeof(field)); \
	} \
	if (!ast_fileexists((const char*) var, NULL, NULL)) { \
		ast_log(LOG_WARNING, "%s file does not exist: %s\n", #field, var); \
	} \
}

#define LONG_WAIT 15	/* A traditional TDM switch waits for 15 seconds for a response */
#define MED_WAIT 7		/* Except, sometimes it only waits 7 seconds */
#define MAIN_WAIT 4		/* Quite typically, 4 seconds */
#define SHORT_WAIT 3	/* And often, only 3 seconds */
#define BRIEF_WAIT 1	/* Or just 1 second */

#define LONG_WAIT_FILE "silence/7&silence/8"
#define MED_WAIT_FILE "silence/7"
#define MAIN_WAIT_FILE "silence/4"
#define SHORT_WAIT_FILE "silence/3"
#define BRIEF_WAIT_FILE "silence/1"

#define ASSERT_ATTRIBUTE_EXISTS(field) { \
	if (ast_strlen_zero(f->field)) { \
		ast_log(LOG_WARNING, "Missing value for '%s' for profile '%s'. Not %s\n", #field, cat, new ? "creating" : "updating"); \
		if (new) { \
			ast_free(f); \
		} \
		continue; \
	} \
} \

/*! \brief Reload application module */
static int selective_reload(int reload)
{
	char *cat = NULL;
	struct selective_proc *f;
	struct ast_variable *var;
	struct ast_config *cfg;
	struct ast_flags config_flags = { reload ? CONFIG_FLAG_FILEUNCHANGED : 0 };

	if (!(cfg = ast_config_load(CONFIG_FILE, config_flags))) {
		ast_debug(1, "Not processing config file (%s), so no profiles loaded\n", CONFIG_FILE);
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEUNCHANGED) {
		ast_debug(1, "Not processing config file (%s), unchanged since last time\n", CONFIG_FILE);
		return 0;
	} else if (cfg == CONFIG_STATUS_FILEINVALID) {
		ast_log(LOG_ERROR, "Config file %s is in an invalid format. Aborting.\n", CONFIG_FILE);
		return 0;
	}

	AST_RWLIST_WRLOCK(&features);

	/* Reset Global Var Values */
	/* General section */
	LOAD_GENERAL_STR(brief_wait, BRIEF_WAIT_FILE);
	LOAD_GENERAL_STR(short_wait, SHORT_WAIT_FILE);
	LOAD_GENERAL_STR(main_wait, MAIN_WAIT_FILE);
	LOAD_GENERAL_STR(med_wait, MED_WAIT_FILE);
	LOAD_GENERAL_STR(long_wait, LONG_WAIT_FILE);

	/* Remaining sections */
	while ((cat = ast_category_browse(cfg, cat))) {
		int new = 0;

		if (!strcasecmp(cat, "general")) {
			continue;
		}

		/* Look for an existing one */
		AST_LIST_TRAVERSE(&features, f, entry) {
			if (!strcasecmp(f->name, cat)) {
				break;
			}
		}

		if (!f) {
			/* Make one then */
			f = alloc_profile(cat);
			new = 1;
		}

		ast_debug(1, "%s profile: '%s'\n", new ? "New" : "Updating", cat);

		/* Totally fail if we fail to find/create an entry */
		if (!f) {
			ast_log(LOG_WARNING, "Failed to create feature profile for '%s'\n", cat);
			continue;
		}

		if (!new) {
			ast_mutex_lock(&f->lock);
		}
		/* Re-initialize the defaults (none currently) */
		ast_copy_string(f->prompt_beep, "beep", sizeof(f->prompt_beep)); /* fallback to built-in sounds */

		/* Search Config */
		var = ast_variable_browse(cfg, cat);
		while (var) {
			ast_debug(2, "Logging parameter %s with value %s from lineno %d\n", var->name, var->value, var->lineno);
			profile_set_param(f, var->name, var->value, var->lineno, 1);
			var = var->next;
		} /* End while(var) loop */

		/* ensure crucial mandatory attributes exist, or refuse to create it */
		ASSERT_ATTRIBUTE_EXISTS(astdb_active);
		ASSERT_ATTRIBUTE_EXISTS(astdb_entries);

		if (!new) {
			ast_mutex_unlock(&f->lock);
		} else {
			AST_RWLIST_INSERT_HEAD(&features, f, entry);
		}
	}

	ast_config_destroy(cfg);
	AST_RWLIST_UNLOCK(&features);

	return 0;
}

static int check_condition(struct ast_channel *chan, struct ast_str *strbuf, char *condition)
{
	int res;

	ast_str_reset(strbuf);
	ast_str_substitute_variables(&strbuf, 0, chan, condition);
	ast_debug(2, "Condition to check: %s (evaluates to %s)\n", condition, ast_str_buffer(strbuf));
	res = pbx_checkcondition(ast_str_buffer(strbuf));
	ast_str_reset(strbuf);
	return res;
}

enum entry_type {
	ENTRY_ANY,
	ENTRY_PRIVATE,
	ENTRY_NORMAL,
	ENTRY_UNKNOWN,
};

#define MAIN_MENU_TERM "3*#"

/*! \brief Primary helper function to read digit input during interactive menus */
static int _selective_read(struct ast_channel *chan, char *buf, int maxdigits, char *termbefore, char *termafter, int timeout, ...)
{
	int res = 0, exitdigit = 0;
	int already = 0; /* digits already read */
	char *arg;
	char *readbuf = buf;
	char last;
	va_list ap;

	while (readbuf[0] != '\0') { /* determine how many digits we've already read */
		readbuf++; /* find where we start reading digits into the buffer, so we don't overwrite anything */
		already++;
	}

	va_start(ap, timeout);
	while ( (arg = (char*) va_arg(ap, char *)) && !ast_strlen_zero(arg)) {
		char *front, *filestmp, *files = ast_strdup(arg);
		filestmp = files; /* don't overwrite files, or we can't free it */
		/* in addition to a variable number of read prompts, support &-delimiters for multiple files at once. */
		while ((front = strsep(&filestmp, "&"))) {
			if (already >= maxdigits) {
				ast_debug(1, "Buffer is now full: size %d\n", already);
				if (already > maxdigits) {
					ast_log(LOG_WARNING, "Exceeded max digits (shouldn't happen)\n");
				}
				break;
			}
			if (!ast_fileexists(front, NULL, NULL)) {
				ast_log(LOG_WARNING, "File '%s' does not exist\n", front);
				continue;
			}
			ast_debug(1, "Waiting for input using '%s' (into digit %d)\n", front, already);
			res = ast_app_getdata_terminator(chan, front, readbuf, 1, 10, termbefore);
			ast_debug(1, "file res: %d\n", res);
			if (res != AST_GETDATA_TIMEOUT) {
				break;
			}
		}
		ast_free(files);
		if (res == AST_GETDATA_COMPLETE) {
			if (!ast_strlen_zero(termafter) && strchr(termafter, readbuf[0])) {
				exitdigit = 1;
			}
			ast_debug(1, "Just read the digit '%c'\n", readbuf[0]);
			last = readbuf[0];
			readbuf++;
			already++;
			break;
		} else if (res == -1) { /* hangup */
			ast_debug(1, "User hung up, that's a shame\n");
			break;
		}
		/* user's still there, and did nothing, go on to the next prompt */
		ast_debug(3, "Nothing of any significance whatsoever has happened\n");
	}
	va_end(ap);

	if (exitdigit) {
		/* if we should stop input after receiving certain digits, see if we got one of those just now */
		ast_debug(1, "Digit '%c' is the last we should read, no more!\n", isprint(last) ? last : '?');
		return res;
	}

	/* wait in silence for remaining digits */
	while (res != 1 && timeout > 0) {
		char *arg = short_wait;
		if (timeout == MED_WAIT) {
			arg = med_wait;
		} else if (timeout == LONG_WAIT) {
			arg = long_wait;
		}
		if (already >= maxdigits) {
			ast_debug(1, "Buffer is now full: size %d\n", already);
			break;
		}
		if (!ast_fileexists(arg, NULL, NULL)) {
			ast_log(LOG_WARNING, "File '%s' does not exist\n", arg);
			break;
		}
		ast_debug(1, "Waiting for input using '%s' (into digit %d)\n", arg, already);
		res = ast_app_getdata_terminator(chan, arg, readbuf, 1, 10, termbefore);
		if (res == AST_GETDATA_COMPLETE) {
			if (!ast_strlen_zero(termafter) && strchr(termafter, readbuf[0])) {
				exitdigit = 1;
			}
			ast_debug(1, "Just read the digit '%c'\n", readbuf[0]);
			readbuf++;
			already++;
			if (exitdigit) {
				break;
			}
		} else if (res == -1) { /* hangup */
			ast_debug(1, "User hung up, that's a shame\n");
			break;
		} else if (res == AST_GETDATA_TIMEOUT) {
			/* user's still there, and did nothing, so we're done */
			ast_debug(1, "Timed out waiting for further digits\n");
			break;
		} else if (res == AST_GETDATA_EMPTY_END_TERMINATED) {
			ast_debug(1, "User terminated input\n");
			break;
		}
	}

	return res;
}

/*! \brief Crawl AstDB and execute a callback function on each key */
static int _db_crawl(struct ast_channel *chan,
	char *parse, struct ast_str *strbuf, char *buf, struct selective_proc *f,
	int (*callback)(char *family, char *key, int res),
	int (*callback2)(int crawlindex, char *family, char *key, struct ast_channel *chan, struct ast_str *strbuf, char *buf, struct selective_proc *f))
{
	size_t parselen = strlen(parse);
	struct ast_db_entry *dbe, *orig_dbe;
	struct ast_str *escape_buf = NULL;
	const char *last = "";
	int crawlindex = 0, res = 0;

	/* Remove leading and trailing slashes */
	while (parse[0] == '/') {
		parse++;
		parselen--;
	}
	while (parse[parselen - 1] == '/') {
		parse[--parselen] = '\0';
	}

	/* Nothing within the database at that prefix? */
	if (!(orig_dbe = dbe = ast_db_gettree(parse, NULL))) {
		return 0;
	}

	for (; dbe; dbe = dbe->next) {
		/* Find the current component */
		char *curkey = &dbe->key[parselen + 1], *slash;
		if (*curkey == '/') {
			curkey++;
		}
		/* Remove everything after the current component */
		if ((slash = strchr(curkey, '/'))) {
			*slash = '\0';
		}

		/* Skip duplicates */
		if (!strcasecmp(last, curkey)) {
			continue;
		}
		last = curkey;

		/* parse = family (at this point, not the parameter when passed in) */
		if (callback) {
			res = callback(parse, curkey, res); /* execute callback function */
		} else if (callback2 && chan && strbuf && buf && f) {
			res = callback2(crawlindex++, parse, curkey, chan, strbuf, buf, f);
			if (res < 0) {
				break;
			}
		} else {
			ast_log(LOG_WARNING, "No callback function available!\n");
		}
	}
	ast_db_freetree(orig_dbe);
	ast_free(escape_buf);
	return res;
}

#define db_crawl(parse, callback) _db_crawl(NULL, parse, NULL, NULL, NULL, callback, NULL)
#define db_crawl2(chan, parse, strbuf, buf, f, callback2) _db_crawl(chan, parse, strbuf, buf, f, NULL, callback2)

#define selective_read(chan, buf, maxdigits, termafter, timeout, ...) _selective_read(chan, buf, maxdigits, "", termafter, timeout, __VA_ARGS__, NULL);

#define selective_term_read(chan, buf, maxdigits, termafter, timeout, ...) _selective_read(chan, buf, maxdigits, "#", termafter, timeout, __VA_ARGS__, NULL);

#define BUFFER_SIZE 2

/*! \brief Determine the visibility type of a list entry */
static int get_entry_type(char *entry)
{
	if (ast_strlen_zero(entry)) {
		ast_log(LOG_WARNING, "Trying to get type of empty entry value?\n");
		return ENTRY_UNKNOWN;
	}
	/* format for DB values is type - where type is 1 for normal and 2 for private */
	if (entry[0] == '1') {
		ast_debug(2, "Entry '%s' is normal\n", entry);
		return ENTRY_NORMAL;
	} else if (entry[0] == '2') {
		ast_debug(2, "Entry '%s' is private\n", entry);
		return ENTRY_PRIVATE;
	}
	ast_debug(2, "Entry '%s' is unknown type\n", entry);
	return ENTRY_UNKNOWN;
}

/*! \brief Callback function to sum up private entries on a list */
static int sum_private(char *family, char *key, int res)
{
	char buf[2];
	int len = 2;

	if (ast_db_get(family, key, buf, len)) {
		ast_log(LOG_WARNING, "db: %s/%s not found in database.\n", family, key);
		return res;
	}
	if (get_entry_type(buf) == ENTRY_PRIVATE) {
		return res + 1;
	}
	return res;
}

/*! \brief Retrieve the number of entries of a particular type on a list */
static int get_number_entries(struct ast_channel *chan, struct ast_str *strbuf, struct selective_proc *f, int flags)
{
	int entries = 0;

	if (ast_strlen_zero(f->astdb_entries)) {
		ast_log(LOG_WARNING, "DB prefix is empty!\n");
		return 0;
	}

	if (flags == ENTRY_ANY) {
		char *tmp;
		ast_str_set(&strbuf, 0, "${DB_KEYCOUNT(%s)}", f->astdb_entries);
		tmp = ast_strdupa(ast_str_buffer(strbuf));
		ast_str_substitute_variables(&strbuf, 0, chan, tmp);
		ast_debug(1, "Substituting '%s' gives us '%s'\n", tmp, ast_str_buffer(strbuf));
		entries = atoi(ast_str_buffer(strbuf));
	} else if (flags == ENTRY_PRIVATE) {
		ast_str_reset(strbuf);
		ast_str_substitute_variables(&strbuf, 0, chan, f->astdb_entries);
		ast_debug(1, "Substituting '%s' gives us '%s'\n", f->astdb_entries, ast_str_buffer(strbuf));
		entries = db_crawl(ast_strdupa(ast_str_buffer(strbuf)), sum_private);
	}
	ast_debug(1, "List has %d%s entries\n", entries, flags == ENTRY_PRIVATE ? " private" : "");
	return entries;
}

/*! \brief Say a telephone number (not a number!) */
static int say_telephone_number(struct ast_channel *chan, struct ast_str *strbuf, char *buf, struct selective_proc *f, char *number)
{
	const char *digit;
	int res;
	if (ast_strlen_zero(number)) {
		ast_debug(1, "No telephone number specified?\n");
		return 0;
	}
	if (ast_strlen_zero(f->saytelnum_dir) || ast_strlen_zero(f->saytelnum_args)) {
		ast_debug(1, "Using SayDigits instead of SayTelephoneNumber\n");
		/* allow any digit to interrupt */
		res = ast_say_digit_str(chan, number, "", ast_channel_language(chan));
		if (res > 0) {
			snprintf(buf, BUFFER_SIZE, "%c", res);
			return 0;
		}
		return res;
	}

	ast_str_reset(strbuf);
	/* these strings are known to be non-NULL at this point */
	/* Add the option to allow DTMF interrupt. For that, we need to know whether %s ends with options */
	if (strchr(f->saytelnum_args, ',')) {
		ast_str_set(&strbuf, 0, "%s,%s,%si", f->saytelnum_dir, number, f->saytelnum_args);
	} else {
		ast_str_set(&strbuf, 0, "%s,%s,%s,i", f->saytelnum_dir, number, f->saytelnum_args);
	}

	*buf = '\0';
	pbx_builtin_setvar_helper(chan, "SAYTELNUM_DIGIT", NULL);
	ast_debug(1, "Calling SayTelephoneNumber with args: %s\n", ast_str_buffer(strbuf));
	res = ast_pbx_exec_application(chan, "SayTelephoneNumber", ast_str_buffer(strbuf));

	if (res < 0) {
		return res; /* user hung up */
	}

	/* see if we got interrupted. If SayTelephoneNumber were in this same module, or a core API, this wouldn't be necessary. */
	ast_channel_lock(chan);
	digit = pbx_builtin_getvar_helper(chan, "SAYTELNUM_DIGIT");
	ast_channel_unlock(chan);

	if (!ast_strlen_zero(digit)) {
		ast_copy_string(buf, digit, BUFFER_SIZE);
		ast_debug(1, "User entered '%s'\n", buf);
	}

	return 0;
}

/*! \brief Say a number (not a telephone number!) */
static int say_number(struct ast_channel *chan, struct ast_str *strbuf, char *buf, struct selective_proc *f, int number)
{
	char numstr[10];
	snprintf(numstr, 9, "%d", number);

	if (number < 10) {
		return say_telephone_number(chan, strbuf, buf, f, numstr);
	} else if (number == 10) {
		return selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_ten);
	} else { /* greater than 10 */
		int res;
		/* Call SayNumber, and allow any digit to interrupt */
		res = ast_say_number(chan, number, AST_DIGIT_ANY, ast_channel_language(chan), ""); /* exec_app(chan, "SayNumber", numstr); */
		if (res > 0) {
			snprintf(buf, BUFFER_SIZE, "%c", res);
			return 0;
		}
		return res;
	}
}

#define RETURN_IF_INPUT(buf) if (!ast_strlen_zero(buf)) return res;

/*! \brief Announce how many entries are on a list */
static int say_number_on_list(struct ast_channel *chan, struct ast_str *strbuf, char *buf, struct selective_proc *f)
{
	int res, privates, entries = get_number_entries(chan, strbuf, f, ENTRY_ANY);

	if (!entries) {
		return selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_no_entries);
	}

	privates = get_number_entries(chan, strbuf, f, ENTRY_PRIVATE);

	if (entries == 1) {
		return selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_there_is,
			privates > 0 ? f->prompt_one_private_entry : f->prompt_one_entry, f->prompt_on_your_list);
	}

	res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_there_are);
	RETURN_IF_INPUT(buf);
	res = say_number(chan, strbuf, buf, f, entries);
	if (entries == privates) { /* all the entries are private entries */
		return selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_private_entries, f->prompt_on_your_list);
	}
	res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_entries_on_your_list);
	if (privates == 0 || !ast_strlen_zero(buf)) {
		return res;
	}
	res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_including);
	RETURN_IF_INPUT(buf);
	if (privates == 1) {
		res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_one_private_entry_down);
	} else { /* more than 1 private entry */
		res = say_number(chan, strbuf, buf, f, privates);
		RETURN_IF_INPUT(buf);
		res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_private_entries_down);
	}
	return res;
}

static int feature_status(struct ast_channel *chan, struct ast_str *strbuf, struct selective_proc *f)
{
	int active;
	if (ast_strlen_zero(f->astdb_active)) {
		ast_log(LOG_WARNING, "astdb_active is empty!\n");
		return 0;
	}
	ast_str_reset(strbuf);
	ast_str_set(&strbuf, 0, "${DB(%s)}", f->astdb_active);
	ast_str_substitute_variables(&strbuf, 0, chan, ast_strdupa(ast_str_buffer(strbuf)));
	active = check_condition(chan, strbuf, ast_strdupa(ast_str_buffer(strbuf)));
	return active;
}

static int feature_number(struct ast_channel *chan, struct ast_str *strbuf, struct selective_proc *f)
{
	if (ast_strlen_zero(f->astdb_featnum)) {
		ast_log(LOG_WARNING, "astdb_active is empty!\n"); /* should never happen */
		return -1;
	}
	ast_str_reset(strbuf);
	ast_str_set(&strbuf, 0, "${DB(%s)}", f->astdb_featnum);
	ast_str_substitute_variables(&strbuf, 0, chan, ast_strdupa(ast_str_buffer(strbuf)));
	return 0; /* retrieve result from ast_str_buffer */
}

/*! \brief Announce whether the feature is currently on or off */
static int say_status(struct ast_channel *chan, struct ast_str *strbuf, char *buf, struct selective_proc *f)
{
	int active = feature_status(chan, strbuf, f);
	ast_verb(4, "Current status of %s feature is %s", f->name, active ? "enabled" : "disabled");
	return selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 0, active ? f->prompt_on : f->prompt_off);
}

/*! \brief Callback function to delete all private entries from a list */
static int delete_private_entries(char *family, char *key, int res)
{
	char buf[2];
	int len = 2;

	if (ast_db_get(family, key, buf, len)) {
		ast_log(LOG_WARNING, "db: %s/%s not found in database.\n", family, key);
		return res;
	}
	if (get_entry_type(buf) != ENTRY_PRIVATE) {
		return res; /* not a private entry */
	}
	ast_debug(1, "Deleting private entry '%s' (%s/%s = %s)\n", key, family, key, buf);
	if (ast_db_del(family, key)) {
		ast_log(LOG_WARNING, "delete: %s/%s not deleted from database.\n", family, key);
	}
	return (res + 1);
}

/*! \brief Remove either all or all private entries from a list */
static int selective_clear(struct ast_channel *chan, struct ast_str *strbuf, char *buf, struct selective_proc *f, int flags)
{
	int privates = 0; /* Can't be used uninitialized since only used if ENTRY_PRIVATE, but whatever */
	int entries = get_number_entries(chan, strbuf, f, ENTRY_ANY);

	if (!entries) { /* list is empty, there is nothing to clear out from it... */
		ast_verb(3, "User requested clearing all%s entries from %s list, but list is empty", flags == ENTRY_PRIVATE ? " private" : "", f->name);
		return selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_sorry_no_entries);
	}

	if (flags == ENTRY_PRIVATE) { /* remove only private entries */
		privates = get_number_entries(chan, strbuf, f, ENTRY_PRIVATE);
		/* yes, we're going to crawl the DB twice, effectively.
			However, this allows us to know how many private entries there are this point.
			Yes, that info gets returned as well, however, this allows us to ensure
			we deleted all of them and emit an error if something weird happened. */
		if (privates) {
			int deleted;
			ast_str_reset(strbuf);
			ast_str_substitute_variables(&strbuf, 0, chan, f->astdb_entries);
			deleted = db_crawl(ast_strdupa(ast_str_buffer(strbuf)), delete_private_entries);
			if (deleted != privates) {
				ast_log(LOG_WARNING, "Found %d private entries, but we only deleted %d?\n", privates, deleted);
			}
		}
	} else { /* delete all list entries */
		/* yeah, this is a little sloppy. We could do the DB stuff directly, but DRY, right? */
		ast_str_reset(strbuf);
		ast_str_substitute_variables(&strbuf, 0, chan, f->astdb_entries);
		if (ast_pbx_exec_application(chan, "DBDeltree", ast_strdupa(ast_str_buffer(strbuf)))) {
			ast_log(LOG_WARNING, "Failed to remove list entries\n");
			return 0;
		}
	}

	ast_verb(3, "User removed all %d%s entries from %s list\n", flags == ENTRY_PRIVATE ? privates : entries, flags == ENTRY_PRIVATE ? " private" : "", f->name);

	return selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT,
		flags == ENTRY_PRIVATE ? f->prompt_no_more_private : f->prompt_no_more);
}

#define VALID() invalids = 0; memset(buf, 0, sizeof(buf));
#define INVALID() invalids++; memset(buf, 0, sizeof(buf));
#define PROCESS_IF_INPUT() if (!ast_strlen_zero(buf)) { ast_debug(1, "Got input '%s' while in command\n", buf); goto process; }
#define CONTINUE_IF_INPUT() if (!ast_strlen_zero(buf)) continue;

/* commands are at most 2 digits. If we get a 1-digit command, stop immediately. */
#define INPUT_DONE(res, buf) (res == AST_GETDATA_COMPLETE && (buf[1] != '\0' || (buf[0] == '3' || buf[0] == '*' || buf[0] == '#')))

/* if we have a complete prefix-free command, we're done. Otherwise, wait for another command. */
#define CHECK_INPUT() \
	if (INPUT_DONE(res, buf)) { \
		ast_debug(1, "Done receiving digits (got '%s')", buf); \
		goto process; \
	} else if (!ast_strlen_zero(buf)) { \
		res = selective_read(chan, buf, 2, MAIN_MENU_TERM, SHORT_WAIT, ""); \
		if (res < 0) { \
			ast_debug(1, "User hung up\n");  \
		} \
		goto process; \
	}

#define IS_MAIN_COMMAND(buf) (!strcmp(buf, "#") || !strcmp(buf, "*") || !strcmp(buf, "1") ||  !strcmp(buf, "11") || !strcmp(buf, "12") || !strcmp(buf, "3") || !strcmp(buf, "0") || !strcmp(buf, "08") || !strcmp(buf, "09"))

static int gosub(struct ast_channel *chan, struct ast_str *strbuf, char *buffer, int buflen, char *location, char *exten, char *args)
{
	int res;

	ast_str_reset(strbuf);
	ast_str_set(&strbuf, 0, "%s,%s,%d", location, exten, 1);

	res = ast_app_run_sub(NULL, chan, ast_str_buffer(strbuf), args, 0);
	if (!res) {
		const char *retval = pbx_builtin_getvar_helper(chan, "GOSUB_RETVAL");
		ast_copy_string(buffer, retval, buflen);
	}
	return res;
}

#define PARSE_DB(numarg) \
	ast_str_reset(strbuf); \
	ast_str_set(&strbuf, 0, "%s/%s", f->astdb_entries, numarg); \
	tmpstr = ast_strdup(ast_str_buffer(strbuf)); \
	ast_str_substitute_variables(&strbuf, 0, chan, tmpstr); \
	ast_free(tmpstr); \
	tmpstr = ast_strdup(ast_str_buffer(strbuf)); \
	AST_NONSTANDARD_APP_ARGS(args, ast_strdupa(tmpstr), '/'); \
	ast_free(tmpstr); \
	if (ast_strlen_zero(args.family) || ast_strlen_zero(args.key)) { \
		ast_debug(1, "Family or key is null or empty\n"); \
		goto nocando; \
	} \
	ast_debug(1, "Family: %s, Key: %s\n", args.family, args.key);

#define PARSE_DBKEY(dbarg) \
	ast_str_reset(strbuf); \
	ast_str_substitute_variables(&strbuf, 0, chan, dbarg); \
	tmpstr = ast_strdup(ast_str_buffer(strbuf)); \
	AST_NONSTANDARD_APP_ARGS(args, ast_strdupa(tmpstr), '/'); \
	ast_free(tmpstr); \
	if (ast_strlen_zero(args.family) || ast_strlen_zero(args.key)) { \
		ast_debug(1, "Family or key is null or empty\n"); \
	} \
	ast_debug(1, "Family: %s, Key: %s\n", args.family, args.key);

static int remove_entry(struct ast_channel *chan, struct ast_str *strbuf, char *mbuf, struct selective_proc *f, int pulse)
{
	char buf[17], result[17];
	int res;
	int buflen = 16;
	int invalids = 0;
	int mindigits = 3, maxdigits = 15;
	memset(buf, 0, sizeof(buf));

	do {
		if (ast_strlen_zero(buf)) { /* if user enters input down below, skip this on the next loop */
			/* we always prompt, even if the list is currently completely empty */
			res = selective_term_read(chan, buf, buflen, "", LONG_WAIT,  f->prompt_dial_remove, long_wait);
		}

process:
		ast_debug(1, "Buffer contains '%s'\n", buf);

		if (res == -1) { /* user hung up */
			return res;
		} else if (res == AST_GETDATA_TIMEOUT && ast_strlen_zero(buf)) { /* no input */
			INVALID();
			ast_verb(4, "User did not enter a number to remove\n");
			res = selective_term_read(chan, buf, buflen, "", LONG_WAIT, f->prompt_must_dial_number, f->prompt_start_again);
		} else if (res == AST_GETDATA_EMPTY_END_TERMINATED && ast_strlen_zero(buf)) { /* just pressed # */
			INVALID();
			ast_verb(4, "User pressed #\n");
			res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_must_dial_number);
			break;
		} else if (get_number_entries(chan, strbuf, f, ENTRY_ANY) == 0) { /* list is empty */
			/* even if the number is invalid, if there are no current entries, this trumps that */
			res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_sorry_no_entries);
			break;
		} else if (IS_MAIN_COMMAND(buf)) {
			ast_verb(4, "User entered '%s', returning to main menu\n", buf);
			ast_copy_string(mbuf, buf, BUFFER_SIZE);
			return 0; /* escape to a main command */
		} else {
			char *number, *tmpstr = NULL;
			char tmpbuf[2];
			AST_DECLARE_APP_ARGS(args,
				AST_APP_ARG(family);
				AST_APP_ARG(key);
			);
			int len = strlen(buf); /* only calculate once! */
			ast_verb(3, "User entered '%s' to remove from list\n", buf);
			number = ast_strdupa(buf);
			memset(buf, 0, sizeof(buf));

			if (len < mindigits) {
				return selective_term_read(chan, mbuf, BUFFER_SIZE, "", MED_WAIT, f->prompt_too_few_digits_start_again);
			} else if (len > maxdigits) {
				return selective_term_read(chan, mbuf, BUFFER_SIZE, "", MED_WAIT, f->prompt_too_many_digits_start_again);
			}

			/* is this a valid number to add in the first place? */
			res = gosub(chan, strbuf, result, buflen, f->sub_numvalid, buf, ""); /* call a userland dialplan subroutine to see if this number is any good */
			if (res || ast_strlen_zero(result) || !strcmp(result, "0")) {
				/* either res is non-zero, or the returned value is empty or 0, no go. */
				INVALID();
				res = selective_term_read(chan, buf, buflen, "", MED_WAIT, f->prompt_number_incorrect);
				continue;
			}
			/* can the user actually add this number? */
			res = gosub(chan, strbuf, result, buflen, f->sub_numaddable, buf, ""); /* call a userland dialplan subroutine to see if this number is any good */
			if (res || ast_strlen_zero(result) || !strcmp(result, "0")) {
				/* either res is non-zero, or the returned value is empty or 0, no go. */
				INVALID();
				res = selective_term_read(chan, buf, buflen, "", MED_WAIT, f->prompt_number_not_available_svc);
				continue;
			}
			invalids = 0;

			PARSE_DB(number);

			if (ast_db_get(args.family, args.key, tmpbuf, BUFFER_SIZE)) {
				INVALID();
				/* number not on list */
				res = selective_term_read(chan, buf, buflen, "", MED_WAIT, f->prompt_number_to_remove_not_on_list);
				if (!ast_strlen_zero(buf)) {
					continue;
				}
				res = say_telephone_number(chan, strbuf, buf, f, number);
				CONTINUE_IF_INPUT();
				res = selective_term_read(chan, buf, buflen, "", MED_WAIT, f->prompt_beep);
				goto process; /* do NOT prompt again! */
			}
			/* Delete the entry */
			if (ast_db_del(args.family, args.key)) {
				ast_log(LOG_WARNING, "db del: Error deleting value from database.\n");
				goto nocando;
			}
			res = selective_term_read(chan, buf, buflen, "", 0, f->prompt_number_removed_is);
			CONTINUE_IF_INPUT();
			res = say_telephone_number(chan, strbuf, buf, f, number);
			CONTINUE_IF_INPUT();
			res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, main_wait, f->prompt_please_continue);
			break;
		}
	}  while (res >= 0 && invalids < 3);

	if (invalids >= 3) {
		if (res == AST_GETDATA_TIMEOUT) {
			res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_must_dial_number);
		} else { /* this is basically the switch saying it doesn't like your input, screw off */
nocando:
			res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_try_again_later);
			return -1; /* tell main menu loop to exit */
		}
	}

	return res;
}

static int add_entry(struct ast_channel *chan, struct ast_str *strbuf, char *mbuf, struct selective_proc *f, int pulse)
{
	char buf[17], result[17];
	int res;
	int buflen = 16;
	int invalids = 0;
	int mindigits = 3, maxdigits = 15, maxentries = 10;
	memset(buf, 0, sizeof(buf));

	do {
		if (ast_strlen_zero(buf)) { /* if user enters input down below, skip this on the next loop */
			res = selective_term_read(chan, buf, buflen, "", LONG_WAIT, pulse ? f->prompt_dial_number_to_add : f->prompt_dial_number_to_add_dtmf, long_wait);
		}

process:
		ast_debug(1, "Buffer contains '%s'\n", buf);

		if (res == -1) { /* user hung up */
			return res;
		} else if (res == AST_GETDATA_TIMEOUT && ast_strlen_zero(buf)) { /* no input */
			INVALID();
			ast_verb(4, "User did not enter a number to add\n");
			res = selective_term_read(chan, buf, buflen, "", LONG_WAIT, f->prompt_must_dial_number, f->prompt_start_again);
		} else if (res == AST_GETDATA_EMPTY_END_TERMINATED && ast_strlen_zero(buf)) { /* just pressed # */
			INVALID();
			ast_verb(4, "User pressed #\n");
			res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_must_dial_number);
			break;
		} else {
			char tmpbuf[2];
			char *tmpstr = NULL;
			int private = 0;
			AST_DECLARE_APP_ARGS(args,
				AST_APP_ARG(family);
				AST_APP_ARG(key);
			);

			if (IS_MAIN_COMMAND(buf)) {
				ast_verb(4, "User entered '%s', returning to main menu\n", buf);
				ast_copy_string(mbuf, buf, BUFFER_SIZE);
				return 0; /* escape to a main command */
			}
			if (!strcmp(buf, "01")) {
				ast_str_reset(strbuf);
				ast_str_substitute_variables(&strbuf, 0, chan, f->last_caller_num);
				if (ast_str_strlen(strbuf) == 0) {
					return selective_term_read(chan, mbuf, BUFFER_SIZE, "", MED_WAIT, f->prompt_last_caller_not_available);
				} else if (ast_str_strlen(strbuf) < mindigits || ast_str_strlen(strbuf) > maxdigits) {
					goto nocando;
				}
				ast_copy_string(buf, ast_str_buffer(strbuf), buflen);
				ast_str_substitute_variables(&strbuf, 0, chan, f->last_caller_pres);
				if (!strncmp(ast_str_buffer(strbuf), "prohib", 6) || !strcmp(ast_str_buffer(strbuf), "unavailable")) {
					private = 1;
				}
				ast_verb(3, "User added last party to list (%s)\n", private ? "private" : buf);
			} else {
				int len = strlen(buf); /* only calculate once! */
				ast_verb(3, "User entered '%s' to add to list\n", buf);
				if (len < mindigits) {
					return selective_term_read(chan, mbuf, BUFFER_SIZE, "", MED_WAIT, f->prompt_too_few_digits_start_again);
				} else if (len > maxdigits) {
					return selective_term_read(chan, mbuf, BUFFER_SIZE, "", MED_WAIT, f->prompt_too_many_digits_start_again);
				}
			}

			/* is this a valid number to add in the first place? */
			res = gosub(chan, strbuf, result, buflen, f->sub_numvalid, buf, ""); /* call a userland dialplan subroutine to see if this number is any good */
			if (res || ast_strlen_zero(result) || !strcmp(result, "0")) {
				/* either res is non-zero, or the returned value is empty or 0, no go. */
				INVALID();
				res = selective_term_read(chan, buf, buflen, "", MED_WAIT, f->prompt_number_incorrect);
				continue;
			}
			/* If the subroutines are the same, we know it'll succeed. Otherwise, check. */
			if (strcmp(f->sub_numvalid, f->sub_numaddable)) {
				/* can the user actually add this number? */
				res = gosub(chan, strbuf, result, buflen, f->sub_numaddable, buf, ""); /* call a userland dialplan subroutine to see if this number is any good */
				if (res || ast_strlen_zero(result) || !strcmp(result, "0")) {
					/* either res is non-zero, or the returned value is empty or 0, no go. */
					INVALID();
					res = selective_term_read(chan, buf, buflen, "", MED_WAIT, f->prompt_number_not_available_svc);
					continue;
				}
			}
			invalids = 0;

			PARSE_DB(buf);

			/* First, check if it already exists on the list. (Cue Hall and Oates...) */
			if (!ast_db_get(args.family, args.key, tmpbuf, BUFFER_SIZE)) {
				/* number is already on list! */
				char *number = ast_strdup(buf); /* probably low risk to using strdupa here, but let's just not... use strdup, and free as soon as we don't need it anymore. */
				INVALID(); /* zero out buf here */
				res = selective_term_read(chan, buf, buflen, "", 0, f->prompt_already_on_list);
				if (!ast_strlen_zero(buf)) {
					ast_free(number);
					continue;
				}
				if (get_entry_type(tmpbuf) == ENTRY_PRIVATE) {
					ast_free(number);
					res = selective_term_read(chan, buf, buflen, "", 0, f->prompt_as_a_private_entry);
				} else {
					res = say_telephone_number(chan, strbuf, buf, f, number);
					ast_free(number);
					if (res < 0) {
						return -1;
					}
				}
				if (!ast_strlen_zero(buf)) {
					continue;
				}
				res = selective_term_read(chan, buf, buflen, "", LONG_WAIT, f->prompt_beep);
				goto process; /* do NOT prompt again! */
			}
			/* okay whew, we can actually try adding the number now! */
			if (get_number_entries(chan, strbuf, f, ENTRY_ANY) >= maxentries) {
				/* so close... */
				res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_list_full);
				break;
			}
			if (ast_db_put(args.family, args.key, private ? "2" : "1")) {
				ast_log(LOG_WARNING, "db put: Error writing value to database.\n");
				goto nocando;
			}
			res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_number_added_is);
			if (!ast_strlen_zero(mbuf)) {
				break;
			}
			if (private) {
				res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_a_private_entry);
			} else {
				res = say_telephone_number(chan, strbuf, mbuf, f, buf);
				if (res < 0) {
					return -1;
				}
			}
			if (!ast_strlen_zero(mbuf)) {
				break;
			}
			if (!f->pending_toggle) { /* skip this if we added an entry to activate the feature */
				res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, main_wait, f->prompt_please_continue);
			}
			break;
		}
	} while (res >= 0 && invalids < 3);

	if (invalids >= 3) {
		if (res == AST_GETDATA_TIMEOUT) {
			res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_must_dial_number);
		} else { /* this is basically the switch saying it doesn't like your input, screw off */
nocando:
			res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, 0, f->prompt_try_again_later);
			return -1; /* tell main menu loop to exit */
		}
	}

	return res;
}

/*! \brief Callback function to delete all private entries from a list */
static int hear_entry(int crawlindex, char *family, char *key, struct ast_channel *chan, struct ast_str *strbuf, char *buf, struct selective_proc *f)
{
	char *number;
	char value[2];
	int res, len = 2;

	if (ast_db_get(family, key, value, len)) {
		ast_log(LOG_WARNING, "db: %s/%s not found in database.\n", family, key);
		return -1;
	}
	if (get_entry_type(value) == ENTRY_PRIVATE) {
		return 0; /* skip private entries */
	}

	number = key;
	ast_debug(1, "Entry index %d: '%s' (%s/%s = %s)\n", crawlindex, key, family, key, value);
	res = selective_read(chan, buf, BUFFER_SIZE, "", 0,
		crawlindex == 0 ? f->prompt_first_entry_is : f->prompt_next);

	res = say_telephone_number(chan, strbuf, buf, f, number);
	if (res < 0) {
		return -1;
	}

	res = selective_read(chan, buf, BUFFER_SIZE, "", SHORT_WAIT, "");
	ast_debug(1, "Management buffer contains '%s'\n", buf);

	if (IS_MAIN_COMMAND(buf)) {
		return -1; /* end list management */
	} else if (ast_strlen_zero(buf)) { /* no input, go to next entry */
		return res;
	} else if (strcmp(buf, "07")) { /* entered something, but not a valid command */
		memset(buf, 0, BUFFER_SIZE);
		res = selective_read(chan, buf, BUFFER_SIZE, "", MED_WAIT, f->prompt_invalid_command);
		return -1; /* end list management */
	}

	memset(buf, 0, BUFFER_SIZE); /* clear the buffer so it's ready for the next entry */

	if (ast_db_del(family, key)) {
		ast_log(LOG_WARNING, "delete: %s/%s not deleted from database.\n", family, key);
		return -1;
	}
	ast_verb(3, "Deleting entry '%s' from %s list\n", number, f->name);
	res = selective_read(chan, buf, BUFFER_SIZE, "", 0, f->prompt_number_removed_is);

	res = say_telephone_number(chan, strbuf, buf, f, number);
	if (res < 0) {
		return -1;
	}

	res = selective_read(chan, buf, BUFFER_SIZE, "", 0, main_wait);
	return res;
}

static int hear_entries(struct ast_channel *chan, struct ast_str *strbuf, char *mbuf, struct selective_proc *f)
{
	char buf[BUFFER_SIZE + 1];
	int res;
	int privates, entries = get_number_entries(chan, strbuf, f, ENTRY_ANY);
	if (!entries) {
		return selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_sorry_no_entries);
	}
	memset(buf, 0, sizeof(buf));
	res = say_number_on_list(chan, strbuf, buf, f);
	RETURN_IF_INPUT(buf);
	privates = get_number_entries(chan, strbuf, f, ENTRY_PRIVATE);
	if (entries == privates) {
		/* if all entries are private, then there is nothing to announce */
		return selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_start_again);
	}
	res = selective_read(chan, buf, BUFFER_SIZE, "", 0, f->prompt_to_delete_dial_07);
	RETURN_IF_INPUT(buf);

	ast_str_reset(strbuf);
	ast_str_substitute_variables(&strbuf, 0, chan, f->astdb_entries);
	ast_debug(1, "Substituting '%s' gives us '%s'\n", f->astdb_entries, ast_str_buffer(strbuf));

	res = db_crawl2(chan, ast_strdupa(ast_str_buffer(strbuf)), strbuf, buf, f, hear_entry);
	if (!ast_strlen_zero(buf) && IS_MAIN_COMMAND(buf)) {
		ast_copy_string(mbuf, buf, BUFFER_SIZE); /* if we got a command for the main menu, copy it to that buffer */
	}
	res = selective_read(chan, mbuf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_end_of_list);
	return res;
}

/*! \brief Actually toggle feature on or off */
static int do_toggle(struct ast_channel *chan, struct selective_proc *f, struct ast_str *strbuf)
{
	char *tmpstr = NULL;
	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(family);
		AST_APP_ARG(key);
	);
	int active = feature_status(chan, strbuf, f);
	if (ast_strlen_zero(f->astdb_active)) {
		ast_log(LOG_WARNING, "astdb_active is empty!\n");
		return -1;
	}

	PARSE_DBKEY(f->astdb_active);

	if (ast_db_put(args.family, args.key, active ? "0" : "1")) {
		ast_log(LOG_WARNING, "db put: Error writing value to database.\n");
		return -1;
	}
	if (feature_status(chan, strbuf, f) == active) {
		ast_log(LOG_WARNING, "Failed to toggle feature status\n");
		return -1;
	}

	ast_verb(3, "Feature %s toggled from %s to %s\n", f->name, active ? "on" : "off", active ? "off" : "on");
	return 0;
}

/*! \brief Toggle feature on or off */
static int toggle(struct ast_channel *chan, struct selective_proc *f, struct ast_str *strbuf, char *buf, int pulse)
{
	char tbuf[17];
	int res, entries, status, invalids = 0;

	if (ast_strlen_zero(f->astdb_featnum) && f->empty_list_allowed) {
		return do_toggle(chan, f, strbuf); /* this can be toggled back and forth at will, with no restrictions */
	}

	if (!ast_strlen_zero(f->astdb_featnum)) { /* this is typically for things like Selective Call Forwarding */
		char *featnum;
		char *tmpstr = NULL;
		AST_DECLARE_APP_ARGS(args,
			AST_APP_ARG(family);
			AST_APP_ARG(key);
		);

		PARSE_DBKEY(f->astdb_featnum);

		/* special number required to use this feature */
		if (feature_number(chan, strbuf, f)) {
			return -1;
		}
		featnum = ast_strdupa(ast_str_buffer(strbuf));
		ast_debug(1, "Special feature number is currently: '%s'\n", featnum);
		
		if (ast_strlen_zero(featnum)) {
			ast_verb(3, "Feature number required to activate %s\n", f->name);
newnumber:
			do {
				int permitted = 1;
				res = selective_term_read(chan, tbuf, 16, "", LONG_WAIT, f->prompt_dial_featnum);
				if (ast_strlen_zero(tbuf)) {
					continue;
				}
				featnum = ast_strdupa(tbuf);
				memset(tbuf, 0, sizeof(tbuf));
				
				if (ast_channel_caller(chan)->id.number.valid && !strcmp(ast_channel_caller(chan)->id.number.str, featnum)) {
					/* sorry, you cannot forward your calls to yourself */
					permitted = 0;
				}
				if (permitted) {
					char result[25];
					memset(result, 0, sizeof(result));
					permitted = 0; /* fail safe, now try to get actual value */
					/* is this a valid number to add in the first place? */
					res = gosub(chan, strbuf, result, 24, f->sub_featnumcheck, tbuf, ""); /* call a userland dialplan subroutine to see if this number is any good */
					if (res && !ast_strlen_zero(result) && strcmp(result, "0")) {
						/* either res is non-zero, or the returned value is empty or 0, no go. */
						permitted = 1;
					}
					break;
				}
				if (!permitted) {
					res = selective_term_read(chan, tbuf, 16, "", LONG_WAIT, f->prompt_number_dialed);
					res = say_telephone_number(chan, strbuf, buf, f, featnum);
					if (res < 0) {
						return -1;
					}
					res = selective_term_read(chan, tbuf, 16, "", LONG_WAIT, f->prompt_not_permitted);
				}
			} while (++invalids < 3);
			if (invalids >= 3) {
				return -1; /* Enough nonsense from you, you're outta here */
			}
			if (ast_db_put(args.family, args.key, featnum)) { /* save featnum to DB */
				ast_log(LOG_WARNING, "db put: Error writing value to database.\n");
				return -1;
			}
			ast_verb(3, "New feature number for %s is %s\n", f->name, featnum);
		}

		/* if we just got a number, or we already had one, confirm it's (still) good */
		invalids = 0;
		do { /* confirm existing number is still good to use */
			res = selective_read(chan, tbuf, 1, "", MED_WAIT,
			f->prompt_active_featnum);
			if (res < 0) {
				return -1;
			}
			if (ast_strlen_zero(tbuf)) {
				res = say_telephone_number(chan, strbuf, buf, f, featnum);
				if (res < 0) {
					return -1;
				}
			}
			res = selective_read(chan, tbuf, 1, "", LONG_WAIT,
			f->prompt_confirm_entry); /* if buffer is full, this will get skipped implicitly */
			if (!strcmp(tbuf, "0")) {
				ast_verb(3, "User cleared feature number for %s, obtaining new number\n", f->name);
				if (ast_db_del(args.family, args.key)) { /* delete number from DB */
					ast_log(LOG_WARNING, "delete: %s/%s not deleted from database.\n", args.family, args.key);
				}
				goto newnumber;
			}
			if (!strcmp(tbuf, "1")) {
				break; /* proceed (no need to save, it's already saved) */
			}
		} while (++invalids < 3);
		if (invalids >= 3) {
			return -1;
		}
	}

	status = feature_status(chan, strbuf, f);
	entries = get_number_entries(chan, strbuf, f, ENTRY_ANY);

	/* either an activation number isn't required, or we just got it, so we're good. */
	if (!f->empty_list_allowed && !entries && !status) {
		/* list is currently empty, feature is off, and feature can't be on with empty list. Problem! */
		f->pending_toggle = 1; /* indicate that if entries are added, the feature should auto-activate */
		if (pulse) {
			res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT,
				f->prompt_entry_required);
		} else {
			res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT,
				f->prompt_entry_required_dtmf);
		}
		return res;
	} else if (!f->empty_list_allowed && !entries && status) {
		ast_log(LOG_WARNING, "Feature %s was active, but list was empty (inconsistent state)\n", f->name);
	}
	/* either we're turning it off, or it has entries already */
	return do_toggle(chan, f, strbuf);
}

/* if feature was previously on, we need to turn it off now, and announce to user if still there */
#define DISABLE_IF_EMPTY() \
	if (!f->empty_list_allowed && get_number_entries(chan, strbuf, f, ENTRY_ANY) == 0) { \
		int status = feature_status(chan, strbuf, f); \
		if (status) { \
			do_toggle(chan, f, strbuf); \
			if (res >= 0) { \
				res = say_status(chan, strbuf, buf, f); \
			} \
		} \
	}

/*! \brief Top-level selective feature menu loop */
static int selective_feature(struct ast_channel *chan, struct selective_proc *fmain, struct ast_str *strbuf, int pulse)
{
	int res, invalids = 0;
	struct selective_proc *f;
	char buf[BUFFER_SIZE + 1];

	memset(buf, 0, sizeof(buf));
	if (!(f = ast_calloc(1, sizeof(*f)))) {
		return -1;
	}

	ast_mutex_lock(&fmain->lock);
	fmain->total++;
	memcpy(f, fmain, sizeof(*fmain)); /* copy contents of fmain into f, so we can use a lockless copy of it */
	ast_mutex_unlock(&fmain->lock);
	f->pending_toggle = 0;

	ast_stopstream(chan); /* Stop anything playing */
	/* Except for the last command before we wait in silence for input, timeout should be very small, i.e. < 1 second */

	/* Begin by indicating whether the feature is enabled or not */
	res = say_status(chan, strbuf, buf, f);
	CHECK_INPUT();

	/* Then announce how many entries are on the list */
	res = say_number_on_list(chan, strbuf, buf, f);
	CHECK_INPUT();

	res = selective_read(chan, buf, 2, MAIN_MENU_TERM, SHORT_WAIT, f->prompt_dial_during_instr, short_wait, pulse ? f->prompt_to_reject_last : f->prompt_to_reject_last_dtmf);
	CHECK_INPUT();

	do {
		if (!ast_strlen_zero(buf)) {
			ast_log(LOG_WARNING, "BUG! Buffer is not empty: %s\n", buf); /* should never happen */
		}

		{ /* limit scope of status to this block */
			int status = feature_status(chan, strbuf, f);
			res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, status ? f->prompt_to_turn_off : f->prompt_to_turn_on, brief_wait, f->prompt_add_delete);
		}

process:
		ast_debug(1, "Instruction buffer contains '%s'\n", buf);
		/* We clear buffer before processing command so that we can read input here (like in option "0") */
		if (res == -1) { /* user hung up */
			break;
		} else if (res == AST_GETDATA_TIMEOUT && ast_strlen_zero(buf)) { /* no input */
			INVALID();
			ast_verb(4, "User did not select a selective feature option\n");
			res = 0;
			continue;
		/* okay, we got some input... was it any good? If it was, reset invalid counter. */
		} else if (!strcmp(buf, "3")) {
			VALID();
			f->pending_toggle = 0; /* in case a user adds an entry and then gets here before we can turn it on ourselves, make sure we don't try to toggle it again */
			res = toggle(chan, f, strbuf, buf, pulse);
			PROCESS_IF_INPUT();
			if (!res) {
				res = say_status(chan, strbuf, buf, f);
			}
		} else if (!strcmp(buf, "#") || !strcmp(buf, "12")) { /* Add Entry */
			VALID();
			ast_verb(3, "User adding entry to %s list\n", f->name);
			res = add_entry(chan, strbuf, buf, f, pulse);
			if (f->pending_toggle) {
				int entries, status = feature_status(chan, strbuf, f);
				entries = get_number_entries(chan, strbuf, f, ENTRY_ANY);
				f->pending_toggle = 0;
				if (status) {
					ast_log(LOG_WARNING, "Feature was pending toggle, but already on?\n");
				} else if (!entries) {
					ast_log(LOG_WARNING, "Feature was pending toggle, but list is empty?\n");
					f->pending_toggle = 1;
				} else {
					do_toggle(chan, f, strbuf); /* if feature was off, turn it on */
					if (res >= 0) { /* but only announce that if the user's still here! */
						res = say_status(chan, strbuf, buf, f);
					}
				}
			}
			if (res < 0) {
				break;
			}
		} else if (!strcmp(buf, "*") || !strcmp(buf, "11")) { /* Remove Entry */
			VALID();
			ast_verb(3, "User removing entry from %s list\n", f->name);
			res = remove_entry(chan, strbuf, buf, f, pulse);
			DISABLE_IF_EMPTY();
			if (res < 0) {
				break;
			}
		} else if (!strcmp(buf, "08")) { /* Remove All Entries */
			VALID();
			res = selective_clear(chan, strbuf, buf, f, ENTRY_ANY);
			DISABLE_IF_EMPTY();
		} else if (!strcmp(buf, "09")) { /* Remove All Private Entries */
			VALID();
			res = selective_clear(chan, strbuf, buf, f, ENTRY_PRIVATE);
			DISABLE_IF_EMPTY();
		} else if (!strcmp(buf, "1")) { /* Manage Entries */
			VALID();
			ast_verb(3, "User managing %s entries\n", f->name);
			res = hear_entries(chan, strbuf, buf, f);
			DISABLE_IF_EMPTY();
		} else if (!strcmp(buf, "0")) { /* Hear Instructions */
			int active = feature_status(chan, strbuf, f);
			VALID();
			ast_verb(3, "User requested instructions for %s\n", f->name);
			res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, 7,
				pulse ? f->prompt_instructions_1 : f->prompt_instructions_1_dtmf,
				active ? f->prompt_instructions_2_on : f->prompt_instructions_2_off,
				f->prompt_instructions_3);
		} else { /* do-si-do, go round again */
			INVALID();
			ast_verb(3, "Selective option '%s' is not valid\n", buf);
			res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, f->prompt_invalid_command);
		}
		/* if user entered a command during the instructions, process it. */
		PROCESS_IF_INPUT();
		/* if we returned from an application, wait several seconds before starting the prompt loop again */
		res = selective_read(chan, buf, BUFFER_SIZE, MAIN_MENU_TERM, MED_WAIT, med_wait);
		PROCESS_IF_INPUT();
	} while ((!res || res == AST_GETDATA_TIMEOUT) && invalids < 3);

	if (invalids >= 3) {
		int active;

		ast_verb(3, "Too many invalid attempts for %s, disconnecting\n", f->name);
		active = feature_status(chan, strbuf, f);
		
		res = ast_streamfile(chan, active ? f->prompt_on : f->prompt_off, ast_channel_language(chan));
		if (!res) {
			res = ast_waitstream(chan, "");
			ast_stopstream(chan);
		}
		if (!res) {
			res = ast_streamfile(chan, f->prompt_hangup, ast_channel_language(chan));
		}
		if (!res) {
			res = ast_waitstream(chan, "");
			ast_stopstream(chan);
		}
	}

	ast_free(f);
	return res;
}

static int selective_exec(struct ast_channel *chan, const char *data)
{
	char *argstr = NULL;
	struct ast_str *strbuf = NULL;
	struct selective_proc *f;
	struct ast_flags flags = {0};
	int res, pulse = 0;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(profile);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "%s requires arguments\n", app);
		return -1;
	}

	argstr = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, argstr);

	if (ast_strlen_zero(args.profile)) {
		ast_log(LOG_WARNING, "%s requires a profile\n", app);
		return -1;
	}
	if (!ast_strlen_zero(args.options)) {
		ast_app_parse_options(selective_app_options, &flags, NULL, args.options);
		if (ast_test_flag(&flags, OPT_PULSE)) {
			pulse = 1;
		}
	}

	AST_RWLIST_RDLOCK(&features);
	AST_LIST_TRAVERSE(&features, f, entry) {
		if (!strcasecmp(f->name, args.profile)) {
			break;
		}
	}
	AST_RWLIST_UNLOCK(&features);

	if (!f) {
		ast_log(LOG_WARNING, "No profile found in configuration for processing selective feature '%s'\n", args.profile);
		return -1;
	}
	if (ast_strlen_zero(f->astdb_entries)) {
		ast_log(LOG_WARNING, "No value specified for astdb_entries\n");
		return -1;
	}

	if (!(strbuf = ast_str_create(512))) {
		ast_log(LOG_ERROR, "Could not allocate memory for response.\n");
		return -1;
	}

	res = selective_feature(chan, f, strbuf, pulse);

	ast_free(strbuf);

	return res;
}

/*! \brief CLI command to list feature processing profiles */
static char *handle_show_profiles(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-20s %20s\n"
#define FORMAT2 "%-20s %20d\n"
	struct selective_proc *f;

	switch(cmd) {
	case CLI_INIT:
		e->command = "selective show profiles";
		e->usage =
			"Usage: selective show profiles\n"
			"       Lists all selective feature profiles.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	ast_cli(a->fd, FORMAT, "Name", "# Interactions");
	ast_cli(a->fd, FORMAT, "--------------------", "--------------");
	AST_RWLIST_RDLOCK(&features);
	AST_LIST_TRAVERSE(&features, f, entry) {
		ast_cli(a->fd, FORMAT2, f->name, f->total);
	}
	AST_RWLIST_UNLOCK(&features);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

/*! \brief CLI command to dump selective feature profile */
static char *handle_show_profile(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-40s : %s\n"
#define FORMAT2 "%-40s : %f\n"
#define FORMAT3  "*** %s ***\n"
	struct selective_proc *f;
	int which = 0;
	char *ret = NULL;

	switch(cmd) {
	case CLI_INIT:
		e->command = "selective show profile";
		e->usage =
			"Usage: selective show profile <profile name>\n"
			"       Displays information about a selective feature profile.\n";
		return NULL;
	case CLI_GENERATE:
		if (a->pos != 3) {
			return NULL;
		}
		AST_LIST_TRAVERSE(&features, f, entry) {
			if (!strncasecmp(a->word, f->name, strlen(a->word)) && ++which > a->n) {
				ret = ast_strdup(f->name);
				break;
			}
		}

		return ret;
	}

	if (a->argc != 4) {
		return CLI_SHOWUSAGE;
	}

	AST_RWLIST_RDLOCK(&features);
	AST_LIST_TRAVERSE(&features, f, entry) {
		if (!strcasecmp(a->argv[3], f->name)) {
			ast_cli(a->fd, FORMAT3, "General");
			ast_cli(a->fd, FORMAT, "Name", f->name);
			ast_cli(a->fd, FORMAT, "AstDB Feature Enabled", f->astdb_active);
			ast_cli(a->fd, FORMAT, "AstDB Entry Prefix", f->astdb_entries);
			ast_cli(a->fd, FORMAT, "AstDB Feature Number", f->astdb_featnum);
			ast_cli(a->fd, FORMAT, "Gosub: Number Valid", f->sub_numvalid);
			ast_cli(a->fd, FORMAT, "Gosub: Number Addable", f->sub_numaddable);
			ast_cli(a->fd, FORMAT, "Gosub: Feature Number Usable", f->sub_featnumcheck);
			ast_cli(a->fd, FORMAT, "Get: Last Caller Number", f->last_caller_num);
			ast_cli(a->fd, FORMAT, "Get: Last Caller Pres", f->last_caller_pres);
			ast_cli(a->fd, FORMAT, "Args: SayTelephoneNumber(<dir>,,)", f->saytelnum_dir);
			ast_cli(a->fd, FORMAT, "Args: SayTelephoneNumber(,,<args>)", f->saytelnum_args);

			ast_cli(a->fd, FORMAT3, "Prompts - Status");
			ast_cli(a->fd, FORMAT, "Prompt: Feature Enabled", f->prompt_on);
			ast_cli(a->fd, FORMAT, "Prompt: Feature Disabled", f->prompt_off);

			ast_cli(a->fd, FORMAT3, "Prompts - Main Menu");
			ast_cli(a->fd, FORMAT, "Prompt: Dial During Instructions", f->prompt_dial_during_instr);
			ast_cli(a->fd, FORMAT, "Prompt: To reject last caller", f->prompt_to_reject_last);
			ast_cli(a->fd, FORMAT, "Prompt: To reject last caller (DTMF)", f->prompt_to_reject_last_dtmf);
			ast_cli(a->fd, FORMAT, "Prompt: To turn off", f->prompt_to_turn_off);
			ast_cli(a->fd, FORMAT, "Prompt: To turn on", f->prompt_to_turn_on);
			ast_cli(a->fd, FORMAT, "Prompt: To add or delete", f->prompt_add_delete);
			ast_cli(a->fd, FORMAT, "Prompt: Long Instructions pt. 1", f->prompt_instructions_1);
			ast_cli(a->fd, FORMAT, "Prompt: Long Instructions pt. 1 (DTMF)", f->prompt_instructions_1_dtmf);
			ast_cli(a->fd, FORMAT, "Prompt: Long Instructions pt. 2 (on)", f->prompt_instructions_2_on);
			ast_cli(a->fd, FORMAT, "Prompt: Long Instructions pt. 2 (off)", f->prompt_instructions_2_off);
			ast_cli(a->fd, FORMAT, "Prompt: Long Instructions pt. 3", f->prompt_instructions_3);

			ast_cli(a->fd, FORMAT3, "Prompts - List Contents");
			ast_cli(a->fd, FORMAT, "Prompt: There is", f->prompt_there_is);
			ast_cli(a->fd, FORMAT, "Prompt: There are", f->prompt_there_are);
			ast_cli(a->fd, FORMAT, "Prompt: No Entries", f->prompt_no_entries);
			ast_cli(a->fd, FORMAT, "Prompt: On your list", f->prompt_on_your_list);
			ast_cli(a->fd, FORMAT, "Prompt: Number '1'", f->prompt_one);
			ast_cli(a->fd, FORMAT, "Prompt: Number '10'", f->prompt_ten);
			ast_cli(a->fd, FORMAT, "Prompt: No Entries", f->prompt_sorry_no_entries);
			ast_cli(a->fd, FORMAT, "Prompt: No More Private Entries", f->prompt_no_more_private);
			ast_cli(a->fd, FORMAT, "Prompt: No More", f->prompt_no_more);
			ast_cli(a->fd, FORMAT, "Prompt: Private Entries", f->prompt_private_entries);
			ast_cli(a->fd, FORMAT, "Prompt: 1 Private Entry", f->prompt_one_private_entry);
			ast_cli(a->fd, FORMAT, "Prompt: 1 Private Entry (down)", f->prompt_one_private_entry_down);
			ast_cli(a->fd, FORMAT, "Prompt: Private Entries (down)", f->prompt_private_entries_down);
			ast_cli(a->fd, FORMAT, "Prompt: 1 Entry", f->prompt_one_entry);
			ast_cli(a->fd, FORMAT, "Prompt: Entries On Your List", f->prompt_entries_on_your_list);
			ast_cli(a->fd, FORMAT, "Prompt: Including", f->prompt_including);

			ast_cli(a->fd, FORMAT3, "Prompts - Activation Caveats");
			ast_cli(a->fd, FORMAT, "Prompt: Entry Required", f->prompt_entry_required);
			ast_cli(a->fd, FORMAT, "Prompt: Entry Required (DTMF)", f->prompt_entry_required_dtmf);

			ast_cli(a->fd, FORMAT3, "Prompts - List Management");
			ast_cli(a->fd, FORMAT, "Prompt: (beep)", f->prompt_beep);
			ast_cli(a->fd, FORMAT, "Prompt: Dial # To Add", f->prompt_dial_number_to_add);
			ast_cli(a->fd, FORMAT, "Prompt: Dial # To Add (DTMF)", f->prompt_dial_number_to_add_dtmf);
			ast_cli(a->fd, FORMAT, "Prompt: Dial # To Remove", f->prompt_dial_remove);
			ast_cli(a->fd, FORMAT, "Prompt: Must Dial a #", f->prompt_must_dial_number);
			ast_cli(a->fd, FORMAT, "Prompt: Start Again", f->prompt_start_again);
			ast_cli(a->fd, FORMAT, "Prompt: # Dialed Is", f->prompt_number_dialed);
			ast_cli(a->fd, FORMAT, "Prompt: # Not Permitted", f->prompt_not_permitted);
			ast_cli(a->fd, FORMAT, "Prompt: Too Few Digits, Start Again", f->prompt_too_few_digits_start_again);
			ast_cli(a->fd, FORMAT, "Prompt: Too Many Digits, Start Again", f->prompt_too_many_digits_start_again);
			ast_cli(a->fd, FORMAT, "Prompt: Last Caller Not Available", f->prompt_last_caller_not_available);
			ast_cli(a->fd, FORMAT, "Prompt: # Incorrect", f->prompt_number_incorrect);
			ast_cli(a->fd, FORMAT, "Prompt: # Not Available With Svc", f->prompt_number_not_available_svc);
			ast_cli(a->fd, FORMAT, "Prompt: # Already On List", f->prompt_already_on_list);
			ast_cli(a->fd, FORMAT, "Prompt: As A Private Entry", f->prompt_as_a_private_entry);
			ast_cli(a->fd, FORMAT, "Prompt: List Full", f->prompt_list_full);
			ast_cli(a->fd, FORMAT, "Prompt: # Not On List", f->prompt_number_to_remove_not_on_list);
			ast_cli(a->fd, FORMAT, "Prompt: Confirm Entry", f->prompt_confirm_entry);

			ast_cli(a->fd, FORMAT3, "Prompts - List Management Egress");
			ast_cli(a->fd, FORMAT, "Prompt: # Added Is", f->prompt_number_added_is);
			ast_cli(a->fd, FORMAT, "Prompt: # Removed Is", f->prompt_number_removed_is);
			ast_cli(a->fd, FORMAT, "Prompt: A Private Entry", f->prompt_a_private_entry);
			ast_cli(a->fd, FORMAT, "Prompt: Please Continue", f->prompt_please_continue);
			ast_cli(a->fd, FORMAT, "Prompt: Try Again Later", f->prompt_try_again_later);
			ast_cli(a->fd, FORMAT, "Prompt: Please Hangup", f->prompt_hangup);
			ast_cli(a->fd, FORMAT, "Prompt: Invalid Command", f->prompt_invalid_command);

			ast_cli(a->fd, FORMAT3, "Prompts - Hear & Manage Entries");
			ast_cli(a->fd, FORMAT, "Prompt: To Delete Dial 07", f->prompt_to_delete_dial_07);
			ast_cli(a->fd, FORMAT, "Prompt: First Entry Is", f->prompt_first_entry_is);
			ast_cli(a->fd, FORMAT, "Prompt: Next Entry Is", f->prompt_next);
			ast_cli(a->fd, FORMAT, "Prompt: End of List", f->prompt_end_of_list);

			break;
		}
	}
	AST_RWLIST_UNLOCK(&features);
	if (!f) {
		ast_cli(a->fd, "No such feature processing profile: '%s'\n", a->argv[3]);
	}

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
#undef FORMAT3
}

/*! \brief CLI command to dump selective feature general config */
static char *handle_show_general(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
#define FORMAT  "%-32s : %s\n"
#define FORMAT2 "%-32s : %f\n"

	switch(cmd) {
	case CLI_INIT:
		e->command = "selective show general";
		e->usage =
			"Usage: selective show general\n"
			"       Displays information about a selective feature profile.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != 3) {
		return CLI_SHOWUSAGE;
	}

	ast_cli(a->fd, FORMAT, "Brief Wait", brief_wait);
	ast_cli(a->fd, FORMAT, "Short Wait", short_wait);
	ast_cli(a->fd, FORMAT, "Main Wait", main_wait);
	ast_cli(a->fd, FORMAT, "Medium Wait", med_wait);
	ast_cli(a->fd, FORMAT, "Long Wait", long_wait);

	return CLI_SUCCESS;
#undef FORMAT
#undef FORMAT2
}

/*! \brief CLI command to reset feature profile stats */
static char *handle_reset_stats(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	struct selective_proc *f;

	switch(cmd) {
	case CLI_INIT:
		e->command = "selective reset stats";
		e->usage =
			"Usage: selective reset stats\n"
			"       Resets feature processing statistics.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	AST_RWLIST_RDLOCK(&features);
	AST_LIST_TRAVERSE(&features, f, entry) {
		f->total = 0;
	}
	AST_RWLIST_UNLOCK(&features);

	return CLI_SUCCESS;
}

static struct ast_cli_entry selective_cli[] = {
	AST_CLI_DEFINE(handle_show_profiles, "Display statistics about feature processing profiles"),
	AST_CLI_DEFINE(handle_show_profile, "Displays information about a feature processing profile"),
	AST_CLI_DEFINE(handle_show_general, "Displays general configuration info for the module"),
	AST_CLI_DEFINE(handle_reset_stats, "Resets feature processing statistics for all feature profiles"),
};

static int unload_module(void)
{
	struct selective_proc *f;

	ast_unregister_application(app);
	ast_cli_unregister_multiple(selective_cli, ARRAY_LEN(selective_cli));

	AST_RWLIST_WRLOCK(&features);
	while ((f = AST_RWLIST_REMOVE_HEAD(&features, entry))) {
		ast_free(f);
	}

	AST_RWLIST_UNLOCK(&features);

	return 0;
}

/*!
 * \brief Load the module
 *
 * Module loading including tests for configuration or dependencies.
 * This function can return AST_MODULE_LOAD_FAILURE, AST_MODULE_LOAD_DECLINE,
 * or AST_MODULE_LOAD_SUCCESS. If a dependency or environment variable fails
 * tests return AST_MODULE_LOAD_FAILURE. If the module can not load the
 * configuration file or other non-critical problem return
 * AST_MODULE_LOAD_DECLINE. On success return AST_MODULE_LOAD_SUCCESS.
 */
static int load_module(void)
{
	int res;

	if (selective_reload(0)) {
		return AST_MODULE_LOAD_DECLINE;
	}

	ast_cli_register_multiple(selective_cli, ARRAY_LEN(selective_cli));

	res = ast_register_application_xml(app, selective_exec);

	return res;
}

static int reload(void)
{
	return selective_reload(1);
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_DEFAULT, "Selective Calling Feature Application",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.reload = reload,
);
