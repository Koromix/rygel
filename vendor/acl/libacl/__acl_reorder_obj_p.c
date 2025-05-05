/*
  File: __acl_reorder_obj_p.c
  (Linux Access Control List Management, Posix Library Functions)

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
#include <alloca.h>
#include "libacl.h"


static inline int
__acl_entry_p_compare(const acl_entry_obj *a_p, const acl_entry_obj *b_p)
{
	if (a_p->etag < b_p->etag)
		return -1;
	else if (a_p->etag > b_p->etag)
		return 1;

	if (a_p->eid.qid < b_p->eid.qid)
		return -1;
	else if (a_p->eid.qid > b_p->eid.qid)
		return 1;
	else
		return 0;
}


static int
__acl_entry_pp_compare(const void *a, const void *b)
{
	return __acl_entry_p_compare(*(const acl_entry_obj **)a,
				   *(const acl_entry_obj **)b);
}


/*
  Take an ACL entry form its current place in the entry ring,
  and insert it at its proper place. Entries that are not valid
  (yet) are not reordered.
*/
int
__acl_reorder_entry_obj_p(acl_entry_obj *entry_obj_p)
{
	acl_obj *acl_obj_p = entry_obj_p->econtainer;
	acl_entry_obj *here_obj_p;

	if (acl_obj_p->aused <= 1)
		return 0;
	switch(entry_obj_p->etag) {
		case ACL_UNDEFINED_TAG:
			return 1;
		case ACL_USER:
		case ACL_GROUP:
			if (qualifier_obj_id(entry_obj_p->eid) ==
			    ACL_UNDEFINED_ID)
				return 1;
	}

	/* Remove entry from ring */
	entry_obj_p->eprev->enext = entry_obj_p->enext;
	entry_obj_p->enext->eprev = entry_obj_p->eprev;

	/* Search for next greater entry */
	FOREACH_ACL_ENTRY(here_obj_p, acl_obj_p) {
		if (__acl_entry_p_compare(here_obj_p, entry_obj_p) > 0)
			break;
	}
	
	/* Re-insert entry into ring */
	entry_obj_p->eprev = here_obj_p->eprev;
	entry_obj_p->enext = here_obj_p;
	entry_obj_p->eprev->enext = entry_obj_p;
	entry_obj_p->enext->eprev = entry_obj_p;

	return 0;
}


/*
  Sort all ACL entries at once, after initializing them. This function is
  only used when converting complete ACLs from external formats to ACLs;
  the ACL entries are always kept in canonical order while an ACL is
  manipulated.
*/
int
__acl_reorder_obj_p(acl_obj *acl_obj_p)
{
	acl_entry_obj **vector = alloca(sizeof(acl_entry_obj *) *
					acl_obj_p->aused), **v, *x;
	acl_entry_obj *entry_obj_p;

	if (acl_obj_p->aused <= 1)
		return 0;

	v = vector;
	FOREACH_ACL_ENTRY(entry_obj_p, acl_obj_p) {
		*v++ = entry_obj_p;
	}

	qsort(vector, acl_obj_p->aused, sizeof(acl_entry_obj *),
	       __acl_entry_pp_compare);

	x = (acl_entry_obj *)acl_obj_p;
	for (v = vector; v != vector + acl_obj_p->aused; v++) {
		(*v)->eprev = x;
		x = *v;
	}
	acl_obj_p->aprev = *(vector + acl_obj_p->aused - 1);

	x = (acl_entry_obj *)acl_obj_p;
	for (v = vector + acl_obj_p->aused - 1; v != vector - 1; v--) {
		(*v)->enext = x;
		x = *v;
	}
	acl_obj_p->anext = *vector;
	return 0;
}

