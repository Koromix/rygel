/* Copy POSIX 1003.1e draft 17 (abandoned) ACLs between files. */
 
/* Copyright (C) 2002 Andreas Gruenbacher <agruen@suse.de>, SuSE Linux AG.

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

#if defined (HAVE_CONFIG_H)
#include "config.h"
#endif
#if defined(HAVE_LIBACL_LIBACL_H)
# include "libacl.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#if defined(HAVE_SYS_ACL_H)
#include <sys/acl.h>
#endif

#if defined(HAVE_ACL_LIBACL_H)
#include <acl/libacl.h>
#endif

#define ERROR_CONTEXT_MACROS
#ifdef HAVE_ATTR_ERROR_CONTEXT_H
#include <attr/error_context.h>
#else
#include "error_context.h"
#endif

#if !defined(ENOTSUP)
# define ENOTSUP (-1)
#endif

#if !defined(HAVE_ACL_FREE)
static int
acl_free(void *obj_p)
{
	free (obj_p);
	return 0;
}
#endif

#if !defined(HAVE_ACL_ENTRIES)
static int
acl_entries(acl_t acl)
{
# if defined(HAVE_ACL_GET_ENTRY)
	/* POSIX 1003.1e draft 17 (abandoned) compatible version.  */
	acl_entry_t entry;
	int entries = 0;

	int entries = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry);
	if (entries > 0) {
		while (acl_get_entry(acl, ACL_NEXT_ENTRY, &entry) > 0)
			entries++;
	}
	return entries;
# else
	return -1;
# endif
}
#endif

#if !defined(HAVE_ACL_FROM_MODE) && defined(HAVE_ACL_FROM_TEXT)
# define HAVE_ACL_FROM_MODE
static acl_t
acl_from_mode(mode_t mode)
{
	char acl_text[] = "u::---,g::---,o::---";
	acl_t acl;

	if (mode & S_IRUSR) acl_text[ 3] = 'r';
	if (mode & S_IWUSR) acl_text[ 4] = 'w';
	if (mode & S_IXUSR) acl_text[ 5] = 'x';
	if (mode & S_IRGRP) acl_text[10] = 'r';
	if (mode & S_IWGRP) acl_text[11] = 'w';
	if (mode & S_IXGRP) acl_text[12] = 'x';
	if (mode & S_IROTH) acl_text[17] = 'r';
	if (mode & S_IWOTH) acl_text[18] = 'w';
	if (mode & S_IXOTH) acl_text[19] = 'x';

	return acl_from_text (acl_text);
}
#endif

/* Set the access control list of path to the permissions defined by mode.  */
static int
set_acl (char const *path, mode_t mode, struct error_context *ctx)
{
	int ret = 0;
#if defined(HAVE_ACL_FROM_MODE) && defined(HAVE_ACL_SET_FILE)
	/* POSIX 1003.1e draft 17 (abandoned) specific version.  */
	acl_t acl = acl_from_mode (mode);
	if (!acl) {
		error (ctx, "");
		return -1;
	}

	if (acl_set_file (path, ACL_TYPE_ACCESS, acl) != 0) {
		ret = -1;
		if (errno == ENOTSUP || errno == ENOSYS) {
			(void) acl_free (acl);
			goto chmod_only;
		} else {
			const char *qpath = quote (ctx, path);
			error (ctx, _("setting permissions for %s"), qpath);
			quote_free (ctx, qpath);
		}
	}
	(void) acl_free (acl);
	if (ret == 0 && S_ISDIR (mode)) {
# if defined(HAVE_ACL_DELETE_DEF_FILE)
		ret = acl_delete_def_file (path);
# else
		acl = acl_init (0);
		ret = acl_set_file (path, ACL_TYPE_DEFAULT, acl);
		(void) acl_free (acl);
# endif
		if (ret != 0) {
			const char *qpath = quote (ctx, path);
			error (ctx, _( "setting permissions for %s"), qpath);
			quote_free (ctx, qpath);
		}
	}
	return ret;
#endif

chmod_only:
	ret = chmod (path, mode);
	if (ret != 0) {
		const char *qpath = quote (ctx, path);
		error (ctx, _("setting permissions for %s"), qpath);
		quote_free (ctx, qpath);
	}
	return ret;
}

/* Copy the permissions of src_path to dst_path. This includes the
   file mode permission bits and ACLs. File ownership is not copied.
 */
int
perm_copy_file (const char *src_path, const char *dst_path,
		 struct error_context *ctx)
{
#if defined(HAVE_ACL_GET_FILE) && defined(HAVE_ACL_SET_FILE)
	acl_t acl;
#endif
	struct stat st;
	int ret = 0;

	ret = stat(src_path, &st);
	if (ret != 0) {
		const char *qpath = quote (ctx, src_path);
		error (ctx, "%s", qpath);
		quote_free (ctx, qpath);
		return -1;
	}
#if defined(HAVE_ACL_GET_FILE) && defined(HAVE_ACL_SET_FILE)
	/* POSIX 1003.1e draft 17 (abandoned) specific version.  */
	acl = acl_get_file (src_path, ACL_TYPE_ACCESS);
	if (acl == NULL) {
		ret = -1;
		if (errno == ENOSYS || errno == ENOTSUP)
			ret = set_acl (dst_path, st.st_mode, ctx);
		else {
			const char *qpath = quote (ctx, src_path);
			error (ctx, "%s", qpath);
			quote_free (ctx, qpath);
		}
		return ret;
	}

	if (acl_set_file (dst_path, ACL_TYPE_ACCESS, acl) != 0) {
		int saved_errno = errno;
		__acl_apply_mask_to_mode(&st.st_mode, acl);
		ret = chmod (dst_path, st.st_mode);
		if ((errno != ENOSYS && errno != ENOTSUP) ||
		    acl_entries (acl) != 3) {
			const char *qpath = quote (ctx, dst_path);
			errno = saved_errno;
			error (ctx, _("preserving permissions for %s"), qpath);
			quote_free (ctx, qpath);
			ret = -1;
		}
	}
	(void) acl_free (acl);

	if (ret == 0 && S_ISDIR (st.st_mode)) {
		acl = acl_get_file (src_path, ACL_TYPE_DEFAULT);
		if (acl == NULL) {
			const char *qpath = quote (ctx, src_path);
			error (ctx, "%s", qpath);
			quote_free (ctx, qpath);
			return -1;
		}
# if defined(HAVE_ACL_DELETE_DEF_FILE)
		if (acl_entries(acl) == 0)
			ret = acl_delete_def_file(dst_path);
		else
			ret = acl_set_file (dst_path, ACL_TYPE_DEFAULT, acl);
# else
		ret = acl_set_file (dst_path, ACL_TYPE_DEFAULT, acl);
# endif
		if (ret != 0) {
			const char *qpath = quote (ctx, dst_path);
			error (ctx, _("preserving permissions for %s"), qpath);
			quote_free (ctx, qpath);
		}
		(void) acl_free(acl);
	}
	return ret;
#else
	/* POSIX.1 version. */
	ret = chmod (dst_path, st.st_mode);
	if (ret != 0) {
		const char *qpath = quote (ctx, dst_path);
		error (ctx, _("setting permissions for %s"), qpath);
		quote_free (ctx, qpath);
	}
	return ret;
#endif
}

