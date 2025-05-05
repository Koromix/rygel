/*
  File: acl_copy_int.c

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
#include "libacl.h"


/* 23.4.6 */
acl_t
acl_copy_int(const void *buf_p)
{
	const struct __acl *ext_acl = (struct __acl *)buf_p;
	const struct __acl_entry *ent_p, *end_p;
	size_t size;
	int entries;
	acl_obj *acl_obj_p;
	acl_entry_obj *entry_obj_p;

	if (!ext_acl || ext_acl->x_size < sizeof(struct __acl)) {
		errno = EINVAL;
		return NULL;
	}
	ent_p = ext_acl->x_entries;
	size = ext_acl->x_size - sizeof(struct __acl);
	if (size % sizeof(struct __acl_entry)) {
		errno = EINVAL;
		return NULL;
	}
	entries = size / sizeof(struct __acl_entry);
	acl_obj_p = __acl_init_obj(entries);
	if (acl_obj_p == NULL)
		return NULL;
	end_p = ext_acl->x_entries + entries;
	for(; ent_p != end_p; ent_p++) {
		entry_obj_p = __acl_create_entry_obj(acl_obj_p);
		if (!entry_obj_p)
			goto fail;
		/* XXX Convert to machine endianness */
		entry_obj_p->eentry = *ent_p;
	}
	if (__acl_reorder_obj_p(acl_obj_p))
		goto fail;
	return int2ext(acl_obj_p);

fail:
	__acl_free_acl_obj(acl_obj_p);
	return NULL;
}
