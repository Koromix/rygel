/*
  File: acl_copy_ext.c

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


/* 23.4.5 */
ssize_t
acl_copy_ext(void *buf_p, acl_t acl, ssize_t size)
{
	struct __acl *acl_ext = (struct __acl *)buf_p;
	struct __acl_entry *ent_p = acl_ext->x_entries;
	acl_obj *acl_obj_p = ext2int(acl, acl);
	acl_entry_obj *entry_obj_p;
	ssize_t size_required;
	
	if (!acl_obj_p)
		return -1;
	size_required = sizeof(struct __acl) +
	                acl_obj_p->aused * sizeof(struct __acl_entry);
	if (size < size_required) {
		errno = ERANGE;
		return -1;
	}
	acl_ext->x_size = size_required;
	FOREACH_ACL_ENTRY(entry_obj_p, acl_obj_p) {
		//ent_p->e_tag  = cpu_to_le16(entry_obj_p->etag);
		//ent_p->e_perm = cpu_to_le16(entry_obj_p->eperm.sperm);
		//ent_p->e_id   = cpu_to_le32(entry_obj_p->eid.quid);
		*ent_p++ = entry_obj_p->eentry;
	}
	return 0;
}

