/*
  File: acl_set_qualifier.c

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


/* 23.4.24 */
int
acl_set_qualifier(acl_entry_t entry_d, const void *tag_qualifier_p)
{
	acl_entry_obj *entry_obj_p = ext2int(acl_entry, entry_d);
	if (!entry_obj_p)
		return -1;


	switch(entry_obj_p->etag) {
		case ACL_USER:
			entry_obj_p->eid.qid = *(id_t *)tag_qualifier_p;
			break;
		case ACL_GROUP:
			entry_obj_p->eid.qid = *(id_t *)tag_qualifier_p;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	__acl_reorder_entry_obj_p(entry_obj_p);
	return 0;
}

