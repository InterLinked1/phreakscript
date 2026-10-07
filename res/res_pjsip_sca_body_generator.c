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
	<support_level>core</support_level>
 ***/

#include "asterisk.h"

#include <pjsip.h>
#include <pjsip_simple.h>
#include <pjlib.h>

#include "asterisk/res_pjsip.h"
#include "asterisk/res_pjsip_pubsub.h"
#include "asterisk/res_pjsip_presence_xml.h"
#include "asterisk/res_pjsip_body_generator_types.h"
#include "asterisk/module.h"
#include "asterisk/strings.h"

/*! \todo move to include file */
#define AST_SIP_SCA_DATA "ast_sip_sca_data"

struct ast_sip_sca_data {
	char park_uri[256];
	char park_display[64];
	unsigned int park_enabled:1;
};



#define SCA_TYPE "application"
#define SCA_SUBTYPE "x-broadworks-callpark-info+xml"

#define CALLINFO_EVENT "call-info"
#define SEIZE_EVENT "line-seize"

static void *sca_allocate_body(void *data)
{
	struct ast_str **str;

	str = ast_malloc(sizeof(*str));
	if (!str) {
		return NULL;
	}
	*str = ast_str_create(128);
	if (!*str) {
		ast_free(str);
		return NULL;
	}
	return str;
}

static int generate_park_content(struct ast_str **bodytext, struct ast_sip_sca_data *sca_data)
{
	char *buf;
	int len;
	struct ast_xml_doc *doc;
	struct ast_xml_node *root, *callpark, *parked, *identity;

	/* This callback is called for *all* NOTIFYs, so we should only add XML to the body if actually necessary. */
	if (!sca_data->park_enabled) {
		return 0;
	}

	/* The spec says if the SUBSCRIBE Accept contains application/xbroadworks-callpark-info+xml,
	 * then we should add a body as follows: */

	doc = ast_xml_new();
	if (!doc) {
		ast_log(LOG_ERROR, "Could not create new XML document\n");
		return -1;
	}

	root = ast_xml_new_node("x-broadworks-callpark-info");
	if (!root) {
		goto cleanup;
	}

	ast_xml_set_root(doc, root);
	ast_xml_set_attribute(root, "xmlns", "http://schema.broadsoft.com/callpark");

	/*
	 * The phone is expecting something like this:
	 *
	 * Normal:
	 * <?xml version="1.0" encoding="UTF-8"?>
	 * <x-broadworks-callpark-info xmlns="http://schema.broadsoft.com/callpark">
	 *  <callpark/>
	 * </x-broadworks-callpark-info>
	 *
	 * Parked:
	 * <?xml version="1.0" encoding="UTF-8"?>
	 * <x-broadworks-callpark-info xmlns=http://schema.broadsoft.com/callpark>
	 * <callpark>
	 *  <parked>
	 *   <identity display="C. Parked">sip:C@as.foo.com;user=phone</identity>
	 *  </parked>
	 * </callpark>
	 * </x-broadworks-callpark-info>
	 */

	callpark = ast_xml_new_node("callpark");
	if (!callpark) {
		goto cleanup;
	}
	ast_xml_add_child(root, callpark);

	/* If the body has something, add it. */
	if (!ast_strlen_zero(sca_data->park_uri)) {
		parked = ast_xml_new_node("parked");
		if (!parked) {
			goto cleanup;
		}
		ast_xml_add_child(callpark, parked);

		identity = ast_xml_new_node("identity");
		if (!identity) {
			goto cleanup;
		}
		ast_xml_set_attribute(identity, "display", sca_data->park_display);
		ast_xml_set_text(identity, sca_data->park_uri);
		ast_xml_add_child(parked, identity);
	}

	/* Finalize. */
	ast_xml_doc_dump_memory(doc, &buf, &len);
	ast_xml_close(doc);
	if (len <= 0) {
		ast_log(LOG_WARNING, "XML document has length %d?\n", len);
	}
	if (buf) {
		ast_str_append(bodytext, 0, "%s", buf);
		ast_xml_free_text(buf);
	}
	return 0;
cleanup:
	ast_xml_close(doc);
	ast_log(LOG_ERROR, "Could not create new XML root node\n");
	return -1;
}

static int sca_generate_body_content(void *body, void *data)
{
	struct ast_str **bodytext = body;
	struct ast_sip_sca_data *sca_data = data;

	ast_debug(2, "Generating body content for %s/%s\n", SCA_TYPE, SCA_SUBTYPE);
	return generate_park_content(bodytext, sca_data);
}

static void sca_to_string(void *body, struct ast_str **str)
{
	struct ast_str **features = body;
	ast_str_set(str, 0, "%s", ast_str_buffer(*features));
}

static void sca_destroy_body(void *body)
{
	struct ast_str **features = body;
	ast_free(*features);
	ast_free(features);
}

static struct ast_sip_pubsub_body_generator sca_generator = {
	.type = SCA_TYPE,
	.subtype = SCA_SUBTYPE,
	.body_type = AST_SIP_SCA_DATA,
	.allocate_body = sca_allocate_body,
	.generate_body_content = sca_generate_body_content,
	.to_string = sca_to_string,
	.destroy_body = sca_destroy_body,
};

static int load_module(void)
{
	if (ast_sip_pubsub_register_body_generator(&sca_generator)) {
		return AST_MODULE_LOAD_DECLINE;
	}
	return AST_MODULE_LOAD_SUCCESS;
}

static int unload_module(void)
{
	ast_sip_pubsub_unregister_body_generator(&sca_generator);
	return 0;
}

AST_MODULE_INFO(ASTERISK_GPL_KEY, AST_MODFLAG_LOAD_ORDER, "PJSIP Shared Call Appearances",
	.support_level = AST_MODULE_SUPPORT_CORE,
	.load = load_module,
	.unload = unload_module,
	.load_pri = AST_MODPRI_CHANNEL_DEPEND,
	.requires = "res_pjsip,res_pjsip_pubsub",
);
