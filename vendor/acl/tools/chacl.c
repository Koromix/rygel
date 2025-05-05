/*
 * Copyright (c) 2001-2002 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/acl.h>
#include <acl/libacl.h>
#include "misc.h"

static int acl_delete_file (const char * path, acl_type_t type);
static int list_acl(char *file);
static int set_acl(acl_t acl, acl_t dacl, const char *fname);
static int walk_dir(acl_t acl, acl_t dacl, const char *fname);

static char *program;
static int rflag;

static void
usage(void)
{
	fprintf(stderr, _("Usage:\n"));
	fprintf(stderr, _("\t%s acl pathname...\n"), program);
	fprintf(stderr, _("\t%s -b acl dacl pathname...\n"), program);
	fprintf(stderr, _("\t%s -d dacl pathname...\n"), program);
	fprintf(stderr, _("\t%s -R pathname...\n"), program);
	fprintf(stderr, _("\t%s -D pathname...\n"), program);
	fprintf(stderr, _("\t%s -B pathname...\n"), program);
	fprintf(stderr, _("\t%s -l pathname...\t[not IRIX compatible]\n"),
			program);
	fprintf(stderr, _("\t%s -r pathname...\t[not IRIX compatible]\n"),
			program);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *file;
	int switch_flag = 0;            /* ensure only one switch is used */
	int args_required = 2;	
	int failed = 0;			/* exit status */
	int c;				/* For use by getopt(3) */
	int dflag = 0;			/* a Default ACL is desired */
	int bflag = 0;			/* a both ACLs are desired */
	int Rflag = 0;			/* set to true to remove an acl */
	int Dflag = 0;			/* set to true to remove default acls */
	int Bflag = 0;			/* set to true to remove both acls */
	int lflag = 0;			/* set to true to list acls */
	acl_t acl = NULL;		/* File ACL */
	acl_t dacl = NULL;		/* Directory Default ACL */

	program = basename(argv[0]);

	setlocale(LC_CTYPE, "");
	setlocale(LC_MESSAGES, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	/* parse arguments */
	while ((c = getopt(argc, argv, "bdlRDBr")) != -1) {
		if (switch_flag) 
			usage();
		switch_flag = 1;

		switch (c) {
			case 'b':
				bflag = 1;
				args_required = 3;
				break;
			case 'd':
				dflag = 1;
				args_required = 2;
				break;
			case 'R':
				Rflag = 1;
				args_required = 1;
				break;
			case 'D':
				Dflag = 1;
				args_required = 1;
				break;
			case 'B':
				Bflag = 1;
				args_required = 1;
				break;
			case 'l':
				lflag = 1;
				args_required = 1;
				break;
			case 'r':
				rflag = 1;
				args_required = 1;
				break;
			default:
				usage();
				break;
		}
	}

	/* if not enough arguments quit */
	if ((argc - optind) < args_required)
		usage();

        /* list the acls */
	if (lflag) {
		for (; optind < argc; optind++) {	
			file = argv[optind];
			if (!list_acl(file))
				failed++;
		}
		return(failed);
	}

	/* remove the acls */
	if (Rflag || Dflag || Bflag) {
		for (; optind < argc; optind++) {	
			file = argv[optind];
			if (!Dflag &&
			    (acl_delete_file(file, ACL_TYPE_ACCESS) == -1)) {
				fprintf(stderr, _(
			"%s: error removing access acl on \"%s\": %s\n"),
					program, file, strerror(errno));
				failed++;
			}
			if (!Rflag &&
			    (acl_delete_file(file, ACL_TYPE_DEFAULT) == -1)) {
				fprintf(stderr, _(
			"%s: error removing default acl on \"%s\": %s\n"),
					program, file, strerror(errno));
				failed++;
			}
		}
		return(failed);
	}

	/* file access acl */
	if (! dflag) { 
		acl = acl_from_text(argv[optind]);
		failed = acl_check(acl, &c);
		if (failed < 0) {
			fprintf(stderr, "%s: %s - %s\n",
				program, argv[optind], strerror(errno));
			return 1;
		}
		else if (failed > 0) {
			fprintf(stderr, _(
				"%s: access ACL '%s': %s at entry %d\n"),
				program, argv[optind], acl_error(failed), c);
			return 1;
		}
		optind++;
	}


	/* directory default acl */
	if (bflag || dflag) {
		dacl = acl_from_text(argv[optind]);
		failed = acl_check(dacl, &c);
		if (failed < 0) {
			fprintf(stderr, "%s: %s - %s\n",
				program, argv[optind], strerror(errno));
			return 1;
		}
		else if (failed > 0) {
			fprintf(stderr, _(
				"%s: access ACL '%s': %s at entry %d\n"),
				program, argv[optind], acl_error(failed), c);
			return 1;
		}
		optind++;
	}

	/* place acls on files */
	for (; optind < argc; optind++)
		failed += set_acl(acl, dacl, argv[optind]);

	if (acl)
		acl_free(acl);
	if (dacl)
		acl_free(dacl);

	return(failed);
}

/* 
 *   deletes an access acl or directory default acl if one exists
 */ 
static int 
acl_delete_file(const char *path, acl_type_t type)
{
	int error = 0;

	/* converts access ACL to a minimal ACL */
	if (type == ACL_TYPE_ACCESS) {
		acl_t acl;
		acl_entry_t entry;
		acl_tag_t tag;

		acl = acl_get_file(path, ACL_TYPE_ACCESS);
		if (!acl)
			return -1;
		error = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry);
		while (error == 1) {
			acl_get_tag_type(entry, &tag);
			switch(tag) {
				case ACL_USER:
				case ACL_GROUP:
				case ACL_MASK:
					acl_delete_entry(acl, entry);
					break;
			 }
			error = acl_get_entry(acl, ACL_NEXT_ENTRY, &entry);
		}
		if (!error)
			error = acl_set_file(path, ACL_TYPE_ACCESS, acl);
	} else
		error = acl_delete_def_file(path);
	return(error);
}

/*
 *    lists the acl for a file/dir in short text form
 *    return 0 on failure
 *    return 1 on success
 */
static int
list_acl(char *file)
{
	acl_t acl = NULL;
	acl_t dacl = NULL;
	char *acl_text, *dacl_text = NULL;

	if ((acl = acl_get_file(file, ACL_TYPE_ACCESS)) == NULL) {
		fprintf(stderr, _("%s: cannot get access ACL on '%s': %s\n"),
			program, file, strerror(errno));
		return 0;
	}
	if ((dacl = acl_get_file(file, ACL_TYPE_DEFAULT)) == NULL &&
	    (errno != EACCES)) {	/* EACCES given if not a directory */
		fprintf(stderr, _("%s: cannot get default ACL on '%s': %s\n"),
			program, file, strerror(errno));
		return 0;
	}
	acl_text = acl_to_any_text(acl, NULL, ',', TEXT_ABBREVIATE);
	if (acl_text == NULL) {
		fprintf(stderr, _("%s: cannot get access ACL text on "
			"'%s': %s\n"), program, file, strerror(errno));
		return 0;
	}
	if (acl_entries(dacl) > 0) {
		dacl_text = acl_to_any_text(dacl, NULL, ',', TEXT_ABBREVIATE);
		if (dacl_text == NULL) {
			fprintf(stderr, _("%s: cannot get default ACL text on "
				"'%s': %s\n"), program, file, strerror(errno));
			return 0;
		}
	}
	if (dacl_text) {
		printf("%s [%s/%s]\n", file, acl_text, dacl_text);
		acl_free(dacl_text);
	} else
		printf("%s [%s]\n", file, acl_text);
	acl_free(acl_text);
	acl_free(acl);
	acl_free(dacl);
	return 1;
}

static int
set_acl(acl_t acl, acl_t dacl, const char *fname)
{
	int failed = 0;

	if (rflag)
		failed += walk_dir(acl, dacl, fname);

	/* set regular acl */
	if (acl && acl_set_file(fname, ACL_TYPE_ACCESS, acl) == -1) {
		fprintf(stderr, _("%s: cannot set access acl on \"%s\": %s\n"),
			program, fname, strerror(errno));
		failed++;
	}
	/* set default acl */
	if (dacl && acl_set_file(fname, ACL_TYPE_DEFAULT, dacl) == -1) {
		fprintf(stderr, _("%s: cannot set default acl on \"%s\": %s\n"),
			program, fname, strerror(errno));
		failed++;
	}

	return(failed);
}

static int
walk_dir(acl_t acl, acl_t dacl, const char *fname)
{
	int failed = 0;
	DIR *dir;
	struct dirent *d;
	char *name;

	if ((dir = opendir(fname)) == NULL) {
		if (errno != ENOTDIR) {
			fprintf(stderr, _("%s: opendir failed: %s\n"),
				program, strerror(errno));
			return(1);
		}
		return(0);	/* got a file, not an error */
	}

	while ((d = readdir(dir)) != NULL) {
		/* skip "." and ".." entries */
		if (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0)
			continue;
		
		name = malloc(strlen(fname) + strlen(d->d_name) + 2);
		if (name == NULL) {
			fprintf(stderr, _("%s: malloc failed: %s\n"),
				program, strerror(errno));
			exit(1);
		}
		sprintf(name, "%s/%s", fname, d->d_name);

		failed += set_acl(acl, dacl, name);
		free(name);
	}
	closedir(dir);

	return(failed);
}
