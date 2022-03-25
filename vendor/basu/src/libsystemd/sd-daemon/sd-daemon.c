/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <mqueue.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "sd-daemon.h"

#include "macro.h"
#include "socket-util.h"

static int sd_is_socket_internal(int fd, int type, int listening) {
        struct stat st_fd;

        assert_return(fd >= 0, -EBADF);
        assert_return(type >= 0, -EINVAL);

        if (fstat(fd, &st_fd) < 0)
                return -errno;

        if (!S_ISSOCK(st_fd.st_mode))
                return 0;

        if (type != 0) {
                int other_type = 0;
                socklen_t l = sizeof(other_type);

                if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &other_type, &l) < 0)
                        return -errno;

                if (l != sizeof(other_type))
                        return -EINVAL;

                if (other_type != type)
                        return 0;
        }

        if (listening >= 0) {
                int accepting = 0;
                socklen_t l = sizeof(accepting);

                if (getsockopt(fd, SOL_SOCKET, SO_ACCEPTCONN, &accepting, &l) < 0)
                        return -errno;

                if (l != sizeof(accepting))
                        return -EINVAL;

                if (!accepting != !listening)
                        return 0;
        }

        return 1;
}

_public_ int sd_is_socket(int fd, int family, int type, int listening) {
        int r;

        assert_return(fd >= 0, -EBADF);
        assert_return(family >= 0, -EINVAL);

        r = sd_is_socket_internal(fd, type, listening);
        if (r <= 0)
                return r;

        if (family > 0) {
                union sockaddr_union sockaddr = {};
                socklen_t l = sizeof(sockaddr);

                if (getsockname(fd, &sockaddr.sa, &l) < 0)
                        return -errno;

                if (l < sizeof(sa_family_t))
                        return -EINVAL;

                return sockaddr.sa.sa_family == family;
        }

        return 1;
}

