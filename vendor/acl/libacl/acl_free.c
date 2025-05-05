/*
  File: acl_free.c

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


void
__acl_free_acl_obj(acl_obj *acl_obj_p)
{
	acl_entry_obj *entry_obj_p;
	while (acl_obj_p->anext != (acl_entry_obj *)acl_obj_p) {
		entry_obj_p = acl_obj_p->anext;
		acl_obj_p->anext = acl_obj_p->anext->enext;
		free_obj_p(entry_obj_p);
	}
	free(acl_obj_p->aprealloc);
	free_obj_p(acl_obj_p);
}


/* 23.4.12 */
int
acl_free(void *obj_p)
{
	obj_prefix *int_p = ((obj_prefix *)obj_p)-1;
	if (!obj_p || !int_p) {
		errno = EINVAL;
		return -1;
	}


	switch(int_p->p_magic) {
		case acl_MAGIC:
			__acl_free_acl_obj((acl_obj *)int_p);
			return 0;
		case qualifier_MAGIC:
		case string_MAGIC:
			free_obj_p(int_p);
			return 0;
		case acl_entry_MAGIC:
		case acl_permset_MAGIC:
#ifdef LIBACL_DEBUG
			fprintf(stderr, "object (magic=0x%X) "
			        "at %p cannot be freed\n",
				int_p->p_magic, int_p);
#endif
			break;
		default:
#ifdef LIBACL_DEBUG
			fprintf(stderr, "invalid object (magic=0x%X) "
			        "at %p\n", int_p->p_magic, int_p);
#endif
			break;
	}
	errno = EINVAL;
	return -1;
}

