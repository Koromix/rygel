/*
  Copyright (C) 2000, 2002, 2003  Andreas Gruenbacher <agruen@suse.de>

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
#include <errno.h>
#include <sys/acl.h>
#include <acl/libacl.h>
#include <errno.h>
#include "libobj.h"

#ifndef ENOATTR
# define ENOATTR ENODATA
#endif
#ifndef ENODATA
# define ENODATA ENOATTR
#endif

typedef unsigned int permset_t;

#define ACL_PERM_NONE		(0x0000)

/* object types */
struct acl_permset_obj_tag;
typedef struct acl_permset_obj_tag acl_permset_obj;
struct qualifier_obj_tag;
typedef struct qualifier_obj_tag qualifier_obj;
struct acl_entry_obj_tag;
typedef struct acl_entry_obj_tag acl_entry_obj;
struct acl_obj_tag;
typedef struct acl_obj_tag acl_obj;

/* permset_t object */
struct __acl_permset_ext {
	permset_t		s_perm;
};
struct acl_permset_obj_tag {
	obj_prefix		o_prefix;
	struct __acl_permset_ext i;
};

#define sperm i.s_perm
#define oprefix i.o_prefix

#define permset_obj_equal(s1, s2) \
	((s1).sperm == (s2).sperm)

/* qualifier object */
struct __qualifier_ext {
        id_t                    q_id;
};

struct qualifier_obj_tag {
	obj_prefix		o_prefix;
	struct __qualifier_ext	i;
};

#define qid i.q_id

#define qualifier_obj_id(q) \
	((q).qid)

/* acl_entry object */
struct __acl_entry {
	acl_tag_t		e_tag;
	qualifier_obj		e_id;
	acl_permset_obj		e_perm;
};

struct __acl_entry_ext {
	acl_entry_obj		*e_prev, *e_next;
	acl_obj			*e_container;
	struct __acl_entry	e_entry;
};

struct acl_entry_obj_tag {
	obj_prefix              o_prefix;
	struct __acl_entry_ext	i;
};
	
#define econtainer i.e_container
#define eprev i.e_prev
#define enext i.e_next
#define eentry i.e_entry
#define etag i.e_entry.e_tag
#define eperm i.e_entry.e_perm
#define eid   i.e_entry.e_id

#define init_acl_entry_obj(entry) do { \
	(entry).etag = ACL_UNDEFINED_TAG; \
	new_obj_p_here(acl_permset, &(entry).eperm); \
	(entry).eperm.sperm = ACL_PERM_NONE; \
	new_obj_p_here(qualifier, &(entry).eid); \
	(entry).eid.qid = ACL_UNDEFINED_ID; \
	} while(0)

/* acl object */
struct __acl_ext {
	acl_entry_obj		*a_prev, *a_next;
	acl_entry_obj		*a_curr;
	acl_entry_obj		*a_prealloc, *a_prealloc_end;
	size_t			a_used;
};
struct acl_obj_tag {
	obj_prefix              o_prefix;
	struct __acl_ext	i;
};

#define aprev		i.a_prev
#define anext		i.a_next
#define acurr		i.a_curr
#define aused		i.a_used
#define aprealloc	i.a_prealloc
#define aprealloc_end	i.a_prealloc_end

/* external ACL representation */
struct __acl {
	size_t			x_size;
	struct __acl_entry	x_entries[0];
};

extern int __acl_reorder_entry_obj_p(acl_entry_obj *acl_entry_obj_p) hidden;
extern int __acl_reorder_obj_p(acl_obj *acl_obj_p) hidden;

extern acl_obj *__acl_init_obj(int count) hidden;
extern acl_entry_obj *__acl_create_entry_obj(acl_obj *acl_obj_p) hidden;
extern void __acl_free_acl_obj(acl_obj *acl_obj_p) hidden;

extern char *__acl_to_any_text(acl_t acl, ssize_t *len_p,
			       const char *prefix, char separator,
			       const char *suffix, int options) hidden;
extern int __acl_apply_mask_to_mode(mode_t *mode, acl_t acl) hidden;

#define FOREACH_ACL_ENTRY(entry_obj_p, acl_obj_p) \
	for( (entry_obj_p) = (acl_obj_p)->anext; \
	     (entry_obj_p) != (acl_entry_obj *)(acl_obj_p); \
	     (entry_obj_p) = (entry_obj_p)->enext )
