/*
  Copyright (C) 2009  Andreas Gruenbacher <agruen@suse.de>

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

#ifndef __MISC_H
#define __MISC_H

#include <stdio.h>
#include <sys/types.h>

/* Mark library internal functions as hidden */
#if defined(HAVE_VISIBILITY_ATTRIBUTE)
# define hidden __attribute__((visibility("hidden")))
#else
# define hidden /* hidden */
#endif

hidden int __acl_high_water_alloc(void **buf, size_t *bufsize, size_t newsize);

hidden const char *__acl_quote(const char *str, const char *quote_chars);
hidden char *__acl_unquote(char *str);

hidden char *__acl_next_line(FILE *file);

hidden int __acl_get_uid(const char *token, uid_t *uid_p);
hidden int __acl_get_gid(const char *token, gid_t *gid_p);

#ifdef ENABLE_NLS
# include <libintl.h>
# define _(x)			gettext(x)
#else
# define _(x)			(x)
# define textdomain(d)		do { } while (0)
# define bindtextdomain(d,dir)	do { } while (0)
#endif
#include <locale.h>

#endif
