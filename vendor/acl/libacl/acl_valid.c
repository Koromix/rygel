/*
  File: acl_valid.c

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
#include <errno.h>
#include <stdio.h>
#include "libacl.h"


/* 23.4.28 */
int
acl_valid(acl_t acl)
{
	int result;

	result = acl_check(acl, NULL);
	if (result != 0) {
		if (result > 0)
			errno = EINVAL;
		return -1;
	}
	return 0;
}

