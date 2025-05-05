/*
  File: libacl.h

  (C) 1999, 2000 Andreas Gruenbacher, <a.gruenbacher@computer.org>

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

#ifndef __ACL_LIBACL_H
#define __ACL_LIBACL_H

#include <sys/acl.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flags for acl_to_any_text() */

/* Print NO, SOME or ALL effective permissions comments. SOME prints
   effective rights comments for entries which have different permissions
   than effective permissions.  */
#define TEXT_SOME_EFFECTIVE		0x01
#define TEXT_ALL_EFFECTIVE		0x02

/* Align effective permission comments to column 32 using tabs or
   use a single tab. */
#define TEXT_SMART_INDENT		0x04

/* User and group IDs instead of names. */
#define TEXT_NUMERIC_IDS		0x08

/* Only output the first letter of entry types
   ("u::rwx" instead of "user::rwx"). */
#define TEXT_ABBREVIATE			0x10

/* acl_check error codes */

#define ACL_MULTI_ERROR		(0x1000)     /* multiple unique objects */
#define ACL_DUPLICATE_ERROR	(0x2000)     /* duplicate Id's in entries */
#define ACL_MISS_ERROR		(0x3000)     /* missing required entry */
#define ACL_ENTRY_ERROR		(0x4000)     /* wrong entry type */

EXPORT char *acl_to_any_text(acl_t acl, const char *prefix,
			     char separator, int options);
EXPORT int acl_cmp(acl_t acl1, acl_t acl2);
EXPORT int acl_check(acl_t acl, int *last);
EXPORT acl_t acl_from_mode(mode_t mode);
EXPORT int acl_equiv_mode(acl_t acl, mode_t *mode_p);
EXPORT int acl_extended_file(const char *path_p);
EXPORT int acl_extended_file_nofollow(const char *path_p);
EXPORT int acl_extended_fd(int fd);
EXPORT int acl_entries(acl_t acl);
EXPORT const char *acl_error(int code);
EXPORT int acl_get_perm(acl_permset_t permset_d, acl_perm_t perm);

/* Copying permissions between files */
struct error_context;
EXPORT int perm_copy_file (const char *, const char *,
			    struct error_context *);
EXPORT int perm_copy_fd (const char *, int, const char *, int,
			  struct error_context *);

#ifdef __cplusplus
}
#endif

#endif  /* __ACL_LIBACL_H */

