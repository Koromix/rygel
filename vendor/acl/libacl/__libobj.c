/*
  File: __libobj.c
  (Linux Access Control List Management)

  Copyright (C) 1999-2002
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
#include <errno.h>
#include <stdlib.h>
#include "libobj.h"

#ifdef LIBACL_DEBUG
# include <stdio.h>
#endif

/* object creation, destruction, conversion and validation */

void *
__acl_new_var_obj_p(int magic, size_t size)
{
	obj_prefix *obj_p = (obj_prefix *)malloc(size);
	if (obj_p) {
		obj_p->p_magic = (long)magic;
		obj_p->p_flags = OBJ_MALLOC_FLAG;
	}
	return obj_p;
}


void
__acl_new_obj_p_here(int magic, void *here)
{
	obj_prefix *obj_p = here;
	obj_p->p_magic = (long)magic;
	obj_p->p_flags = 0;
}


void
__acl_free_obj_p(obj_prefix *obj_p)
{
	obj_p->p_magic = 0;
	if (obj_p->p_flags & OBJ_MALLOC_FLAG)
		free(obj_p);
}


obj_prefix *
__acl_check_obj_p(obj_prefix *obj_p, int magic)
{
	if (!obj_p || obj_p->p_magic != (long)magic) {
		errno = EINVAL;
		return NULL;
	}
	return obj_p;
}


#ifdef LIBACL_DEBUG
obj_prefix *
__acl_ext2int_and_check(void *ext_p, int magic, const char *typename)
#else
obj_prefix *
__acl_ext2int_and_check(void *ext_p, int magic)
#endif
{
	obj_prefix *obj_p = ((obj_prefix *)ext_p)-1;
	if (!ext_p) {
#ifdef LIBACL_DEBUG
		fprintf(stderr, "invalid %s object at %p\n",
		        typename, obj_p);
#endif
		errno = EINVAL;
		return NULL;
	}
	return __acl_check_obj_p(obj_p, magic);
}

