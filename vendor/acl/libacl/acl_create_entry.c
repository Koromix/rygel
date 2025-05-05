/*
  File: acl_create_entry.c

  Copyright (C) 1999, 2000
  Andreas Gruenbacher <andreas.gruenbacher@gmail.com>

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


acl_entry_obj *
__acl_create_entry_obj(acl_obj *acl_obj_p)
{
	acl_entry_obj *entry_obj_p, *prev;

	if (acl_obj_p->aprealloc == acl_obj_p->aprealloc_end) {
		entry_obj_p = new_obj_p(acl_entry);
		if (!entry_obj_p)
			return NULL;
	} else {
		entry_obj_p = --acl_obj_p->aprealloc_end;
		new_obj_p_here(acl_entry, entry_obj_p);
	}
	acl_obj_p->aused++;

	/* Insert at the end of the entry ring */
	prev = acl_obj_p->aprev;
	entry_obj_p->eprev = prev;
	entry_obj_p->enext = (acl_entry_obj *)acl_obj_p;
	prev->enext = entry_obj_p;
	acl_obj_p->aprev = entry_obj_p;
	
	entry_obj_p->econtainer = acl_obj_p;
	init_acl_entry_obj(*entry_obj_p);

	return entry_obj_p;
}

/* 23.4.7 */
int
acl_create_entry(acl_t *acl_p, acl_entry_t *entry_p)
{
	acl_obj *acl_obj_p;
	acl_entry_obj *entry_obj_p;
	if (!acl_p || !entry_p) {
		if (entry_p)
			*entry_p = NULL;
		errno = EINVAL;
		return -1;
	}
	acl_obj_p = ext2int(acl, *acl_p);
	if (!acl_obj_p)
		return -1;
	entry_obj_p = __acl_create_entry_obj(acl_obj_p);
	if (entry_obj_p == NULL)
		return -1;
	*entry_p = int2ext(entry_obj_p);
	return 0;
}

