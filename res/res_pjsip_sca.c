/*
 * Asterisk -- An open source telephony toolkit.
 *
 * Copyright (C) 2022-2023, Naveen Albert
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
 * \brief Shared Call Appearances
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \note This is a Broadworks compatible Shared Call Appearance implementation
 *       This includes implementing line-seize for the richest user experience.
 *       There is no explicit configuration required for this module.
 *       Any registrations to the same endpoint will be able to use SCA functionality.
 *       (assuming the clients support it). "It just works".
 *       When dialing SCA endpoints, it may be desirable to use the 'c' option to avoid missed calls for answered elsewhere.
 *       In the dialplan, an extension must exist for the endpoint name (priority 1)
 *       in order for phones to be able to join or unhold calls that they did not previously own.
 *       However, the dialplan extension itself is not executed in any way.
 */

/*** MODULEINFO
	<depend>pjproject</depend>
	<depend>res_pjsip</depend>
	<depend>res_pjsip_pubsub</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <pjsip.h>
#include <pjsip_simple.h>
#include <pjlib.h>

#include "asterisk/res_pjsip.h"
#include "asterisk/res_pjsip_outbound_publish.h"
#include "asterisk/res_pjsip_pubsub.h"
#include "asterisk/res_pjsip_body_generator_types.h"
#include "asterisk/res_pjsip_session.h"
#include "asterisk/module.h"
#include "asterisk/logger.h"
#include "asterisk/astobj2.h"
#include "asterisk/sorcery.h"
#include "asterisk/taskprocessor.h"
#include "asterisk/app.h"
#include "asterisk/astdb.h"
#include "asterisk/lock.h"
#include "asterisk/cli.h"
#include "asterisk/bridge.h"
#include "asterisk/bridge_channel.h"
#include "asterisk/indications.h"
#include "asterisk/format_cache.h" /* use ast_format_slin */

/*! \todo move to include file */
#define AST_SIP_SCA_DATA "ast_sip_sca_data"

struct ast_sip_sca_data {
	char park_uri[256];
	char park_display[64];
	unsigned int park_enabled:1;
};

/*** DOCUMENTATION
	<application name="PJSIPSharedCallAppearance" language="en_US">
		<synopsis>
			Add an arbitrary channel to a PJSIP shared call appearance
		</synopsis>
		<syntax>
			<parameter name="endpoint">
				<para>The name of the PJSIP endpoint name for the shared call appearance.</para>
			</parameter>
			<parameter name="appearance">
				<para>The appearance number. It is highly recommended that you use 1, which is the default.</para>
			</parameter>
			<parameter name="state">
				<para>The state to try to set for the appearance.</para>
				<para>The following are valid:</para>
				<enumlist>
					<enum name="seized">
						<para>Phone is off-hook and Asterisk will be collecting digits.</para>
					</enum>
					<enum name="progressing">
						<para>A number has been dialed.</para>
						<para>You generally only need seized OR progressing (seized if your phones
						begin executing dialplan immediately when they go off-hook, and you
						collect digits in the dialplan, and progressing if calls arrive to Asterisk
						with a number already).</para>
					</enum>
					<enum name="alerting">
						<para>Incoming call to the phone (invoked within a pre-dial handler).</para>
					</enum>
				</enumlist>
			</parameter>
			<parameter name="options">
				<optionlist>
					<option name="b">
						<para>If the line is already active, barge in to the existing call.</para>
						<para>This is only compatible with "seized", not "progressing".</para>
						<para>By default, barge-in is not enabled and the call will be rejected.</para>
					</option>
				</optionlist>
			</parameter>
		</syntax>
		<description>
			<para>Add an arbitrary channel to a PJSIP shared call appearance.</para>
			<example title="Add analog phone to appearance">
			[from-internal]
			exten => s,1,PJSIPSharedCallAppearance(MySharedLine,1,seized)
				same => n,WaitExten(,d)
			exten => _2XXX,1,PJSIPSharedCallAppearance(MySharedLine,1,progressing)
				same => n,Goto(extensions,${EXTEN},1)

			[extensions]
			exten => 2368,1,Dial(${PJSIP_DIAL_CONTACTS(PJSIP/MySharedLine)}&amp;DAHDI/1,,b(predial,s,1))
				same => n,Hangup()

			[predial]
			exten => s,1,ExecIf($["${CHANNEL:0:6}"="PJSIP/"]?Return) ; only need this predial handler for phones that aren't using native PJSIP SCA.
				same => n,PJSIPSharedCallAppearance(MySharedLine,1,alerting)
				same => n,Return()
			</example>
		</description>
	</application>
 ***/

#define ao2_count(o) ao2_ref(o, 0)

#define PARK_MIME_TYPE "application/x-broadworks-callpark-info+xml"
#define CALLINFO_EVENT "call-info"
#define SEIZE_EVENT "line-seize"

#define MAX_APPEARANCES 32

enum sca_appearance_state {
	SCA_APPEARANCE_NONE = 0,		/* Internal: Indicates no such appearance exists (default). Indicates we can stop searching for appearances in the array. */
	SCA_APPEARANCE_IDLE,			/* Appearance not currently in use. */
	SCA_APPEARANCE_ALERTING,		/* Phone is ringing for incoming call */
	SCA_APPEARANCE_SEIZED,			/* Endpoint gone off-hook, dialing digits */
	SCA_APPEARANCE_PROGRESSING, 	/* Outgoing call: INVITE sent for call, but call not yet answered. */
	SCA_APPEARANCE_ACTIVE,			/* Call answered and is now active. */
	SCA_APPEARANCE_HELD,			/* Call is on hold */
	SCA_APPEARANCE_HELD_PRIVATE,	/* Call is on private hold */
	/* These two are in the spec, but not as important as the states above. Implemented for the sake of completeness. */
	SCA_APPEARANCE_BRIDGE_ACTIVE,	/* Call answered and now active, multiple contacts bridged */
	SCA_APPEARANCE_BRIDGE_HELD,		/* Same as BRIDGE_ACTIVE, but has been held from a client. */
};

/* Indexes start as 1, but our array starts at 0. */
#define APPEARANCE_ARRAY_INDEX(x) (x - 1)
#define APPEARANCE_REAL_INDEX(x) (x + 1)

static const char *sca_appearance_state_str(enum sca_appearance_state state)
{
	switch (state) {
	case SCA_APPEARANCE_IDLE:
		return "idle";
	case SCA_APPEARANCE_ALERTING:
		return "alerting";
	case SCA_APPEARANCE_SEIZED:
		return "seized";
	case SCA_APPEARANCE_PROGRESSING:
		return "progressing";
	case SCA_APPEARANCE_ACTIVE:
		return "active";
	case SCA_APPEARANCE_HELD:
		return "held";
	case SCA_APPEARANCE_HELD_PRIVATE:
		return "held-private";
	case SCA_APPEARANCE_BRIDGE_ACTIVE:
		return "bridge-active";
	case SCA_APPEARANCE_BRIDGE_HELD:
		return "bridge-held";
	case SCA_APPEARANCE_NONE:
		/* This function should never be called with this value. */
		ast_assert_return("", 0);
	}
	return NULL;
}

static enum sca_appearance_state sca_appearance_state_from_str(const char *s)
{
	if (!strcasecmp(s, "idle")) {
		return SCA_APPEARANCE_IDLE;
	} else if (!strcasecmp(s, "alerting")) {
		return SCA_APPEARANCE_ALERTING;
	} else if (!strcasecmp(s, "seized")) {
		return SCA_APPEARANCE_SEIZED;
	} else if (!strcasecmp(s, "progressing")) {
		return SCA_APPEARANCE_PROGRESSING;
	} else if (!strcasecmp(s, "active")) {
		return SCA_APPEARANCE_ACTIVE;
	} else if (!strcasecmp(s, "held")) {
		return SCA_APPEARANCE_HELD;
	} else if (!strcasecmp(s, "held-private")) {
		return SCA_APPEARANCE_HELD_PRIVATE;
	} else if (!strcasecmp(s, "bridge-active")) {
		return SCA_APPEARANCE_BRIDGE_ACTIVE;
	} else if (!strcasecmp(s, "bridge-held")) {
		return SCA_APPEARANCE_BRIDGE_HELD;
	}
	return SCA_APPEARANCE_NONE;
}

struct sca_subscription;

struct sca_sip_sub {
	struct ast_sip_subscription *sub;
	AST_LIST_ENTRY(sca_sip_sub) entry;
};

AST_RWLIST_HEAD(sca_sip_sub_list, sca_sip_sub);

struct sca_session {
	struct ast_sip_session *session;
	AST_LIST_ENTRY(sca_session) entry;
};

struct sca_channel {
	struct ast_channel *chan;
	AST_LIST_ENTRY(sca_channel) entry;
};

AST_RWLIST_HEAD(sca_session_list, sca_session);
AST_RWLIST_HEAD(sca_channel_list, sca_channel);

/*! \brief Information about a single line appearance. */
struct sca_appearance {
	/*! The appearance state */
	enum sca_appearance_state state;
	/* The last appearance state */
	enum sca_appearance_state laststate;
	/* Use count */
	int usecount;
	/* Time the appearance was seized */
	int seizetime;
	/*! The appearance URI */
	char *uri;
	/*! The Call ID, if in use */
	char *callid;
	/*! The contact (owner), if in use */
	char *contact;
	/* The linked ID associated with a call */
	char *linkedid;
	/*! The sessions associated with a call (used for phones that natively support the SCA standard) */
	struct sca_session_list sessionlist;
	/*! The alternative channels associated with a call (used for phones that don't natively do SCA, e.g. analog phones, non-PJSIP stuff, etc.) */
	struct sca_channel_list chanlist;
};

/*!
 * \brief A subscription for shared call appearances
 *
 * This structure acts as the owner for the underlying SIP subscription.
 */
struct sca_subscription {
	/*! The name of the SCA endpoint */
	char *endpoint;
	/*! The serializer to use for notifications */
	struct ast_taskprocessor *serializer;
	/*! Info used by res_pjsip_sca_body_generator */
	struct ast_sip_sca_data sca_data;
	/*! Array of call appearances */
	struct sca_appearance appearances[MAX_APPEARANCES];
	/*! Mutex for atomic operations */
	ast_mutex_t lock;
	/* List of SIP subscriptions using this subscription (many ast_sip_subscriptions may use this underlying sca_subscription). */
	struct sca_sip_sub_list subs;
};

/*! \note If you look through the code, you may notice that send_update is almost always called after set_appearance
 * returns success, and wonder why we don't just call send_update inside set_appearance_full. The reason is because
 * sometimes we may need to do other things in the calling function on success, BEFORE the NOTIFY goes out. */
static int send_update(struct sca_subscription *sca_sub);

struct subscription_item {
	char *endpoint;
	struct sca_subscription *sub;
	AST_LIST_ENTRY(subscription_item) entry;
};

static AST_RWLIST_HEAD_STATIC(sublist, subscription_item);

static void seize_shutdown(struct ast_sip_subscription *sub);
static void subscription_shutdown(struct ast_sip_subscription *sub);
static int new_seize(struct ast_sip_endpoint *endpoint, const char *resource, pjsip_rx_data *rdata);
static int new_subscribe(struct ast_sip_endpoint *endpoint, const char *resource);
static int seize_established(struct ast_sip_subscription *sub);
static int subscription_established(struct ast_sip_subscription *sub);
static int on_tdata(struct ast_sip_subscription *sub, pjsip_tx_data *tdata);
static void *get_seize_notify_data(struct ast_sip_subscription *sub);
static void *get_notify_data(struct ast_sip_subscription *sub);
static int get_resource_display_name(struct ast_sip_endpoint *endpoint, const char *resource, char *display_name, int display_name_size);
static void to_ami(struct ast_sip_subscription *sub, struct ast_str **buf);

/* For call-info */
struct ast_sip_notifier sca_notifier = {
	.default_accept = PARK_MIME_TYPE,
	.new_subscribe = new_subscribe,
	/* Don't need a refresh handler, SCA doesn't do anything special with their contents. */
	.subscription_established = subscription_established,
	.notify_created = on_tdata,
	.get_notify_data = get_notify_data,
	.get_resource_display_name = get_resource_display_name,
};

struct ast_sip_subscription_handler sca_handler = {
	.event_name = CALLINFO_EVENT,
	.body_type = AST_SIP_SCA_DATA,
	/* XXX In theory, basic SCA functionality doesn't require anything in the body.
	 * However, res_pjsip_pubsub is currently written to reject subscriptions if there
	 * is no body generator for the event.
	 * The body handler *is* used for call park bodies, so this isn't just a dummy handler.
	 */
	.accept = { PARK_MIME_TYPE, },
	.subscription_shutdown = subscription_shutdown,
	.to_ami = to_ami,
	.notifier = &sca_notifier,
};

/* For line-seize */
struct ast_sip_notifier sca_seize_notifier = {
	.default_accept = PARK_MIME_TYPE,
	.new_subscribe_with_rdata = new_seize,
	/* Don't need a refresh handler, SCA doesn't do anything special with their contents. */
	.subscription_established = seize_established,
	.get_notify_data = get_seize_notify_data,
	.get_resource_display_name = get_resource_display_name,
};

struct ast_sip_subscription_handler sca_seize_handler = {
	.event_name = SEIZE_EVENT,
	.body_type = AST_SIP_SCA_DATA,
	.accept = { PARK_MIME_TYPE, },
	.subscription_shutdown = seize_shutdown,
	.to_ami = to_ami,
	.notifier = &sca_seize_notifier,
};

static void sca_subscription_destructor(void *obj)
{
	struct sca_sip_sub *sipsub;
	int subs = 0;
	struct sca_subscription *sca_sub = obj;

	ast_debug(2, "Completely destroying SCA subscription object\n");

	/* The SIP subscriptions themselves should have all already been destroyed. Otherwise, we would still have a positive refcount. */
	AST_RWLIST_WRLOCK(&sca_sub->subs);
	AST_LIST_TRAVERSE(&sca_sub->subs, sipsub, entry) {
		subs++;
	}
	AST_RWLIST_UNLOCK(&sca_sub->subs);

	ast_taskprocessor_unreference(sca_sub->serializer);

	ast_free(sca_sub->endpoint);

	if (subs) {
		ast_log(LOG_WARNING, "SCA subscription being destroyed but still has %d SIP subscriptions? This is bad...\n", subs);
	}
	/* If this happens, we'll probably crash anyways in the future. May as well do it now. */
	ast_assert(subs == 0);
}

static struct ast_datastore_info ds_info = { };
static const char ds_name[] = "SCA datastore";

/*!
 * \internal
 * \brief Add a datastore for exten sca_subscription.
 *
 * Adds the sca_subscription wrapper object to a datastore so it can be retrieved
 * later based upon its association with the ast_sip_subscription.
 */
static int add_datastore(struct ast_sip_subscription *sip_sub, struct sca_subscription *sca_sub)
{
	RAII_VAR(struct ast_datastore *, datastore, ast_sip_subscription_alloc_datastore(&ds_info, ds_name), ao2_cleanup);

	if (!datastore) {
		return -1;
	}

	datastore->data = sca_sub;
	ast_sip_subscription_add_datastore(sip_sub, datastore);
	ao2_ref(sca_sub, +1);
	return 0;
}

/*!
 * \internal
 * \brief Get the sca_subscription object associated with the given ast_sip_subscription in the datastore.
 */
static struct sca_subscription *get_sca_sub(struct ast_sip_subscription *sub)
{
	RAII_VAR(struct ast_datastore *, datastore, ast_sip_subscription_get_datastore(sub, ds_name), ao2_cleanup);
	return datastore ? datastore->data : NULL;
}

/*! \note Must ao2_ref -1 when finished with subscription */
static struct sca_subscription *sca_sub_by_endpoint(const char *endpoint)
{
	struct sca_subscription *sca_sub = NULL;
	struct subscription_item *subitem;
	AST_RWLIST_RDLOCK(&sublist);
	AST_LIST_TRAVERSE(&sublist, subitem, entry) {
		if (!strcmp(subitem->endpoint, endpoint)) {
			sca_sub = subitem->sub;
			break;
		}
	}
	if (sca_sub) {
		ao2_ref(sca_sub, +1);
	}
	AST_RWLIST_UNLOCK(&sublist);
	return sca_sub;
}

static int sca_add_sip_sub(struct ast_sip_subscription *sip_sub, struct sca_subscription *sca_sub, const char *endpoint_name)
{
	int subs = 0;
	struct sca_sip_sub *sipsub;

	sipsub = ast_calloc(1, sizeof(*sipsub));
	if (!sipsub) {
		ast_log(LOG_WARNING, "Failed to add subscription to SCA subscription list\n");
		return -1;
	}
	/* We need to keep track of what ast_sip_subscription's are using this sca_sub. */
	sipsub->sub = sip_sub;
	AST_RWLIST_WRLOCK(&sca_sub->subs);
	AST_RWLIST_INSERT_HEAD(&sca_sub->subs, sipsub, entry);
	AST_LIST_TRAVERSE(&sca_sub->subs, sipsub, entry) {
		subs++;
	}
	AST_RWLIST_UNLOCK(&sca_sub->subs);

	ast_debug(1, "Now have %d subscription%s for SCA endpoint %s (SCA refcount: %d)\n", subs, ESS(subs), endpoint_name, ao2_count(sca_sub));
	return 0;
}

/*!
 * \internal
 * \brief Allocates an sca_subscription object.
 *
 * Creates the underlying SIP subscription for the given request. First makes
 * sure that there are registered handler and provider objects available.
 */
static struct sca_subscription *sca_subscription_alloc(struct ast_sip_subscription *sip_sub, struct ast_sip_endpoint *endpoint)
{
	struct sca_subscription *sca_sub;
	struct subscription_item *subitem;
	char *endpoint_dup;
	const char *endpoint_name = ast_sorcery_object_get_id(endpoint);

	/* Each contact for an endpoint will have its own subscription.
	 * Therefore, to make sure all of these contacts share the same state,
	 * we should only have 1 sca_sub per endpoint (each will still have its own ast_sip_subscription)
	 * So, check if this endpoint already has a subscription, and if so, use the same one. */

	sca_sub = sca_sub_by_endpoint(endpoint_name);
	if (sca_sub) {
		/* This has been ref bumped, so just return it. */
		ast_debug(1, "Endpoint %s already has an SCA subscription, reusing the subscription data\n", endpoint_name);

		/* The datastore is what allows us to call get_sca_sub on an ast_sip_subscription to retrieve the appropriate sca_sub */
		if (add_datastore(sip_sub, sca_sub)) {
			ast_log(LOG_WARNING, "Unable to add to subscription datastore.\n");
			ao2_cleanup(sca_sub);
			return NULL;
		}
		sca_add_sip_sub(sip_sub, sca_sub, endpoint_name);
		return sca_sub;
	}

	/* No existing subscriptions for this SCA, make a new one. */
	subitem = ast_calloc(1, sizeof(*subitem));
	if (!subitem) {
		return NULL;
	}
	subitem->endpoint = ast_strdup(endpoint_name);
	if (!subitem->endpoint) {
		ast_free(subitem);
		return NULL;
	}

	/* The subscription needs a copy of its name. */
	endpoint_dup = ast_strdup(endpoint_name);
	if (!endpoint_dup) {
		ast_free(subitem);
		return NULL;
	}

	/* ao2_alloc will also increment the refcount, so no need to manually +1 it */
	sca_sub = ao2_alloc(sizeof(*sca_sub), sca_subscription_destructor);
	if (!sca_sub) {
		return NULL;
	}

	ast_debug(2, "Allocating first %s subscription for %s\n", CALLINFO_EVENT, endpoint_name);

	sca_sub->endpoint = endpoint_dup;
	if (add_datastore(sip_sub, sca_sub)) {
		ast_log(LOG_WARNING, "Unable to add to subscription datastore.\n");
		ast_free(subitem);
		ao2_cleanup(sca_sub);
		return NULL;
	}

	ast_mutex_init(&sca_sub->lock);

	/* We keep our own reference to the serializer as there is no guarantee in state_changed
	 * that the subscription tree is still valid when it is called. This can occur when
	 * the subscription is terminated at around the same time as the state_changed
	 * callback is invoked. */
	sca_sub->serializer = ao2_bump(ast_sip_subscription_get_serializer(sip_sub));

	sca_add_sip_sub(sip_sub, sca_sub, endpoint_name);

	/* Insert into the linked list. */
	subitem->sub = sca_sub;
	AST_RWLIST_WRLOCK(&sublist);
	AST_RWLIST_INSERT_HEAD(&sublist, subitem, entry);
	AST_RWLIST_UNLOCK(&sublist);

	return sca_sub;
}

struct notify_task_data {
	struct ast_sip_sca_data sca_data;
	struct sca_subscription *sca_sub;
	int terminate;
};

static void notify_task_data_destructor(void *obj)
{
	struct notify_task_data *task_data = obj;
	ao2_ref(task_data->sca_sub, -1);
}

static struct notify_task_data *alloc_notify_task_data(struct sca_subscription *sca_sub)
{
	struct notify_task_data *task_data = ao2_alloc(sizeof(*task_data), notify_task_data_destructor);

	if (!task_data) {
		ast_log(LOG_WARNING, "Unable to create notify task data\n");
		return NULL;
	}

	task_data->sca_sub = sca_sub;
	ao2_ref(task_data->sca_sub, +1);
	ast_debug(2, "Allocating notify task\n");

	return task_data;
}

static int on_tdata(struct ast_sip_subscription *sub, pjsip_tx_data *tdata)
{
	int i;
	struct ast_str *str;
	char domain[256];
	struct sca_subscription *sca_sub = get_sca_sub(sub);

	if (!sca_sub) {
		ast_log(LOG_WARNING, "No SCA subscription?\n");
		return -1;
	}

	/* Find the domain to use. */
	ast_sip_subscription_get_local_uri(sub, domain, sizeof(domain));

	/* Length will vary depending on number of appearances, so use an ast_str instead of fixed size buffer to be extra sure. */
	str = ast_str_create(128);
	if (!str) {
		return -1;
	}

	/* General format of Call-Info header is comma-separated list of
	 * "<sip:DOMAIN>;appearance-index=N;appearance-state=STATE"
	 * domain already has a sip: in it, so not needed in the <%s> */

	/* For each appearance. */
	for (i = 0; i < MAX_APPEARANCES; i++) {
		enum sca_appearance_state state = sca_sub->appearances[i].state;
		if (state == SCA_APPEARANCE_NONE) {
			break; /* There are no more appearances for this subscription. */
		}
		/* If it's idle, skip it, the catch all below will encompass these. This will reduce the size of the header by not repeating idles. */
		if (state == SCA_APPEARANCE_IDLE) {
			continue;
		}
		ast_str_append(&str, 0, "%s<%s>;appearance-index=%d;" "%s%s%s" "appearance-state=%s",
			ast_str_strlen(str) ? "," : "", domain,
			APPEARANCE_REAL_INDEX(i),
			/* If we have an appearance-uri, provide it.
			 * It's not really clear to me what this accomplishes, but it's in the spec and Broadworks does this... */
			sca_sub->appearances[i].uri ? "appearance-uri=" : "", S_OR(sca_sub->appearances[i].uri, ""), sca_sub->appearances[i].uri ? ";" : "",
			sca_appearance_state_str(state));
	}

	/* If we didn't have any appearances that have been used, then everything is idle.
	 * And, since we don't really know how many appearances exist necessarily, always
	 * finish with the wildcard "everything else is idle". */
	ast_str_append(&str, 0, "%s<%s>;appearance-index=%s;appearance-state=%s",
		ast_str_strlen(str) ? "," : "", domain, "*", sca_appearance_state_str(SCA_APPEARANCE_IDLE));

	/* Add the final Call-Info header to the response. */
	ast_sip_add_header(tdata, "Call-Info", ast_str_buffer(str));
	ast_debug(5, "Full Call-Info header: %s\n", ast_str_buffer(str));

	ast_free(str);
	return 0;
}

/*! \todo BUGBUG occasionally on startup, we crash due to restoring cached subscriptins
 * It only happens once at a time, the next startup will not crash.
 * Running "database deltree subscription_persistence" before restarting seems to prevent this.
 Always like this, PJSIP_EVSUB_STATE_NULL:
 
#2  0x00007f0f82fdc40f in __assert_fail_base (fmt=0x7f0f831546a8 "%s%s%s:%u: %s%sAssertion `%s' failed.n%n", assertion=0x7f0f83d76788 "sub->dst_state!=PJSIP_>
        str = 0x7f0f6c042270 ""
        total = 4096
#3  0x00007f0f82feb662 in __GI___assert_fail (assertion=0x7f0f83d76788 "sub->dst_state!=PJSIP_EVSUB_STATE_NULL", file=0x7f0f83d76282 "../src/pjsip-simple/evs>
#4  0x00007f0f83c7e43f in pjsip_evsub_send_request (sub=0x7f0f6c023718, tdata=0x7f0f6c02e058) at ../src/pjsip-simple/evsub.c:1402
        status = 0
        __PRETTY_FUNCTION__ = "pjsip_evsub_send_request"
#5  0x00007f0f3fe81663 in sip_subscription_send_request (sub_tree=0x7f0f6c020af0, tdata=0x7f0f6c02e058) at res_pjsip_pubsub.c:2064
        res = 0
        __FUNCTION__ = "sip_subscription_send_request"

XXX: This bug appears to have been in pjproject (resolved by a Dec 2022 commit) and no longer happens.
This handling and comments, etc. can now all be restored to normal.
*/

#if 1
static int notify_task(void *obj)
{
	int notified = 0;
	RAII_VAR(struct notify_task_data *, task_data, obj, ao2_cleanup);
	struct ast_sip_body_data data = {
		.body_type = AST_SIP_SCA_DATA,
		.body_data = &task_data->sca_data,
	};
	struct sca_sip_sub *sipsub;

	/* Pool allocation has to happen here so that we allocate within a PJLIB thread */

	/* The sca_sub could have multiple sip_sub's that it's using, not just the primary.
	 * So we need to send NOTIFYs to all the contacts of the endpoint, essentially,
	 * so that all subscriptions using the underlying sca_sub are updated with the new state. */

	/* XXX Should the AST_LIST_TRAVERSE happen in send_update, with individual task_data's
	 * dispatched to each ast_sip_subscription_notify? This way does work without any issues
	 * so maybe it's more efficient.
	 * If we decide to do this, then we can do #if 0 instead of #if 1 above.
	 */

	AST_RWLIST_RDLOCK(&task_data->sca_sub->subs);
	AST_LIST_TRAVERSE(&task_data->sca_sub->subs, sipsub, entry) {
		/* The subscription was terminated while notify_task was in queue.
		 * Terminated subscriptions are no longer associated with a valid tree, and sending
		 * NOTIFY messages on a subscription which has already been terminated won't work. */
		if (ast_sip_subscription_is_terminated(sipsub->sub)) {
			continue;
		}
		ast_sip_subscription_notify(sipsub->sub, &data, task_data->terminate);
		notified++;
	}
	AST_RWLIST_UNLOCK(&task_data->sca_sub->subs);

	ast_debug(2, "Sent NOTIFY to %d contact%s\n", notified, ESS(notified));
	return 0;
}

static int send_update(struct sca_subscription *sca_sub)
{
	struct notify_task_data *task_data;

	if (!(task_data = alloc_notify_task_data(sca_sub))) {
		return -1;
	}

	ast_debug(1, "Doing NOTIFY\n");

	/* Initialize the task_data's sca_data with any data the body generator might need. */
	memset(&task_data->sca_data, 0, sizeof(task_data->sca_data));

	/*! \todo This is a stub for providing Call Parking information.
	 * Conceivably we would need to have a dialplan function that tells us that a call is parked.
	 * We can then store that on the sca_subscription and then find it here.
	 * Alternately, try to integrate with res_parking in some way? Not sure how though,
	 * since parking spots in Asterisk don't correspond 1:1 to "extensions" in the sense of numbers.
	 * (However, it may be reasonable to assume that in a 1:1 arrangement, Caller ID = parking extension.)
	 * We at least need some way to determine that a parked call is parked against us or for us...
	 * The body generator will then generate the appropriate park body in the NOTIFY.
	 * XXX What we need here are the number and name of the parked call.
	 */
	if (0) {
		/* If there's something parked, then indicate so. */
		task_data->sca_data.park_enabled = 1;
		snprintf(task_data->sca_data.park_uri, sizeof(task_data->sca_data.park_uri), "sip:C@as.foo.com;user=phone");
		snprintf(task_data->sca_data.park_display, sizeof(task_data->sca_data.park_display), "C. Parked");
	}

	if (ast_sip_push_task(task_data->sca_sub->serializer, notify_task, task_data)) {
		ao2_cleanup(task_data);
		return -1;
	}
	return 0;
}
#else
static int notify_task(void *obj)
{
	RAII_VAR(struct notify_task_data *, task_data, obj, ao2_cleanup);
	struct ast_sip_body_data data = {
		.body_type = AST_SIP_SCA_DATA,
		.body_data = &task_data->sca_data,
	};

	/* The subscription was terminated while notify_task was in queue.
	 * Terminated subscriptions are no longer associated with a valid tree, and sending
	 * NOTIFY messages on a subscription which has already been terminated won't work. */
	if (ast_sip_subscription_is_terminated(task_data->sip_sub)) {
		ast_debug(1, "Subscription is already terminated\n");
		return 0;
	}
	/* Pool allocation has to happen here so that we allocate within a PJLIB thread */
	ast_sip_subscription_notify(task_data->sip_sub, &data, task_data->terminate);

	return 0;
}

static int send_update(struct sca_subscription *sca_sub)
{
	struct notify_task_data *task_data;
	struct sca_sip_sub *sipsub;
	int notified = 0;

	/* The sca_sub could have multiple sip_sub's that it's using, not just the primary.
	 * So we need to send NOTIFYs to all the contacts of the endpoint, essentially,
	 * so that all subscriptions using the underlying sca_sub are updated with the new state. */
	AST_RWLIST_RDLOCK(&sca_sub->subs);
	AST_LIST_TRAVERSE(&sca_sub->subs, sipsub, entry) {
		if (!(task_data = alloc_notify_task_data(sca_sub))) {
			continue;
		}

		/* Initialize the task_data's sca_data with any data the body generator might need. */
		memset(&task_data->sca_data, 0, sizeof(task_data->sca_data));

		task_data->sip_sub = sipsub->sub; /* A particular SIP subscription for this SCA. */

		/*! \todo This is a stub for providing Call Parking information.
		 * Conceivably we would need to have a dialplan function that tells us that a call is parked.
		 * We can then store that on the sca_subscription and then find it here.
		 * Alternately, try to integrate with res_parking in some way? Not sure how though,
		 * since parking spots in Asterisk don't correspond 1:1 to "extensions" in the sense of numbers.
		 * (However, it may be reasonable to assume that in a 1:1 arrangement, Caller ID = parking extension.)
		 * We at least need some way to determine that a parked call is parked against us or for us...
		 * The body generator will then generate the appropriate park body in the NOTIFY.
		 * XXX What we need here are the number and name of the parked call.
		 */
		if (0) {
			/* If there's something parked, then indicate so. */
			task_data->sca_data.park_enabled = 1;
			snprintf(task_data->sca_data.park_uri, sizeof(task_data->sca_data.park_uri), "sip:C@as.foo.com;user=phone");
			snprintf(task_data->sca_data.park_display, sizeof(task_data->sca_data.park_display), "C. Parked");
		}

		if (ast_sip_push_task(task_data->sca_sub->serializer, notify_task, task_data)) {
			ao2_cleanup(task_data);
			continue;
		}
		notified++;
	}
	AST_RWLIST_UNLOCK(&sca_sub->subs);

	ast_debug(2, "Sent NOTIFY to %d contact%s\n", notified, ESS(notified));
	return 0;
}
#endif

static char *sip_subscription_get_callid(struct ast_sip_subscription *sub)
{
	pjsip_cid_hdr *cid_hdr;
	char *callid_dup;
	pj_str_t *call_id;

	cid_hdr = ast_sip_subscription_get_header(sub, "Call-ID");
	if (!cid_hdr) {
		ast_log(LOG_WARNING, "Missing Call-ID header?\n");
		return NULL;
	}
	call_id = &cid_hdr->id;

	callid_dup = ast_malloc(pj_strlen(call_id) + 1);
	if (!callid_dup) {
		ast_log(LOG_WARNING, "Failed to duplicate Call-ID\n");
		return NULL;
	}
	ast_copy_pj_str(callid_dup, call_id, pj_strlen(call_id) + 1);
	return callid_dup;
}

#define APPEARANCE_STATE_AVAILABLE(a) ((a == SCA_APPEARANCE_NONE || a == SCA_APPEARANCE_IDLE))
#define APPEARANCE_AVAILABLE(a) ((a.state == SCA_APPEARANCE_NONE || a.state == SCA_APPEARANCE_IDLE))
#define APPEARANCE_STATE_HELD(a) ((a == SCA_APPEARANCE_HELD || a == SCA_APPEARANCE_HELD_PRIVATE || a == SCA_APPEARANCE_BRIDGE_HELD))
#define APPEARANCE_HELD(a) ((a.state == SCA_APPEARANCE_HELD || a.state == SCA_APPEARANCE_HELD_PRIVATE || a.state == SCA_APPEARANCE_BRIDGE_HELD))

#define APPEARANCE_STATE_ACTIVE(a) ((a == SCA_APPEARANCE_ACTIVE || a == SCA_APPEARANCE_BRIDGE_ACTIVE || a == SCA_APPEARANCE_BRIDGE_HELD))
#define APPEARANCE_STATE_MULTI_ACTIVE(a) ((a == SCA_APPEARANCE_BRIDGE_ACTIVE || a == SCA_APPEARANCE_BRIDGE_HELD))

/*! \note Don't forget to set to NULL if needed when using this macro */
#define FREE_IF_EXISTS(x) if (x) { ast_free(x); }

static inline int sca_session_count(struct sca_appearance *appearance)
{
	struct sca_session *s;
	int sessioncount = 0;

	AST_RWLIST_RDLOCK(&appearance->sessionlist);
	AST_LIST_TRAVERSE(&appearance->sessionlist, s, entry) {
		sessioncount++;
	}
	AST_RWLIST_UNLOCK(&appearance->sessionlist);

	return sessioncount;
}

static inline int sca_channel_count(struct sca_appearance *appearance)
{
	struct sca_channel *c;
	int chancount = 0;

	AST_RWLIST_RDLOCK(&appearance->chanlist);
	AST_LIST_TRAVERSE(&appearance->chanlist, c, entry) {
		chancount++;
	}
	AST_RWLIST_UNLOCK(&appearance->chanlist);

	return chancount;
}

static inline int sca_session_exists(struct sca_appearance *appearance, struct ast_sip_session *session, int alreadylocked)
{
	struct sca_session *s;

	if (!alreadylocked) {
		AST_RWLIST_RDLOCK(&appearance->sessionlist);
	}
	AST_LIST_TRAVERSE(&appearance->sessionlist, s, entry) {
		if (s->session == session) {
			break;
		}
	}
	if (!alreadylocked) {
		AST_RWLIST_UNLOCK(&appearance->sessionlist);
	}
	return s ? 1 : 0;
}

static inline int sca_channel_exists(struct sca_appearance *appearance, struct ast_channel *chan, int alreadylocked)
{
	struct sca_channel *c;

	if (!alreadylocked) {
		AST_RWLIST_RDLOCK(&appearance->chanlist);
	}
	AST_LIST_TRAVERSE(&appearance->chanlist, c, entry) {
		if (c->chan == chan) {
			break;
		}
	}
	if (!alreadylocked) {
		AST_RWLIST_UNLOCK(&appearance->chanlist);
	}
	return c ? 1 : 0;
}

/*! \brief Retrieve the current appearance for an ast_sip_session */
static int sca_appearance_by_session(struct sca_subscription *sca_sub, struct ast_sip_session *session)
{
	int appearance = 0;
	int i;

	ast_mutex_lock(&sca_sub->lock);
	for (i = 0; i < MAX_APPEARANCES; i++) {
		if (sca_sub->appearances[i].state == SCA_APPEARANCE_NONE) {
			break;
		}
		if (sca_session_exists(&sca_sub->appearances[i], session, 0)) {
			appearance = APPEARANCE_REAL_INDEX(i);
			break;
		}
	}
	ast_mutex_unlock(&sca_sub->lock);

	return appearance;
}

static int sca_add_session(struct sca_appearance *appearance, struct ast_sip_session *session)
{
	int total = 0;
	struct sca_session *s;

	if (!session) {
		return 0;
	}

	AST_RWLIST_WRLOCK(&appearance->sessionlist);
	/* Only insert if it's not already in the list. */
	if (sca_session_exists(appearance, session, 1)) {
		AST_RWLIST_UNLOCK(&appearance->sessionlist);
		ast_debug(5, "Session %p already exists, not adding to appearance list\n", session);
		return 0;
	}

	s = ast_calloc(1, sizeof(*s));
	if (!s) {
		return -1;
	}
	s->session = session;
	AST_RWLIST_INSERT_HEAD(&appearance->sessionlist, s, entry);

	/* Get the current count. */
	AST_LIST_TRAVERSE(&appearance->sessionlist, s, entry) {
		total++;
	}
	AST_RWLIST_UNLOCK(&appearance->sessionlist);

	ast_debug(5, "Added session %p to appearance list\n", session);
	/* In theory, the session count should never be higher than the use count. Probably a bug if it is, a session that wasn't removed when it should've been.
	 * It could be lower because both sessions and channels can contribute to the usecount. */
	if (total > appearance->usecount) {
		ast_log(LOG_WARNING, "Use count is %d but we have %d session%s?\n", appearance->usecount, total, ESS(total));
	}
	return 0;
}

static int sca_add_channel(struct sca_appearance *appearance, struct ast_channel *chan)
{
	int total = 0;
	struct sca_channel *c;

	if (!chan) {
		return 0;
	}

	AST_RWLIST_WRLOCK(&appearance->chanlist);
	/* Only insert if it's not already in the list. */
	if (sca_channel_exists(appearance, chan, 1)) {
		AST_RWLIST_UNLOCK(&appearance->chanlist);
		ast_debug(5, "Session %p already exists, not adding to appearance list\n", chan);
		return 0;
	}

	c = ast_calloc(1, sizeof(*c));
	if (!c) {
		return -1;
	}
	c->chan = chan;
	AST_RWLIST_INSERT_HEAD(&appearance->chanlist, c, entry);

	/* Get the current count. */
	AST_LIST_TRAVERSE(&appearance->chanlist, c, entry) {
		total++;
	}
	AST_RWLIST_UNLOCK(&appearance->chanlist);

	ast_debug(5, "Added channel %s to appearance list\n", ast_channel_name(chan));
	/* In theory, the session count should never be higher than the use count. Probably a bug if it is, a session that wasn't removed when it should've been.
	 * It could be lower because both sessions and channels can contribute to the usecount. */
	if (total > appearance->usecount) {
		ast_log(LOG_WARNING, "Use count is %d but we have %d channel%s?\n", appearance->usecount, total, ESS(total));
	}
	return 0;
}

static int sca_remove_sessions(struct sca_appearance *appearance)
{
	struct sca_session *s;

	AST_RWLIST_WRLOCK(&appearance->sessionlist);
	AST_RWLIST_TRAVERSE_SAFE_BEGIN(&appearance->sessionlist, s, entry) {
		AST_RWLIST_REMOVE_CURRENT(entry);
		ast_debug(5, "Removed session %p from appearance list\n", s->session);
		ast_free(s);
	}
	AST_RWLIST_TRAVERSE_SAFE_END;
	AST_RWLIST_UNLOCK(&appearance->sessionlist);
	return 0;
}

static int sca_remove_session(struct sca_appearance *appearance, struct ast_sip_session *session)
{
	struct sca_session *s;

	if (!session) {
		return 0;
	}

	AST_RWLIST_WRLOCK(&appearance->sessionlist);
	AST_RWLIST_TRAVERSE_SAFE_BEGIN(&appearance->sessionlist, s, entry) {
		if (s->session == session) {
			AST_RWLIST_REMOVE_CURRENT(entry);
			ast_free(s);
			ast_debug(5, "Removed session %p from appearance list\n", session);
			break;
		}
	}
	AST_RWLIST_TRAVERSE_SAFE_END;
	AST_RWLIST_UNLOCK(&appearance->sessionlist);
	return 0;
}

static int sca_remove_channel(struct sca_appearance *appearance, struct ast_channel *chan)
{
	struct sca_channel *c;

	if (!chan) {
		return 0;
	}

	AST_RWLIST_WRLOCK(&appearance->chanlist);
	AST_RWLIST_TRAVERSE_SAFE_BEGIN(&appearance->chanlist, c, entry) {
		if (c->chan == chan) {
			AST_RWLIST_REMOVE_CURRENT(entry);
			ast_free(c);
			/* Channel name will still be valid during datastore cleanup. */
			ast_debug(5, "Removed channel %s from appearance list\n", ast_channel_name(chan));
			break;
		}
	}
	AST_RWLIST_TRAVERSE_SAFE_END;
	AST_RWLIST_UNLOCK(&appearance->chanlist);
	return 0;
}

/*! \brief strcmp wrapper that tolerates NULL arguments */
static inline int safe_strcmp(const char *s1, const char *s2)
{
	if (!s1 || !s2) {
		/* For our purposes, treat NULL == NULL. This is for convenience where this function is actually used. */
		return !s1 && !s2 ? 0 : -1;
	}
	return strcmp(s1, s2);
}

static enum ast_device_state sca_to_ast_devstate(enum sca_appearance_state appearance_state)
{
	switch (appearance_state) {
	case SCA_APPEARANCE_IDLE:
		return AST_DEVICE_NOT_INUSE;
	case SCA_APPEARANCE_ALERTING:
		return AST_DEVICE_RINGING;
	case SCA_APPEARANCE_SEIZED:
		return AST_DEVICE_INUSE;
	case SCA_APPEARANCE_PROGRESSING:
	case SCA_APPEARANCE_ACTIVE:
	case SCA_APPEARANCE_BRIDGE_ACTIVE:
	case SCA_APPEARANCE_BRIDGE_HELD:
		return AST_DEVICE_BUSY;
	case SCA_APPEARANCE_HELD:
	case SCA_APPEARANCE_HELD_PRIVATE:
		return AST_DEVICE_ONHOLD;
	case SCA_APPEARANCE_NONE:
		/* This function should never be called with this value. */
		break;
	}
	return AST_DEVICE_INVALID;
}

/*! \brief Aggregate device state of all SCA appearances for an endpoint */
static enum ast_device_state sca_aggregate_state(struct sca_subscription *sca_sub)
{
	enum sca_appearance_state appearance_state;
	enum ast_device_state newstate, res = AST_DEVICE_NOT_INUSE;
	int i;

	for (i = 0; i < MAX_APPEARANCES; i++) {
		appearance_state = sca_sub->appearances[i].state;
		if (appearance_state == SCA_APPEARANCE_NONE) {
			break;
		}
		ast_debug(5, "Considering SCA appearance state %s (#%d)\n", sca_appearance_state_str(appearance_state), APPEARANCE_REAL_INDEX(i));
		/* Aggregate all. */
		newstate = sca_to_ast_devstate(appearance_state);
		if (res == AST_DEVICE_NOT_INUSE) {
			/* Anything trumps not in use. */
			ast_debug(7, "Changing from %s to %s\n", ast_devstate2str(res), ast_devstate2str(newstate));
			res = newstate;
		} else if (newstate == AST_DEVICE_BUSY) {
			/* If something is in use, that's our match. */
			ast_debug(7, "Changing from %s to %s\n", ast_devstate2str(res), ast_devstate2str(newstate));
			res = newstate;
			break;
		}
	}
	return res;
}

/*! \note sca_sub lock must be held when calling */
/*! \note It is always expected that this function succeeds at what it was asked to do, which is why it returns void. */
static void assign_appearance(struct sca_subscription *sca_sub, int appearance, enum sca_appearance_state newstate, enum sca_appearance_state requested,
	char *callid, char *contact, struct ast_sip_session *session, struct ast_channel *chan)
{
	int i;
	int was_idle = APPEARANCE_AVAILABLE(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)]);

	/* If it wasn't seized before, it is now... or it's just the same call updating its status */
	sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].laststate = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state;
	sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state = newstate;

	/* Update the core with the new device state, if it changed. */
	if (sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].laststate != newstate) {
		enum ast_device_state indiv, aggregate;
		indiv = sca_to_ast_devstate(newstate);
		aggregate = sca_aggregate_state(sca_sub);
		ast_debug(4, "Changing device state for %s, appearance %d = %s, aggregate = %s\n", sca_sub->endpoint, appearance, ast_devstate2str(indiv), ast_devstate2str(aggregate));
		ast_devstate_changed(indiv, AST_DEVSTATE_CACHABLE, "PJSIPSCA:%s_%d", sca_sub->endpoint, appearance); /* This specific appearance */
		ast_devstate_changed(aggregate, AST_DEVSTATE_CACHABLE, "PJSIPSCA:%s", sca_sub->endpoint); /* Aggregate for all appearances */
		if (newstate == SCA_APPEARANCE_SEIZED) {
			/* Keep track of the time at which the appearance was seized.
			 * Seizures are only valid for 30 seconds, after which time they expire. */
			sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].seizetime = time(NULL);
			/*! \todo Check appearances that are seized, either periodically or when doing stuff,
			 * and if seizetime is longer than 30s ago, change it back to idle and send a notify */
		}
	}

	/* Store the session or channel, if we don't already have it. Unless we're going idle, of course.
	 * We use the requested state because newstate is the new state of the appearance, now the new state of whatever phone triggered the request.
	 * We want to use that here because if a phone hung up and the appearance is still active, we shouldn't add the phone's session
	 * back to the session list. */
	if (requested != SCA_APPEARANCE_IDLE) {
		sca_add_session(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)], session); /* For real PJSIP SCA endpoints */
		sca_add_channel(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)], chan); /* For secondary (not necessarily PJSIP) endpoints */
	}

	/* Store the linked ID for reference, since this particular underlying session might disappear even while the linked ID remains a valid reference. */
	FREE_IF_EXISTS(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].linkedid);
	sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].linkedid = NULL;
	if (session && session->channel && newstate != SCA_APPEARANCE_IDLE) {
		/* Store the linked ID unless we're going idle... in which case why bother? */
		sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].linkedid = ast_strdup(ast_channel_linkedid(session->channel));
	}

	/* If there are any appearances at lower indices that are still SCA_APPEARANCE_NONE, change them to idle.
	 * (This can happen since an endpoint can choose any arbitrary appearance it wants from a line key)
	 * That way we properly continue array traversal until the last used appearance. */
	for (i = 0; i < APPEARANCE_ARRAY_INDEX(appearance); i++) {
		if (sca_sub->appearances[i].state == SCA_APPEARANCE_NONE) {
			ast_debug(2, "Changing as yet unused appearance %d from none to idle\n", i);
			sca_sub->appearances[i].state = SCA_APPEARANCE_IDLE;
		}
	}

	/* Store the Call ID and Contact for reference. */
	if (was_idle) {
		/* There shouldn't be any Call ID associated with it, if it's idle. If there is, throw a warning and free it so we don't leak memory.
		 * Actually, just assert, because it's probably unregistered memory and we'll crash anyways. */
		ast_assert(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].callid == NULL);
		ast_assert(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact == NULL);
		sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].callid = callid;
		sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact = contact;
	} else if (safe_strcmp(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].callid, callid) || safe_strcmp(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact, contact)) {
		/* If we're going idle, then don't call strcmp anywhere since callid and contact are NULL
		 * This doesn't include just SCA_APPEARANCE_IDLE, it could be other active states,
		 * because we'll intercept IDLE if needed and convert it something else if the appearance was multiactive.
		 * Therefore, just check explicitly for the existence of callid and contact.
		 */

		/* There was a Call ID, but the new Call ID is different... huh?
		 * Actually, if we change from seized to progressing, and several other circumstances, the Call IDs are different so this is expected. */
		if (safe_strcmp(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].callid, callid)) {
			ast_debug(1, "Oooh, old Call ID is %s, new Call ID is %s!\n", sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].callid, callid);
		}
		if (safe_strcmp(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact, contact)) {
			ast_debug(1, "Oooh, old contact is %s, new contact is %s!\n", sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact, contact);
		}

		/* Only get rid of if we're going idle, or we actually have updated info to replace.
		 * That way if the call is still up (previously multiactive) and somebody hung up, we don't NULL these out just yet.
		 * No risk of a memory leak since both callid and contact are NULL if we skip this.
		 */
		if (newstate == SCA_APPEARANCE_IDLE || callid) {
			FREE_IF_EXISTS(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].callid);
			if (newstate == SCA_APPEARANCE_IDLE) {
				/* Sometimes we get a Call ID even if we're going idle, which we're not going to store. */
				ast_free(callid);
				callid = NULL;
			}
			sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].callid = callid;
		}
		if (newstate == SCA_APPEARANCE_IDLE || contact) {
			FREE_IF_EXISTS(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact);
			sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact = contact;
		}
	} else {
		/* The caller of this function expects us to take care of callid and contact on success.
		 * If the Call ID and contact are still the same and we didn't replace them, then we don't need them and can safely just free them.
		 * And free them we must, because the caller of set_appearance expects the references handled on success, so if we don't free them now,
		 * we'll lose the references forever and leak memory.
		 */
		ast_debug(4, "Neither the Call ID nor the contact has changed\n");
		ast_free(callid);
		ast_free(contact);
		callid = contact = NULL;
	}
}

static inline int session_matches_linkedid(const char *linkedid, struct ast_sip_session *s)
{
	if (!s || ast_strlen_zero(linkedid)) {
		return 0;
	}
	if (!s->channel) {
		return 0;
	}
	ast_debug(6, "Comparing linked IDs: %s / %s\n", linkedid, ast_channel_linkedid(s->channel));
	if (strcmp(linkedid, ast_channel_linkedid(s->channel))) {
		return 0;
	}
	return 1;
}

/*! \note This MUST BE CALLED before sca_appearance_new_state is called to calculate the real newstate.
 * i.e. we want to see SCA_APPEARANCE_IDLE if a phone hangs up, even if the appearance won't actually transition to that state.
 * Likewise we want to see SCA_APPEARANCE_ACTIVE even if we're going to a multiactive (_BRIDGE_) state.
 */
static inline int update_use_count(struct sca_subscription *sca_sub, int appearance, enum sca_appearance_state newstate)
{
	/* Update our use counts. */
	int newusecount, oldusecount;
	enum sca_appearance_state oldstate = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state;

	oldusecount = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount;

	if (APPEARANCE_STATE_AVAILABLE(oldstate) && !APPEARANCE_STATE_AVAILABLE(newstate)) {
		/* Went from not in use to being in use. */
		sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount += 1;
		ast_debug(6, "%s\n", sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount == 1 ? "Went from not in use to in use" : "Somebody else joined");
	} else if (oldstate == SCA_APPEARANCE_ALERTING && newstate == SCA_APPEARANCE_ALERTING) {
		/* Another fork of the call (separate INVITE) for same appearance. Bump the use count. */
		sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount += 1;
		ast_debug(6, "Another contact is being alerted\n");
	} else if (0 && oldstate == SCA_APPEARANCE_SEIZED && !APPEARANCE_STATE_AVAILABLE(newstate)) {
		/* When we change from seized to in use (i.e. INVITE received for outgoing call),
		 * then the subscription isn't necessarily terminated so we should manually decrement. */
		sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount -= 1;
		ast_debug(6, "Went from seized to other in use\n");
	} else if (!APPEARANCE_STATE_AVAILABLE(oldstate) && APPEARANCE_STATE_AVAILABLE(newstate)) {
		/* Went from being in use to not in use.
		 * We have to be careful here because this should only be allowed to change state if we actually reach 0. */
		sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount -= 1;
		ast_debug(6, "%s\n", sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount == 0 ? "Went from in use to not in use" : "Somebody left, still in use");
	} else {
		/* else, it was either available -> available or unavailable -> unavailable... don't really care. */
		ast_debug(6, "No meaningful change\n");
	}

	/* Return new use count. */
	if (sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount < 0) {
		ast_log(LOG_WARNING, "Use count hit went from %d -> %d?\n", oldusecount, sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount);
		sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount = 0;
	}
	newusecount = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount;
	ast_debug(5, "Use count for appearance %d: %d -> %d\n", appearance, oldusecount, newusecount);
	return newusecount;
}

static int save_appearance_uri(struct sca_appearance *appearance, struct pjsip_msg *msg)
{
	char cid_name[AST_CHANNEL_NAME];
	char cid_num[AST_CHANNEL_NAME * 2];
	char full_appearance_uri[AST_CHANNEL_NAME * 3 + 10];
	char *str;
	int cid_num_len;
	pjsip_name_addr *id_name_addr;
	pjsip_uri *uri;

	/* For incoming (alerting) calls, use the From header. For outgoing (progressing) calls, use the To header. */
	pjsip_fromto_hdr *from = pjsip_msg_find_hdr(msg, appearance->state == SCA_APPEARANCE_ALERTING ? PJSIP_H_FROM : PJSIP_H_TO, msg->hdr.next);

	if (!from) {
		return -1;
	}

	id_name_addr = (pjsip_name_addr *) from->uri;
	ast_copy_pj_str(cid_name, &id_name_addr->display, sizeof(cid_name));

	uri = pjsip_uri_get_uri(from->uri);
	if (!uri) {
		return -1;
	}

	/* We want the full URI, not just the user or host part. */
	cid_num_len = pjsip_uri_print(PJSIP_URI_IN_FROMTO_HDR, uri, cid_num, sizeof(cid_num));
	cid_num[cid_num_len] = '\0';

	/* Format is "<NAME><URI>", where <NAME> is also enclosed (and properly escaped) in quotes
	 * Since we're snprintf'ing into a string, we also need to escape the first/last quotes,
	 * so don't get that mixed up.
	 */

	if (!ast_strlen_zero(cid_name)) {
		snprintf(full_appearance_uri, sizeof(full_appearance_uri), "\"\\\"%s\\\"<%s>\"", cid_name, cid_num);
	} else {
		/* If there's nothing to escape, then don't just include "" for the name */
		snprintf(full_appearance_uri, sizeof(full_appearance_uri), "\"<%s>\"", cid_num);
	}
	ast_debug(3, "appearance-uri: %s\n", full_appearance_uri);

	str = ast_strdup(full_appearance_uri);
	if (appearance->uri) {
		ast_free(appearance->uri);
	}
	appearance->uri = str;
	return 0;
}

#define SESSION_CHANNEL(s) (s ? s->channel ? ast_channel_name(s->channel) : "(no channel)" : "(no session)")
#define SESSION_LINKEDID(s) (s ? s->channel ? ast_channel_linkedid(s->channel) : "(no channel)" : "(no session)")

#define set_appearance(sca_sub, appearance, newstate, callid, contact, session) set_appearance_full(NULL, sca_sub, appearance, newstate, callid, contact, session, 0)
#define set_appearance_force(sca_sub, appearance, newstate, callid, contact, session) set_appearance_full(NULL, sca_sub, appearance, newstate, callid, contact, session, 1)
#define set_appearance_cust(chan, sca_sub, appearance, newstate) set_appearance_full(chan, sca_sub, appearance, newstate, NULL, NULL, NULL, 0)
#define set_appearance_cust_force(chan, sca_sub, appearance, newstate) set_appearance_full(chan, sca_sub, appearance, newstate, NULL, NULL, NULL, 1)

/*! \brief Calculate the actual new state */
static inline enum sca_appearance_state sca_appearance_new_state(enum sca_appearance_state oldstate, enum sca_appearance_state newstate, int newusecount)
{
	enum sca_appearance_state orignewstate = newstate;

	ast_debug(4, "Computing real new state: was %s, want %s, use count now %d\n",
		sca_appearance_state_str(oldstate), sca_appearance_state_str(newstate), newusecount);

	/* If there's only one left, then we should go from SCA_APPEARANCE_BRIDGE_ACTIVE or SCA_APPEARANCE_BRIDGE_HELD
	 * to SCA_APPEARANCE_ACTIVE or SCA_APPEARANCE_HELD, respectively.
	 * Calling assign_appearance also ensures we don't leak memory (see note about 30 lines below)
	 */
	if (newstate == SCA_APPEARANCE_IDLE) {
		if (newusecount > 1) {
			/* Basically newstate = oldstate, but to make this explicit:
			 * SCA_APPEARANCE_BRIDGE_ACTIVE -> SCA_APPEARANCE_BRIDGE_ACTIVE
			 * SCA_APPEARANCE_BRIDGE_HELD -> SCA_APPEARANCE_BRIDGE_HELD
			 */
			newstate = oldstate == SCA_APPEARANCE_BRIDGE_HELD ? SCA_APPEARANCE_BRIDGE_HELD : SCA_APPEARANCE_ACTIVE;
		} else if (newusecount == 1) {
			/* When we get back to 1, basically the state loses the _BRIDGE_ part.
			 * SCA_APPEARANCE_BRIDGE_ACTIVE -> SCA_APPEARANCE_ACTIVE
			 * SCA_APPEARANCE_BRIDGE_HELD -> SCA_APPEARANCE_HELD
			 */
			newstate = oldstate == SCA_APPEARANCE_BRIDGE_HELD ? SCA_APPEARANCE_HELD : SCA_APPEARANCE_ACTIVE;
		}
	} else if (newstate == SCA_APPEARANCE_ACTIVE) {
		/*! \note usecount will be for 2 for unhold, not 1, so > 2 for bridge held */
		if (newusecount > 2) {
			/* Only transitions possible. We can't go from HELD to BRIDGE_ACTIVE. */
			/* Go to BRIDGE_HELD if we were held, so that if somebody drops, we go back to HELD instead of ACTIVE. */
			newstate = oldstate == SCA_APPEARANCE_HELD ? SCA_APPEARANCE_BRIDGE_HELD : SCA_APPEARANCE_BRIDGE_ACTIVE;
		}
	} else if (newstate == SCA_APPEARANCE_HELD) {
		if (newusecount > 1) {
			newstate = SCA_APPEARANCE_BRIDGE_HELD;
		}
	}
	if (orignewstate != newstate) {
		ast_debug(2, "Overriding state transition: requested %s, granted %s\n", sca_appearance_state_str(orignewstate), sca_appearance_state_str(newstate));
	}
	return newstate;
}

static enum sca_appearance_state sca_appearance_new_state_helper(struct sca_appearance *appearance, enum sca_appearance_state oldstate, enum sca_appearance_state newstate, int newusecount, struct ast_sip_session *session)
{
	if (newstate == SCA_APPEARANCE_IDLE) {
		if (newusecount > 0) {
			ast_debug(3, "I think this appearance is still in use, ignoring idle transition\n");
			/* If there are still other contacts using the SCA appearance, leave it alone.
			 * This will frequently happen right on answer of an incoming call to the phone,
			 * when a CANCEL is sent to the other contacts. */
			sca_remove_session(appearance, session);
			/* Don't call sca_remove_channel, the datastore destructor will call that. */
			newstate = sca_appearance_new_state(oldstate, newstate, newusecount);
		} else {
			/* Clean up information about the call we don't need anymore. */
			FREE_IF_EXISTS(appearance->callid);
			FREE_IF_EXISTS(appearance->contact);
			FREE_IF_EXISTS(appearance->linkedid);
			FREE_IF_EXISTS(appearance->uri);
			appearance->callid = NULL;
			appearance->contact = NULL;
			appearance->linkedid = NULL;
			appearance->uri = NULL;
			/* Remove ALL sessions, if any are left. */
			sca_remove_sessions(appearance);
			/* Channels should definitely remove themselves, we'll leave them alone. */
			/* No need to call sca_appearance_new_state, if nobody is left, it's definitely going idle. */
		}
	} else {
		newstate = sca_appearance_new_state(oldstate, newstate, newusecount);
	}
	return newstate;
}

static inline void ensure_state_integrity(struct sca_subscription *sca_sub, int appearance)
{
	/* We haven't updated state yet, so state is really the old state, not the new state (and laststate would be the super old state) */
	enum sca_appearance_state oldstate = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state;
	int usecount = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount;

	/* Simple check to make sure we weren't in a state we shouldn't have been in. */
	/* You might think that this should be usecount > 1, since if > 1, then we should be bridge-active or bridge-held.
	 * However, we increment the usecount before we change the state when we're multiactive,
	 * so usecount could be 2 and we just haven't quite yet changed from active to bridge-active. So don't warn in that case.
	 * However, if we're going from 2 to 3 (or any higher number), then we should definitely be bridge-active or bridge-held. */

	if (oldstate == SCA_APPEARANCE_ALERTING && usecount >= 1) {
		return; /* We rang multiple phones, that's fine. */
	}

	if (usecount > 2 && !APPEARANCE_STATE_MULTI_ACTIVE(oldstate) && !APPEARANCE_STATE_ACTIVE(oldstate)) {
		/* If usecount > 1, then we must be SCA_APPEARANCE_BRIDGE_ACTIVE or SCA_APPEARANCE_BRIDGE_HELD, unless we were just held and temporarily transitioning.
		 * If not, we messed up our bookkeeping somewhere.
		 * In theory the state should not be purely "ACTIVE", but if we ring multiple phones and somebody answers, the cancelled legs will all abort
		 * and so in the meantime our state might temporarily be "active" with a usecount > 1.
		 */
		if (oldstate != SCA_APPEARANCE_HELD) {
			ast_log(LOG_WARNING, "Use count for appearance %d was %d, but state was %s?\n",
				appearance, sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount, sca_appearance_state_str(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state));
		}
	}
}

/*! \retval -1 if the new state should be denied, 0 if we're okay */
static inline int ensure_new_state_integrity(struct sca_subscription *sca_sub, int appearance, enum sca_appearance_state newstate)
{
	/* Some basic sanity checks on the new state. */
	/* If we want to go on hold, then we must be active now. */
	if (APPEARANCE_STATE_HELD(newstate) && !APPEARANCE_STATE_ACTIVE(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state)) {
		/* Shouldn't happen, this would be a bug in the code. */
		ast_log(LOG_WARNING, "Hold transitions can only be initiated from an active call (appearance %d is %s)\n",
			appearance, sca_appearance_state_str(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state));
		return -1;
	}
	return 0;
}

static int set_appearance_full(struct ast_channel *chan, struct sca_subscription *sca_sub, int appearance, enum sca_appearance_state newstate, char *callid, char *contact, struct ast_sip_session *session, int force_allow)
{
	enum sca_appearance_state oldstate;
	enum sca_appearance_state requested_state = newstate;
	int newusecount, res = 0;

	if (appearance == -1) {
		ast_debug(3, "Checking if we can set some appearance to %s for Call ID %s / session %p / channel %s / linked ID %s\n",
			sca_appearance_state_str(newstate), S_OR(callid, ""), session, SESSION_CHANNEL(session), SESSION_LINKEDID(session));
	} else {
		if (appearance <= 0 || appearance >= MAX_APPEARANCES) {
			ast_log(LOG_WARNING, "Invalid appearance number: %d\n", appearance);
			return -1;
		}
		ast_debug(3, "Checking if we can set appearance %d to %s for Call ID %s / session %p / channel %s / linked ID %s\n",
			appearance, sca_appearance_state_str(newstate), callid, session, SESSION_CHANNEL(session), SESSION_LINKEDID(session));
	}

	/* This operation must be atomic, to prevent race conditions. */
	ast_mutex_lock(&sca_sub->lock);

	if (appearance == -1) { /* If appearance == -1, that's an indication we don't know the appearance number but want to update an appearance by some property of it. */
		int i;
		/* Change the appearance from seized to idle. */
		res = -1;
		for (i = 0; i < MAX_APPEARANCES; i++) {
			int match = 0;
			if (sca_sub->appearances[i].state == SCA_APPEARANCE_NONE) {
				break; /* There are no more appearances remaining. */
			}
			if (sca_sub->appearances[i].state == SCA_APPEARANCE_IDLE) {
				continue; /* No way this has a Call ID. */
			}
			/* We will generally always have a Call ID or a session.
			 * Match first by Call ID if possible, since that's an obvious quality.
			 * In the case of hangups, we'll try to fall back to the session.
			 * Finally, in the case of an outgoing answer (change from alerting to active), we will have to rely on comparing linked IDs. */
			if (!safe_strcmp(sca_sub->appearances[i].callid, callid)) {
				match = 1;
				ast_debug(5, "Match on Call ID: %s\n", callid);
			} else if (sca_session_exists(&sca_sub->appearances[i], session, 0)) {
				match = 1;
				ast_debug(5, "Match on session: %p\n", session);
			} else if (session_matches_linkedid(sca_sub->appearances[i].linkedid, session)) {
				match = 1;
				ast_debug(5, "Match on linked ID: %s\n", ast_channel_linkedid(session->channel));
			}
			/* We shouldn't transition to ACTIVE except from PROGRESSING or ALERTING */
			if (match && newstate == SCA_APPEARANCE_ACTIVE && sca_sub->appearances[i].state != SCA_APPEARANCE_PROGRESSING && sca_sub->appearances[i].state != SCA_APPEARANCE_ALERTING) {
				ast_debug(1, "Skipping match on state %s\n", sca_appearance_state_str(sca_sub->appearances[i].state));
				continue;
			}
			if (!match) {
				continue;
			}

			appearance = APPEARANCE_REAL_INDEX(i);
			ensure_state_integrity(sca_sub, appearance);
			if (ensure_new_state_integrity(sca_sub, appearance, newstate)) {
				break;
			}
			newusecount = update_use_count(sca_sub, appearance, newstate);
			oldstate = sca_sub->appearances[i].state;
			newstate = sca_appearance_new_state_helper(&sca_sub->appearances[i], oldstate, newstate, newusecount, session);
			res = 0;
			/* Don't ever just manually update state!
			 * Using assign_appearance ensures callid and contact are freed if not needed.
			 * We can't just ignore them as that would leak memory. */
			assign_appearance(sca_sub, appearance, newstate, requested_state, callid, contact, session, chan);
			ast_debug(1, "Updated appearance %d from %s to %s\n", APPEARANCE_REAL_INDEX(i), sca_appearance_state_str(oldstate), sca_appearance_state_str(newstate));
			break;
		}
		if (res) {
			/* XXX Sometimes this is legitimate, which is why it's no longer a WARNING, but we should refine this so we know if it's okay to have not found something or not */
			ast_debug(1, "Didn't find any appearances with Call ID %s (or session %p, or matching linked ID)\n", callid, session);
		}
	} else if (!APPEARANCE_AVAILABLE(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)]) && safe_strcmp(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact, contact) && !force_allow) {
		/* It's okay if the appearance is already seized, as long as it's the same contact doing so (which means we seized it)
		 * We can't always use the Call ID because the Call ID for the seizure is different from the one sent for the INVITE for a call.
		 * So if we were to use that, it would look like somebody else was trying to make a call when it may be the same phone.
		 * Note that while the Call ID will be unique, the contact, however, may not be. So only use the contact to verify, not for searching. */
		/* This appearance has already been seized by another contact, or is in use in some way. */
		/* This should NOT be a WARNING, because it's something that can legitimately happen from phones. We should deny the request and that is correct behavior, nothing wrong with it. */
		ast_debug(1, "Rejected transition of appearance %d, since it's %s (contact %s != %s)\n", appearance,
			sca_appearance_state_str(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state), sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact, contact);
		ast_verb(4, "Rejected transition of appearance %d, since it's %s\n", appearance,
			sca_appearance_state_str(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state));
		res = -1; /* Decline conflicts with a 480 Temporarily Unavailable. */
	} else {
		oldstate = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state;
		ensure_state_integrity(sca_sub, appearance);
		if (ensure_new_state_integrity(sca_sub, appearance, newstate)) {
			ast_mutex_unlock(&sca_sub->lock);
			return -1;
		}
		newusecount = update_use_count(sca_sub, appearance, newstate);
		newstate = sca_appearance_new_state_helper(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)], oldstate, newstate, newusecount, session);
		assign_appearance(sca_sub, appearance, newstate, requested_state, callid, contact, session, chan);
		ast_debug(1, "Updated appearance %d from %s to %s\n", appearance, sca_appearance_state_str(oldstate), sca_appearance_state_str(newstate));
	}
	ast_mutex_unlock(&sca_sub->lock);

	return res;
}

/*! \brief Get the appearance with the provided Call ID, assigning one with a certain state if not currently present. */
/*! \note Currently only used to transition an appearance into the ALERTING state */
static int appearance_by_callid(struct sca_subscription *sca_sub, enum sca_appearance_state newstate, char *callid, char *contact, struct ast_sip_session *session, struct pjsip_msg *msg)
{
	int i, appearance = 0;

	ast_debug(3, "Checking for appearance for Call ID %s, session %p, channel %s, linked ID %s\n", S_OR(callid, ""), session, SESSION_CHANNEL(session), SESSION_LINKEDID(session));

	ast_mutex_lock(&sca_sub->lock);
	for (i = 0; i < MAX_APPEARANCES; i++) {
		if (sca_sub->appearances[i].state == SCA_APPEARANCE_NONE) {
			break; /* There are no more appearances remaining. */
		}
		if (sca_sub->appearances[i].state == SCA_APPEARANCE_IDLE) {
			continue; /* No way this has a Call ID. */
		}
		/* Use the session to compare.
		 * Specifically, retrieve the channels associated with each session.
		 * The channels will actually be different. However, if they are part of the same outgoing call, they will have the same Linked ID. */
		if (!session_matches_linkedid(sca_sub->appearances[i].linkedid, session)) {
			continue;
		}
		/* Indeed, it's the same call. */
		appearance = APPEARANCE_REAL_INDEX(i);
		update_use_count(sca_sub, appearance, newstate);
		/* Also add this session to the list of sessions */
		sca_add_session(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)], session);
		ast_debug(3, "Found existing match for this call on appearance %d\n", appearance);
		break;
	}

	/* No appearance assigned yet, so assign one now. */
	if (!appearance) {
		for (i = 0; i < MAX_APPEARANCES; i++) {
			if (!APPEARANCE_AVAILABLE(sca_sub->appearances[i])) {
				continue;
			}
			/* Found an available appearance, use it. */
			appearance = APPEARANCE_REAL_INDEX(i);
			update_use_count(sca_sub, appearance, newstate);
			ast_debug(2, "Assigning appearance %d for Call ID %s\n", appearance, callid);
			assign_appearance(sca_sub, appearance, newstate, newstate, callid, contact, session, NULL); /* We'll always have a session and never a channel if there's a Call ID, duh. */
			/* This is an incoming call to the phone(s), so we should save off an appearance-uri. */
			save_appearance_uri(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)], msg);
			send_update(sca_sub); /* Send NOTIFY (before the INVITE) */
			break;
		}
	}

	if (!appearance) {
		ast_log(LOG_WARNING, "Failed to assign appearance for Call ID %s\n", callid);
	} else {
		ast_debug(1, "Appearance for Call ID %s is %d\n", callid, appearance);
	}

	ast_mutex_unlock(&sca_sub->lock);
	return appearance;
}

static void seize_shutdown(struct ast_sip_subscription *sub)
{
	char *callid;
	struct sca_subscription *sca_sub;
	struct ast_sip_endpoint *endpoint;
	const char *endpoint_name;

	/* We will use the Call ID to figure out exactly which seizure it is that is being cancelled now. */
	callid = sip_subscription_get_callid(sub);
	if (!callid) {
		/* This is bad, we won't be able to remove the appearance seizure... */
		return;
	}

	endpoint = ast_sip_subscription_get_endpoint(sub);
	ast_assert(endpoint != NULL);
	endpoint_name = ast_sorcery_object_get_id(endpoint);
	sca_sub = sca_sub_by_endpoint(endpoint_name);

	if (!sca_sub) {
		ast_log(LOG_WARNING, "No SCA subscription for endpoint %s?\n", endpoint_name);
		goto cleanup;
	}

	/* Change the appearance from seized to idle, for whichever appearance we were using. */
	ast_debug(2, "Seizure terminated for %s\n", callid);
	if (set_appearance(sca_sub, -1, SCA_APPEARANCE_IDLE, callid, NULL, NULL)) {
		/*! \todo This can happen currently because line-seize isn't cancelled when the call is made, so when they expire
		 * the appearance has already moved on. Ideally when the seize turns into a call, we should cancel the subscription from our end. */
		ast_log(LOG_WARNING, "No seized appearance for Call-ID %s?\n", callid);
		ast_free(callid);
	} else {
		send_update(sca_sub); /* Notify everyone that this appearance is no longer seized. */
	}

	/* Decrement */
	ao2_ref(sca_sub, -1);
cleanup:
	ao2_ref(endpoint, -1);
}

static void subscription_shutdown(struct ast_sip_subscription *sub)
{
	struct ast_sip_endpoint *endpoint;
	const char *endpoint_name;
	struct sca_sip_sub *sipsub;
	int removedsub = 0;
	int subsleft = 0;
	struct sca_subscription *sca_sub = get_sca_sub(sub);

	if (!sca_sub) {
		return;
	}

	endpoint = ast_sip_subscription_get_endpoint(sub);
	ast_assert(endpoint != NULL);
	endpoint_name = ast_sorcery_object_get_id(endpoint);

	/* Remove this ast_sip_subscription from the list of subscriptions using the sca_subscription */
	AST_RWLIST_WRLOCK(&sca_sub->subs);
	AST_RWLIST_TRAVERSE_SAFE_BEGIN(&sca_sub->subs, sipsub, entry) {
		if (sipsub->sub == sub) {
			AST_RWLIST_REMOVE_CURRENT(entry);

			/* Each ast_sip_subscription has its own datastore... so always remove it. */
			ast_sip_subscription_remove_datastore(sipsub->sub, ds_name);
			ast_sip_subscription_destroy(sipsub->sub);
			ast_free(sipsub);
			removedsub = 1;
			/////ao2_ref(sca_sub, -1); /* We bumped the ref XXX of what??? not sca_sub in sca_subscription_alloc, decrement now. */
			/* Don't break, we want to count how many there are. */
		} else {
			subsleft++;
		}
	}
	AST_RWLIST_TRAVERSE_SAFE_END;
	AST_RWLIST_UNLOCK(&sca_sub->subs);

	ast_debug(1, "Unsubscription for %s from %s, %d subscription%s remaining (SCA refcount: %d)\n", CALLINFO_EVENT, endpoint_name, subsleft, ESS(subsleft), ao2_count(sca_sub));

	if (!removedsub) {
		ast_log(LOG_WARNING, "Failed to remove subscription from SCA subscriptions list for %s?\n", endpoint_name);
	}

	if (!subsleft) {
		int refcount;
		struct subscription_item *subitem;
		ast_debug(1, "No subscriptions remain for the SCA endpoint %s, destroying\n", endpoint_name);
		AST_RWLIST_WRLOCK(&sublist);
		AST_RWLIST_TRAVERSE_SAFE_BEGIN(&sublist, subitem, entry) {
			if (!strcmp(endpoint_name, subitem->endpoint)) {
				AST_RWLIST_REMOVE_CURRENT(entry);
				ast_free(subitem->endpoint);
				ast_free(subitem);
				break;
			}
		}
		AST_RWLIST_TRAVERSE_SAFE_END;
		AST_RWLIST_UNLOCK(&sublist);
		if (!subitem) {
			ast_log(LOG_ERROR, "Failed to remove subscription item on subscription shutdown for %s?\n", endpoint_name);
			ao2_cleanup(endpoint);
		}
		refcount = ao2_count(sca_sub);
		if (refcount != 1) {
			/* In theory, there could be a race condition, which is why this is not an assertion, but it SHOULD be 1... */
			ast_log(LOG_WARNING, "Reference count of SCA subscription was %d?\n", refcount);
		}
	}

	ao2_cleanup(endpoint);

	/* Note that this could be used by multiple contacts for the same endpoint, so this will not necessarily result in sca_sub getting destroyed. */
	ao2_ref(sca_sub, -1); /* remove data store reference */
}

static char *rdata_get_header_value(pjsip_rx_data *rdata, const char *header)
{
	char *hdr_value;
	pjsip_generic_string_hdr *hdr;
	pj_str_t name;

	pj_cstr(&name, header);

	hdr = (pjsip_generic_string_hdr*) pjsip_msg_find_hdr_by_name(rdata->msg_info.msg, &name, NULL);
	if (!hdr) {
		return NULL;
	}

	hdr_value = ast_malloc(hdr->hvalue.slen + 1);
	if (!hdr_value) {
		return NULL;
	}

	ast_copy_pj_str(hdr_value, &hdr->hvalue, hdr->hvalue.slen + 1);
	return hdr_value;
}

/*! \brief Parse Call-Info header for appearance number */
/*! \retval -1 on failure, positive appearance number on success */
static int parse_appearance(const char *callinfo)
{
	const char *appearance_string;
	int appearance;

	appearance_string = strchr(callinfo, ';'); /* Skip the domain. */
	if (!appearance_string++) {
		return -1;
	}
	appearance_string = strchr(appearance_string, '=');
	if (!appearance_string++ || ast_strlen_zero(appearance_string)) {
		return -1;
	}
	appearance = atoi(appearance_string);
	if (appearance <= 0 || appearance >= MAX_APPEARANCES) {
		/* This also ensures we don't have to check the array bounds in the future of using a phone-provided appearance number */
		ast_log(LOG_WARNING, "Appearance %d is not valid\n", appearance);
		return -1;
	}
	return appearance;
}

static char *callid_from_msg(pjsip_msg *msg)
{
	pjsip_cid_hdr *cid_hdr;
	const pj_str_t *call_id;
	char *callid_dup;

	cid_hdr = PJSIP_MSG_CID_HDR(msg);
	call_id = &cid_hdr->id;

	callid_dup = ast_malloc(pj_strlen(call_id) + 1);
	if (!callid_dup) {
		ast_log(LOG_WARNING, "Failed to duplicate Call-ID\n");
		return NULL;
	}
	ast_copy_pj_str(callid_dup, call_id, pj_strlen(call_id) + 1);
	return callid_dup;
}

static char *contact_from_msg(pjsip_msg *msg)
{
	pjsip_contact_hdr *contact_hdr;
	pjsip_sip_uri *contact_uri;
	char *contact_str;

	contact_hdr = pjsip_msg_find_hdr(msg, PJSIP_H_CONTACT, NULL);
	if (!contact_hdr) {
		return NULL;
	}

	contact_uri = pjsip_uri_get_uri(contact_hdr->uri);

	/* The user part is going to be the same for all the contacts for this endpoint, so use the host to differentiate.
	 * The host will be the private IP address of the phone. */
	contact_str = ast_malloc(pj_strlen(&contact_uri->host) + 1);
	if (!contact_str) {
		return NULL;
	}
	ast_copy_pj_str(contact_str, &contact_uri->host, pj_strlen(&contact_uri->host) + 1);
	return contact_str;
}

static int new_seize(struct ast_sip_endpoint *endpoint, const char *resource, pjsip_rx_data *rdata)
{
	struct sca_subscription *sca_sub;
	char *callinfo;
	int appearance;
	int res;
	char *callid, *contact;

	/* Figure out which appearance the endpoint wants to seize.
	 * We can't use ast_sip_subscription_get_header since the subscription hasn't been created yet. */
	callinfo = rdata_get_header_value(rdata, "Call-Info");
	if (!callinfo) {
		ast_log(LOG_WARNING, "Received line-seize subscription without Call-Info header?\n");
		return 480;
	}

	/* Parse the Call-Info header */
	appearance = parse_appearance(callinfo);
	if (appearance <= 0) {
		ast_log(LOG_WARNING, "Call-Info header is malformed: %s\n", callinfo);
		ast_free(callinfo);
		return 480;
	}

	/* We also want the Call-ID */
	callid = callid_from_msg(rdata->msg_info.msg);
	if (!callid) {
		ast_free(callinfo);
		return 480;
	}

	/* We also want the contact */
	contact = contact_from_msg(rdata->msg_info.msg);
	if (!contact) {
		ast_free(callid);
		ast_free(callinfo);
		return 480;
	}

	/* If there's no subscription for the SCA, then bail... this should not be possible. */
	sca_sub = sca_sub_by_endpoint(ast_sorcery_object_get_id(endpoint));
	if (!sca_sub) {
		ast_log(LOG_WARNING, "No Call-Info subscription exists for %s, but received line-seize?\n", ast_sorcery_object_get_id(endpoint));
		return 480;
	}

	/* If nobody else has seized the requested appearance, grant the subscription.
	 * Granting the subscription is like granting a lock on a resource.
	 * Only one subscription can ever exist. */

	ast_debug(2, "Received %s subscription for %s, appearance %d (Call-Info: %s)\n", SEIZE_EVENT, resource, appearance, callinfo);

	if (set_appearance(sca_sub, appearance, SCA_APPEARANCE_SEIZED, callid, contact, NULL)) {
		res = 480;
		ast_free(callid);
		ast_free(contact);
	} else {
		res = 200;
		/* If the subscription is being granted, notify everyone that this appearance has been seized. */
		/* XXX Note that this will result in a NOTIFY going out before the OK response goes out.
		 * Seems to be fine, but if it's problematic, send_update should be moved to seize_established. */
		send_update(sca_sub);
		ast_verb(5, "%s seized line appearance %d\n", ast_sorcery_object_get_id(endpoint), appearance);
	}

	/* Clean up and return */
	ast_free(callinfo);
	ao2_ref(sca_sub, -1);
	return res;
}

static int new_subscribe(struct ast_sip_endpoint *endpoint, const char *resource)
{
	ast_debug(2, "New %s subscription for %s\n", CALLINFO_EVENT, resource);
	return 200;
}

static int get_resource_display_name(struct ast_sip_endpoint *endpoint, const char *resource, char *display_name, int display_name_size)
{
	if (!endpoint || ast_strlen_zero(resource) || !display_name || display_name_size <= 0) {
		return -1;
	}

	ast_copy_string(display_name, ast_sorcery_object_get_id(endpoint), display_name_size);
	return 0;
}

static int seize_established(struct ast_sip_subscription *sip_sub)
{
	return 0;
}

static int subscription_established(struct ast_sip_subscription *sip_sub)
{
	struct ast_sip_endpoint *endpoint = ast_sip_subscription_get_endpoint(sip_sub);
	struct sca_subscription *sca_sub;

	if (!(sca_sub = sca_subscription_alloc(sip_sub, endpoint))) {
		ao2_cleanup(endpoint);
		return -1;
	}

	/* Go ahead and cleanup the endpoint since we don't need it anymore */
	ao2_cleanup(endpoint);

	ast_debug(2, "%s subscription added for %s\n", CALLINFO_EVENT, ast_sorcery_object_get_id(endpoint));
	send_update(sca_sub);

	ao2_cleanup(sca_sub);
	return 0;
}

static void sca_data_destructor(void *obj)
{
	struct ast_sip_sca_data *sca_data = obj;
	/* Nothing to do, since there's no dynamically allocated memory in the task data. */
	sca_data->park_enabled = 0; /* Just so we do something. */
	return;
}

static struct ast_sip_sca_data *sca_data_alloc(struct ast_sip_subscription *sip_sub, struct sca_subscription *sca_sub)
{
	struct ast_sip_sca_data *sca_data;
	sca_data = ao2_alloc(sizeof(*sca_data), sca_data_destructor);
	return sca_data;
}

static void *get_seize_notify_data(struct ast_sip_subscription *sub)
{
	/* XXX We must return SOMETHING or res_pjsip_pubsub will terminate the subscription. */
	struct ast_sip_sca_data *sca_data;
	sca_data = ao2_alloc(sizeof(*sca_data), sca_data_destructor);
	return sca_data;
}

static void *get_notify_data(struct ast_sip_subscription *sub)
{
	struct sca_subscription *sca_sub;

	sca_sub = get_sca_sub(sub);
	if (!sca_sub) {
		return NULL;
	}
	return sca_data_alloc(sub, sca_sub);
}

static void to_ami(struct ast_sip_subscription *sub, struct ast_str **buf)
{
	return;
}

static void send_response(struct ast_sip_session *session, struct pjsip_rx_data *rdata, int code)
{
	pjsip_tx_data *tdata;
	pjsip_dialog *dlg = session->inv_session->dlg;

	if (pjsip_dlg_create_response(dlg, rdata, code, NULL, &tdata) == PJ_SUCCESS) {
		struct pjsip_transaction *tsx = pjsip_rdata_get_tsx(rdata);
		pjsip_dlg_send_response(dlg, tsx, tdata);
	}
}

/*! \brief Whether the SDP says the phone wants to hold this call. */
static inline int sdp_hold_directive(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	int i;
	pjsip_rdata_sdp_info *sdp_info;
	const pjmedia_sdp_session *sdp;

	/* XXX Is there a better way to do this?
	 * We can't use a res_pjsip_session SDP callback because we're not handling anything.
	 * We just want to know what the stream state is. */

	if (session->inv_session && session->inv_session->state == PJSIP_INV_STATE_DISCONNECTED) {
		return 0;
	}

	sdp_info = pjsip_rdata_get_sdp_info(rdata);
	if (!sdp_info) {
		return -1;
	}
	sdp = sdp_info->sdp;

	for (i = 0; i < sdp->media_count; ++i) {
		/* See if there are registered handlers for this media stream type */
		char media[20];
		pjmedia_sdp_media *remote_stream = sdp->media[i];

		/* We need a null-terminated version of the media string */
		ast_copy_pj_str(media, &sdp->media[i]->desc.media, sizeof(media));

		if (strcmp(media, "audio")) {
			continue;
		}

		if (pjmedia_sdp_media_find_attr2(remote_stream, "sendonly", NULL)) {
			ast_debug(1, "Request to hold appearance from %p\n", session);
			return 1;
		}
	}
	return 0;
}

/*! \brief Whether the user part of the To header is the endpoint name, indicating this is resuming hold or barging in for existing call */
static inline int to_is_endpoint(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	pjsip_to_hdr *to_hdr;
	pjsip_sip_uri *uri;
	char *to_str;
	int res;
	const char *endpoint_name = ast_sorcery_object_get_id(session->endpoint);

	to_hdr = PJSIP_MSG_TO_HDR(rdata->msg_info.msg);
	uri = pjsip_uri_get_uri(to_hdr->uri);

	to_str = ast_malloc(pj_strlen(&uri->user) + 1);
	if (!to_str) {
		return 0;
	}
	ast_copy_pj_str(to_str, &uri->user, pj_strlen(&uri->user) + 1);
	ast_debug(5, "To URI user: %s\n", to_str);

	/* In *ALL* cases, to join an existing call (resume from hold, barge), the user part of the To URI is
	 * the name of the endpoint itself. So check for that.
	 * NOTE: We only get here after res_pjsip_session checks for the extension dialed in the dialplan.
	 * In a way, this is actually nice as it means we can prevent people from doing anything other
	 * than resuming held calls from an endpoint other than the one from which they held it,
	 * simply by not having any such extension in the dialplan.
	 * If the user wants to be able to resume holds from different phones or do barge-in,
	 * then extensions with the name of the endpoint for each SCA will need to exist.
	 */

	res = !strcmp(to_str, endpoint_name);
	ast_free(to_str);
	return res;
}

static int process_hold_unhold(struct sca_subscription *sca_sub, struct ast_sip_session *session, struct pjsip_rx_data *rdata, char *callinfo, char *callid, char *contact, int appearance)
{
	int is_to_endpoint = to_is_endpoint(session, rdata);

	/* When a phone puts a call on hold, at least 1 of 2 things must be true:
	 * - SDP will have sendonly attribute
	 * - ;appearance-state=held will appear at end of Call-Info header.
	 *
	 * For some reason I see sendonly/sendrecv with a SoundPoint 550, and ;appearance-state=held with some VVX phones,
	 * but we can safely assume either indicates a request to hold the call.
	 * The SoundPoint series does not support private hold, but the VVX does (which may explain this discrepancy).
	 * Possibly appearance-state=held may be a newer standard and we support both methods for compatibility.
	 *
	 * Unholding a call is significantly more difficult, because it could be done from a different phone than the one that held it.
	 * For unhold, the Call-Info header is not special in any way (it merely contains the appearance number).
	 * If it's the same phone, then if there's a channel and the above is not true, it's an unhold, pretty simple.
	 *
	 * More complex scenarios:
	 * If it's a different phone, then it will look like a request to make a new call on that appearance.
	 * Additionally, it could also be a barge-in, in which case we need to conference everyone together.
	 * One thing these both have in common is the "To" header will contain the endpoint name
	 * (i.e. no number has been dialed, we are trying to reconnect to an existing call).
	 * The difference between resuming a hold from a different phone and barging in simply depends
	 * on whether or not the original phone is still active or not.
	 *
	 * At a high level, if a held call is resumed from a different phone, then the INVITE is accepted
	 * as with same-phone resumption (200 OK), but the original phone is then disconnected (BYE).
	 */

	if (!session->channel) {
		/* Okay, so this MUST be the initial INVITE (as opposed to a re-INVITE).
		 *
		 * (We know this since sca_incoming_invite is a callback that executes before channel creation (AST_SIP_SUPPLEMENT_PRIORITY_FIRST)
		 * This is more reliable than trying to guess based on the CSeq, since it could be 1 or 2 for initial INVITEs, etc.)
		 *
		 * That doesn't necessarily mean we're making a new call. It could be an attempt to resume or join
		 * an in-progress call held or active at another station. */
		if (!is_to_endpoint) {
			/* This is actually somebody trying to make a new call, so process it normally. */
			return -1;
		}
		/* It's a new phone trying to join the party... either resume another station's call on hold or barge into it. */
		ast_debug(1, "Interesting... appearance is being resumed or joined from a different phone...\n");
	}

	/* It's only a private hold if the phone said so. */
	if (strstr(callinfo, ";appearance-state=held-private")) {
		/* Private hold */
		ast_debug(1, "Request to private hold appearance from %p\n", session);
		if (!set_appearance(sca_sub, appearance, SCA_APPEARANCE_HELD_PRIVATE, callid, contact, session)) {
			return 0;
		}
	} else if (strstr(callinfo, ";appearance-state=held") || sdp_hold_directive(session, rdata)) {
		/* Regular hold */
		ast_debug(1, "Request to public hold appearance from %p\n", session);
		/* If this the only call currently, then proceed normally.
		 * If we're multiactive (usecount > 1, i.e. APPEARANCE_STATE_MULTI_ACTIVE), then we
		 * the phone putting the call on hold is not necessarily the phone that made the call.
		 * So in that case, force it.
		 */
		if (APPEARANCE_STATE_MULTI_ACTIVE(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state)) {
			/* This is purely a sanity check. This property should hold.
			 * I'm not comfortable making this an assertion, because we don't hold any locks here (and shouldn't hold any)
			 * so there is a very small chance there could be a race condition that might cause this to fail.
			 * But this really should not happen. */
			if (sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount <= 1) {
				ast_log(LOG_WARNING, "Appearance %d has state %s but use count %d???\n", appearance,
					sca_appearance_state_str(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state), sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount);
			}
			/* It's okay to force it, there's no security implication of doing that.
			 * The hold request is going to get converted into SCA_APPEARANCE_BRIDGE_HELD. */
			if (!set_appearance_force(sca_sub, appearance, SCA_APPEARANCE_HELD, callid, contact, session)) {
				/* Again, we don't hold any locks here, but this should also hold: */
				if (sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state != SCA_APPEARANCE_BRIDGE_HELD) {
					ast_log(LOG_WARNING, "Expected appearance %d to be %s, but it's %s?\n", appearance,
						sca_appearance_state_str(SCA_APPEARANCE_BRIDGE_HELD), sca_appearance_state_str(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state));
				}
				return 0;
			}
		} else {
			/* We shouldn't need to force it, this will ensure that only the party that made the call can put it on hold, since only 1 phone is active.
			 * XXX However I've seen before where for some reason the contacts get messed up and then this can fail. For some reason it seems to happen on the first hold but not subsequent ones.
			 * So if we detect this condition, then force it to happen anyways. Emit a warning too because something is weird / not being accounted for... but we should oblige.
			 */
			if (sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount <= 1 && safe_strcmp(contact, sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact)) {
				ast_log(LOG_WARNING, "Call is being held by its owner, but current contact %s != %s?\n", contact, sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].contact);
				/* Do the right thing anyways, put it on hold. */
				if (!set_appearance_force(sca_sub, appearance, SCA_APPEARANCE_HELD, callid, contact, session)) {
					return 0;
				}
			} else {
				if (!set_appearance(sca_sub, appearance, SCA_APPEARANCE_HELD, callid, contact, session)) {
					return 0;
				}
			}
		}
	} else if (APPEARANCE_HELD(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)])
		|| sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state == SCA_APPEARANCE_ACTIVE) {
		/* If it's a private hold, then naturally only the phone that put it on hold can unhold it,
		 * in which case we simply go ahead and grant that (and the session *WILL* match in that case). */
		if (session->channel) {
			/* It's a phone resuming its own call that was on hold.
			 * Even though we know the appearance, pass -1 to force set_appearance to actually
			 * search for a match on either the contact or the session (it should get to appearance).
			 * That way, if we're resuming a private hold, we ensure that only the phone
			 * the actually held the call can resume it.
			 */
			if (!set_appearance(sca_sub, -1, SCA_APPEARANCE_ACTIVE, callid, contact, session)) {
				return 0;
			}
		} else if (is_to_endpoint) {
			/* This is somebody else trying to join the party (or take over it). */
			enum sca_appearance_state oldstate;
			/* Join an existing appearance (unhold or barge, depending on whether it's active or purely held)
			 * None of our information can be matched by set_appearance (it will fail if we try to call it.)
			 * However, we can simply go ahead and grant the request, unless it's a private hold. */
			if (sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state == SCA_APPEARANCE_HELD_PRIVATE) {
				ast_debug(1, "Rejecting attempt to resume privately held call on appearance %d by %s\n", appearance, contact);
				return -1;
			}
			oldstate = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state;

			/* We're definitely going to want to bump the use count here.
			 * If the call was beind held, the other phone is now going to say BYE and let us take over fully.
			 * If we barged, obviously we should bump the use count because we have an additional phone active on the appearance. */

			/* This MUST increment before we process the other phone saying BYE or we'll reset the appearance to idle
			 * You might think there could be a race condition with the BYE here, which could cause usecount to hit 0
			 * and set the appearance to idle. In practice, we don't send the NOTIFY until we return from whatever called set_appearance,
			 * so as long as we bump the use count before the NOTIFY goes out, we're good.
			 *
			 * Also, we must increment before calling set_appearance_force, because we'll need the right use count to calculate the new state.
			 * So if we fail, decrement the use count.
			 *
			 * XXX Ideally, this could/should be done directly in update_use_count... since that's where all the other usecount adjustments are,
			 * and that way we don't have to increment now and decrement on failure, which is a little hacky.
			 * It'll just be tricky since we won't necessarily have the right context there...
			 * If this is done in the future, this must be done EXTREMELY CAREFULLY (and tested rigorously) as it would be very easy to screw
			 * it up and accidentally bump the use count when we shouldn't.
			 */
			sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount += 1;

			/* If already active, we're now bridge active. Otherwise, just active.
			 * Note that an INVITE for a barge-in isn't special in any identifying way. It's up to us know that it's a barge based on current state.
			 * The phone itself will behave differently: for example Polycoms that don't have reg.X.bargeInEnabled set to 1 will try to
			 * seize a new appearance on that line key if you push it. Pressing and holding will provide an option to barge, if enabled.
			 *
			 * Since usecount is going to be at least 2 now, we'll always be transitioning BRIDGE_ACTIVE, at least for a second.
			 * Therefore, we use the laststate because, temporarily, while we're adding a channel to the appearance and kicking the old one out,
			 * we can't rely on the current state to tell us the transition that just happened. The laststate will tell us this right now,
			 * and the current state will work itself out momentarily.
			 */

			/* If already active, we're now bridge active. Otherwise, just active.
			 * However, set_appearance internally will take care of assigning SCA_APPEARANCE_BRIDGE_ACTIVE vs SCA_APPEARANCE_ACTIVE appropriately.
			 * That's why we just pass in SCA_APPEARANCE_ACTIVE.
			 */
			if (!set_appearance_force(sca_sub, appearance, SCA_APPEARANCE_ACTIVE, callid, contact, session)) {
				/* This is all atomic, since we still hold the lock, so if we succeeded, then the state better be what we expected. */
				if (!APPEARANCE_STATE_ACTIVE(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state)) {
					ast_log(LOG_WARNING, "Appearance %d in unexpected state %s?\n",
						appearance, sca_appearance_state_str(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state));
				}

				ast_debug(4, "Bumped the use count of appearance %d up to %d\n", appearance, sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount);

				if (APPEARANCE_STATE_ACTIVE(oldstate)) {
					ast_debug(1, "Barge-in on appearance %d by %s\n", appearance, contact);
				} else {
					ast_debug(1, "Hold resumed on appearance %d by different contact: %s\n", appearance, contact);
				}

				/* In sca_incoming_unhold, we actually join the call,
				 * This is because the channel doesn't exist yet, since this supplement is AST_SIP_SUPPLEMENT_PRIORITY_FIRST.
				 * We have another supplement with post-channel creation priority, and we can do the bridging manipulation there.
				 */
				return 0;
			} else {
				sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount -= 1; /* Undo what we did. */
			}
		}
	} else {
		ast_log(LOG_WARNING, "Can this even happen?\n"); /* XXX Not sure, but don't think so, at least for legitimate requests. */
	}

	ast_debug(1, "Hold/unhold processing either failed or was unhandled\n");
	/* Don't return -1 or sca_incoming_invite will continue and try to set the appearance to SCA_APPEARANCE_PROGRESSING, which is wrong.
	 * We may have screwed up already but don't screw up even more. */
	return 0;
}

/*! \brief Play barge warning tone into a bridge */
#include "asterisk/core_unreal.h"

static struct ast_channel_tech barge_tech = {
	.type = "BridgeBarge",
	.description = "Simple channel technology for bridge barging",
};

static struct ast_channel_tech *barge_get_tech(void)
{
	return &barge_tech;
}

static void set_channel_timeout(struct ast_channel *chan, long timeout)
{
	struct timeval when = {0,};

	when.tv_sec = timeout;

	ast_channel_lock(chan);
	ast_channel_setwhentohangup_tv(chan, when);
	ast_channel_unlock(chan);
}

#if 0
static void bridge_play_tone(struct ast_bridge_channel *bridge_channel, const char *playfile)
{
	int waitms = 0;
	struct ast_channel *chan = ast_bridge_channel_get_chan(bridge_channel);
	char *waitstr = strchr(playfile, '/');

	if (!chan) {
		ast_log(LOG_WARNING, "No channel?\n");
		return;
	}

	if (!ast_channel_is_bridged(chan)) {
		ast_log(LOG_WARNING, "Barge channel %s is not bridged?\n", ast_channel_name(chan));
	}

	/* Determine how long we should sleep from the indications string (assuming it's just a single tone) */
	if (waitstr && !ast_strlen_zero(++waitstr)) {
		waitms = atoi(waitstr);
	}

	ast_debug(3, "Streaming tone %s on %s (for %d ms)\n", playfile, ast_channel_name(chan), waitms);
	if (!ast_playtones_start(chan, 0, playfile, 0)) {
		ast_safe_sleep(chan, waitms);
	}
	ao2_ref(chan, -1);
	return;
}
#endif

static int bridge_barge(struct ast_channel *chan, struct ast_bridge *bridge, const char *endpoint, int appearance)
{
	struct ast_channel *tonechan;
	static unsigned int barge_number = 0;
	RAII_VAR(struct ast_format_cap *, capabilities, NULL, ao2_cleanup);
	//struct ast_bridge_channel *bridge_channel;
	//struct ast_unreal_pvt *pvt;
	int generated_seqno = ast_atomic_fetchadd_int((int *) &barge_number, +1);

	/*! \todo the barge stuff does not currently work, figure out how to do this simply and correctly */

	/* Remember, bridge needs to be cleaned up unless ast_bridge_impart succeeds.
	 * bridge_channel always needs to be cleaned up. */

	if (!bridge) {
		return -1;
	}

	/* Here we create a simple channel to add to the bridge and play the tone.
	 * This is (and must be) non-blocking. */

	capabilities = ast_format_cap_alloc(AST_FORMAT_CAP_FLAG_DEFAULT);
	if (!capabilities) {
		goto cleanup;
	}
	ast_format_cap_append_by_type(capabilities, AST_MEDIA_TYPE_AUDIO);

	/* Just use everything after the PJSIP/ as part of the channel name, that way we have something unique we can use.
	 * This is because multiple barges could happen on an SCA appearance in an overlapping manner, so that wouldn't be a sufficient source of uniqueness. */
	tonechan = ast_channel_alloc(0, AST_STATE_UP, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, "BridgeBarge/%s-%d-%08x", endpoint, appearance, (unsigned) generated_seqno);
	if (!tonechan) {
		ast_log(LOG_WARNING, "Failed to allocate channel\n");
		goto cleanup;
	}

	ast_channel_tech_set(tonechan, barge_get_tech());
	ast_channel_nativeformats_set(tonechan, capabilities);
	ast_channel_set_writeformat(tonechan, ast_format_slin);
	ast_channel_set_rawwriteformat(tonechan, ast_format_slin);
	ast_channel_set_readformat(tonechan, ast_format_slin);
	ast_channel_set_rawreadformat(tonechan, ast_format_slin);
	ast_channel_unlock(tonechan);

	set_channel_timeout(tonechan, 1L); /* Channel only needed for 1 second. */

	if (1) {
		ao2_ref(bridge, -1);
		return 0;
	}
	if (ast_bridge_impart(bridge, tonechan, NULL, NULL, AST_BRIDGE_IMPART_CHAN_INDEPENDENT)) {
		ast_log(LOG_WARNING, "Failed to impart channel into bridge\n");
		goto cleanup;
	}

	ast_log(LOG_WARNING, "bridge refcount now: %d\n", ao2_count(bridge));
	ao2_ref(bridge, -1);
	return 0;

cleanup:
	ao2_ref(bridge, -1);
	return -1;
}

/* XXX Could be moved to channel.c, ast_get_channel_by_ptr maybe?
 * The callback itself could be wrapped to ast_channel_exists_by_ptr */
static int find_by_ptr(void *obj, void *arg, void *data, int flags)
{
	struct ast_channel *target = obj;/*!< Potential pickup target */
	struct ast_channel *chan = data;

	if (chan == target) {
		ast_channel_ref(chan);
		return CMP_MATCH | CMP_STOP;
	}

	return 0;
}

static inline int is_hold(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	int hold = 0;
	char *callinfo;
	callinfo = rdata_get_header_value(rdata, "Call-Info");
	/* Yes, this includes held and held-private; we want to include all holds. */
	hold = (callinfo && strstr(callinfo, ";appearance-state=held")) || sdp_hold_directive(session, rdata);
	if (callinfo) {
		ast_free(callinfo);
	}
	return hold;
}

/*! \brief Barge into an active call / unhold a held call */
static int do_barge_unhold(struct ast_channel *chan, struct sca_subscription *sca_sub, int appearance, const char *endpoint_name)
{
	int usecount;
	enum sca_appearance_state state, oldstate = SCA_APPEARANCE_NONE; /* Initialize because if we goto cleanup before we set this, it won't be initialized otherwise. */
	int res = -1;
	struct ast_channel *ochan = NULL;
	struct sca_session *s;
	struct sca_channel *c;
	int barge_tone = 1; /*! \todo Barge warning tone currently forced, should be configurable? */

	/* There was previously a bug where we didn't have the .method = "INVITE" filter on the supplement, which led to this function getting called twice.
	 * This was a mechanism that detected that, left here now because it's still the case that a channel should never be bridged at this point.
	 * If it is for any reason and we try to bridge again, we'll crash. */
	if (ast_channel_is_bridged(chan)) {
		/* Channel shouldn't be bridged if we're trying to bridge it... */
		ast_log(LOG_WARNING, "Channel %s is already bridged?\n", ast_channel_name(chan));
		return -1;
	}

	usecount = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount;
	oldstate = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].laststate;
	state = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state;
	if (usecount < 1) {
		ast_log(LOG_WARNING, "Use count of appearance %d is %d?\n", appearance, usecount);
		return -1;
	}
	if (!APPEARANCE_STATE_ACTIVE(state) && !APPEARANCE_STATE_HELD(state)) {
		ast_log(LOG_WARNING, "Unexpected state: %s\n", sca_appearance_state_str(state));
		return -1;
	}

	ast_verb(4, "%s %s on appearance %d\n", ast_channel_name(chan),
		APPEARANCE_STATE_ACTIVE(oldstate) ? "barging in" : "resuming hold", appearance);

	/* Find *a* channel (if we're barging into a conference, could already be more than one),
	 * that's associated with the appearance we want to join. */
	AST_RWLIST_RDLOCK(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].sessionlist);
	AST_LIST_TRAVERSE(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].sessionlist, s, entry) {
		if (!s || !s->session) {
			continue;
		}
		if (s->session->channel == chan) {
			continue; /* Skip ourself, duh... */
		}
		if (!s->session->channel) {
			/* The only time that we store a session w/o a channel is when a line is seized.
			 * However, a seize is basically a mutex on the whole appearance, if we're trying unhold or
			 * barge, then it can't possibly be seized, so any sessions in the sessionlist must have channels. */
			ast_log(LOG_WARNING, "Huh, session %p has no channel?\n", s->session);
			continue; /* Session has no channel... so can't be it. */
		}

		/* We have the slight possibility that ochan is a reference to a channel that no longer exists.
		 * If ochan is not null, then we +1'd the ref and will need to unref it.
		 */
		ochan = ast_channel_callback(find_by_ptr, NULL, s->session->channel, 0);
		if (!ochan) {
			/* s probably should have been REMOVED from the sessionlist already (it's stale)
			 * It could possibly be a session associated with a seizure (the seizure for which is long stale by now).
			 * We don't even know the name of the poor channel, since this is a stale reference
			 * so we can't dereference it. */
			ast_log(LOG_WARNING, "Channel %p does not exist anymore (from session %p)\n", s->session->channel, s->session);
			continue; /* Try again. Maybe we'll find a channel that does still exist. */
		}

		/* Okay, we have a reference to some channel on this appearance that isn't ourself.
		 * It doesn't matter which one, because if there are multiple,
		 * they'd all be in the same bridge.
		 * Importantly, we also know that the channel still exists!
		 */
		break;
	}
	AST_RWLIST_UNLOCK(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].sessionlist);

	/* Didn't find any session channels for native SCA endpoints... try the foreign channels. */
	if (!ochan) {
		AST_RWLIST_RDLOCK(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].chanlist);
		AST_LIST_TRAVERSE(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].chanlist, c, entry) {
			if (c->chan == chan) {
				continue; /* Skip ourself, duh... */
			}
			/* Unlike sessions, this is a lot simpler. If a channel is in the list, it is guaranteed to be valid,
			 * since channels are removed by the datastore destructor on the channel. */
			ochan = c->chan;
			ast_channel_ref(ochan); /* Bump the refcount manually since we're grabbing a reference directly from the list. */
			ast_debug(3, "Found an existing channel: %s\n", ast_channel_name(ochan));
			break;
		}
		AST_RWLIST_UNLOCK(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].chanlist);
	}

	if (!ochan) { /* Shouldn't happen, would've been caught in process_hold_unhold */
		ast_log(LOG_WARNING, "Couldn't find any native or foreign channel associated with appearance %d?\n", appearance);
		return -1;
	}

	/* If there's another channel actively in the call, then we'll want to barge in, i.e. create a conference.
	 * Otherwise, we simply want to "steal" the call from that channel. If the channel is bridged, we'll swap with it; if not, do a masquerade (yuck!). */
	if (!ast_channel_is_bridged(ochan)) {
		if (oldstate == SCA_APPEARANCE_HELD) {
			struct ast_channel *mychan = chan;
			/* The channel should really be bridged, which is why we throw a warning here. This should be avoided if possible.
			 * For incoming calls (from Asterisk to the phone), we will always be bridged.
			 * For "real" outgoing calls, we will also be bridged (e.g. if Dial()) is called, and calls can't
			 * be held unless they are answered, so this means we would indeed have a bridge.
			 * The only case where we might not have a bridge is for an outgoing call (phone to Asterisk)
			 * that is executing dialplan, e.g. Playback().
			 * The workaround is to ensure that all calls execute a Dial() in the dialplan to actually
			 * make the call, e.g. do a Dial() on the real channel and execute dialplan stuff
			 * in a Local channel. Unfortunately, that's not something we have any visibility into here.
			 *
			 * So ideally the user always does a Local Dial() in the dialplan, and we never hit this case.
			 * If we do get here, this will kind of suck, but just attempt to handle this in the sanest way we can.
			 *
			 * If we do a masquerade, it may not actually be transparent to the other party in practice,
			 * and I don't really think there's a way to do barge-in for non-bridged... since we need a bridge for that.
			 *
			 * For example, Playback seems to masquerade properly without issues.
			 * However, PlayTones does not (which is probably a bug with channel_do_masquerade in channel.c)
			 * - Update: I have patched channel.c to fix tones not continuing after masquerades.
			 *
			 * Still, it proves my point:
			 * Even though in theory, this should work fine, in practice it's not guaranteed and
			 * masquerades are yucky in general and should be avoided if possible.
			 */

			ast_log(LOG_WARNING, "Channel %s is not bridged. This unhold may not preserve the current call state.\n"
				"Please consider using Local channels to execute outgoing dialplan!\n", ast_channel_name(ochan));
			/*! \todo BUGBUG now getting:
			 * ERROR[1802692][C-00000001]: channel.c:3572 __ast_read: ast_read() on chan 'PJSIP/PolycomSCA1-00000001' called with no recorded file descriptor. */
			if (!ast_channel_move(ochan, mychan)) {
				/* ast_channel_move does a masquerade, so keep a local ref to session->channel */
				ast_hangup(mychan);
				res = 0;
			} else {
				ast_log(LOG_WARNING, "Failed to masquerade with %s\n", ast_channel_name(ochan));
			}
		} else { /* oldstate was SCA_APPEARANCE_ACTIVE */
			/* I don't know how we can barge in to a call if there's no bridge. Give up. */
			ast_log(LOG_WARNING, "Sorry, can't barge in to a call with no bridge! Please use Local channels to execute outgoing dialplan!\n");
		}
	} else { /* channel is bridged */
		struct ast_bridge *bridge, *bargebridge;

		ast_channel_lock(ochan);
		bridge = ast_channel_get_bridge(ochan);
		/* Note that we call ast_channel_get_bridge(ochan) again for bridge_barge so that it gets its own reference.
		 * This is to avoid the refcount hitting 0 and the bridge getting destroyed while we're trying to use it, since we give each participant its own ref. */
		ast_channel_unlock(ochan);

		/* A housekeeping note about ref counting here, that was figured out the hard way because documentation for this is poor:
		 * If we successfully impart a channel, then the bridging thread will clean up once it cleans up the bridge channel.
		 * So essentially, when using ast_bridge_impart here, we only need to unref on failure.
		 * In barge_tone, we call ast_channel_get_bridge_channel, so we always need to cleanup the reference it gets before returning.
		 * I only know this because I was initially always unreffing and refcount would hit 0 while channels were in the bridge :)
		 * I have confirmed via refdebug that we're unreferring where we should be and not where we shouldn't.
		 */

		/* For unhold, replace the channel currently in the bridge with ourself. (The original channel will get hung up)
		 * For barge-in, simply don't replace the channel, that way we're both there. */

		/* If we're configured to play barge-in warning tone, then do so. */
		if (barge_tone && APPEARANCE_STATE_MULTI_ACTIVE(state)) {
			ast_channel_lock(ochan);
			bargebridge = ast_channel_get_bridge(ochan);
			ast_channel_unlock(ochan);
			bridge_barge(chan, bargebridge, endpoint_name, appearance);
		}

		if (ast_channel_pbx(chan)) {
			/* For channels joining via the dialplan application. */
			struct ast_bridge_features chan_features; /* The bridge.h example makes it seem like we can pass in NULL features, but this will cause an assertion! */

			res = ast_bridge_features_init(&chan_features);
			if (!res) {
				/* ast_bridge_join here is blocking (which is fine). */
				res = ast_bridge_join(bridge, chan, APPEARANCE_STATE_MULTI_ACTIVE(state) ? NULL : ochan, &chan_features, NULL, AST_BRIDGE_JOIN_PASS_REFERENCE);
				ast_log(LOG_WARNING, "bridge refcount now: %d\n", ao2_count(bridge));
			}
			ast_bridge_features_cleanup(&chan_features);
			if (res) {
				ast_log(LOG_WARNING, "Failed to join channel into bridge\n");
			}
		} else {
			/* This is non-blocking. */
			res = ast_bridge_impart(bridge, chan, APPEARANCE_STATE_MULTI_ACTIVE(state) ? NULL : ochan, NULL, AST_BRIDGE_IMPART_CHAN_INDEPENDENT);
			if (res) {
				ast_log(LOG_WARNING, "Failed to impart channel into bridge\n");
			}
		}

		ast_log(LOG_WARNING, "bridge %p refcount now: %d\n", bridge, ao2_count(bridge));
		if (res) {
			ao2_cleanup(bridge);
		}
	}

	/* No need to call send_update here.
	 * This is because if the call was active, it's still active and no state change has occured.
	 * If it was held, then this phone has stolen the call from the original phone that held it.
	 * This will hang that channel up and it will consequently end its session.
	 * In the course of events, set_appearance will get called.
	 * The appearance is still active, so we'll ignore the "idle transition", but return 0
	 * so that a NOTIFY goes out. That's the right time to do it, because if we sent an update
	 * now, it might get to the phone before its session has time to terminate (race condition),
	 * and then the indicator won't show the right state, since it'll think it's idle.
	 */

	ast_channel_unref(ochan);

	if (res) {
		/* If we never set oldstate, then we don't actually know what the phone was trying to do, since we never got that context. Just say something generic in that case. */
		const char *opname = oldstate == SCA_APPEARANCE_NONE ? "Operation" : SCA_APPEARANCE_HELD ? "Unhold" : "Barge";
		/* Since the channel has already been created at this point, hang it up since we can't proceed.
		 * Otherwise the phone will think it successfully barged, when it hasn't.
		 * Hanging up the channel will terminate the session and cause it to release the appearance.
		 */
		ast_debug(1, "%s failed on channel '%s', hanging up\n", opname, ast_channel_name(chan));
	} else {
		ast_debug(3, "Bridging handling finished successfully for %s\n", ast_channel_name(chan));
	}
	return res;
}

static int sca_incoming_unhold(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	int res;
	int appearance;
	const char *endpoint_name = ast_sorcery_object_get_id(session->endpoint);
	struct sca_subscription *sca_sub;

	if (!session->channel) {
		return 0;
	}
	if (is_hold(session, rdata)) {
		/* This callback will get called on holds too, if that happens, return immediately because we don't need to do anything for them here, only in the high priority callback. */
		return 0;
	}

	sca_sub = sca_sub_by_endpoint(endpoint_name);
	if (!sca_sub) {
		ast_debug(4, "Receive INVITE for endpoint %s, with no associated %s subscription, ignoring\n", endpoint_name, CALLINFO_EVENT);
		return 0; /* Let another module respond. */
	}
	if (!to_is_endpoint(session, rdata)) {
		ast_debug(3, "INVITE does not require any bridging manipulation\n");
		ao2_ref(sca_sub, -1);
		return 0; /* return 0 (do not goto cleanup) */
	}

	/* In process_hold_unhold, when we called set_appearance_force, we added the session to the according appearance list.
	 * So retrieve the appearance we're supposed to use. */
	appearance = sca_appearance_by_session(sca_sub, session);
	if (!appearance) {
		ast_log(LOG_WARNING, "No appearance associated with %s\n", ast_channel_name(session->channel));
		goto cleanup;
	}

	ast_debug(2, "Received INVITE on appearance %d (post-channel creation: %p = %s)\n", appearance, session, ast_channel_name(session->channel));

	/* Not 100% sure why these other branches are getting hit, but we do need them to avoid issues. */
	if (!session->inv_session->invite_tsx) {
		ast_log(LOG_WARNING, "No associated SIP transaction?\n");
	} else if (!session->inv_session->last_answer) {
		ast_log(LOG_WARNING, "No answer created before?\n"); /* PJSIP will assert if we don't check this. */
	} else {
		/* We should answer the channel, or otherwise if a phone picks up a held call, it won't see
		 * the option to hold it again, etc. */
		ast_debug(3, "Answering channel %s\n", ast_channel_name(session->channel));
		ast_channel_lock(session->channel);
		ast_setstate(session->channel, AST_STATE_RING);
		ast_channel_unlock(session->channel);
		ast_raw_answer(session->channel);
	}

	ast_channel_lock(session->channel);
	if (ast_channel_state(session->channel) != AST_STATE_UP) {
		/* Regardless of whether we actually call ast_raw_answer or not, the channel MUST
		 * be in the UP state. If not, and we fail to barge, ast_queue_hangup will have no
		 * effect and the channel will simply hang forever.
		 * This ensure that we can terminate the channel and the session if needed.
		 * Ideally, however, we will have already answered it for real.
		 */
		ast_log(LOG_WARNING, "Setting channel to answered anyways\n");
		ast_setstate(session->channel, AST_STATE_UP);
	}
	ast_channel_unlock(session->channel);

	res = do_barge_unhold(session->channel, sca_sub, appearance, endpoint_name);
	if (res) {
		if (session->inv_session->dlg->state != PJSIP_DIALOG_STATE_ESTABLISHED) {
			int response = 480;
			pjsip_tx_data *packet;
			ast_debug(1, "Bridging failed on channel '%s', sending response of '%d'\n", ast_channel_name(session->channel), response);
			session->defer_terminate = 1;
			ast_hangup(session->channel);
			if (pjsip_inv_end_session(session->inv_session, response, NULL, &packet) == PJ_SUCCESS && packet) {
				ast_sip_session_send_response(session, packet);
			}
		} else {
			ast_debug(1, "Bridging failed on channel '%s', hanging up\n", ast_channel_name(session->channel));
			/* Interestingly, once we do this, Polycom phones prevent the user from trying to barge again on that call.
			 * That might be intentional, not sure, but since it won't work in the future anyways, that's fine. */
			ast_queue_hangup(session->channel); /* This doesn't work unless the channel state is UP. */
			/*! \todo BUGBUG Even if the channel IS up, this still doesn't seem to work sometimes.
			 * We can't call ast_hangup because that will cause a crash, so dunno... */
		}
	}
	/* Don't proceed to the dialplan, no matter what. */

cleanup:
	/* Don't send any response, no matter what. We already accepted the INVITE, so that doesn't make any sense. */
	ao2_ref(sca_sub, -1);
	return 1;
}

static int sca_incoming_invite(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	char *callinfo, *callid, *contact;
	int appearance;
	const char *endpoint_name = ast_sorcery_object_get_id(session->endpoint);
	struct sca_subscription *sca_sub = sca_sub_by_endpoint(endpoint_name);
	if (!sca_sub) {
		ast_debug(4, "Receive INVITE for endpoint %s, with no associated %s subscription, ignoring\n", endpoint_name, CALLINFO_EVENT);
		return 0; /* Let another module respond. */
	}

	/* Check what appearance the INVITE is using. */
	callinfo = rdata_get_header_value(rdata, "Call-Info");
	if (!callinfo) {
		/* This can happen if phones/endpoints that don't "explicitly" support
		 * Shared Call Appearance are also using this SCA.
		 * For example, an analog phone on an ATA, or a SIP phone that
		 * doesn't support SCA or isn't set up as a shared line.
		 * In this case, we just assume appearance 1 and continue.
		 */
		ast_debug(1, "Received INVITE without Call-Info header, assuming appearance is 1\n");
		callinfo = ast_strdup(""); /* So we don't have to handle this allocation case specially. */
		if (!callinfo) {
			goto cleanup;
		}
		appearance = 1;
	} else {
		/* Parse the Call-Info header */
		appearance = parse_appearance(callinfo);
		if (appearance <= 0) {
			ast_log(LOG_WARNING, "Call-Info header is malformed: %s\n", callinfo);
			ast_free(callinfo);
			goto cleanup;
		}
	}

	callid = callid_from_msg(rdata->msg_info.msg);
	if (!callid) {
		ast_free(callinfo);
		goto cleanup;
	}

	contact = contact_from_msg(rdata->msg_info.msg);
	if (!contact) {
		ast_free(callinfo);
		ast_free(callid);
		goto cleanup;
	}

	ast_debug(2, "Received INVITE on appearance %d for endpoint %s with active %s subscription (Call-Info: %s, contact: %s)\n",
		appearance, endpoint_name, CALLINFO_EVENT, callinfo, contact);

	/* This function gets called on INVITEs to hold/unhold as well, not necessarily new calls.
	 * If it's a re-INVITE, then handle the SDP update for hold/unhold/barge, etc. */
	if (!process_hold_unhold(sca_sub, session, rdata, callinfo, callid, contact, appearance)) {
		/* This is success for hold/unhold, not failure */
		ast_free(callinfo);
		/* callid, contact are saved references on the appearances on success, don't free them */
		send_update(sca_sub);
		ao2_ref(sca_sub, -1);
		return 0;
	}

	ast_free(callinfo); /* Don't need anymore. */

	/* Okay, we're trying to make a new call. */
	if (set_appearance(sca_sub, appearance, SCA_APPEARANCE_PROGRESSING, callid, contact, session)) {
		ast_free(callid);
		ast_free(contact);
		goto cleanup;
	} else {
		/* So the Broadworks spec is strangely silent on where we get the appearance-uri from for *outgoing* calls.
		 * For incoming calls, it seems to be from the To header URI.
		 * For outgoing calls, you can find a "B Foo" referenced in the example NOTIFYs, but this doesn't appear
		 * anywhere outside of the Call-Info headers themselves.
		 * However, it's not that hard to figure out.  Clearly, the logical thing to do is use the To URI
		 * for the outgoing call; we'll get the number from that, even though we won't have a name.
		 *
		 * And yes, we do want to save appearance-uri for outgoing calls, that way a user
		 * can press and hold the line key and it will display the number of the party (called party, in this case).
		 *
		 * XXX A possible future optimization might be to, when a call transitions from progressing to answered,
		 * check the Connected Line on the channel to see what the name is and then rebuild the appearance-uri
		 * to include that.
		 */
		save_appearance_uri(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)], rdata->msg_info.msg);

		/* Technically, we are supposed to terminate the line-seize subscription when we get an INVITE.
		 * We've already updated our bookkeeping from seized to progressing, so it actually doesn't
		 * matter if we just let it expire normally. The behavior is correct either way. */
		/*! \todo not 100% true, it would be nice to be able to explicitly cancel any seizure subscriptions (can only be 1 per appearance) */
		send_update(sca_sub);
	}
	/* Don't free callid on success, it's now associated with the appearance. */
	ast_debug(1, "Outgoing call from %s on appearance %d\n", endpoint_name, appearance);

	/* Everything looks good to us, so let things continue normally. */
	ao2_ref(sca_sub, -1);
	return 0;

cleanup:
	ao2_ref(sca_sub, -1);
	send_response(session, rdata, 480);
	return 1;
}

static int domain_from_sca_sub(struct sca_subscription *sca_sub, char *buf, size_t len)
{
	struct sca_sip_sub *subitem;
	struct sca_sip_sub_list *sublist = &sca_sub->subs;

	/* We can use the first ast_sip_subscription in the linked list for this SCA subscription object.
	 * It may not necessarily correspond to THIS particular contact, but the domain is the same for all.
	 * We just need to use some valid subscription. */
	AST_RWLIST_RDLOCK(sublist);
	AST_LIST_TRAVERSE(sublist, subitem, entry) {
		ast_sip_subscription_get_local_uri(subitem->sub, buf, len);
		break;
	}
	AST_RWLIST_UNLOCK(sublist);
	if (!subitem) {
		return -1;
	}
	return 0;
}

static void sca_outgoing_invite(struct ast_sip_session *session, struct pjsip_tx_data *tdata)
{
	struct ast_str *str;
	char *callid, *contact;
	int appearance;
	char domain[256];
	const char *endpoint_name = ast_sorcery_object_get_id(session->endpoint);
	struct sca_subscription *sca_sub;

	/* The official Broadworks spec is a little scanty on details
	 * for calls to phones (as opposed to calls from phones), but essentially
	 * we need to add an appearance to the INVITE here. */

	sca_sub = sca_sub_by_endpoint(endpoint_name);
	if (!sca_sub) {
		return;
	}

	if (domain_from_sca_sub(sca_sub, domain, sizeof(domain))) {
		goto cleanup;
	}

	/* Get the Call ID */
	callid = callid_from_msg(tdata->msg);
	if (!callid) {
		goto cleanup;
	}

	contact = contact_from_msg(tdata->msg);
	if (!contact) {
		ast_free(callid);
		goto cleanup;
	}

	/* If this is the first outgoing INVITE we send for this call, then there
	 * won't yet be any appearances for this call.
	 *
	 * If we don't find the call, then we'll assign an appearance: specifically,
	 * the lowest number appearance that is currently available.
	 *
	 * If we do find the call, great! Somebody has already done this work for us.
	 * We merely need to grab the appearance number.
	 *
	 * In both cases, we'll include the appearance number in the INVITE that gets sent.
	 * We're not sending any updates per se (NOTIFY), because all the phones are getting
	 * an INVITE with this information.
	 *
	 * So, how, you might ask, do we actually identify that two INVITEs are for
	 * the same call, and should use the same appearance? We can't use the Call ID,
	 * as that is unique for each INVITE. In fact, even in Broadworks they are unique.
	 * The contact is not going to be unique, in general, either.
	 * A clever trick however that we can do is check what channel the SIP session
	 * is associated with. In fact, the channels themselves will also be different.
	 * However, if it's the same outgoing call, their linked IDs will be the same.
	 *
	 * Also, in appearance_by_callid, we grab the lock here and hold it until after an appearance is assigned,
	 * so that the check and set are done atomically. Otherwise we could have a race condition
	 * due to multiple INVITEs going out for multiple contacts.
	 *
	 * Also: Broadworks dispatches NOTIFYs before the INVITEs, so we do the same here.
	 * appearance_by_callid will send an update if an appearance was assigned (so only once per call, not per contact's INVITE)
	 */

	appearance = appearance_by_callid(sca_sub, SCA_APPEARANCE_ALERTING, callid, contact, session, tdata->msg);
	if (!appearance) {
		/* If we didn't get an appearance, then there's nothing we can really do... */
		/* Free these on failure only. */
		ast_free(callid);
		ast_free(contact);
		goto cleanup;
	}

	/* Now add the appearance number to the outgoing INVITE. */
	str = ast_str_create(128);
	if (!str) {
		goto cleanup;
	}

	/* Domain already has a sip: in it, so not needed in the <%s> */
	ast_str_append(&str, 0, "<%s>;appearance-index=%d;appearance-state=%s",
		domain, appearance, sca_appearance_state_str(SCA_APPEARANCE_ALERTING));

	/* Add the final Call-Info header to the response. */
	ast_debug(3, "Call-Info header: %s\n", ast_str_buffer(str));
	ast_sip_add_header(tdata, "Call-Info", ast_str_buffer(str));
	ast_free(str);

cleanup:
	ao2_ref(sca_sub, -1);
}

static void sca_incoming_answer(struct ast_sip_session *session, struct pjsip_rx_data *rdata)
{
	char *callid, *contact;
	const char *endpoint_name = ast_sorcery_object_get_id(session->endpoint);
	struct sca_subscription *sca_sub;

	/* Same comment as in sca_outgoing_answer: we only care about 200 responses. */
	if (rdata->msg_info.msg->line.status.code != 200) {
		return;
	}

	sca_sub = sca_sub_by_endpoint(endpoint_name);
	if (!sca_sub) {
		return;
	}

	/* Get the Call ID */
	callid = callid_from_msg(rdata->msg_info.msg);
	if (!callid) {
		goto cleanup;
	}
	contact = contact_from_msg(rdata->msg_info.msg);
	if (!contact) {
		ast_free(callid);
		goto cleanup;
	}

	/* This one is easy.
	 * Somebody just answered the call. Change from SCA_APPEARANCE_ALERTING
	 * to SCA_APPEARANCE_ACTIVE and send an update. */

	if (set_appearance(sca_sub, -1, SCA_APPEARANCE_ACTIVE, callid, contact, session)) {
		ast_free(callid);
		ast_free(contact);
	} else {
		/* Notify all contacts that call is now answered.
		 * Otherwise they'll just think the call itself was cancelled and stop the blinking lights. */
		send_update(sca_sub);
	}
cleanup:
	ao2_ref(sca_sub, -1);
}

static void sca_outgoing_answer(struct ast_sip_session *session, struct pjsip_tx_data *tdata)
{
	char *contact, *callid;
	const char *endpoint_name = ast_sorcery_object_get_id(session->endpoint);
	struct sca_subscription *sca_sub;

	/* This callback will get executed for both Progress and Answer.
	 * We don't care about progress since we already set the state to SCA_APPEARANCE_PROGRESSING when we approved the INVITE.
	 * So bail if it's not a 200 OK response. */
	if (tdata->msg->line.status.code != 200) {
		return;
	}

	sca_sub = sca_sub_by_endpoint(endpoint_name);
	if (!sca_sub) {
		return; /* Don't care. */
	}

	/* This one is easy... just look for something that's currently in the SCA_APPEARANCE_PROGRESSING state,
	 * with the same Call ID as whatever we've got here... */
	callid = callid_from_msg(tdata->msg);
	if (!callid) {
		ao2_ref(sca_sub, -1);
		return;
	}

	contact = contact_from_msg(tdata->msg);
	if (!contact) {
		ast_free(callid);
		ao2_ref(sca_sub, -1);
		return;
	}

	ast_debug(3, "SCA call answered: %s\n", callid);
	if (set_appearance(sca_sub, -1, SCA_APPEARANCE_ACTIVE, callid, contact, session)) {
		ast_free(callid); /* Only free on failure */
		ast_free(contact);
	} else {
		send_update(sca_sub);
	}
	ao2_ref(sca_sub, -1);
}

static void sca_hangup(struct ast_sip_session *session)
{
	const char *endpoint_name = ast_sorcery_object_get_id(session->endpoint);
	struct sca_subscription *sca_sub = sca_sub_by_endpoint(endpoint_name);

	if (!sca_sub) {
		return; /* Don't care. */
	}

	/* Oof, so since we don't have access to the msg, somehow we need to figure out what appearance to clear.
	 * We know that it is something in state SCA_APPEARANCE_PROGRESSING or SCA_APPEARANCE_ACTIVE.
	 * But wait... we store the session on the appearance from the INVITE and/or answer.
	 * So simply look for something with the same session pointer. We don't actually reference it
	 * so we don't need to refbump or anything. We only care if the pointer matches what we had before.
	 */

	ast_debug(3, "SCA call session ended (%p)\n", session); /* Includes BYE, CANCEL, etc. */

	/* We want to be really careful here. The hangup callback will execute for a number of reasons,
	 * including when a contact answers and the other forks of the call are cancelled.
	 * Obviously, if this type of thing happens, the event should simply be ignored.
	 * Otherwise, we would immediately go from active to idle if there are multiple appearances... not good!
	 *
	 * To prevent changing states erroneously when we shouldn't, we'll need to be extra careful,
	 * and actually verify that there are no other channels left that are using the appearance.
	 * This is why we keep track of usecount within set_appearance.
	 */

	if (!set_appearance(sca_sub, -1, SCA_APPEARANCE_IDLE, NULL, NULL, session)) {
		send_update(sca_sub);
	}

	ao2_ref(sca_sub, -1);
}

static struct ast_sip_session_supplement sca_invite_supplement = {
	.method = "INVITE",
	.priority = AST_SIP_SUPPLEMENT_PRIORITY_FIRST, /* We must be able to reject this INVITE outright, before the channel is created. */
	.incoming_request = sca_incoming_invite, /* INVITE from phone */
	.incoming_response = sca_incoming_answer, /* Phone answered a call to it */
	.outgoing_request = sca_outgoing_invite, /* INVITE to phone */
	.outgoing_response = sca_outgoing_answer, /* The response is for the INVITE */
	.session_end = sca_hangup, /* Either side hung up... this isn't an OK because res_pjsip_session doesn't check the method for this one, so tack it on to this supplement. */
};

/*! \brief For joining to existing calls, we do need the channel, so this supplement has a different priority. */
static struct ast_sip_session_supplement sca_invite_supplement2 = {
	.method = "INVITE",
	.priority = AST_SIP_SUPPLEMENT_PRIORITY_CHANNEL + 1,
	.incoming_request = sca_incoming_unhold, /* INVITE from phone */
};

/* It may be desirable to integrate non-PJSIP or non-supporting endpoints
 * into a shared line appearance in a seamless manner.
 * For example, analog lines (albeit they could only use the "first" appearance).
 * If they're using PJSIP, like with an ATA, then it will work fine, since
 * the endpoint itself doesn't need to do anything to make the SCA work,
 * other than provide an appearance number (which defaults to 1 if not provided).
 * However, the phone could be using some other channel technology, like DAHDI.
 * Obviously that is beyond the scope of this module, but this provides a way
 * to at least have a unified cross-technology shared appearance. The user
 * will be responsible for manually calling this application in the appropriate
 * places in the dialplan.
 *
 * Obviously, there are some limitations to this, i.e. at least some of the
 * devices involved need to be PJSIP endpoints that do subscribe.
 *
 * For example, if a DAHDI line goes off hook with immediate=yes, user can call:
 * exten => s,1,PJSIPSharedCallAppearance(MySharedLine,1,seized)
 *     same => n,WaitExten(,d) ; provide dial tone to collect digits
 *
 * Alternately, if it sends calls directly to Asterisk (collects digits locally):
 * exten => s,1,PJSIPSharedCallAppearance(MySharedLine,1,progressing)
 *
 * PJSIPSharedCallAppearance will first check if we can seize appearance 1 on MySharedLine.
 * (You could use a different appearance number in theory but you probably want 1
 *  since non-PJSIP stuff would likely only have one "appearance" available.)
 * If it can, it will seize it.
 * If it's seized, it will return -1 and end dialplan execution immediately.
 * If it's active or held, it will barge in on or unhold the call
 * and dialplan execution will not continue to the next priority.
 *
 * And in a pre-dial handler when calling the phone:
 * exten => s,1,PJSIPSharedCallAppearance(MySharedLine,1,alerting)
 *     same => n,Return()
 *
 * This will likewise set the appearance state to alerting if possible,
 * and then install the framehook. Note that although normally
 * the module assigns the appearance number on incoming calls here,
 * that can be done manually here. If the appearance is not idle,
 * the call will be cancelled.
 *! \todo Can we do this from a pre-dial handler? The U option allows it but not the b option.
 * Might need to be creative in how we queue congestion.
 *
 * If no appearance number is specified, it will be automatically assigned,
 * but note if the appearance is higher than 1, it may not be available
 * to your analog phones, so explicitly specifying an appearance is recommended
 * for this reason.
 *
 * After the framehook has been installed, the framehook will then
 * update the SCA appearance when the channel is answered, etc. by processing
 * control frames appropriately, notably:
 * - AST_CONTROL_HOLD, AST_CONTROL_UNHOLD (regular hold only, no private hold)
 * - AST_CONTROL_PROGRESS, AST_CONTROL_RINGING (progressing)
 * - AST_CONTROL_ANSWER (call answered)
 * - AST_CONTROL_HANGUP
 *
 * When the channel dies, as part of cleanup, usecount is decremented.
 *
 * As far as the bridging itself goes, this is done as it is normally.
 * The only difference is that channels are added to our container
 * while they are active (while the framehook is running, basically),
 * so we can find what bridge they are in and use that during runtime.
 *
 * So this isn't quite a replacement for app_sla (this doesn't even work the same way),
 * it's just a different method of providing similar functionality.
 * It's primarily intended for allowing non-PJSIP stuff to be bridged on appearances
 * that do have some PJSIP endpoints. Otherwise, you should just use app_sla instead
 * since that actually does everything for you.
 */

struct sca_framehook_data {
	struct ast_channel *chan;
	char *endpoint;
	int appearance;
	int framehook_id;
	enum sca_appearance_state state;
	struct sca_subscription *sca_sub;
	unsigned int failed:1;
};

static void sca_destroy_cb(void *data)
{
	struct sca_framehook_data *sfd = data;

	sca_remove_channel(&sfd->sca_sub->appearances[APPEARANCE_ARRAY_INDEX(sfd->appearance)], sfd->chan);
	/* Call set_appearance_cust with SCA_APPEARANCE_IDLE to decrement the usecount for the appearance.
	 * It's okay to provide sca->chan as it will not be added to the chanlist if we're requesting idle. */
	if (!sfd->failed) { /* If we failed, we never set the appearance in the first place, so don't try to undo something we never did. */
		if (set_appearance_cust_force(sfd->chan, sfd->sca_sub, sfd->appearance, SCA_APPEARANCE_IDLE)) {
			ast_debug(3, "Failed to set appearance to idle\n");
		} else {
			send_update(sfd->sca_sub);
		}
	}
	ao2_ref(sfd->sca_sub, -1); /* Remove the reference added in sca_custom_exec */
	ast_free(sfd->endpoint);
	ast_free(sfd);
}

static const struct ast_datastore_info sca_framehook_datastore = {
	.type = "res_pjsip_sca_framehook",
	.destroy = sca_destroy_cb,
};

static struct ast_frame *sca_framehook(struct ast_channel *chan, struct ast_frame *f, enum ast_framehook_event event, void *data)
{
	int res = 0;
	enum sca_appearance_state oldstate;
	struct sca_framehook_data *sfd = data;

	if (!f || f->frametype != AST_FRAME_CONTROL) {
		return f;
	}

	oldstate = sfd->sca_sub->appearances[APPEARANCE_ARRAY_INDEX(sfd->appearance)].state;

	switch (f->subclass.integer) {
	case AST_CONTROL_PROGRESS:
	case AST_CONTROL_RINGING:
		sfd->state = SCA_APPEARANCE_PROGRESSING;
		res = set_appearance_cust_force(chan, sfd->sca_sub, sfd->appearance, sfd->state);
		break;
	case -1: /* For some reason answer seems to come through as -1? */
	case AST_CONTROL_ANSWER:
		sfd->state = SCA_APPEARANCE_ACTIVE;
		res = set_appearance_cust_force(chan, sfd->sca_sub, sfd->appearance, sfd->state);
		break;
	case AST_CONTROL_HANGUP:
		/* Call hung up. */
	case AST_CONTROL_HOLD:
	case AST_CONTROL_UNHOLD:
	default:
		/* Don't care. */
		ast_debug(3, "SCA custom hook: ignoring control frame %d on channel %s\n", f->subclass.integer, ast_channel_name(chan));
		return f;
	}

	if (res) {
		ast_log(LOG_WARNING, "Failed to transition %s from %s to %s\n", ast_channel_name(chan), sca_appearance_state_str(oldstate), sca_appearance_state_str(sfd->state));
		sfd->state = oldstate; /* Revert our state */
	} else {
		ast_debug(1, "SCA custom hook: transitioned %s from %s to %s\n", ast_channel_name(chan), sca_appearance_state_str(oldstate), sca_appearance_state_str(sfd->state));
		send_update(sfd->sca_sub);
	}

	return f;
}

static int sca_framehook_consume(void *data, enum ast_frame_type type)
{
	return (type == AST_FRAME_CONTROL ? 1 : 0);
}

static int sca_add_framehook_callback(struct ast_channel *chan, struct sca_framehook_data *sfd)
{
	struct ast_datastore *datastore = NULL;
	static struct ast_framehook_interface sca_framehook_interface = {
		.version = AST_FRAMEHOOK_INTERFACE_VERSION,
		.event_cb = sca_framehook,
		.consume_cb = sca_framehook_consume,
		/* No destroy_cb because the datastore has its own destructor that is responsible for freeing sfd. */
		.disable_inheritance = 1,
	};
	SCOPED_CHANNELLOCK(chan_lock, chan);

	sca_framehook_interface.data = sfd;
	datastore = ast_channel_datastore_find(chan, &sca_framehook_datastore, NULL);
	if (datastore) {
		ast_log(AST_LOG_WARNING, "SCA framehook already set on '%s'\n", ast_channel_name(chan));
		return 0;
	}

	datastore = ast_datastore_alloc(&sca_framehook_datastore, NULL);
	if (!datastore) {
		return -1;
	}

	sfd->framehook_id = ast_framehook_attach(chan, &sca_framehook_interface);
	if (sfd->framehook_id < 0) {
		ast_log(AST_LOG_WARNING, "Failed to attach SCA framehook to '%s'\n", ast_channel_name(chan));
		ast_datastore_free(datastore);
		ast_free(sfd);
		return -1;
	}
	datastore->data = sfd;

	ast_channel_datastore_add(chan, datastore);
	ast_debug(4, "Set up SCA framehook callback on %s\n", ast_channel_name(chan));
	return 0;
}

/*! \brief Remove the framehook, but not the datastore */
static int remove_sca_framehook(struct ast_channel *chan)
{
	struct ast_datastore *datastore = NULL;
	struct sca_framehook_data *sfd;
	SCOPED_CHANNELLOCK(chan_lock, chan);

	datastore = ast_channel_datastore_find(chan, &sca_framehook_datastore, NULL);
	if (!datastore) {
		ast_log(AST_LOG_WARNING, "Cannot remove SCA from %s: SCA not currently enabled\n", ast_channel_name(chan));
		return -1;
	}
	sfd = datastore->data;
	sfd->failed = 1; /* remove_sca_framehook is only called if we fail to do what we wanted, so signal that to the destructor */

	if (ast_framehook_detach(chan, sfd->framehook_id)) {
		ast_log(AST_LOG_WARNING, "Failed to remove SCA framehook from channel %s\n", ast_channel_name(chan));
		return -1;
	}
	sfd->framehook_id = -1;

	return 0;
}

static int sca_custom_register(struct ast_channel *chan, const char *endpoint, int appearance, enum sca_appearance_state initstate, struct sca_subscription *sca_sub)
{
	struct sca_framehook_data *sfd;

	sfd = ast_calloc(1, sizeof(*sfd));

	if (!sfd) {
		return -1;
	}
	sfd->endpoint = ast_strdup(endpoint);
	if (!sfd->endpoint) {
		ast_free(sfd);
		return -1;
	}
	sfd->appearance = appearance;
	sfd->state = initstate;
	sfd->chan = chan;
	sfd->sca_sub = sca_sub;

	return sca_add_framehook_callback(chan, sfd);
}

static const char *app = "PJSIPSharedCallAppearance";

static int sca_custom_exec(struct ast_channel *chan, const char *data)
{
	struct sca_subscription *sca_sub;
	char *appdata;
	enum sca_appearance_state initstate;
	int appearance = 1; /* Default to 1. */
	int barge = 0;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(endpoint);
		AST_APP_ARG(appearance);
		AST_APP_ARG(state);
		AST_APP_ARG(options);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "Missing arguments\n");
		return -1;
	}
	appdata = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, appdata);

	if (ast_strlen_zero(args.endpoint)) {
		ast_log(LOG_WARNING, "Missing SCA endpoint name\n");
		return -1;
	} else if (ast_strlen_zero(args.state)) {
		ast_log(LOG_WARNING, "Missing SCA initial state\n");
		return -1;
	}

	if (!ast_strlen_zero(args.appearance)) {
		appearance = atoi(args.appearance);
		if (appearance <= 0) {
			ast_log(LOG_WARNING, "Invalid appearance number: %s\n", args.appearance);
			return -1;
		}
	}

	if (!ast_strlen_zero(args.options)) {
		barge = strchr(args.options, 'b') ? 1 : 0;
	}

	initstate = sca_appearance_state_from_str(args.state);
	/* If off-hook and collecting digits, SEIZED. If made outgoing call, PROGRESSING. If receiving incoming, ALERTING. */
	if (initstate != SCA_APPEARANCE_SEIZED && initstate != SCA_APPEARANCE_PROGRESSING && initstate != SCA_APPEARANCE_ALERTING) {
		ast_log(LOG_WARNING, "State not allowed for SCA custom initialization:  %s\n", args.state);
		return -1;
	}

	/* Make sure the endpoint exists. */
	sca_sub = sca_sub_by_endpoint(args.endpoint);
	if (!sca_sub) {
		ast_log(LOG_WARNING, "No such SCA endpoint: %s\n", args.endpoint);
		return -1;
	}

	/* Note that sca_custom_register and set_appearance_cust will unref sca_sub on failure,
	 * so we don't (generally) need to clean up in this function. */

	/* Add the framehook to the channel, and register the channel in the chanlist. */
	if (sca_custom_register(chan, args.endpoint, appearance, initstate, sca_sub)) {
		return -1; /* Do NOT unref. */
	}

	/* See if we can set the appearance.
	 * If we succeed, then this channel is automatically added to the chanlist.
	 */
	if (set_appearance_cust(chan, sca_sub, appearance, initstate)) {
		int bargeable = APPEARANCE_HELD(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)]) || sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state == SCA_APPEARANCE_ACTIVE;

		/* Okay, Plan B: If we tried to seize and fail, but there's a call active, and we're allowed to, barge into it. */
		/* Remove the framehook, not needed if we fail.
		 * In theory we'll return -1 now to hang up the channel, but just in case we don't or the caller does TryExec
		 * for some convoluted reason, we don't want this channel to persist in the chanlist. */

		/* We don't need the framehook after all, so remove it.
		 * remove_sca_framehook will only remove the framehook, not the datastore, so it will not decrement the refcount,
		 * which is good because we need a ref for what we're going to try next. */
		remove_sca_framehook(chan);

		ast_debug(3, "Appearance %d is %s joinable (%s)\n", appearance, bargeable ? "currently" : "not",
			sca_appearance_state_str(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state));
		if (bargeable && barge) {
			/* Similar logic as in process_hold_unhold, but a little bit simpler. */
			enum sca_appearance_state oldstate;

			/* Don't allow resuming private holds. Putting a call on private hold is only supported for phones
			 * that actually natively support SCA, and since we don't, we couldn't have put it on the private hold in the first place. */
			if (sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state == SCA_APPEARANCE_HELD_PRIVATE) {
				ast_debug(1, "Rejecting attempt to resume privately held call on appearance %d by %s\n", appearance, ast_channel_name(chan));
			} else {
				oldstate = sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state;
				sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount += 1;
				if (!set_appearance_cust_force(chan, sca_sub, appearance, SCA_APPEARANCE_ACTIVE)) {
					ast_debug(4, "Bumped the use count of appearance %d up to %d\n", appearance, sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount);
					if (APPEARANCE_STATE_ACTIVE(oldstate)) {
						ast_debug(1, "Barge-in on appearance %d by %s\n", appearance, ast_channel_name(chan));
					} else {
						ast_debug(1, "Hold resumed on appearance %d by %s\n", appearance, ast_channel_name(chan));
					}
					/* Actually do the barge or unhold. */
					send_update(sca_sub);
					do_barge_unhold(chan, sca_sub, appearance, args.endpoint);
					/* When we're done, we go idle. */
					if (set_appearance_cust_force(chan, sca_sub, appearance, SCA_APPEARANCE_IDLE)) {
						ast_debug(3, "Failed to set appearance to idle\n");
					} else {
						send_update(sca_sub);
					}
				} else {
					sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].usecount -= 1; /* Undo what we did. */
				}
			}
			/* Don't decrement sca_sub refcount, the datastore on the channel will ensure we get cleaned up properly. */
		} else if (!bargeable) {
			ast_debug(1, "Appearance %d is not currently joinable (bargeable or on hold)\n", appearance);
		} else if (!barge) {
			ast_debug(1, "Channel %s is not allowed to barge in on calls\n", ast_channel_name(chan));
		}

		/* Channel should now hangup. */
		return -1;
	} else {
		send_update(sca_sub);
	}

	/* Do NOT unref on success. The framehook has the +1'd refed copy which will get unref when the framehook destructor is called. */
	return 0;
}

/* == End of non-PJSIP SCA integration == */

static enum ast_device_state sca_state(const char *data)
{
	/* Simple, convert the current state of an SCA endpoint + appearance to a device state */
	char *buf, *endpoint, *appearance_str;
	int appearance = 0;
	enum ast_device_state res;
	struct sca_subscription *sca_sub;

	appearance_str = buf = ast_strdupa(data);
	if (ast_strlen_zero(appearance_str)) {
		ast_log(LOG_WARNING, "Empty SCA device?\n");
		return AST_DEVICE_INVALID;
	}
	endpoint = strsep(&appearance_str, "_");
	if (ast_strlen_zero(endpoint)) {
		ast_log(LOG_WARNING, "Empty SCA device?\n");
		return AST_DEVICE_INVALID;
	}

	if (!ast_strlen_zero(appearance_str)) {
		appearance = atoi(appearance_str);
	}
	if (appearance < 0 || appearance > MAX_APPEARANCES) {
		ast_log(LOG_WARNING, "Invalid call appearance: %s\n", appearance_str);
	}

	/* If we have an appearance, then limit the conversion to that appearance.
	 * Otherwise, just aggregate all the appearances. */

	sca_sub = sca_sub_by_endpoint(endpoint);
	if (!sca_sub) {
		ast_log(LOG_WARNING, "No SCA endpoint for %s\n", endpoint);
		return AST_DEVICE_INVALID;
	}

	if (appearance) {
		res = sca_to_ast_devstate(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(appearance)].state);
	} else {
		res = sca_aggregate_state(sca_sub);
	}

	ast_debug(4, "Device state for %s (%d) = %s\n", endpoint, appearance, ast_devstate2str(res));
	ao2_ref(sca_sub, -1);
	return res;
}

static char *handle_show_appearances(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	int i;
	int contacts = 0;
	struct sca_subscription *sca_sub;
	struct sca_sip_sub *sipsub;
	struct sca_session *s;
	struct sca_channel *c;
	int sessioncount = 0, chancount = 0;
	struct subscription_item *subitem;
	char *ret = NULL;
	int which = 0;
	int refcount;

	switch (cmd) {
	case CLI_INIT:
		e->command = "pjsip show sca appearances";
		e->usage =
			"Usage: pjsip show sca appearances [endpoint [appearance]]\n"
			"       Show line appearances for an endpoint\n";
		return NULL;
	case CLI_GENERATE:
		if (a->argc == 5) {
			/* Tab completion for SCA endpoint names */
			AST_RWLIST_RDLOCK(&sublist);
			AST_LIST_TRAVERSE(&sublist, subitem, entry) {
				if (!strncasecmp(a->word, subitem->endpoint, strlen(a->word)) && ++which > a->n) {
					ret = ast_strdup(subitem->endpoint);
					break;
				}
			}
			AST_RWLIST_UNLOCK(&sublist);
			return ret;
		}
		return NULL;
	}

	/* Show all endpoints with call-info subscriptions. */
	if (a->argc == 4) {
		int total = 0;
		AST_RWLIST_RDLOCK(&sublist);
		AST_LIST_TRAVERSE(&sublist, subitem, entry) {
			ast_cli(a->fd, "%s\n", subitem->endpoint);
			total++;
		}
		AST_RWLIST_UNLOCK(&sublist);
		ast_cli(a->fd, "%d endpoint%s with SCA subscriptions\n", total, ESS(total));
		return CLI_SUCCESS;
	}

	if (a->argc != 5 && a->argc != 6) {
		return CLI_SHOWUSAGE;
	}

	sca_sub = sca_sub_by_endpoint(a->argv[4]);
	if (!sca_sub) {
		ast_cli(a->fd, "No appearances for endpoint %s\n", a->argv[4]);
		return CLI_FAILURE;
	}

	/* Show internal details of just a specific appearance. */
	if (a->argc == 6) {
		i = atoi(a->argv[5]);
		/* Array bounds check! */
		if (i < 0 || i > MAX_APPEARANCES || APPEARANCE_AVAILABLE(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)])) {
			ast_cli(a->fd, "Appearance %d is not in use\n", i);
		} else {
			/* List the sessions. */
			AST_RWLIST_RDLOCK(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)].sessionlist);
			AST_LIST_TRAVERSE(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)].sessionlist, s, entry) {
				/* s->channel could be a reference to a dead channel, so don't print the channel name even if s->channel is not NULL. */
				ast_cli(a->fd, "Native Session: %p\n", s);
				sessioncount++;
			}
			AST_RWLIST_UNLOCK(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)].sessionlist);
			ast_cli(a->fd, "%d active (native) session%s\n", sessioncount, ESS(sessioncount));
			/* List the foreign channels. */
			AST_RWLIST_RDLOCK(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)].chanlist);
			AST_LIST_TRAVERSE(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)].chanlist, c, entry) {
				ast_cli(a->fd, "Foreign Channel: %s\n", ast_channel_name(c->chan));
				chancount++;
			}
			AST_RWLIST_UNLOCK(&sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)].chanlist);
			ast_cli(a->fd, "%d active (foreign) channel%s\n", chancount, ESS(chancount));
		}
		ao2_ref(sca_sub, -1);
		return CLI_SUCCESS;
	}

	for (i = 0; i < MAX_APPEARANCES; i++) {
		if (sca_sub->appearances[i].state == SCA_APPEARANCE_NONE) {
			break;
		}
		if (i == 0) {
			/* Only print the header if there are results. */
			ast_cli(a->fd, "%3s %-13s %-9s %-8s %-13s %-20s %-45s %s\n", "#", "State", "Use Count", "Sessions", "Foreign Chans", "Last Contact", "Last Call ID", "Appearance URI");
		}

		/* Compute session and foreign channel counts */
		sessioncount = sca_session_count(&sca_sub->appearances[i]); /* We already have the array index. */
		chancount = sca_channel_count(&sca_sub->appearances[i]);

		ast_cli(a->fd, "%3d %-13s %9d %8d %13d %-20s %-45s %s\n", APPEARANCE_REAL_INDEX(i), sca_appearance_state_str(sca_sub->appearances[i].state),
			sca_sub->appearances[i].usecount,
			sessioncount, chancount,
			sca_sub->appearances[i].state != SCA_APPEARANCE_IDLE ? sca_sub->appearances[i].contact : "",
			sca_sub->appearances[i].state != SCA_APPEARANCE_IDLE ? sca_sub->appearances[i].callid : "",
			S_OR(sca_sub->appearances[i].uri, "")
		);
	}

	AST_RWLIST_RDLOCK(&sca_sub->subs);
	AST_LIST_TRAVERSE(&sca_sub->subs, sipsub, entry) {
		contacts++;
	}
	AST_RWLIST_UNLOCK(&sca_sub->subs);

	/* Subtract 1 from the ref count because we added one for the CLI command, and we don't want to include that...
	 * otherwise we can't observe the count without mutating it, effectively... */
	refcount = ao2_count(sca_sub) - 1;
	ast_cli(a->fd, "%d total line appearance%s, %d total contact%s, %d reference%s\n", i, ESS(i), contacts, ESS(contacts), refcount, ESS(refcount));
	ao2_ref(sca_sub, -1);
	return CLI_SUCCESS;
}

static char *handle_unseize_appearance(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	int i;
	struct sca_subscription *sca_sub;
	struct subscription_item *subitem;
	char *ret = NULL;
	int which = 0;

	switch (cmd) {
	case CLI_INIT:
		e->command = "pjsip unseize sca appearance";
		e->usage =
			"Usage: pjsip unseize sca appearance [endpoint] [appearance]\n"
			"       Unseize a line appearance for an endpoint\n";
		return NULL;
	case CLI_GENERATE:
		if (a->argc == 5) {
			/* Tab completion for SCA endpoint names */
			AST_RWLIST_RDLOCK(&sublist);
			AST_LIST_TRAVERSE(&sublist, subitem, entry) {
				if (!strncasecmp(a->word, subitem->endpoint, strlen(a->word)) && ++which > a->n) {
					ret = ast_strdup(subitem->endpoint);
					break;
				}
			}
			AST_RWLIST_UNLOCK(&sublist);
			return ret;
		}
		return NULL;
	}

	if (a->argc != 6) {
		return CLI_SHOWUSAGE;
	}

	sca_sub = sca_sub_by_endpoint(a->argv[4]);
	if (!sca_sub) {
		ast_cli(a->fd, "No appearances for endpoint %s\n", a->argv[4]);
		return CLI_FAILURE;
	}

	/* Show internal details of just a specific appearance. */
	i = atoi(a->argv[5]);
	/* Array bounds check! */
	if (i < 0 || i > MAX_APPEARANCES) {
		ast_cli(a->fd, "Appearance %d is not in use\n", i);
	} else {
		int changed = 0;
		/* If the appearance is currently seized, manually unseize it. */
		ast_mutex_lock(&sca_sub->lock);
		if (sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)].state == SCA_APPEARANCE_NONE) {
			ast_cli(a->fd, "Appearance %d does not exist\n", i);
		} else if (sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)].state != SCA_APPEARANCE_SEIZED) {
			ast_cli(a->fd, "Appearance %d is currently %s\n", i, sca_appearance_state_str(sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)].state));
		} else {
			int seizeago = time(NULL) - sca_sub->appearances[APPEARANCE_ARRAY_INDEX(i)].seizetime;
			/* Change it to idle, as requested. If there was a phone actually still seizing this line,
			 * the call will now fail since its seizure has expired. */
			set_appearance_cust_force(NULL, sca_sub, i, SCA_APPEARANCE_IDLE);
			changed = 1;
			ast_cli(a->fd, "Appearance %d has been unseized (was seized for %d second%s)\n", i, seizeago, ESS(seizeago));
		}
		ast_mutex_unlock(&sca_sub->lock);
		if (changed) {
			/* Send a NOTIFY to all the endpoints since appearance state has changed */
			send_update(sca_sub);
		}
	}
	ao2_ref(sca_sub, -1);
	return CLI_SUCCESS;
}

static struct ast_cli_entry sca_cli[] = {
	AST_CLI_DEFINE(handle_show_appearances, "Show shared line appearances"),
	AST_CLI_DEFINE(handle_unseize_appearance, "Unseize shared line appearances"),
};

static int unload_module(void)
{
#if 0
	ast_cli_unregister_multiple(sca_cli, ARRAY_LEN(sca_cli));
	ast_sip_session_unregister_supplement(&sca_invite_supplement);
	ast_sip_session_unregister_supplement(&sca_invite_supplement2);
	ast_sip_unregister_subscription_handler(&sca_handler);
	ast_sip_unregister_subscription_handler(&sca_seize_handler);
	ast_unregister_application(app);
	ast_devstate_prov_del("PJSIPSCA");
	return 0;
#else
	/* Can't unload modules that call ast_sip_register_subscription_handler */
	return -1;
#endif
}

static int load_module(void)
{
	if (ast_sip_register_subscription_handler(&sca_handler)) {
		ast_log(LOG_WARNING, "Unable to register subscription handler %s\n", sca_handler.event_name);
		unload_module();
		return AST_MODULE_LOAD_DECLINE;
	}
	if (ast_sip_register_subscription_handler(&sca_seize_handler)) {
		ast_log(LOG_WARNING, "Unable to register subscription handler %s\n", sca_seize_handler.event_name);
		unload_module();
		return AST_MODULE_LOAD_DECLINE;
	}
	ast_sip_session_register_supplement(&sca_invite_supplement);
	ast_sip_session_register_supplement(&sca_invite_supplement2);
	ast_cli_register_multiple(sca_cli, ARRAY_LEN(sca_cli));
	ast_devstate_prov_add("PJSIPSCA", sca_state);
	return ast_register_application_xml(app, sca_custom_exec);
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "PJSIP Shared Call Appearances",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_CHANNEL_DEPEND + 5,
	.requires = "res_pjsip,res_pjsip_pubsub",
);
