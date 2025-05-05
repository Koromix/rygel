/*
  File: acl_copy_entry.c

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


/* 23.4.4 */
int
acl_copy_entry(acl_entry_t dest_d, acl_entry_t src_d)
{
	acl_entry_obj *dest_p = ext2int(acl_entry, dest_d),
	               *src_p = ext2int(acl_entry,  src_d);
	if (!dest_p || !src_p)
		return -1;

	dest_p->etag  = src_p->etag;
	dest_p->eid   = src_p->eid;
	dest_p->eperm = src_p->eperm;
	__acl_reorder_entry_obj_p(dest_p);
	return 0;
}

