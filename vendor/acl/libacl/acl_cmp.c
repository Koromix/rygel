/*
  File: acl_cmp.c

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


int
acl_cmp(acl_t acl1, acl_t acl2)
{
	acl_obj *acl1_obj_p = ext2int(acl, acl1),
	        *acl2_obj_p = ext2int(acl, acl2);
	acl_entry_obj *p1_obj_p, *p2_obj_p;
	if (!acl1_obj_p || !acl2_obj_p)
		return -1;
	if (acl1_obj_p->aused != acl2_obj_p->aused)
		return 1;
	p2_obj_p = acl2_obj_p->anext;
	FOREACH_ACL_ENTRY(p1_obj_p, acl1_obj_p) {
		if (p1_obj_p->etag != p2_obj_p->etag)
			return 1;
		if (!permset_obj_equal(p1_obj_p->eperm,p2_obj_p->eperm))
			return 1;
		switch(p1_obj_p->etag) {
			case ACL_USER:
			case ACL_GROUP:
				if (qualifier_obj_id(p1_obj_p->eid) !=
				    qualifier_obj_id(p2_obj_p->eid))
					return 1;
				
		}
		p2_obj_p = p2_obj_p->enext;
	}
	return 0;
}

