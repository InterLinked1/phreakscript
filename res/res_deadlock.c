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

/*!
 * \file
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \brief Simulated Deadlock Testing from the CLI
 *
 */

/*** MODULEINFO
	<defaultenabled>no</defaultenabled>
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <pthread.h>

#include "asterisk/module.h"
#include "asterisk/lock.h"
#include "asterisk/cli.h"
#include "asterisk/utils.h"

static ast_mutex_t test_lock;
static pthread_t thread_id = AST_PTHREADT_NULL, thread_id2 = AST_PTHREADT_NULL;
static int flag = 0;

static void *lock_thread(void *arg)
{
	ast_mutex_lock(&test_lock);
	flag = 1;
	while (flag) {
		usleep(100000); /* Sleep for 100 ms */
	}
	ast_mutex_unlock(&test_lock);
	thread_id = AST_PTHREADT_NULL;
	return NULL;
}

static void *lockwait_thread(void *arg)
{
	while (!flag) {
		usleep(1000); /* Wait for other thread to grab lock first. */
	}
	ast_mutex_lock(&test_lock);
	ast_mutex_unlock(&test_lock);
	thread_id2 = AST_PTHREADT_NULL;
	return NULL;
}

static char *handle_start(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "simulation start deadlock";
		e->usage =
			"Usage: simulation start deadlock\n"
			"       Start a simulated deadlock.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != e->args) {
		return CLI_SHOWUSAGE;
	}

	if (thread_id != AST_PTHREADT_NULL || thread_id2 != AST_PTHREADT_NULL) {
		ast_cli(a->fd, "Deadlock test already in progress\n");
		return CLI_FAILURE;
	} else if (ast_pthread_create(&thread_id, NULL, lock_thread, NULL)) {
		return CLI_FAILURE;
	} else if (ast_pthread_create(&thread_id2, NULL, lockwait_thread, NULL)) {
		return CLI_FAILURE;
	}

	ast_cli(a->fd, "Simulated deadlock started\n");
	return CLI_SUCCESS;
}

static char *handle_stop(struct ast_cli_entry *e, int cmd, struct ast_cli_args *a)
{
	switch (cmd) {
	case CLI_INIT:
		e->command = "simulation stop deadlock";
		e->usage =
			"Usage: simulation stop deadlock\n"
			"       Stop a simulated deadlock.\n";
		return NULL;
	case CLI_GENERATE:
		return NULL;
	}

	if (a->argc != e->args) {
		return CLI_SHOWUSAGE;
	}

	if (!flag) {
		ast_cli(a->fd, "No deadlock test currently in progress\n");
		return CLI_FAILURE;
	}

	flag = 0;
	ast_cli(a->fd, "Simulated deadlock stopped\n");
	return CLI_SUCCESS;
}

static struct ast_cli_entry cli_testdeadlock[] = {
	AST_CLI_DEFINE(handle_start, "Start a simulated deadlock"),
	AST_CLI_DEFINE(handle_stop, "Stop a simulated deadlock"),
};

static int unload_module(void)
{
	int res = ast_cli_unregister_multiple(cli_testdeadlock, ARRAY_LEN(cli_testdeadlock));
	flag = 0; /* If lock thread still running, signal it to end now. */
	return res;
}

static int load_module(void)
{
	int res;
	ast_mutex_init(&test_lock);
	res = ast_cli_register_multiple(cli_testdeadlock, ARRAY_LEN(cli_testdeadlock));
	return res ? AST_MODULE_LOAD_DECLINE : AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Simulated Deadlock Testing from the CLI");
