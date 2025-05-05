#include "config.h"
#include <assert.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <pwd.h>

#define TEST_PASSWD "test/test.passwd"
static char pwfile[] = BASEDIR "/" TEST_PASSWD;

#define ALIGN_MASK(x, mask)    (((x) + (mask)) & ~(mask))
#define ALIGN(x, a)            ALIGN_MASK(x, (typeof(x))(a) - 1)

static int test_getpwent_r(FILE *file, struct passwd *pwd, char *buf,
			   size_t buflen, struct passwd **result)
{
	char *str, *line;
	int index = 0;

	*result = NULL;

	line = fgets(buf, buflen, file);
	if (!line) {
		return 0;
	}

	while ((str = strtok(line, ":"))) {
		switch (index++) {
		case 0:
			pwd->pw_name = str;
			break;
		case 1:
			pwd->pw_passwd = str;
			break;
		case 2:
			errno = 0;
			pwd->pw_uid = strtol(str, NULL, 10);
			if (errno)
				return -1;
			break;
		case 3:
			errno = 0;
			pwd->pw_gid = strtol(str, NULL, 10);
			if (errno)
				return -1;
			break;
		case 4:
			pwd->pw_gecos = str;
			break;
		case 5:
			pwd->pw_dir = str;
			break;
		case 6:
			pwd->pw_shell = str;
			break;
		}
		line = NULL;
	}

	*result = pwd;

	return 0;
}

static int test_getpw_match(struct passwd *pwd, char *buf, size_t buflen,
			    struct passwd **result,
			    int (*match)(const struct passwd *, const void *),
			    const void *data)
{
	FILE *file;
	struct passwd *_result;

	*result = NULL;

	file = fopen(pwfile, "r");
	if (!file) {
		fprintf(stderr, "Failed to open %s\n", pwfile);
		errno = EBADF;
		return -1;
	}

	errno = 0;
	while (!test_getpwent_r(file, pwd, buf, buflen, &_result)) {
		if (!_result)
			break;
		else if (match(pwd, data)) {
			*result = pwd;
			break;
		}
	}

	fclose(file);
	if (errno)
		return -1;
	return 0;
}

static int match_name(const struct passwd *pwd, const void *data)
{
	const char *name = data;
	return !strcmp(pwd->pw_name, name);
}

EXPORT
int getpwnam_r(const char *name, struct passwd *pwd, char *buf, size_t buflen,
	       struct passwd **result)
{
	static size_t last_buflen = -1;

	assert(last_buflen == -1 || buflen > last_buflen);
	if (buflen < 170000) {
		last_buflen = buflen;
		*result = NULL;
		return ERANGE;
	}
	last_buflen =- 1;

	return test_getpw_match(pwd, buf, buflen, result, match_name, name);
}

EXPORT
struct passwd *getpwnam(const char *name)
{
	static char buf[16384];
	static struct passwd pwd;
	struct passwd *result;

	(void) getpwnam_r(name, &pwd, buf, sizeof(buf), &result);
	return result;
}

static int match_uid(const struct passwd *pwd, const void *data)
{
	uid_t uid = *(uid_t *)data;
	return pwd->pw_uid == uid;
}

EXPORT
int getpwuid_r(uid_t uid, struct passwd *pwd, char *buf, size_t buflen,
	       struct passwd **result)
{
	return test_getpw_match(pwd, buf, buflen, result, match_uid, &uid);
}

EXPORT
struct passwd *getpwuid(uid_t uid)
{
	static char buf[16384];
	static struct passwd pwd;
	struct passwd *result;

	(void) getpwuid_r(uid, &pwd, buf, sizeof(buf), &result);
	return result;
}
