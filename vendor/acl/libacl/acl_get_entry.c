/*
  File: acl_get_entry.c

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


/* 23.4.14 */
int
acl_get_entry(acl_t acl, int entry_id, acl_entry_t *entry_p)
{
	acl_obj *acl_obj_p = ext2int(acl, acl);
	if (!acl_obj_p) {
		if (entry_p)
			*entry_p = NULL;
		return -1;
	}
	if (!entry_p) {
		errno = EINVAL;
		return -1;
	}

	if (entry_id == ACL_FIRST_ENTRY) {
		acl_obj_p->acurr = acl_obj_p->anext;
	} else if (entry_id == ACL_NEXT_ENTRY) {
		/*if (acl_obj_p->acurr == (acl_entry_obj *)acl_obj_p) {
			errno = EINVAL;
			return -1;
		}*/
		acl_obj_p->acurr = acl_obj_p->acurr->enext;
	}
	if (acl_obj_p->acurr == (acl_entry_obj *)acl_obj_p) {
		*entry_p = NULL;
		return 0;
	}
	if (!check_obj_p(acl_entry, acl_obj_p->acurr)) {
		return -1;
	}
	*entry_p = int2ext(acl_obj_p->acurr);
	return 1;
}

