/*
  File: __acl_from_xattr.c

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

#include "byteorder.h"
#include "acl_ea.h"


acl_t
__acl_from_xattr(const char *ext_acl_p, size_t size)
{
	acl_ea_header *ext_header_p = (acl_ea_header *)ext_acl_p;
	acl_ea_entry *ext_entry_p = (acl_ea_entry *)(ext_header_p+1);
	acl_ea_entry *ext_end_p;
	acl_obj *acl_obj_p;
	acl_entry_obj *entry_obj_p;
	int entries;

	if (size < sizeof(acl_ea_header)) {
		errno = EINVAL;
		return NULL;
	}
	if (ext_header_p->a_version != cpu_to_le32(ACL_EA_VERSION)) {
		errno = EINVAL;
		return NULL;
	}
	size -= sizeof(acl_ea_header);
	if (size % sizeof(acl_ea_entry)) {
		errno = EINVAL;
		return NULL;
	}
	entries = size / sizeof(acl_ea_entry);
	ext_end_p = ext_entry_p + entries;

	acl_obj_p = __acl_init_obj(entries);
	if (acl_obj_p == NULL)
		return NULL;
	while (ext_end_p != ext_entry_p) {
		entry_obj_p = __acl_create_entry_obj(acl_obj_p);
		if (!entry_obj_p)
			goto fail;

		entry_obj_p->etag        = le16_to_cpu(ext_entry_p->e_tag);
		entry_obj_p->eperm.sperm = le16_to_cpu(ext_entry_p->e_perm);

		switch(entry_obj_p->etag) {
			case ACL_USER_OBJ:
			case ACL_GROUP_OBJ:
			case ACL_MASK:
			case ACL_OTHER:
				entry_obj_p->eid.qid = ACL_UNDEFINED_ID;
				break;

			case ACL_USER:
			case ACL_GROUP:
				entry_obj_p->eid.qid =
					le32_to_cpu(ext_entry_p->e_id);
				break;

			default:
				errno = EINVAL;
				goto fail;
		}
		ext_entry_p++;
	}
	if (__acl_reorder_obj_p(acl_obj_p))
		goto fail;
	return int2ext(acl_obj_p);

fail:
	__acl_free_acl_obj(acl_obj_p);
	return NULL;
}

