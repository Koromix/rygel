/*
  File: acl_get_fd.c

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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/xattr.h>
#include "libacl.h"
#include "__acl_from_xattr.h"

#include "byteorder.h"
#include "acl_ea.h"

/* 23.4.15 */
acl_t
acl_get_fd(int fd)
{
	const size_t size_guess = acl_ea_size(16);
	char *ext_acl_p = alloca(size_guess);
	int retval;

	if (!ext_acl_p)
		return NULL;
	retval = fgetxattr(fd, ACL_EA_ACCESS, ext_acl_p, size_guess);
	if (retval == -1 && errno == ERANGE) {
		retval = fgetxattr(fd, ACL_EA_ACCESS, NULL, 0);
		if (retval > 0) {
			ext_acl_p = alloca(retval);
			if (!ext_acl_p)
				return NULL;
			retval = fgetxattr(fd, ACL_EA_ACCESS, ext_acl_p,retval);
		}
	}
	if (retval > 0) {
		acl_t acl = __acl_from_xattr(ext_acl_p, retval);
		return acl;
	} else if (retval == 0 || errno == ENOATTR || errno == ENODATA) {
		struct stat st;

		if (fstat(fd, &st) == 0)
			return acl_from_mode(st.st_mode);
		else
			return NULL;
	} else
		return NULL;
}

