#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

void destructor(void) __attribute__((destructor));

struct file {
    char *name;
    uid_t uid;
    gid_t gid;
} file = {0};

void
destructor(void)
{
    free(file.name);
}

typedef int (*__libc_chown)(const char *pathname, uid_t owner, gid_t group);

typedef int (*__libc_fchownat)(int dirfd,
                               const char *pathname,
                               uid_t owner,
                               gid_t group,
                               int flags);

typedef int (*__libc_stat)(const char *pathname, struct stat *statbuf);

typedef int (*__libc_xstat)(int ver,
                            const char *pathname,
                            struct stat *statbuf);

typedef int (*__libc_lxstat)(int ver,
                             const char *pathname,
                             struct stat *statbuf);

typedef int (*__libc_lstat)(const char *pathname, struct stat *statbuf);

static int
chown_helper(const char *pathname, uid_t owner, gid_t group)
{
    if (strlen(pathname) > 7 && strncmp(pathname, "/dev/pt", 7) == 0) {
        /*
         * The OpenSSH server modified the PTY which requires root permissions
         * see torture_request_pty_modes
         * */
        return 0;
    }
    if (strlen(pathname) > 4 && strncmp(pathname, "/tmp", 4) == 0) {
        /*
         * faking chown because It requires root permissions to modify the owner
         * under /tmp
         * It's also a helper for torture_sftp_setstat
         * */
        if (file.name != NULL) {
            free((char *)file.name);
        }
        file.name = strdup(pathname);
        file.uid = owner;
        file.gid = group;
        return 0;
    }
    return -1;
}

static void
stat_helper(const char *pathname, struct stat *statbuf)
{
    if (file.name != NULL && strcmp(pathname, file.name) == 0) {
        statbuf->st_uid = file.uid;
        statbuf->st_gid = file.gid;
    }
}

/* silent gcc */
int chown(const char *pathname, uid_t owner, gid_t group);

int
chown(const char *pathname, uid_t owner, gid_t group)
{
    __libc_chown original_chown = NULL;
    int rc;

    rc = chown_helper(pathname, owner, group);
    if (rc == 0) {
        return 0;
    }
    original_chown = (__libc_chown)dlsym(RTLD_NEXT, "chown");
    return (*original_chown)(pathname, owner, group);
}

/* SFTP Server calls fchownat for symlinks  */
int
fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags);

int
fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags)
{
    __libc_fchownat original_fchownat = NULL;
    int rc;

    rc = chown_helper(pathname, owner, group);
    if (rc == 0) {
        return 0;
    }

    original_fchownat = (__libc_fchownat)dlsym(RTLD_NEXT, "fchownat");
    return (*original_fchownat)(dirfd, pathname, owner, group, flags);
}
int stat(const char *pathname, struct stat *statbuf);

int
stat(const char *pathname, struct stat *statbuf)
{
    int rc;
    __libc_stat original_stat = NULL;

    original_stat = (__libc_stat)dlsym(RTLD_NEXT, "stat");
    rc = (*original_stat)(pathname, statbuf);
    stat_helper(pathname, statbuf);

    return rc;
}

/* CentOS8 calls xstat */
int __xstat(int ver, const char *pathname, struct stat *statbuf);

int
__xstat(int ver, const char *pathname, struct stat *statbuf)
{
    int rc;
    __libc_xstat original_xstat = NULL;

    original_xstat = (__libc_xstat)dlsym(RTLD_NEXT, "__xstat");
    rc = (*original_xstat)(ver, pathname, statbuf);
    stat_helper(pathname, statbuf);

    return rc;
}

int __lxstat(int ver, const char *pathname, struct stat *statbuf);

int
__lxstat(int ver, const char *pathname, struct stat *statbuf)
{
    int rc;
    __libc_lxstat original_lxstat = NULL;

    original_lxstat = (__libc_lxstat)dlsym(RTLD_NEXT, "__lxstat");
    rc = (*original_lxstat)(ver, pathname, statbuf);
    stat_helper(pathname, statbuf);

    return rc;
}
int lstat(const char *pathname, struct stat *statbuf);

int
lstat(const char *pathname, struct stat *statbuf)
{
    int rc;
    __libc_lstat original_lstat = NULL;

    original_lstat = (__libc_lstat)dlsym(RTLD_NEXT, "lstat");
    rc = (*original_lstat)(pathname, statbuf);
    stat_helper(pathname, statbuf);

    return rc;
}
