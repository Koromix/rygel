/*
  File: __acl_to_any_text.c

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
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include "libacl.h"
#include "misc.h"

static ssize_t acl_entry_to_any_str(const acl_entry_t entry_d, char *text_p,
				    ssize_t size, const acl_entry_t mask_d,
				    const char *prefix, int options);
static ssize_t snprint_uint(char *text_p, ssize_t size, unsigned int i);
static const char *user_name(uid_t uid);
static const char *group_name(gid_t uid);

char *
__acl_to_any_text(acl_t acl, ssize_t *len_p, const char *prefix,
		  char separator, const char *suffix, int options)
{
	acl_obj *acl_obj_p = ext2int(acl, acl);
	ssize_t size, len = 0, entry_len = 0,
		suffix_len = suffix ? strlen(suffix) : 0;
	string_obj *string_obj_p, *tmp;
	acl_entry_obj *entry_obj_p, *mask_obj_p = NULL;
	if (!acl_obj_p)
		return NULL;
	size = acl->a_used * 15 + 1;
	string_obj_p = new_var_obj_p(string, size);
	if (!string_obj_p)
		return NULL;

	if (options & (TEXT_SOME_EFFECTIVE|TEXT_ALL_EFFECTIVE)) {
		/* fetch the ACL_MASK entry */
		FOREACH_ACL_ENTRY(entry_obj_p, acl_obj_p) {
			if (entry_obj_p->etag == ACL_MASK) {
				mask_obj_p = entry_obj_p;
				break;
			}
		}
	}

	FOREACH_ACL_ENTRY(entry_obj_p, acl_obj_p) {
	repeat:
		entry_len = acl_entry_to_any_str(int2ext(entry_obj_p),
		                                 string_obj_p->s_str + len,
						 size-len,
						 int2ext(mask_obj_p),
						 prefix,
						 options);
		if (entry_len < 0)
			goto fail;
		else if (len + entry_len + suffix_len + 1 > size) {
			while (len + entry_len + suffix_len + 1 > size)
				size <<= 1;
			tmp = realloc_var_obj_p(string, string_obj_p, size);
			if (tmp == NULL)
				goto fail;
			string_obj_p = tmp;
			goto repeat;
		} else
			len += entry_len;
		string_obj_p->s_str[len] = separator;
		len++;
	}
	if (len)
		len--;
	if (len && suffix) {
		strcpy(string_obj_p->s_str + len, suffix);
		len += suffix_len;
	} else
		string_obj_p->s_str[len] = '\0';

	if (len_p)
		*len_p = len;
	return string_obj_p ? string_obj_p->s_str : NULL;

fail:
	free_obj_p(string_obj_p);
	return NULL;
}

#define ADVANCE(x) \
	text_p += (x); \
	size -= (x); \
	if (size < 0) \
		size = 0;

#define ABBREV(s, str_len) \
	if (options & TEXT_ABBREVIATE) { \
		if (size > 0) \
			text_p[0] = *(s); \
		if (size > 1) \
			text_p[1] = ':'; \
		ADVANCE(2); \
	} else { \
		strncpy(text_p, (s), size); \
		ADVANCE(str_len); \
	}

#define EFFECTIVE_STR		"#effective:"

static ssize_t
acl_entry_to_any_str(const acl_entry_t entry_d, char *text_p, ssize_t size,
	const acl_entry_t mask_d, const char *prefix, int options)
{
	#define TABS 4
	static const char *tabs = "\t\t\t\t";
	acl_entry_obj *entry_obj_p = ext2int(acl_entry, entry_d);
	acl_entry_obj *mask_obj_p = NULL;
	permset_t effective;
	acl_tag_t type;
	ssize_t x;
	const char *orig_text_p = text_p, *str;
	if (!entry_obj_p)
		return -1;
	if (mask_d) {
		mask_obj_p = ext2int(acl_entry, mask_d);
		if (!mask_obj_p)
			return -1;
	}
	if (text_p == NULL)
		size = 0;

	if (prefix) {
		strncpy(text_p, prefix, size);
		ADVANCE(strlen(prefix));
	}

	type = entry_obj_p->etag;
	switch (type) {
		case ACL_USER_OBJ:  /* owner */
			mask_obj_p = NULL;
			/* fall through */
		case ACL_USER:  /* additional user */
			ABBREV("user:", 5);
			if (type == ACL_USER) {
				if (options & TEXT_NUMERIC_IDS)
					str = NULL;
				else
					str = __acl_quote(user_name(
						entry_obj_p->eid.qid), ":, \t\n\r");
				if (str != NULL) {
					strncpy(text_p, str, size);
					ADVANCE(strlen(str));
				} else {
					x = snprint_uint(text_p, size,
					             entry_obj_p->eid.qid);
					ADVANCE(x);
				}
			}
			if (size > 0)
				*text_p = ':';
			ADVANCE(1);
			break;

		case ACL_GROUP_OBJ:  /* owning group */
		case ACL_GROUP:  /* additional group */
			ABBREV("group:", 6);
			if (type == ACL_GROUP) {
				if (options & TEXT_NUMERIC_IDS)
					str = NULL;
				else
					str = __acl_quote(group_name(
						entry_obj_p->eid.qid), ":, \t\n\r");
				if (str != NULL) {
					strncpy(text_p, str, size);
					ADVANCE(strlen(str));
				} else {
					x = snprint_uint(text_p, size,
					             entry_obj_p->eid.qid);
					ADVANCE(x);
				}
			}
			if (size > 0)
				*text_p = ':';
			ADVANCE(1);
			break;

		case ACL_MASK:  /* acl mask */
			mask_obj_p = NULL;
			ABBREV("mask:", 5);
			if (size > 0)
				*text_p = ':';
			ADVANCE(1);
			break;

		case ACL_OTHER:  /* other users */
			mask_obj_p = NULL;
			/* fall through */
			ABBREV("other:", 6);
			if (size > 0)
				*text_p = ':';
			ADVANCE(1);
			break;

		default:
			return 0;
	}

	switch ((size >= 3) ? 3 : size) {
		case 3:
			text_p[2] = (entry_obj_p->eperm.sperm &
			             ACL_EXECUTE) ? 'x' : '-'; 
			/* fall through */
		case 2:
			text_p[1] = (entry_obj_p->eperm.sperm &
			             ACL_WRITE) ? 'w' : '-'; 
			/* fall through */
		case 1:
			text_p[0] = (entry_obj_p->eperm.sperm &
			             ACL_READ) ? 'r' : '-'; 
			break;
	}
	ADVANCE(3);

	if (mask_obj_p &&
	    (options & (TEXT_SOME_EFFECTIVE|TEXT_ALL_EFFECTIVE))) {
		mask_obj_p = ext2int(acl_entry, mask_d);
		if (!mask_obj_p)
			return -1;

		effective = entry_obj_p->eperm.sperm &
		                 mask_obj_p->eperm.sperm;
		if (effective != entry_obj_p->eperm.sperm ||
		    options & TEXT_ALL_EFFECTIVE) {
			x = (options & TEXT_SMART_INDENT) ?
				((text_p - orig_text_p)/8) : TABS-1;

			/* use at least one tab for indentation */
			if (x > (TABS-1))
				x = (TABS-1);

			strncpy(text_p, tabs+x, size);
			ADVANCE(TABS-x);

			strncpy(text_p, EFFECTIVE_STR, size);
			ADVANCE(sizeof(EFFECTIVE_STR)-1);

			switch ((size >= 3) ? 3 : size) {
				case 3:
					text_p[2] = (effective &
						     ACL_EXECUTE) ? 'x' : '-'; 
					/* fall through */
				case 2:
					text_p[1] = (effective &
						     ACL_WRITE) ? 'w' : '-'; 
					/* fall through */
				case 1:
					text_p[0] = (effective &
						     ACL_READ) ? 'r' : '-'; 
					break;
			}
			ADVANCE(3);

		}
	}

	/* zero-terminate string (but don't count '\0' character) */
	if (size > 0)
		*text_p = '\0';
	
	return (text_p - orig_text_p);  /* total size required, excluding
	                                   final NULL character. */
}

#undef ADVANCE



/*
  This function is equivalent to the proposed changes to snprintf:
    snprintf(text_p, size, "%u", i)
  (The current snprintf returns -1 if the buffer is too small; the proposal
   is to return the number of characters that would be required. See the
   snprintf manual page.)
*/

static ssize_t
snprint_uint(char *text_p, ssize_t size, unsigned int i)
{
	unsigned int tmp = i;
	int digits = 1;
	unsigned int factor = 1;

	while ((tmp /= 10) != 0) {
		digits++;
		factor *= 10;
	}
	if (size && (i == 0)) {
		*text_p++ = '0';
	} else {
		while (size > 0 && factor > 0) {
			*text_p++ = '0' + (i / factor);
			size--;
			i %= factor;
			factor /= 10;
		}
	}
	if (size)
		*text_p = '\0';

	return digits;
}


static const char *
user_name(uid_t uid)
{
	struct passwd *passwd = getpwuid(uid);

	if (passwd != NULL)
		return passwd->pw_name;
	else
		return NULL;
}


static const char *
group_name(gid_t gid)
{
	struct group *group = getgrgid(gid);

	if (group != NULL)
		return group->gr_name;
	else
		return NULL;
}

