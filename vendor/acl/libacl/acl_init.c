/*
  File: acl_init.c

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


acl_obj *
__acl_init_obj(int count)
{
	acl_obj *acl_obj_p = new_obj_p(acl);
	if (!acl_obj_p)
		return NULL;
	acl_obj_p->aused = 0;
	acl_obj_p->aprev = acl_obj_p->anext = (acl_entry_obj *)acl_obj_p;
	acl_obj_p->acurr = (acl_entry_obj *)acl_obj_p;

	/* aprealloc points to an array of pre-allocated ACL entries.
	   Entries between [aprealloc, aprealloc_end) are still available.
	   Pre-allocated entries are consumed from the last entry to the
	   first and aprealloc_end decremented. After all pre-allocated
	   entries are consumed, further entries are malloc'ed.
	   aprealloc == aprealloc_end is true when no more pre-allocated	
	   entries are available. */

	if (count > 0)
		acl_obj_p->aprealloc = (acl_entry_obj *)
			malloc(count * sizeof(acl_entry_obj));
	else
		acl_obj_p->aprealloc = NULL;
	if (acl_obj_p->aprealloc != NULL)
		acl_obj_p->aprealloc_end = acl_obj_p->aprealloc + count;
	else
		acl_obj_p->aprealloc_end = NULL;

	return acl_obj_p;
}


/* 23.4.20 */
acl_t
acl_init(int count)
{
	acl_obj *obj;
	if (count < 0) {
		errno = EINVAL;
		return NULL;
	}
	obj = __acl_init_obj(count);
	return int2ext(obj);
}

