/*
  File: acl_check.c

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
#include <stdio.h>
#include <errno.h>
#include "libacl.h"


#define FAIL_CHECK(error) \
	do { return error; } while (0)

/*
  Check if an ACL is valid.

  The e_id fields of ACL entries that don't use them are ignored.

  last
  	contains the index of the last valid entry found
	after acl_check returns.
  returns
  	0 on success, -1 on error, or an ACL_*_ERROR value for invalid ACLs.
*/

int
acl_check(acl_t acl, int *last)
{
	acl_obj *acl_obj_p = ext2int(acl, acl);
	id_t qual = 0;
	int state = ACL_USER_OBJ;
	acl_entry_obj *entry_obj_p;
	int needs_mask = 0;

	if (!acl_obj_p)
		return -1;
	if (last)
		*last = 0;
	FOREACH_ACL_ENTRY(entry_obj_p, acl_obj_p) {
		/* Check permissions for ~(ACL_READ|ACL_WRITE|ACL_EXECUTE) */
		switch (entry_obj_p->etag) {
			case ACL_USER_OBJ:
				if (state == ACL_USER_OBJ) {
					qual = 0;
					state = ACL_USER;
					break;
				}
				FAIL_CHECK(ACL_MULTI_ERROR);

			case ACL_USER:
				if (state != ACL_USER)
					FAIL_CHECK(ACL_MISS_ERROR);
				if (qualifier_obj_id(entry_obj_p->eid) < qual ||
				    qualifier_obj_id(entry_obj_p->eid) ==
				    ACL_UNDEFINED_ID)
					FAIL_CHECK(ACL_DUPLICATE_ERROR);
				qual = qualifier_obj_id(entry_obj_p->eid)+1;
				needs_mask = 1;
				break;

			case ACL_GROUP_OBJ:
				if (state == ACL_USER) {
					qual = 0;
					state = ACL_GROUP;
					break;
				}
				if (state >= ACL_GROUP)
					FAIL_CHECK(ACL_MULTI_ERROR);
				FAIL_CHECK(ACL_MISS_ERROR);

			case ACL_GROUP:
				if (state != ACL_GROUP)
					FAIL_CHECK(ACL_MISS_ERROR);
				if (qualifier_obj_id(entry_obj_p->eid) < qual ||
				    qualifier_obj_id(entry_obj_p->eid) ==
				    ACL_UNDEFINED_ID)
					FAIL_CHECK(ACL_DUPLICATE_ERROR);
				qual = qualifier_obj_id(entry_obj_p->eid)+1;
				needs_mask = 1;
				break;

			case ACL_MASK:
				if (state == ACL_GROUP) {
					state = ACL_OTHER;
					break;
				}
				if (state >= ACL_OTHER)
					FAIL_CHECK(ACL_MULTI_ERROR);
				FAIL_CHECK(ACL_MISS_ERROR);

			case ACL_OTHER:
				if (state == ACL_OTHER ||
				    (state == ACL_GROUP && !needs_mask)) {
					state = 0;
					break;
				}
				FAIL_CHECK(ACL_MISS_ERROR);

			default:
				FAIL_CHECK(ACL_ENTRY_ERROR);
		}
		if (last)
			(*last)++;
	}

	if (state != 0)
		FAIL_CHECK(ACL_MISS_ERROR);
	return 0;
}
#undef FAIL_CHECK

