/*
  File: acl_get_file.c

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

/* 23.4.16 */
acl_t
acl_get_file(const char *path_p, acl_type_t type)
{
	const size_t size_guess = acl_ea_size(16);
	char *ext_acl_p = alloca(size_guess);
	const char *name;
	int retval;

	switch(type) {
		case ACL_TYPE_ACCESS:
			name = ACL_EA_ACCESS;
			break;
		case ACL_TYPE_DEFAULT:
			name = ACL_EA_DEFAULT;
			break;
		default:
			errno = EINVAL;
			return NULL;
	}

	if (!ext_acl_p)
		return NULL;
	retval = getxattr(path_p, name, ext_acl_p, size_guess);
	if (retval == -1 && errno == ERANGE) {
		retval = getxattr(path_p, name, NULL, 0);
		if (retval > 0) {
			ext_acl_p = alloca(retval);
			if (!ext_acl_p)
				return NULL;
			retval = getxattr(path_p, name, ext_acl_p, retval);
		}
	}
	if (retval > 0) {
		acl_t acl = __acl_from_xattr(ext_acl_p, retval);
		return acl;
	} else if (retval == 0 || errno == ENOATTR || errno == ENODATA) {
		struct stat st;

		if (stat(path_p, &st) != 0)
			return NULL;

		if (type == ACL_TYPE_DEFAULT) {
			if (S_ISDIR(st.st_mode))
				return acl_init(0);
			else {
				errno = EACCES;
				return NULL;
			}
		} else
			return acl_from_mode(st.st_mode);
	} else
		return NULL;
}

