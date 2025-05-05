/*
  File: acl_dup.c

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


/* 23.4.11 */
acl_t
acl_dup(acl_t acl)
{
	acl_entry_obj *entry_obj_p, *dup_entry_obj_p;
	acl_obj *acl_obj_p = ext2int(acl, acl);
	acl_obj *dup_obj_p;

	if (!acl_obj_p)
		return NULL;
	dup_obj_p = __acl_init_obj(acl_obj_p->aused);
	if (!dup_obj_p)
		return NULL;

	FOREACH_ACL_ENTRY(entry_obj_p, acl_obj_p) {
		dup_entry_obj_p = __acl_create_entry_obj(dup_obj_p);
		if (dup_entry_obj_p == NULL)
			goto fail;

		dup_entry_obj_p->etag  = entry_obj_p->etag;
		dup_entry_obj_p->eid   = entry_obj_p->eid;
		dup_entry_obj_p->eperm = entry_obj_p->eperm;
	}
	return int2ext(dup_obj_p);

fail:
	__acl_free_acl_obj(dup_obj_p);
	return NULL;
}

