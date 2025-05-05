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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>
#include <sys/acl.h>

const char *progname;

int main(int argc, char *argv[])
{
	acl_t acl, default_acl;
	int n, ret = 0;

	progname = basename(argv[0]);

	if (argc < 3) {
		printf("%s -- copy access control lists between files \n"
			"Usage: %s file1 file2 ...\n",
			progname, progname);
		return 1;
	}

	acl = acl_get_file(argv[1], ACL_TYPE_ACCESS);
	if (acl == NULL) {
		fprintf(stderr, "%s: getting acl of %s: %s\n",
			progname, argv[1], strerror(errno));
		return 1;
	}
	default_acl = acl_get_file(argv[1], ACL_TYPE_DEFAULT);
	if (default_acl == NULL) {
		fprintf(stderr, "%s: getting default acl of %s: %s\n",
			progname, argv[1], strerror(errno));
		return 1;
	}

	for (n = 2; n < argc; n++) {
		if (acl_set_file(argv[n], ACL_TYPE_ACCESS, acl) != 0) {
			fprintf(stderr, "%s: setting acl for %s: %s\n",
				progname, argv[n], strerror(errno));
			ret = 1;
		} else if (acl_set_file(argv[n], ACL_TYPE_DEFAULT,
		                        default_acl) != 0) {
			fprintf(stderr, "%s: setting default acl for %s: %s\n",
				progname, argv[n], strerror(errno));
			ret = 1;
		}
	}

	acl_free(acl);
	acl_free(default_acl);

	return ret;
}
