/*
  File: quote.c

  Copyright (C) 2003 Andreas Gruenbacher <andreas.gruenbacher@gmail.com>

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
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "misc.h"

const char *__acl_quote(const char *str, const char *quote_chars)
{
	static char *quoted_str;
	static size_t quoted_str_len;
	const unsigned char *s;
	char *q;
	size_t nonpr;

	if (!str)
		return str;

	for (nonpr = 0, s = (unsigned char *)str; *s != '\0'; s++)
		if (*s == '\\' || strchr(quote_chars, *s))
			nonpr++;
	if (nonpr == 0)
		return str;

	if (__acl_high_water_alloc((void **)&quoted_str, &quoted_str_len,
			     (s - (unsigned char *)str) + nonpr * 3 + 1))
		return NULL;
	for (s = (unsigned char *)str, q = quoted_str; *s != '\0'; s++) {
		if (strchr(quote_chars, *s)) {
			*q++ = '\\';
			*q++ = '0' + ((*s >> 6)    );
			*q++ = '0' + ((*s >> 3) & 7);
			*q++ = '0' + ((*s     ) & 7);
		} else if (*s == '\\') {
			*q++ = '\\';
			*q++ = '\\';
		} else
			*q++ = *s;
	}
	*q++ = '\0';

	return quoted_str;
}
