/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <netinet/in.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/socket.h>

#include "macro.h"

union sockaddr_union {
        /* The minimal, abstract version */
        struct sockaddr sa;

        /* The libc provided version that allocates "enough room" for every protocol */
        struct sockaddr_storage storage;

        /* Protoctol-specific implementations */
        struct sockaddr_in in;
        struct sockaddr_in6 in6;
        struct sockaddr_un un;
};

#ifdef __FreeBSD__
struct ucred {
        uint32_t pid;
        uint32_t uid;
        uint32_t gid;
};
#endif

int fd_inc_sndbuf(int fd, size_t n);
int fd_inc_rcvbuf(int fd, size_t n);

int getpeercred(int fd, struct ucred *ucred);
int getpeersec(int fd, char **ret);
int getpeergroups(int fd, gid_t **ret);

#define CMSG_FOREACH(cmsg, mh)                                          \
        for ((cmsg) = CMSG_FIRSTHDR(mh); (cmsg); (cmsg) = CMSG_NXTHDR((mh), (cmsg)))

static inline int setsockopt_int(int fd, int level, int optname, int value) {
        if (setsockopt(fd, level, optname, &value, sizeof(value)) < 0)
                return -errno;

        return 0;
}
