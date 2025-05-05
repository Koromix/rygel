/*
  File: acl_from_mode.c

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
#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include "libacl.h"


/*
  Create an ACL from a file mode.

  returns
  	the new ACL.
*/

acl_t
acl_from_mode(mode_t mode)
{
	acl_obj *acl_obj_p;
	acl_entry_obj *entry_obj_p;

	acl_obj_p = __acl_init_obj(3);
	if (!acl_obj_p)
		return NULL;

	entry_obj_p = __acl_create_entry_obj(acl_obj_p);
	if (!entry_obj_p)
		goto fail;
	entry_obj_p->etag = ACL_USER_OBJ;
	entry_obj_p->eid.qid = ACL_UNDEFINED_ID;
	entry_obj_p->eperm.sperm = (mode & S_IRWXU) >> 6;

	entry_obj_p = __acl_create_entry_obj(acl_obj_p);
	if (!entry_obj_p)
		goto fail;
	entry_obj_p->etag = ACL_GROUP_OBJ;
	entry_obj_p->eid.qid = ACL_UNDEFINED_ID;
	entry_obj_p->eperm.sperm = (mode & S_IRWXG) >> 3;

	entry_obj_p = __acl_create_entry_obj(acl_obj_p);
	if (!entry_obj_p)
		goto fail;
	entry_obj_p->etag = ACL_OTHER;
	entry_obj_p->eid.qid = ACL_UNDEFINED_ID;
	entry_obj_p->eperm.sperm = mode & S_IRWXO;
	return int2ext(acl_obj_p);

fail:
	__acl_free_acl_obj(acl_obj_p);
	return NULL;
}

