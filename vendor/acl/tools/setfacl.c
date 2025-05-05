/*
  File: setfacl.c
  (Linux Access Control List Management)

  Copyright (C) 1999-2002
  Andreas Gruenbacher, <andreas.gruenbacher@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <https://www.gnu.org/licenses/>.
*/

#include "config.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <getopt.h>
#include "misc.h"
#include "sequence.h"
#include "parse.h"
#include "do_set.h"
#include "walk_tree.h"

#define POSIXLY_CORRECT_STR "POSIXLY_CORRECT"

/* '-' stands for `process non-option arguments in loop' */
#if !POSIXLY_CORRECT
#  define CMD_LINE_OPTIONS "-:bkndvhm:M:x:X:RLP"
#  define CMD_LINE_SPEC "[-bkndRLP] { -m|-M|-x|-X ... } file ..."
#endif
#define POSIXLY_CMD_LINE_OPTIONS "-:bkndvhm:M:x:X:"
#define POSIXLY_CMD_LINE_SPEC "[-bknd] {-m|-M|-x|-X ... } file ..."

static const struct option long_options[] = {
#if !POSIXLY_CORRECT
	{ "set",		1, 0, 's' },
	{ "set-file",		1, 0, 'S' },

	{ "mask",		0, 0, 'r' },
	{ "recursive",		0, 0, 'R' },
	{ "logical",		0, 0, 'L' },
	{ "physical",		0, 0, 'P' },
	{ "restore",		1, 0, 'B' },
	{ "test",		0, 0, 't' },
#endif
	{ "modify",		1, 0, 'm' },
	{ "modify-file",	1, 0, 'M' },
	{ "remove",		1, 0, 'x' },
	{ "remove-file",	1, 0, 'X' },

	{ "default",		0, 0, 'd' },
	{ "no-mask",		0, 0, 'n' },
	{ "remove-all",		0, 0, 'b' },
	{ "remove-default",	0, 0, 'k' },
	{ "version",		0, 0, 'v' },
	{ "help",		0, 0, 'h' },
	{ NULL,			0, 0, 0   },
};

const char *progname;
static const char *cmd_line_options, *cmd_line_spec;

static int walk_flags = WALK_TREE_DEREFERENCE_TOPLEVEL;
int opt_recalculate;  /* recalculate mask entry (0=default, 1=yes, -1=no) */
static int opt_promote;  /* promote access ACL to default ACL */
int opt_test;  /* do not write to the file system.
                      Print what would happen instead. */
#if POSIXLY_CORRECT
static const int posixly_correct = 1;  /* Posix compatible behavior! */
#else
static int posixly_correct;  /* Posix compatible behavior? */
#endif
static int chown_error;
static int promote_warning;


static const char *xquote(const char *str, const char *quote_chars)
{
	const char *q = __acl_quote(str, quote_chars);
	if (q == NULL) {
		fprintf(stderr, "%s: %s\n", progname, strerror(errno));
		exit(1);
	}
	return q;
}

static int
has_any_of_type(
	cmd_t cmd,
	acl_type_t acl_type)
{
	while (cmd) {
		if (cmd->c_type == acl_type)
			return 1;
		cmd = cmd->c_next;
	}
	return 0;
}
	

#if !POSIXLY_CORRECT
static int
restore(
	FILE *file,
	const char *filename)
{
	char *path_p;
	struct stat st;
	uid_t uid;
	gid_t gid;
	mode_t mask, flags;
	struct do_set_args args = { };
	int lineno = 0, backup_line;
	int error, status = 0;
	int chmod_required = 0;

	memset(&st, 0, sizeof(st));

	for(;;) {
		backup_line = lineno;
		error = read_acl_comments(file, &lineno, &path_p, &uid, &gid,
					  &flags);
		if (error < 0) {
			error = -error;
			goto fail;
		}
		if (error == 0)
			return status;

		if (path_p == NULL) {
			if (filename) {
				fprintf(stderr, _("%s: %s: No filename found "
						  "in line %d, aborting\n"),
					progname, xquote(filename, "\n\r"),
					backup_line);
			} else {
				fprintf(stderr, _("%s: No filename found in "
						 "line %d of standard input, "
						 "aborting\n"),
					progname, backup_line);
			}
			status = 1;
			goto getout;
		}

		if (!(args.seq = seq_init()))
			goto fail_errno;
		if (seq_append_cmd(args.seq, CMD_REMOVE_ACL, ACL_TYPE_ACCESS) ||
		    seq_append_cmd(args.seq, CMD_REMOVE_ACL, ACL_TYPE_DEFAULT))
			goto fail_errno;

		error = read_acl_seq(file, args.seq, CMD_ENTRY_REPLACE,
		                     SEQ_PARSE_WITH_PERM |
				     SEQ_PARSE_DEFAULT |
				     SEQ_PARSE_MULTI,
				     &lineno, NULL);
		if (error != 0) {
			fprintf(stderr, _("%s: %s: %s in line %d\n"),
			        progname, xquote(filename, "\n\r"), strerror(errno),
				lineno);
			status = 1;
			goto getout;
		}

		error = stat(path_p, &st);
		if (opt_test && error != 0) {
			fprintf(stderr, "%s: %s: %s\n", progname,
				xquote(path_p, "\n\r"), strerror(errno));
			status = 1;
		}

		args.mode = 0;
		error = do_set(path_p, &st, 0, &args);
		if (error != 0) {
			status = 1;
			goto resume;
		}

		if (uid != ACL_UNDEFINED_ID && uid != st.st_uid)
			st.st_uid = uid;
		else
			st.st_uid = -1;
		if (gid != ACL_UNDEFINED_ID && gid != st.st_gid)
			st.st_gid = gid;
		else
			st.st_gid = -1;
		if (!opt_test &&
		    (st.st_uid != -1 || st.st_gid != -1)) {
			if (chown(path_p, st.st_uid, st.st_gid) != 0) {
				fprintf(stderr, _("%s: %s: Cannot change "
					          "owner/group: %s\n"),
					progname, xquote(path_p, "\n\r"),
					strerror(errno));
				status = 1;
			}

			/* chown() clears setuid/setgid so force a chmod if
			 * S_ISUID/S_ISGID was expected */
			if ((st.st_mode & flags) & (S_ISUID | S_ISGID))
				chmod_required = 1;
		}

		mask = S_ISUID | S_ISGID | S_ISVTX;
		if (chmod_required || ((st.st_mode & mask) != (flags & mask))) {
			if (!args.mode)
				args.mode = st.st_mode;
			args.mode &= (S_IRWXU | S_IRWXG | S_IRWXO);
			if (chmod(path_p, flags | args.mode) != 0) {
				fprintf(stderr, _("%s: %s: Cannot change "
					          "mode: %s\n"),
					progname, xquote(path_p, "\n\r"),
					strerror(errno));
				status = 1;
			}
		}
resume:
		if (path_p) {
			free(path_p);
			path_p = NULL;
		}
		if (args.seq) {
			seq_free(args.seq);
			args.seq = NULL;
		}
	}

getout:
	if (path_p) {
		free(path_p);
		path_p = NULL;
	}
	if (args.seq) {
		seq_free(args.seq);
		args.seq = NULL;
	}
	return status;

fail_errno:
	error = errno;
fail:
	fprintf(stderr, "%s: %s: %s\n", progname, xquote(filename, "\n\r"),
		strerror(error));
	status = 1;
	goto getout;
}
#endif


static void help(void)
{
	printf(_("%s %s -- set file access control lists\n"),
		progname, VERSION);
	printf(_("Usage: %s %s\n"),
		progname, cmd_line_spec);
	printf(_(
"  -m, --modify=acl        modify the current ACL(s) of file(s)\n"
"  -M, --modify-file=file  read ACL entries to modify from file\n"
"  -x, --remove=acl        remove entries from the ACL(s) of file(s)\n"
"  -X, --remove-file=file  read ACL entries to remove from file\n"
"  -b, --remove-all        remove all extended ACL entries\n"
"  -k, --remove-default    remove the default ACL\n"));
#if !POSIXLY_CORRECT
	if (!posixly_correct) {
		printf(_(
"      --set=acl           set the ACL of file(s), replacing the current ACL\n"
"      --set-file=file     read ACL entries to set from file\n"
"      --mask              do recalculate the effective rights mask\n"));
	}
#endif
  	printf(_(
"  -n, --no-mask           don't recalculate the effective rights mask\n"
"  -d, --default           operations apply to the default ACL\n"));
#if !POSIXLY_CORRECT
	if (!posixly_correct) {
		printf(_(
"  -R, --recursive         recurse into subdirectories\n"
"  -L, --logical           logical walk, follow symbolic links\n"
"  -P, --physical          physical walk, do not follow symbolic links\n"
"      --restore=file      restore ACLs (inverse of `getfacl -R')\n"
"      --test              test mode (ACLs are not modified)\n"));
	}
#endif
	printf(_(
"  -v, --version           print version and exit\n"
"  -h, --help              this help text\n"));
}


static int next_file(const char *arg, seq_t seq)
{
	char *line;
	int errors = 0;
	struct do_set_args args;

	args.seq = seq;

	if (strcmp(arg, "-") == 0) {
		while ((line = __acl_next_line(stdin)))
			errors = walk_tree(line, walk_flags, 0, do_set, &args);
		if (!feof(stdin)) {
			fprintf(stderr, _("%s: Standard input: %s\n"),
				progname, strerror(errno));
			errors = 1;
		}
	} else {
		errors = walk_tree(arg, walk_flags, 0, do_set, &args);
	}
	return errors ? 1 : 0;
}


#define ERRNO_ERROR(s) \
	({status = (s); goto errno_error; })


int main(int argc, char *argv[])
{
	int opt;
	int saw_files = 0;
	int status = 0, status2;
	FILE *file;
	int which;
	int lineno;
	int error;
	seq_t seq;
	int seq_cmd, parse_mode;
	
	progname = basename(argv[0]);

#if POSIXLY_CORRECT
	cmd_line_options = POSIXLY_CMD_LINE_OPTIONS;
	cmd_line_spec = _(POSIXLY_CMD_LINE_SPEC);
#else
	if (getenv(POSIXLY_CORRECT_STR))
		posixly_correct = 1;
	if (!posixly_correct) {
		cmd_line_options = CMD_LINE_OPTIONS;
		cmd_line_spec = _(CMD_LINE_SPEC);
	} else {
		cmd_line_options = POSIXLY_CMD_LINE_OPTIONS;
		cmd_line_spec = _(POSIXLY_CMD_LINE_SPEC);
	}
#endif

	setlocale(LC_CTYPE, "");
	setlocale(LC_MESSAGES, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	seq = seq_init();
	if (!seq)
		ERRNO_ERROR(1);

	while ((opt = getopt_long(argc, argv, cmd_line_options,
		                  long_options, NULL)) != -1) {
		/* we remember the two REMOVE_ACL commands of the set
		   operations because we may later need to delete them.  */
		cmd_t seq_remove_default_acl_cmd = NULL;
		cmd_t seq_remove_acl_cmd = NULL;

		if (opt != '\1' && saw_files) {
			seq_free(seq);
			seq = seq_init();
			if (!seq)
				ERRNO_ERROR(1);
			saw_files = 0;
		}

		switch (opt) {
			case 'b':  /* remove all extended entries */
				if (seq_append_cmd(seq, CMD_REMOVE_EXTENDED_ACL,
				                        ACL_TYPE_ACCESS) ||
				    seq_append_cmd(seq, CMD_REMOVE_ACL,
				                        ACL_TYPE_DEFAULT))
					ERRNO_ERROR(1);
				break;

			case 'k':  /* remove default ACL */
				if (seq_append_cmd(seq, CMD_REMOVE_ACL,
				                        ACL_TYPE_DEFAULT))
					ERRNO_ERROR(1);
				break;

			case 'n':  /* do not recalculate mask */
				opt_recalculate = -1;
				break;

			case 'r':  /* force recalculate mask */
				opt_recalculate = 1;
				break;

			case 'd':  /*  operations apply to default ACL */
				opt_promote = 1;
				break;

			case 's':  /* set */
				if (seq_append_cmd(seq, CMD_REMOVE_ACL,
					                ACL_TYPE_ACCESS))
					ERRNO_ERROR(1);
				seq_remove_acl_cmd = seq->s_last;
				if (seq_append_cmd(seq, CMD_REMOVE_ACL,
				                        ACL_TYPE_DEFAULT))
					ERRNO_ERROR(1);
				seq_remove_default_acl_cmd = seq->s_last;

				seq_cmd = CMD_ENTRY_REPLACE;
				parse_mode = SEQ_PARSE_WITH_PERM;
				goto set_modify_delete;

			case 'm':  /* modify */
				seq_cmd = CMD_ENTRY_REPLACE;
				parse_mode = SEQ_PARSE_WITH_PERM;
				goto set_modify_delete;

			case 'x':  /* delete */
				seq_cmd = CMD_REMOVE_ENTRY;
#if POSIXLY_CORRECT
				parse_mode = SEQ_PARSE_ANY_PERM;
#else
				if (posixly_correct)
					parse_mode = SEQ_PARSE_ANY_PERM;
				else
					parse_mode = SEQ_PARSE_NO_PERM;
#endif
				goto set_modify_delete;

			set_modify_delete:
				if (!posixly_correct)
					parse_mode |= SEQ_PARSE_DEFAULT;
				if (opt_promote)
					parse_mode |= SEQ_PROMOTE_ACL;
				if (parse_acl_seq(seq, optarg, &which,
				                  seq_cmd, parse_mode) != 0) {
					if (which < 0 ||
					    (size_t) which >= strlen(optarg)) {
						fprintf(stderr, _(
							"%s: Option "
						        "-%c incomplete\n"),
							progname, opt);
					} else {
						fprintf(stderr, _(
							"%s: Option "
						        "-%c: %s near "
							"character %d\n"),
							progname, opt,
							strerror(errno),
							which+1);
					}
					status = 2;
					goto cleanup;
				}
				break;

			case 'S':  /* set from file */
				if (seq_append_cmd(seq, CMD_REMOVE_ACL,
					                ACL_TYPE_ACCESS))
					ERRNO_ERROR(1);
				seq_remove_acl_cmd = seq->s_last;
				if (seq_append_cmd(seq, CMD_REMOVE_ACL,
				                        ACL_TYPE_DEFAULT))
					ERRNO_ERROR(1);
				seq_remove_default_acl_cmd = seq->s_last;

				seq_cmd = CMD_ENTRY_REPLACE;
				parse_mode = SEQ_PARSE_WITH_PERM;
				goto set_modify_delete_from_file;

			case 'M':  /* modify from file */
				seq_cmd = CMD_ENTRY_REPLACE;
				parse_mode = SEQ_PARSE_WITH_PERM;
				goto set_modify_delete_from_file;

			case 'X':  /* delete from file */
				seq_cmd = CMD_REMOVE_ENTRY;
#if POSIXLY_CORRECT
				parse_mode = SEQ_PARSE_ANY_PERM;
#else
				if (posixly_correct)
					parse_mode = SEQ_PARSE_ANY_PERM;
				else
					parse_mode = SEQ_PARSE_NO_PERM;
#endif
				goto set_modify_delete_from_file;

			set_modify_delete_from_file:
				if (!posixly_correct)
					parse_mode |= SEQ_PARSE_DEFAULT;
				if (opt_promote)
					parse_mode |= SEQ_PROMOTE_ACL;
				if (strcmp(optarg, "-") == 0) {
					file = stdin;
				} else {
					file = fopen(optarg, "r");
					if (file == NULL) {
						fprintf(stderr, "%s: %s: %s\n",
							progname,
							xquote(optarg, "\n\r"),
							strerror(errno));
						status = 2;
						goto cleanup;
					}
				}

				lineno = 0;
				error = read_acl_seq(file, seq, seq_cmd,
				                     parse_mode, &lineno, NULL);
				
				if (file != stdin) {
					fclose(file);
				}

				if (error) {
					if (!errno)
						errno = EINVAL;

					if (file != stdin) {
						fprintf(stderr, _(
							"%s: %s in line "
						        "%d of file %s\n"),
							progname,
							strerror(errno),
							lineno,
							xquote(optarg, "\n\r"));
					} else {
						fprintf(stderr, _(
							"%s: %s in line "
						        "%d of standard "
							"input\n"), progname,
							strerror(errno),
							lineno);
					}
					status = 2;
					goto cleanup;
				}
				break;


			case '\1':  /* file argument */
				if (seq_empty(seq))
					goto synopsis;
				saw_files = 1;

				status2 = next_file(optarg, seq);
				if (status == 0)
					status = status2;
				break;

			case 'B':  /* restore ACL backup */
				saw_files = 1;

				if (strcmp(optarg, "-") == 0)
					file = stdin;
				else {
					file = fopen(optarg, "r");
					if (file == NULL) {
						fprintf(stderr, "%s: %s: %s\n",
							progname,
							xquote(optarg, "\n\r"),
							strerror(errno));
						status = 2;
						goto cleanup;
					}
				}

				status = restore(file,
				               (file == stdin) ? NULL : optarg);

				if (file != stdin)
					fclose(file);
				if (status != 0)
					goto cleanup;
				break;

			case 'R':  /* recursive */
				walk_flags |= WALK_TREE_RECURSIVE;
				break;

			case 'L':  /* follow symlinks */
				walk_flags |= WALK_TREE_LOGICAL | WALK_TREE_DEREFERENCE;
				walk_flags &= ~WALK_TREE_PHYSICAL;
				break;

			case 'P':  /* do not follow symlinks */
				walk_flags |= WALK_TREE_PHYSICAL;
				walk_flags &= ~(WALK_TREE_LOGICAL | WALK_TREE_DEREFERENCE |
						WALK_TREE_DEREFERENCE_TOPLEVEL);
				break;

			case 't':  /* test mode */
				opt_test = 1;
				break;

			case 'v':  /* print version and exit */
				printf("%s " VERSION "\n", progname);
				status = 0;
				goto cleanup;

			case 'h':  /* help! */
				help();
				status = 0;
				goto cleanup;

			case ':':  /* option missing */
			case '?':  /* unknown option */
			default:
				goto synopsis;
		}
		if (seq_remove_acl_cmd) {
			/* This was a set operation. Check if there are
			   actually entries of ACL_TYPE_ACCESS; if there
			   are none, we need to remove this command! */
			if (!has_any_of_type(seq_remove_acl_cmd->c_next,
				            ACL_TYPE_ACCESS))
				seq_delete_cmd(seq, seq_remove_acl_cmd);
		}
		if (seq_remove_default_acl_cmd) {
			/* This was a set operation. Check if there are
			   actually entries of ACL_TYPE_DEFAULT; if there
			   are none, we need to remove this command! */
			if (!has_any_of_type(seq_remove_default_acl_cmd->c_next,
				            ACL_TYPE_DEFAULT))
				seq_delete_cmd(seq, seq_remove_default_acl_cmd);
		}
	}
	while (optind < argc) {
		if(!seq)
			goto synopsis;
		if (seq_empty(seq))
			goto synopsis;
		saw_files = 1;

		status2 = next_file(argv[optind++], seq);
		if (status == 0)
			status = status2;
	}
	if (!saw_files)
		goto synopsis;

	goto cleanup;

synopsis:
	fprintf(stderr, _("Usage: %s %s\n"),
		progname, cmd_line_spec);
	fprintf(stderr, _("Try `%s --help' for more information.\n"),
		progname);
	status = 2;
	goto cleanup;

errno_error:
	fprintf(stderr, "%s: %s\n", progname, strerror(errno));
	goto cleanup;

cleanup:
	if (seq)
		seq_free(seq);
	return status;
}

