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
 * \brief Trivial application to randomly play a sound file
 *
 * \author Naveen Albert <asterisk@phreaknet.org>
 *
 * \ingroup applications
 */

/*** MODULEINFO
	<support_level>extended</support_level>
 ***/

#include "asterisk.h"

#include <dirent.h>
#include <sys/stat.h>
#include <libgen.h>

#include "asterisk/file.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/app.h"
#include "asterisk/paths.h"	/* use ast_config_AST_DATA_DIR */

/*** DOCUMENTATION
	<application name="RandomPlayback" language="en_US">
		<synopsis>
			Plays a random file with a particular directory and/or file prefix
		</synopsis>
		<syntax>
			<parameter name="prefix">
				<para>Directory/file prefix that must match.</para>
			</parameter>
		</syntax>
		<description>
			<para>Plays back a random file with the provided prefix which contains
			a specific directory, optionally followed by a file prefix. If there
			is no file prefix, be sure to end with a trailing slash to search
			in a directory of the given name as opposed to a trailing file prefix.</para>
			<para>Knowledge of actual specific file candidates is not necessary.</para>
			<para>A random file matching this full prefix will be played.</para>
			<para>This application does not automatically answer the channel and should
			be preceded by <literal>Progress</literal> or <literal>Answer</literal> as
			appropriate.</para>
		</description>
		<see-also>
			<ref type="application">Playback</ref>
			<ref type="application">ControlPlayback</ref>
		</see-also>
	</application>
 ***/

static char *app = "RandomPlayback";

/*!
 * \brief Traverses a directory and returns number of files or a specific file with specified prefix
 *
 * \param chan Channel
 * \param directoryprefix Directory name and file prefix (optional)
 * \param filename 0 if getting the file count, otherwise positive 1-indexed file number to retrieve
 * \param buffer Buffer to fill. Do not allocate beforehand.
 *
 * \retval -1 Failure
 * \retval 0 No files in directory (file count)
 * \retval non-zero Number of files in directory (file count) or file number that matched (retrieval)
 */
static int traverse_directory(struct ast_channel *chan, char *directoryprefix, int filenum, char **buffer)
{
	char *fullpath;
	char *dir, *base;
	struct dirent* dent;
	DIR* srcdir;
	int files = 0, baselen = 0;
	RAII_VAR(struct ast_str *, media_dir, ast_str_create(64), ast_free);
	RAII_VAR(struct ast_str *, variant_dir, ast_str_create(64), ast_free);

	if (!media_dir || !variant_dir) {
		return -1;
	}

	if (directoryprefix[0] == '/') { /* absolute path */
		ast_str_set(&media_dir, 0, "%s", directoryprefix);
	} else if (directoryprefix[0]) {
		ast_str_set(&media_dir, 0, "%s/sounds/%s/%s", ast_config_AST_DATA_DIR, chan ? ast_channel_language(chan) : "en", directoryprefix);
	} else {
		ast_str_set(&media_dir, 0, "%s/sounds/%s", ast_config_AST_DATA_DIR, chan ? ast_channel_language(chan) : "en");
	}

	fullpath = ast_str_buffer(media_dir);
	baselen = strlen(fullpath);
	if (!fullpath[0]) {
		return -1;
	}
	if (fullpath[baselen - 1] == '/') { /* ends in trailing slash... but dirname ignores trailing slash */
		dir = fullpath;
		dir[baselen - 1] = '\0';
		base = "";
	} else {
		dir = dirname(ast_strdupa(fullpath));
		base = basename(ast_strdupa(fullpath));
	}
	
	ast_debug(1, "'%s' -> '%s' -> dirname '%s', basename '%s'\n", directoryprefix, fullpath, dir, base);

	baselen = strlen(base);
	srcdir = opendir(dir);
	if (!srcdir) {
		ast_debug(1, "Failed to open '%s'\n", dir);
		return -1;
	}

	while((dent = readdir(srcdir)) != NULL) {
		struct stat st;

		if(!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
			continue; /* parent directory */
		}
		if (*base && strncmp(dent->d_name, base, baselen)) {
			continue; /* doesn't match required file prefix */
		}

		ast_str_reset(variant_dir);
		ast_str_set(&variant_dir, 0, "%s/%s", dir, dent->d_name);

		if (stat(ast_str_buffer(variant_dir), &st) < 0) {
			ast_log(LOG_ERROR, "Failed to stat %s\n", ast_str_buffer(variant_dir));
			continue;
		}
		if (S_ISDIR(st.st_mode)) {
			continue; /* don't care about subdirectories */
		}
		files++;
		if (filenum && filenum == files) {
			int slen = ast_str_strlen(variant_dir) + 1;
			*buffer = ast_malloc(slen);
			if (!*buffer) {
				files = -1;
				break;
			}
			ast_copy_string(*buffer, ast_str_buffer(variant_dir), slen);
			break;
		}
	}

	closedir(srcdir);
	return files;
}

static int dir_file_count(struct ast_channel *chan, char *directoryprefix)
{
	return traverse_directory(chan, directoryprefix, 0, NULL);
}

static char *dir_file_match(struct ast_channel *chan, char *directoryprefix, int filenum)
{
	char *buf = NULL;
	traverse_directory(chan, directoryprefix, filenum, &buf);
	return buf;
}

static int playback_exec(struct ast_channel *chan, const char *data)
{
	int numfiles, rand;
	int res = 0;
	char *tmp, *buf;

	AST_DECLARE_APP_ARGS(args,
		AST_APP_ARG(dirprefix);
	);

	if (ast_strlen_zero(data)) {
		ast_log(LOG_WARNING, "RandomPlayback requires an argument (filename)\n");
		return -1;
	}

	tmp = ast_strdupa(data);
	AST_STANDARD_APP_ARGS(args, tmp);
	/* do not auto-answer channel */

	numfiles = dir_file_count(chan, args.dirprefix);
	if (numfiles == -1) {
		ast_log(LOG_WARNING, "Failed to calculate number of files prefixed with '%s'\n", args.dirprefix);
		return -1;
	}
	if (!numfiles) {
		ast_log(LOG_WARNING, "No files prefixed with '%s'\n", args.dirprefix);
		return 0;
	}
	rand = 1 + (ast_random() % numfiles);
	buf = dir_file_match(chan, args.dirprefix, rand);
	if (!buf) {
		ast_log(LOG_WARNING, "Failed to find random file # %d prefixed with '%s'\n", rand, args.dirprefix);
		return -1;
	}

	ast_debug(1, "Lucky file was #%d/%d: '%s'\n", rand, numfiles, buf);

	tmp = buf + strlen(buf) - 1;
	while (tmp != buf && tmp[0] != '.') {
		tmp--;
	}
	tmp[0] = '\0'; /* get rid of the file extension, for ast_streamfile */

	ast_stopstream(chan);
	res = ast_streamfile(chan, buf, ast_channel_language(chan));
	if (!res) {
		res = ast_waitstream(chan, "");
		ast_stopstream(chan);
	}

	ast_free(buf);
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
	return ast_register_application_xml(app, playback_exec);
}

AST_MODULE_INFO_STANDARD_EXTENDED(ASTERISK_GPL_KEY, "Random Playback Application");
