/*
  Copyright (C) 2009  Andreas Gruenbacher <agruen@suse.de>

  This program is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <locale.h>

#include <attr/error_context.h>
#include <attr/libattr.h>
#include <acl/libacl.h>

void
error(struct error_context *ctx, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (vfprintf(stderr, fmt, ap))
		fprintf(stderr, ": ");
	fprintf(stderr, "%s\n", strerror(errno));
	va_end(ap);
}

struct error_context ctx = {
	error
};

int
main(int argc, char *argv[])
{
	int ret;

	setlocale(LC_MESSAGES, "");
	setlocale(LC_CTYPE, "");

	if (argc != 3) {
		fprintf(stderr, "Usage: %s from to\n", argv[0]);
		exit(1);
	}

	ret = perm_copy_file(argv[1], argv[2], &ctx);
	exit (ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

