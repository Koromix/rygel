/*
  File: __acl_to_xattr.c

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
#include "libacl.h"

#include "byteorder.h"
#include "acl_ea.h"


char *
__acl_to_xattr(const acl_obj *acl_obj_p, size_t *size)
{
	const acl_entry_obj *entry_obj_p;
	acl_ea_header *ext_header_p;
	acl_ea_entry *ext_ent_p;

	*size = sizeof(acl_ea_header) + acl_obj_p->aused * sizeof(acl_ea_entry);
	ext_header_p = (acl_ea_header *)malloc(*size);
	if (!ext_header_p)
		return NULL;

	ext_header_p->a_version = cpu_to_le32(ACL_EA_VERSION);
	ext_ent_p = (acl_ea_entry *)(ext_header_p+1);
	FOREACH_ACL_ENTRY(entry_obj_p, acl_obj_p) {
		ext_ent_p->e_tag   = cpu_to_le16(entry_obj_p->etag);
		ext_ent_p->e_perm  = cpu_to_le16(entry_obj_p->eperm.sperm);

		switch(entry_obj_p->etag) {
			case ACL_USER:
			case ACL_GROUP:
				ext_ent_p->e_id =
					cpu_to_le32(entry_obj_p->eid.qid);
				break;

			default:
				ext_ent_p->e_id = ACL_UNDEFINED_ID;
				break;
		}
		ext_ent_p++;
	}
	return (char *)ext_header_p;
}

