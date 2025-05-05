/*
  File: acl_ea.h

  (extended attribute representation of access control lists)

  Copyright (C) 2002  Andreas Gruenbacher, <agruen@suse.de>

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

#define ACL_EA_ACCESS		"system.posix_acl_access"
#define ACL_EA_DEFAULT		"system.posix_acl_default"

#define ACL_EA_VERSION		0x0002

typedef struct {
	u_int16_t	e_tag;
	u_int16_t	e_perm;
	u_int32_t	e_id;
} acl_ea_entry;

typedef struct {
	u_int32_t	a_version;
	acl_ea_entry	a_entries[0];
} acl_ea_header;

static inline size_t acl_ea_size(int count)
{
	return sizeof(acl_ea_header) + count * sizeof(acl_ea_entry);
}

static inline int acl_ea_count(size_t size)
{
	if (size < sizeof(acl_ea_header))
		return -1;
	size -= sizeof(acl_ea_header);
	if (size % sizeof(acl_ea_entry))
		return -1;
	return size / sizeof(acl_ea_entry);
}

