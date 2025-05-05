/*
  File: acl_calc_mask.c

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


/* 23.4.2 */
int
acl_calc_mask(acl_t *acl_p)
{
	acl_obj *acl_obj_p;
	acl_entry_obj *entry_obj_p, *mask_obj_p = NULL;
	permset_t perm = ACL_PERM_NONE;
	if (!acl_p) {
		errno = EINVAL;
		return -1;
	}
	acl_obj_p = ext2int(acl, *acl_p);
	if (!acl_obj_p)
		return -1;
	FOREACH_ACL_ENTRY(entry_obj_p, acl_obj_p) {
		switch(entry_obj_p->etag) {
			case ACL_USER_OBJ:
			case ACL_OTHER:
				break;
			case ACL_MASK:
				mask_obj_p = entry_obj_p;
				break;
			case ACL_USER:
			case ACL_GROUP_OBJ:
			case ACL_GROUP:
				perm |= entry_obj_p->eperm.sperm;
				break;
			default:
				errno = EINVAL;
				return -1;
		}
	}
	if (mask_obj_p == NULL) {
		mask_obj_p = __acl_create_entry_obj(acl_obj_p);
		if (mask_obj_p == NULL)
			return -1;
		mask_obj_p->etag = ACL_MASK;
		__acl_reorder_entry_obj_p(mask_obj_p);
	}
	mask_obj_p->eperm.sperm = perm;
	return 0;
}

