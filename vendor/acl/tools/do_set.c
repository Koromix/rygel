/*
  File: do_set.c
  (Linux Access Control List Management)

  Copyright (C) 1999, 2000
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
#include <stdio.h>
#include <errno.h>
#include <sys/acl.h>
#include <acl/libacl.h>

#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <ftw.h>
#include "misc.h"
#include "sequence.h"
#include "do_set.h"
#include "parse.h"
#include "walk_tree.h"


static acl_entry_t
find_entry(
	acl_t acl,
	acl_tag_t type,
	id_t id)
{
	acl_entry_t ent;
	acl_tag_t e_type;
	id_t *e_id_p;

	if (acl_get_entry(acl, ACL_FIRST_ENTRY, &ent) != 1)
		return NULL;

	for(;;) {
		acl_get_tag_type(ent, &e_type);
		if (type == e_type) {
			if (id != ACL_UNDEFINED_ID) {
				e_id_p = acl_get_qualifier(ent);
				if (e_id_p == NULL)
					return NULL;
				if (*e_id_p == id) {
					acl_free(e_id_p);
					return ent;
				}
				acl_free(e_id_p);
			} else {
				return ent;
			}
		}
		if (acl_get_entry(acl, ACL_NEXT_ENTRY, &ent) != 1)
			return NULL;
	}
}

static int
has_execute_perms(
	acl_t acl)
{
	acl_entry_t ent;

	if (acl_get_entry(acl, ACL_FIRST_ENTRY, &ent) != 1)
		return 0;

	for(;;) {
		acl_permset_t permset;

		acl_get_permset(ent, &permset);
		if (acl_get_perm(permset, ACL_EXECUTE) != 0)
			return 1;

		if (acl_get_entry(acl, ACL_NEXT_ENTRY, &ent) != 1)
			return 0;
	}
}

static int
clone_entry(
	acl_t from_acl,
	acl_tag_t from_type,
	acl_t *to_acl,
	acl_tag_t to_type)
{
	acl_entry_t from_entry, to_entry;
	from_entry = find_entry(from_acl, from_type, ACL_UNDEFINED_ID);
	if (from_entry) {
		if (acl_create_entry(to_acl, &to_entry) != 0)
			return -1;
		acl_copy_entry(to_entry, from_entry);
		acl_set_tag_type(to_entry, to_type);
		return 0;
	} else {
		return 1;
	}
}


static void
print_test(
	FILE *file,
	const char *path_p,
	const struct stat *st,
	const acl_t acl,
	const acl_t default_acl)
{
	char *acl_text, *default_acl_text;

	acl_text = acl_to_any_text(acl, NULL, ',', TEXT_ABBREVIATE);
	default_acl_text =
		acl_to_any_text(default_acl, "d:", ',', TEXT_ABBREVIATE);
	fprintf(file, "%s: %s,%s\n", path_p,
		acl_text ? acl_text : "*",
		default_acl_text ? default_acl_text : "*");
	acl_free(acl_text);
	acl_free(default_acl_text);
}


static void
set_perm(
	acl_entry_t ent,
	mode_t perm)
{
	acl_permset_t set;

	acl_get_permset(ent, &set);
	if (perm & CMD_PERM_READ)
		acl_add_perm(set, ACL_READ);
	else
		acl_delete_perm(set, ACL_READ);
	if (perm & CMD_PERM_WRITE)
		acl_add_perm(set, ACL_WRITE);
	else
		acl_delete_perm(set, ACL_WRITE);
	if (perm & CMD_PERM_EXECUTE)
		acl_add_perm(set, ACL_EXECUTE);
	else
		acl_delete_perm(set, ACL_EXECUTE);
}


static int
retrieve_acl(
	const char *path_p,
	acl_type_t type,
	const struct stat *st,
	acl_t *old_acl,
	acl_t *acl)
{
	if (*acl)
		return 0;
	*acl = NULL;
	if (type == ACL_TYPE_ACCESS || S_ISDIR(st->st_mode)) {
		*old_acl = acl_get_file(path_p, type);
		if (*old_acl == NULL && (errno == ENOSYS || errno == ENOTSUP)) {
			if (type == ACL_TYPE_DEFAULT)
				*old_acl = acl_init(0);
			else
				*old_acl = acl_from_mode(st->st_mode);
		}
	} else
		*old_acl = acl_init(0);
	if (*old_acl == NULL)
		return -1;
	*acl = acl_dup(*old_acl);
	if (*acl == NULL)
		return -1;
	return 0;
}


static int
remove_extended_entries(
	acl_t acl)
{
	acl_entry_t ent, group_obj;
	acl_permset_t mask_permset, group_obj_permset;
	acl_tag_t tag;
	int error;
	
	/*
	 * Removing the ACL_MASK entry from the ACL results in
	 * increased permissions for the owning group if the
	 * ACL_GROUP_OBJ entry contains permissions not contained
	 * in the ACL_MASK entry. We remove these permissions from
	 * the ACL_GROUP_OBJ entry to avoid that.
	 *
	 * After removing the ACL, the file owner and the owning group
	 * therefore have the same permissions as before.
	 */

	ent = find_entry(acl, ACL_MASK, ACL_UNDEFINED_ID);
	group_obj = find_entry(acl, ACL_GROUP_OBJ, ACL_UNDEFINED_ID);
	if (ent && group_obj) {
		if (!acl_get_permset(ent, &mask_permset) &&
		    !acl_get_permset(group_obj, &group_obj_permset)) {
			if (!acl_get_perm(mask_permset, ACL_READ))
				acl_delete_perm(group_obj_permset, ACL_READ);
			if (!acl_get_perm(mask_permset, ACL_WRITE))
				acl_delete_perm(group_obj_permset, ACL_WRITE);
			if (!acl_get_perm(mask_permset, ACL_EXECUTE))
				acl_delete_perm(group_obj_permset, ACL_EXECUTE);
		}
	}

	error = acl_get_entry(acl, ACL_FIRST_ENTRY, &ent);
	while (error == 1) {
		acl_get_tag_type(ent, &tag);
		switch(tag) {
			case ACL_USER:
			case ACL_GROUP:
			case ACL_MASK:
				acl_delete_entry(acl, ent);
				break;
			default:
				break;
		}
	
		error = acl_get_entry(acl, ACL_NEXT_ENTRY, &ent);
	}
	if (error < 0)
		return -1;
	return 0;
}


#define RETRIEVE_ACL(type) do { \
	error = retrieve_acl(path_p, type, st, old_xacl, xacl); \
	if (error) \
		goto fail; \
	} while(0)

int
do_set(
	const char *path_p,
	const struct stat *st,
	int walk_flags,
	void *arg)
{
	struct do_set_args *args = arg;
	acl_t old_acl = NULL, old_default_acl = NULL;
	acl_t acl = NULL, default_acl = NULL;
	acl_t *xacl, *old_xacl;
	acl_entry_t ent;
	cmd_t cmd;
	int which_entry;
	int errors = 0, error;
	char *acl_text;
	int acl_modified = 0, default_acl_modified = 0;
	int acl_mask_provided = 0, default_acl_mask_provided = 0;

	if (walk_flags & WALK_TREE_FAILED) {
		fprintf(stderr, "%s: %s: %s\n", progname, path_p, strerror(errno));
		return 1;
	}

	/*
	 * Symlinks can never have ACLs, so when doing a physical walk, we
	 * skip symlinks altogether, and when doing a half-logical walk, we
	 * skip all non-toplevel symlinks. 
	 */
	if ((walk_flags & WALK_TREE_SYMLINK) &&
	    ((walk_flags & WALK_TREE_PHYSICAL) ||
	     !(walk_flags & (WALK_TREE_TOPLEVEL | WALK_TREE_LOGICAL))))
		return 0;

	/* Execute the commands in seq (read ACLs on demand) */
	error = seq_get_cmd(args->seq, SEQ_FIRST_CMD, &cmd);
	if (error == 0)
		return 0;
	while (error == 1) {
		mode_t perm = cmd->c_perm;

		if (cmd->c_type == ACL_TYPE_ACCESS) {
			xacl = &acl;
			old_xacl = &old_acl;
			acl_modified = 1;
			if (cmd->c_tag == ACL_MASK)
				acl_mask_provided = 1;
		} else {
			xacl = &default_acl;
			old_xacl = &old_default_acl;
			default_acl_modified = 1;
			if (cmd->c_tag == ACL_MASK)
				default_acl_mask_provided = 1;
		}

		RETRIEVE_ACL(cmd->c_type);

		/* Check for `X', and replace with `x' as appropriate. */
		if (perm & CMD_PERM_COND_EXECUTE) {
			perm &= ~CMD_PERM_COND_EXECUTE;
			if (S_ISDIR(st->st_mode) || has_execute_perms(*xacl))
				perm |= CMD_PERM_EXECUTE;
		}

		switch(cmd->c_cmd) {
			case CMD_ENTRY_REPLACE:
				ent = find_entry(*xacl, cmd->c_tag, cmd->c_id);
				if (!ent) {
					if (acl_create_entry(xacl, &ent) != 0)
						goto fail;
					acl_set_tag_type(ent, cmd->c_tag);
					if (cmd->c_id != ACL_UNDEFINED_ID)
						acl_set_qualifier(ent,
								  &cmd->c_id);
				}
				set_perm(ent, perm);
				break;

			case CMD_REMOVE_ENTRY:
				ent = find_entry(*xacl, cmd->c_tag, cmd->c_id);
				if (ent)
					acl_delete_entry(*xacl, ent);
				else
					/* ignore */;
				break;

			case CMD_REMOVE_EXTENDED_ACL:
				remove_extended_entries(acl);
				break;

			case CMD_REMOVE_ACL:
				acl_free(*xacl);
				*xacl = acl_init(5);
				if (!*xacl)
					goto fail;
				break;

			default:
				errno = EINVAL;
				goto fail;
		}

		error = seq_get_cmd(args->seq, SEQ_NEXT_CMD, &cmd);
	}

	if (error < 0)
		goto fail;

	/* Try to fill in missing entries */
	if (default_acl && acl_entries(default_acl) != 0) {
		xacl = &acl;
		old_xacl = &old_acl;
	
		if (!find_entry(default_acl, ACL_USER_OBJ, ACL_UNDEFINED_ID)) {
			if (!acl)
				RETRIEVE_ACL(ACL_TYPE_ACCESS);
			clone_entry(acl, ACL_USER_OBJ,
			            &default_acl, ACL_USER_OBJ);
		}
		if (!find_entry(default_acl, ACL_GROUP_OBJ, ACL_UNDEFINED_ID)) {
			if (!acl)
				RETRIEVE_ACL(ACL_TYPE_ACCESS);
			clone_entry(acl, ACL_GROUP_OBJ,
			            &default_acl, ACL_GROUP_OBJ);
		}
		if (!find_entry(default_acl, ACL_OTHER, ACL_UNDEFINED_ID)) {
			if (!acl)
				RETRIEVE_ACL(ACL_TYPE_ACCESS);
			clone_entry(acl, ACL_OTHER,
			            &default_acl, ACL_OTHER);
		}
	}

	/* update mask entries and check if ACLs are valid */
	if (acl && acl_modified) {
		if (acl_equiv_mode(acl, NULL) != 0) {
			if (!acl_mask_provided &&
			    !find_entry(acl, ACL_MASK, ACL_UNDEFINED_ID))
				clone_entry(acl, ACL_GROUP_OBJ,
				            &acl, ACL_MASK);
			if (opt_recalculate != -1 &&
			    (!acl_mask_provided || opt_recalculate == 1))
				acl_calc_mask(&acl);
		}

		error = acl_check(acl, &which_entry);
		if (error < 0)
			goto fail;
		if (error > 0) {
			acl_text = acl_to_any_text(acl, NULL, ',', 0);
			fprintf(stderr, _("%s: %s: Malformed access ACL "
				"`%s': %s at entry %d\n"), progname, path_p,
				acl_text, acl_error(error), which_entry+1);
			acl_free(acl_text);
			errors++;
			goto cleanup;
		}
	}

	if (default_acl && acl_entries(default_acl) != 0 &&
	    default_acl_modified) {
		if (acl_equiv_mode(default_acl, NULL) != 0) {
			if (!default_acl_mask_provided &&
			    !find_entry(default_acl,ACL_MASK,ACL_UNDEFINED_ID))
				clone_entry(default_acl, ACL_GROUP_OBJ,
				            &default_acl, ACL_MASK);
			if (opt_recalculate != -1 &&
			    (!default_acl_mask_provided ||
			     opt_recalculate == 1))
				acl_calc_mask(&default_acl);
		}

		error = acl_check(default_acl, &which_entry);
		if (error < 0)
			goto fail;
		if (error > 0) {
			acl_text = acl_to_any_text(default_acl, NULL, ',', 0);
			fprintf(stderr, _("%s: %s: Malformed default ACL "
			                  "`%s': %s at entry %d\n"),
				progname, path_p, acl_text,
				acl_error(error), which_entry+1);
			acl_free(acl_text);
			errors++;
			goto cleanup;
		}
	}

	/* Only directories can have default ACLs */
	if (default_acl && !S_ISDIR(st->st_mode) && (walk_flags & WALK_TREE_RECURSIVE)) {
		/* In recursive mode, ignore default ACLs for files */
		acl_free(default_acl);
		default_acl = NULL;
	}

	/* check which ACLs have changed */
	if (acl && old_acl && acl_cmp(old_acl, acl) == 0) {
		acl_free(acl);
		acl = NULL;
	}
	if ((default_acl && old_default_acl &&
	    acl_cmp(old_default_acl, default_acl) == 0)) {
		acl_free(default_acl);
		default_acl = NULL;
	}

	/* update the file system */
	if (opt_test) {
		print_test(stdout, path_p, st,
		           acl, default_acl);
		goto cleanup;
	}
	if (acl) {
		int equiv_mode;
		mode_t mode = 0;

		equiv_mode = acl_equiv_mode(acl, &mode);

		if (acl_set_file(path_p, ACL_TYPE_ACCESS, acl) != 0) {
			if (errno == ENOSYS || errno == ENOTSUP) {
				if (equiv_mode != 0)
					goto fail;
				else {
					struct stat st;

					if (stat(path_p, &st) != 0)
						goto fail;
					mode |= st.st_mode & 07000;
					if (chmod(path_p, mode) != 0)
						goto fail;
				}
			} else
				goto fail;
		}
		args->mode = mode;
	}
	if (default_acl) {
		if (S_ISDIR(st->st_mode)) {
			if (acl_entries(default_acl) == 0) {
				if (acl_delete_def_file(path_p) != 0 &&
				    errno != ENOSYS && errno != ENOTSUP)
					goto fail;
			} else {
				if (acl_set_file(path_p, ACL_TYPE_DEFAULT,
						 default_acl) != 0)
					goto fail;
			}
		} else {
			if (acl_entries(default_acl) != 0) {
				fprintf(stderr, _("%s: %s: Only directories "
						"can have default ACLs\n"),
					progname, path_p);
				errors++;
				goto cleanup;
			}
		}
	}

	error = 0;

cleanup:
	if (acl)
		acl_free(acl);
	if (old_acl)
		acl_free(old_acl);
	if (default_acl)
		acl_free(default_acl);
	if (old_default_acl)
		acl_free(old_default_acl);
	return errors;
	
fail:
	fprintf(stderr, "%s: %s: %s\n", progname, path_p, strerror(errno));
	errors++;
	goto cleanup;
}

