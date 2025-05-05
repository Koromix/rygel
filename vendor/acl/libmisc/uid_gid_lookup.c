/*
  File: uid_gid_lookup.c

  Copyright (C) 2023 Andreas Gruenbacher <andreas.gruenbacher@gmail.com>

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
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include "libacl.h"
#include "misc.h"

static int
get_id(const char *token, id_t *id_p)
{
	char *ep;
	long l;
	l = strtol(token, &ep, 0);
	if (*ep != '\0')
		return -1;
	if (l < 0) {
		/*
		  Negative values are interpreted as 16-bit numbers
		  so that id -2 maps to 65534 (nobody/nogroup), etc.
		*/
		l &= 0xFFFF;
	}
	*id_p = l;
	return 0;
}

static char *
grow_buffer(char **buffer, size_t *bufsize, int type)
{
	long size = *bufsize;
	char *buf;

	if (!size) {
		size = sysconf(type);
		if (size <= 0)
			size = 16384;
	} else {
		size *= 2;
	}

	buf = realloc(*buffer, size);
	if (buf) {
		*buffer = buf;
		*bufsize = size;
	}
	return buf;
}

int
__acl_get_uid(const char *token, uid_t *uid_p)
{
	struct passwd passwd, *result = NULL;
	char *buffer = NULL;
	size_t bufsize = 0;
	int err;
	if (get_id(token, uid_p) == 0)
		return 0;

	for(;;) {
		if(!grow_buffer(&buffer, &bufsize, _SC_GETPW_R_SIZE_MAX))
			break;

		err = getpwnam_r(token, &passwd, buffer, bufsize, &result);
		if (result) {
			*uid_p = passwd.pw_uid;
			break;
		}
		if (err == ERANGE)
			continue;
		errno = err ? err : EINVAL;
		break;
	}
	free(buffer);
	return result ? 0 : -1;
}


int
__acl_get_gid(const char *token, gid_t *gid_p)
{
	struct group group, *result = NULL;
	char *buffer = NULL;
	size_t bufsize = 0;
	int err;
	if (get_id(token, (uid_t *)gid_p) == 0)
		return 0;

	for(;;) {
		if(!grow_buffer(&buffer, &bufsize, _SC_GETGR_R_SIZE_MAX))
			break;

		err = getgrnam_r(token, &group, buffer, bufsize, &result);
		if (result) {
			*gid_p = group.gr_gid;
			break;
		}
		if (err == ERANGE)
			continue;
		errno = err ? err : EINVAL;
		break;
	}

	free(buffer);
	return result ? 0 : -1;
}
