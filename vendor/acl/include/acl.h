/*
  File: sys/acl.h

  (C) 1999 Andreas Gruenbacher, <a.gruenbacher@computer.org>

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

#ifndef __SYS_ACL_H
#define __SYS_ACL_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=== Data types ===*/

struct __acl_ext;
struct __acl_entry_ext;
struct __acl_permset_ext;

typedef unsigned int		acl_type_t;
typedef int			acl_tag_t;
typedef unsigned int		acl_perm_t;

typedef struct __acl_ext	*acl_t;
typedef struct __acl_entry_ext	*acl_entry_t;
typedef struct __acl_permset_ext *acl_permset_t;

/*=== Constants ===*/

/* 23.2.2 acl_perm_t values */

#define ACL_READ		(0x04)
#define ACL_WRITE		(0x02)
#define ACL_EXECUTE		(0x01)

/* 23.2.5 acl_tag_t values */

#define ACL_UNDEFINED_TAG	(0x00)
#define ACL_USER_OBJ		(0x01)
#define ACL_USER		(0x02)
#define ACL_GROUP_OBJ		(0x04)
#define ACL_GROUP		(0x08)
#define ACL_MASK		(0x10)
#define ACL_OTHER		(0x20)

/* 23.3.6 acl_type_t values */

#define ACL_TYPE_ACCESS		(0x8000)
#define ACL_TYPE_DEFAULT	(0x4000)

/* 23.2.7 ACL qualifier constants */

#define ACL_UNDEFINED_ID	((id_t)-1)

/* 23.2.8 ACL Entry Constants */

#define ACL_FIRST_ENTRY		0
#define ACL_NEXT_ENTRY		1

/*=== ACL manipulation ===*/

EXPORT acl_t acl_init(int count);
EXPORT acl_t acl_dup(acl_t acl);
EXPORT int acl_free(void *obj_p);
EXPORT int acl_valid(acl_t acl);

/*=== Entry manipulation ===*/

EXPORT int
acl_copy_entry(acl_entry_t dest_d, acl_entry_t src_d);
EXPORT int acl_create_entry(acl_t *acl_p, acl_entry_t *entry_p);
EXPORT int acl_delete_entry(acl_t acl, acl_entry_t entry_d);
EXPORT int acl_get_entry(acl_t acl, int entry_id, acl_entry_t *entry_p);

/* Manipulate ACL entry permissions */

EXPORT int acl_add_perm(acl_permset_t permset_d, acl_perm_t perm);
EXPORT int acl_calc_mask(acl_t *acl_p);
EXPORT int acl_clear_perms(acl_permset_t permset_d);
EXPORT int acl_delete_perm(acl_permset_t permset_d, acl_perm_t perm);
EXPORT int acl_get_permset(acl_entry_t entry_d, acl_permset_t *permset_p);
EXPORT int acl_set_permset(acl_entry_t entry_d, acl_permset_t permset_d);

/* Manipulate ACL entry tag type and qualifier */

EXPORT void * acl_get_qualifier(acl_entry_t entry_d);
EXPORT int acl_get_tag_type(acl_entry_t entry_d, acl_tag_t *tag_type_p);
EXPORT int acl_set_qualifier(acl_entry_t entry_d, const void *tag_qualifier_p);
EXPORT int acl_set_tag_type(acl_entry_t entry_d, acl_tag_t tag_type);

/*=== Format translation ===*/

EXPORT ssize_t acl_copy_ext(void *buf_p, acl_t acl, ssize_t size);
EXPORT acl_t acl_copy_int(const void *buf_p);
EXPORT acl_t acl_from_text(const char *buf_p);
EXPORT ssize_t acl_size(acl_t acl);
EXPORT char *acl_to_text(acl_t acl, ssize_t *len_p);

/*=== Object manipulation ===*/

EXPORT int acl_delete_def_file(const char *path_p);
EXPORT acl_t acl_get_fd(int fd);
EXPORT acl_t acl_get_file(const char *path_p, acl_type_t type);
EXPORT int acl_set_fd(int fd, acl_t acl);
EXPORT int acl_set_file(const char *path_p, acl_type_t type, acl_t acl);

#ifdef __cplusplus
}
#endif

#endif  /* __SYS_ACL_H */

