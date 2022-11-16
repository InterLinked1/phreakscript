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

/*** MODULEINFO
	<depend>pjproject</depend>
	<depend>res_pjsip</depend>
	<depend>res_pjsip_pubsub</depend>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <pjsip.h>
#include <pjsip_simple.h>

#include "asterisk/res_pjsip.h"
#include "asterisk/res_pjsip_pubsub.h"
#include "asterisk/module.h"
#include "asterisk/logger.h"
#include "asterisk/presencestate.h"
#include "asterisk/xml.h"
#include "asterisk/astdb.h"

static const char astdb_family[] = "CustomPresence";

static int parse_incoming_xml(char *xmlbody, int xmllen, const char *endpoint)
{
	int res = -1;
	const char *nodename;
	const char *tmpstr;
	struct ast_xml_node *root, *tuple = NULL, *tmpnode = NULL;
	struct ast_xml_doc *xmldoc = ast_xml_read_memory(xmlbody, xmllen);
	enum ast_presence_state state = AST_PRESENCE_NOT_SET;
	int open;
	char note[32] = "";
	char full_dev[256];
	char *dev = full_dev;
	char *subtype = "";

	if (!xmldoc) {
		ast_log(LOG_WARNING, "Failed to parse as XML: %.*s\n", xmllen, xmlbody);
		return -1;
	}

	/*
	 * RFC 3863
	 *
	 * <?xml version="1.0" encoding="UTF-8"?>
	 * <presence entity="sip:username@server:port;transport=tls" xmlns="urn:ietf:params:xml:ns:pidf"
	 *  xmlns:dm="urn:ietf:params:xml:ns:pidf:data-model" xmlns:rpid="urn:ietf:params:xml:ns:pidf:rpid">
	 *  <tuple id="pj85c1bddf8bc24e66a43be6c352067e4a">
	 *   <status>
	 *    <basic>open</basic>
	 *   </status>
	 *   <timestamp>2022-10-23T11:48:04.538Z</timestamp>
	 *   <note>Idle</note>
	 *  </tuple>
	 *  <dm:person id="pjeb5ceedf917e41fb94f38edc5f3a4227">
	 *   <rpid:activities>
	 *    <rpid:unknown />
	 *   </rpid:activities>
	 *   <dm:note>Idle</dm:note>
	 *  </dm:person>
	 * </presence>
	 */

	if (DEBUG_ATLEAST(1)) {
		char *doc_str = NULL;
		int doc_len = 0;
		ast_xml_doc_dump_memory(xmldoc, &doc_str, &doc_len);
		ast_debug(4, "Incoming doc len: %d\n%s\n", doc_len, doc_len ? doc_str : "<empty>");
		ast_xml_free_text(doc_str);
		doc_str = NULL;
		doc_len = 0;
	}

	root = ast_xml_get_root(xmldoc);
	if (!root) {
		ast_log(LOG_WARNING, "No root?\n");
		goto cleanup;
	}
	nodename = ast_xml_node_get_name(root);

	if (strcmp(nodename, "presence")) {
		ast_log(LOG_WARNING, "Unexpected root node name: %s\n", nodename);
		goto cleanup;
	}

	/* The device is not used by Broadworks, and the phone can set this to any value. See 11-BD5196-00, 3.1.2 */
	tuple = ast_xml_find_child_element(root, "tuple", NULL, NULL);
	if (!tuple) {
		ast_log(LOG_WARNING, "Presence update contains no tuple?\n");
		goto cleanup;
	}

	/* Note is optional, but note it down (no pun intended) if we get one. */
	tmpnode = ast_xml_find_child_element(tuple, "note", NULL, NULL);
	if (tmpnode) {
		tmpstr = ast_xml_get_text(tmpnode);
		ast_copy_string(note, tmpstr, sizeof(note));
		ast_xml_free_text(tmpstr);
	}

	tmpnode = ast_xml_find_child_element(tuple, "status", NULL, NULL);
	if (!tmpnode) {
		ast_log(LOG_WARNING, "Tuple missing status node?\n");
		goto cleanup;
	}

	tmpnode = ast_xml_find_child_element(tmpnode, "basic", NULL, NULL);
	if (!tmpnode) {
		ast_log(LOG_WARNING, "status missing basic node?\n");
		goto cleanup;
	}

	tmpstr = ast_xml_get_text(tmpnode);

	/* <basic> must contain either open or closed. */
	/*! \todo We should support more than just available/unavailable, we can parse using ast_presence_state_val */
	if (!strcmp(tmpstr, "open")) {
		open = 1;
	} else if (!strcmp(tmpstr, "closed")) {
		open = 0;
	} else {
		ast_xml_free_text(tmpstr);
		ast_log(LOG_WARNING, "Unexpected status: %s\n", tmpstr);
		goto cleanup;
	}

	ast_xml_free_text(tmpstr);
	res = 0;

	state = open ? AST_PRESENCE_AVAILABLE : AST_PRESENCE_UNAVAILABLE;

	ast_verb(4, "Presence for %s changed to %s%s%s%s\n", endpoint, ast_presence_state2str(state),
		!ast_strlen_zero(note) ? " (" : "", S_OR(note, ""), !ast_strlen_zero(note) ? ")" : "");

	/* Publish a presence update, same as func_presencestate. */
	snprintf(full_dev, sizeof(full_dev), "CustomPresence:PJSIP/%s", endpoint);
	dev += strlen("CustomPresence:");

	ast_db_put(astdb_family, dev, ast_presence_state2str(state));
	ast_presence_state_changed_literal(state, subtype, S_OR(note, ""), full_dev);

cleanup:
	ast_xml_close(xmldoc);
	return res;
}

static int presence_publication_new(struct ast_sip_endpoint *endpoint, const char *resource, const char *event_configuration)
{
	return 200; /* Accept anything as long as the inbound publication is accordingly configured to allow it. */
}

static int presence_publication_state_change(struct ast_sip_publication *pub, pjsip_msg_body *body, enum ast_sip_publish_state state)
{
	struct ast_sip_endpoint *endpoint;

	/* If no body exists this is a refresh and can be ignored */
	if (!body) {
		return 0;
	}

	if (!ast_sip_is_content_type(&body->content_type, "application", "pidf+xml")) {
		ast_debug(2, "Received unsupported content type for presence event\n");
		return -1;
	}

	endpoint = ast_sip_publication_get_endpoint(pub);
	if (!endpoint) {
		return -1;
	}

	parse_incoming_xml(body->data, body->len, ast_sorcery_object_get_id(endpoint));
	/* No need to cleanup the endpoint. ast_sip_publication_get_endpoint doesn't bump the ref count. */
	return 0;
}

struct ast_sip_publish_handler presence_publication_handler = {
	.event_name = "presence",
	.new_publication = presence_publication_new,
	.publication_state_change = presence_publication_state_change,
};

static int load_module(void)
{
	if (ast_sip_register_publish_handler(&presence_publication_handler)) {
		ast_log(LOG_WARNING, "Unable to register event publication handler %s\n", presence_publication_handler.event_name);
		return AST_MODULE_LOAD_DECLINE;
	}

	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	ast_sip_unregister_publish_handler(&presence_publication_handler);
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "PJSIP Presence PUBLISH Support",
	.support_level = AST_MODULE_SUPPORT_EXTENDED,
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_CHANNEL_DEPEND + 5,
	.requires = "res_pjsip,res_pjsip_pubsub",
);
