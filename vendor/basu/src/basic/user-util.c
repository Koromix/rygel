/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "alloc-util.h"
#include "fileio.h"
#include "format-util.h"
#include "parse-util.h"
#include "string-util.h"
#include "user-util.h"

bool uid_is_valid(uid_t uid) {

        /* Also see POSIX IEEE Std 1003.1-2008, 2016 Edition, 3.436. */

        /* Some libc APIs use UID_INVALID as special placeholder */
        if (uid == (uid_t) UINT32_C(0xFFFFFFFF))
                return false;

        /* A long time ago UIDs where 16bit, hence explicitly avoid the 16bit -1 too */
        if (uid == (uid_t) UINT32_C(0xFFFF))
                return false;

        return true;
}

int parse_uid(const char *s, uid_t *ret) {
        uint32_t uid = 0;
        int r;

        assert(s);

        assert_cc(sizeof(uid_t) == sizeof(uint32_t));
        r = safe_atou32(s, &uid);
        if (r < 0)
                return r;

        if (!uid_is_valid(uid))
                return -ENXIO; /* we return ENXIO instead of EINVAL
                                * here, to make it easy to distuingish
                                * invalid numeric uids from invalid
                                * strings. */

        if (ret)
                *ret = uid;

        return 0;
}

char* uid_to_name(uid_t uid) {
        char *ret;
        int r;

        /* Shortcut things to avoid NSS lookups */
        if (uid == 0)
                return strdup("root");
        if (synthesize_nobody() &&
            uid == UID_NOBODY)
                return strdup(NOBODY_USER_NAME);

        if (uid_is_valid(uid)) {
                long bufsize;

                bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
                if (bufsize <= 0)
                        bufsize = 4096;

                for (;;) {
                        struct passwd pwbuf, *pw = NULL;
                        _cleanup_free_ char *buf = NULL;

                        buf = malloc(bufsize);
                        if (!buf)
                                return NULL;

                        r = getpwuid_r(uid, &pwbuf, buf, (size_t) bufsize, &pw);
                        if (r == 0 && pw)
                                return strdup(pw->pw_name);
                        if (r != ERANGE)
                                break;

                        bufsize *= 2;
                }
        }

        if (asprintf(&ret, UID_FMT, uid) < 0)
                return NULL;

        return ret;
}

bool synthesize_nobody(void) {

#ifdef NOLEGACY
        return true;
#else
        /* Returns true when we shall synthesize the "nobody" user (which we do by default). This can be turned off by
         * touching /etc/systemd/dont-synthesize-nobody in order to provide upgrade compatibility with legacy systems
         * that used the "nobody" user name and group name for other UIDs/GIDs than 65534.
         *
         * Note that we do not employ any kind of synchronization on the following caching variable. If the variable is
         * accessed in multi-threaded programs in the worst case it might happen that we initialize twice, but that
         * shouldn't matter as each initialization should come to the same result. */
        static int cache = -1;

        if (cache < 0)
                cache = access("/etc/systemd/dont-synthesize-nobody", F_OK) < 0;

        return cache;
#endif
}

