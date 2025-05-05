/*
  File: acl_from_text.c

  Copyright (C) 1999, 2000, 2001
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
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include "libacl.h"
#include "misc.h"


#define SKIP_WS(x) do { \
	while (*(x)==' ' || *(x)=='\t' || *(x)=='\n' || *(x)=='\r') \
		(x)++; \
	if (*(x)=='#') { \
		while (*(x)!='\n' && *(x)!='\0') \
			(x)++; \
	} \
	} while (0)


static int parse_acl_entry(const char **text_p, acl_t *acl_p);


/* 23.4.13 */
acl_t
acl_from_text(const char *buf_p)
{
	acl_t acl;
	acl = acl_init(0);
	if (!acl)
		return NULL;
	if (!buf_p) {
		errno = EINVAL;
		return NULL;
	}
	while (*buf_p != '\0') {
		if (parse_acl_entry(&buf_p, &acl) != 0)
			goto fail;
		SKIP_WS(buf_p);
		if (*buf_p == ',') {
			buf_p++;
			SKIP_WS(buf_p);
		}
	}
	if (*buf_p != '\0') {
		errno = EINVAL;
		goto fail;
	}

	return acl;

fail:
	acl_free(acl);
	return NULL;
}


static int
skip_tag_name(const char **text_p, const char *token)
{
	size_t len = strlen(token);
	const char *text = *text_p;

	SKIP_WS(text);
	if (strncmp(text, token, len) == 0) {
		text += len;
		goto delimiter;
	}
	if (*text == *token) {
		text++;
		goto delimiter;
	}
	return 0;

delimiter:
	SKIP_WS(text);
	if (*text == ':')
		text++;
	*text_p = text;
	return 1;
}


static char *
get_token(const char **text_p)
{
	char *token = NULL;
	const char *ep;

	ep = *text_p;
	SKIP_WS(ep);

	while (*ep!='\0' && *ep!='\r' && *ep!='\n' && *ep!=':' && *ep!=',')
		ep++;
	if (ep == *text_p)
		goto after_token;
	token = (char*)malloc(ep - *text_p + 1);
	if (token == 0)
		goto after_token;
	memcpy(token, *text_p, (ep - *text_p));
	token[ep - *text_p] = '\0';
after_token:
	if (*ep == ':')
		ep++;
	*text_p = ep;
	return token;
}


/*
	Parses the next acl entry in text_p.

	Returns:
		-1 on error, 0 on success.
*/

static int
parse_acl_entry(const char **text_p, acl_t *acl_p)
{
	acl_entry_obj entry_obj;
	acl_entry_t entry_d;
	char *str;
	const char *backup;
	int error, perm_chars;

	new_obj_p_here(acl_entry, &entry_obj);
	init_acl_entry_obj(entry_obj);

	/* parse acl entry type */
	SKIP_WS(*text_p);
	switch (**text_p) {
		case 'u':  /* user */
			if (!skip_tag_name(text_p, "user"))
				goto fail;
			backup = *text_p;
			str = get_token(text_p);
			if (str) {
				entry_obj.etag = ACL_USER;
				error = __acl_get_uid(__acl_unquote(str),
						      &entry_obj.eid.qid);
				free(str);
				if (error) {
					*text_p = backup;
					return -1;
				}
			} else {
				entry_obj.etag = ACL_USER_OBJ;
			}
			break;

		case 'g':  /* group */
			if (!skip_tag_name(text_p, "group"))
				goto fail;
			backup = *text_p;
			str = get_token(text_p);
			if (str) {
				entry_obj.etag = ACL_GROUP;
				error = __acl_get_gid(__acl_unquote(str),
						      &entry_obj.eid.qid);
				free(str);
				if (error) {
					*text_p = backup;
					return -1;
				}
			} else {
				entry_obj.etag = ACL_GROUP_OBJ;
			}
			break;

		case 'm':  /* mask */
			if (!skip_tag_name(text_p, "mask"))
				goto fail;
			/* skip empty entry qualifier field (this field may
			   be missing for compatibility with Solaris.) */
			SKIP_WS(*text_p);
			if (**text_p == ':')
				(*text_p)++;
			entry_obj.etag = ACL_MASK;
			break;

		case 'o':  /* other */
			if (!skip_tag_name(text_p, "other"))
				goto fail;
			/* skip empty entry qualifier field (this field may
			   be missing for compatibility with Solaris.) */
			SKIP_WS(*text_p);
			if (**text_p == ':')
				(*text_p)++;
			entry_obj.etag = ACL_OTHER;
			break;

		default:
			goto fail;
	}

	for (perm_chars=0; perm_chars<3; perm_chars++, (*text_p)++) {
		switch(**text_p) {
			case 'r':
				if (entry_obj.eperm.sperm & ACL_READ)
					goto fail;
				entry_obj.eperm.sperm |= ACL_READ;
				break;

			case 'w':
				if (entry_obj.eperm.sperm  & ACL_WRITE)
					goto fail;
				entry_obj.eperm.sperm  |= ACL_WRITE;
				break;

			case 'x':
				if (entry_obj.eperm.sperm  & ACL_EXECUTE)
					goto fail;
				entry_obj.eperm.sperm  |= ACL_EXECUTE;
				break;

			case '-':
				/* ignore */
				break;

			default:
				if (perm_chars == 0)
					goto fail;
				goto create_entry;
		}
	}

create_entry:
	if (acl_create_entry(acl_p, &entry_d) != 0)
		return -1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Waddress"
	if (acl_copy_entry(entry_d, int2ext(&entry_obj)) != 0)
		return -1;
#pragma GCC diagnostic pop
	return 0;

fail:
	errno = EINVAL;
	return -1;
}

