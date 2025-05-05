#include "config.h"
#include <assert.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <grp.h>

#define TEST_GROUP "test/test.group"
static char grfile[] = BASEDIR "/" TEST_GROUP;

#define ALIGN_MASK(x, mask)    (((x) + (mask)) & ~(mask))
#define ALIGN(x, a)            ALIGN_MASK(x, (typeof(x))(a) - 1)

static int test_getgrent_r(FILE *file, struct group *grp, char *buf,
			   size_t buflen, struct group **result)
{
	char *line, *str, *remain;
	int count, index = 0;
	int gr_mem_cnt = 0;

	*result = NULL;

	line = fgets(buf, buflen, file);
	if (!line)
		return 0;

	/* We'll stuff the gr_mem array in the remaining space in the buffer */
	remain = buf + ALIGN(line + strlen(line) - buf, sizeof(char *));
	grp->gr_mem = (char **)remain;
	count = (buf + buflen - remain) / sizeof (char *);
	if (!count) {
		errno = ERANGE;
		return -1;
	}

	grp->gr_mem[--count] = NULL;

	while ((str = strtok(line, ":"))) {
		char *ptr;
		switch (index++) {
		case 0:
			grp->gr_name = str;
			break;
		case 1:
			grp->gr_passwd = str;
			break;
		case 2:
			errno = 0;
			grp->gr_gid = strtol(str, NULL, 10);
			if (errno)
				return -1;
			break;
		case 3:
			while ((str = strtok_r(str, ",", &ptr))) {
				if (count-- <= 0) {
					errno = ERANGE;
					return -1;
				}
				grp->gr_mem[gr_mem_cnt++] = str;
				str = NULL;
			}
		}
		line = NULL;
	}

	*result = grp;

	return 0;
}

static int test_getgr_match(struct group *grp, char *buf, size_t buflen,
			    struct group **result,
			    int (*match)(const struct group *, const void *),
			    const void *data)
{
	FILE *file;
	struct group *_result;

	*result = NULL;

	file = fopen(grfile, "r");
	if (!file) {
		errno = EBADF;
		return -1;
	}

	errno = 0;
	while (!test_getgrent_r(file, grp, buf, buflen, &_result)) {
		if (!_result)
			break;
		else if (match(grp, data)) {
			*result = grp;
			break;
		}
	}

	fclose(file);
	if (!errno && !*result)
		errno = ENOENT;
	if (errno)
		return -1;
	return 0;
}

static int match_name(const struct group *grp, const void *data)
{
	const char *name = data;
	return !strcmp(grp->gr_name, name);
}

EXPORT
int getgrnam_r(const char *name, struct group *grp, char *buf, size_t buflen,
	       struct group **result)
{
	static size_t last_buflen = -1;

	assert(last_buflen == -1 || buflen > last_buflen);
	if (buflen < 170000) {
		last_buflen = buflen;
		*result = NULL;
		return ERANGE;
	}
	last_buflen = -1;

	return test_getgr_match(grp, buf, buflen, result, match_name, name);
}

EXPORT
struct group *getgrnam(const char *name)
{
	static char buf[16384];
	static struct group grp;
	struct group *result;

	(void) getgrnam_r(name, &grp, buf, sizeof(buf), &result);
	return result;
}

static int match_gid(const struct group *grp, const void *data)
{
	gid_t gid = *(gid_t *)data;
	return grp->gr_gid == gid;
}

EXPORT
int getgrgid_r(gid_t gid, struct group *grp, char *buf, size_t buflen,
	       struct group **result)
{
	return test_getgr_match(grp, buf, buflen, result, match_gid, &gid);
}

EXPORT
struct group *getgrgid(gid_t gid)
{
	static char buf[16384];
	static struct group grp;
	struct group *result;

	(void) getgrgid_r(gid, &grp, buf, sizeof(buf), &result);
	return result;
}
