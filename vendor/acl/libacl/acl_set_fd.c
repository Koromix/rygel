/*
  File: acl_set_fd.c

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
#include <unistd.h>
#include <sys/xattr.h>
#include "libacl.h"
#include "__acl_to_xattr.h"

#include "byteorder.h"
#include "acl_ea.h"


/* 23.4.21 */
int
acl_set_fd(int fd, acl_t acl)
{
	acl_obj *acl_obj_p = ext2int(acl, acl);
	char *ext_acl_p;
	const char *name = ACL_EA_ACCESS;
	size_t size;
	int error;

	if (!acl_obj_p)
		return -1;
	ext_acl_p = __acl_to_xattr(acl_obj_p, &size);
	if (!ext_acl_p)
		return -1;
	error = fsetxattr(fd, name, (char *)ext_acl_p, size, 0);
	free(ext_acl_p);
	return error;
}

