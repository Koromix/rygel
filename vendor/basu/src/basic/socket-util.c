/* SPDX-License-Identifier: LGPL-2.1+ */

#include "fd-util.h"
#include "process-util.h"
#include "socket-util.h"
#include "strv.h"

#ifdef __FreeBSD__
#include <sys/ucred.h>
#include <sys/un.h>
#endif

int fd_inc_sndbuf(int fd, size_t n) {
        int r, value;
        socklen_t l = sizeof(value);

        r = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &value, &l);
        if (r >= 0 && l == sizeof(value) && (size_t) value >= n*2)
                return 0;

        return 1;
}

int fd_inc_rcvbuf(int fd, size_t n) {
        int r, value;
        socklen_t l = sizeof(value);

        r = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &value, &l);
        if (r >= 0 && l == sizeof(value) && (size_t) value >= n*2)
                return 0;

        return 1;
}

int getpeercred(int fd, struct ucred *ucred) {
#ifdef __FreeBSD__
        struct xucred cred;
        socklen_t len = sizeof cred;
        if (getsockopt(fd, 0, LOCAL_PEERCRED, &cred, &len) == -1) {
                return -errno;
        }

        struct ucred u = {
#if __FreeBSD_version >= 1300030
                .pid = cred.cr_pid,
#else
                .pid = -1,
#endif
                .uid = cred.cr_uid,
                .gid = cred.cr_ngroups > 0 ? cred.cr_groups[0] : (gid_t)-1,
        };

        *ucred = u;
#else
        socklen_t n = sizeof(struct ucred);
        struct ucred u;
        int r;

        assert(fd >= 0);
        assert(ucred);

        r = getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &u, &n);
        if (r < 0)
                return -errno;

        if (n != sizeof(struct ucred))
                return -EIO;

        /* Check if the data is actually useful and not suppressed due to namespacing issues */
        if (!pid_is_valid(u.pid))
                return -ENODATA;

        /* Note that we don't check UID/GID here, as namespace translation works differently there: instead of
         * receiving in "invalid" user/group we get the overflow UID/GID. */

        *ucred = u;
#endif
        return 0;
}

int getpeersec(int fd, char **ret) {
#ifdef __FreeBSD__
        return -EOPNOTSUPP;
#else
        _cleanup_free_ char *s = NULL;
        socklen_t n = 64;

        assert(fd >= 0);
        assert(ret);

        for (;;) {
                s = new0(char, n+1);
                if (!s)
                        return -ENOMEM;

                if (getsockopt(fd, SOL_SOCKET, SO_PEERSEC, s, &n) >= 0)
                        break;

                if (errno != ERANGE)
                        return -errno;

                s = mfree(s);
        }

        if (isempty(s))
                return -EOPNOTSUPP;

        *ret = TAKE_PTR(s);

        return 0;
#endif
}

int getpeergroups(int fd, gid_t **ret) {
        socklen_t n = sizeof(gid_t) * 64;
        _cleanup_free_ gid_t *d = NULL;

        assert(fd >= 0);
        assert(ret);

        for (;;) {
                d = malloc(n);
                if (!d)
                        return -ENOMEM;

                if (getsockopt(fd, SOL_SOCKET, SO_PEERGROUPS, d, &n) >= 0)
                        break;

                if (errno != ERANGE)
                        return -errno;

                d = mfree(d);
        }

        assert_se(n % sizeof(gid_t) == 0);
        n /= sizeof(gid_t);

        if ((socklen_t) (int) n != n)
                return -E2BIG;

        *ret = TAKE_PTR(d);

        return (int) n;
}
