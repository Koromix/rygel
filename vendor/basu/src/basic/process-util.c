/* SPDX-License-Identifier: LGPL-2.1+ */

#include <ctype.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

#include "alloc-util.h"
#include "def.h"
#include "escape.h"
#include "fd-util.h"
#include "fileio.h"
#include "fs-util.h"
#include "process-util.h"
#include "string-util.h"
#include "util.h"

int get_process_state(pid_t pid) {
        const char *p;
        char state;
        int r;
        _cleanup_free_ char *line = NULL;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "stat");

        r = read_one_line_file(p, &line);
        if (r == -ENOENT)
                return -ESRCH;
        if (r < 0)
                return r;

        p = strrchr(line, ')');
        if (!p)
                return -EIO;

        p++;

        if (sscanf(p, " %c", &state) != 1)
                return -EIO;

        return (unsigned char) state;
}

int get_process_comm(pid_t pid, char **ret) {
        _cleanup_free_ char *escaped = NULL, *comm = NULL;
        const char *p;
        int r;

        assert(ret);
        assert(pid >= 0);

        escaped = new(char, TASK_COMM_LEN);
        if (!escaped)
                return -ENOMEM;

        p = procfs_file_alloca(pid, "comm");

        r = read_one_line_file(p, &comm);
        if (r == -ENOENT)
                return -ESRCH;
        if (r < 0)
                return r;

        /* Escape unprintable characters, just in case, but don't grow the string beyond the underlying size */
        cellescape(escaped, TASK_COMM_LEN, comm);

        *ret = TAKE_PTR(escaped);
        return 0;
}

static int get_process_link_contents(const char *proc_file, char **name) {
        int r;

        assert(proc_file);
        assert(name);

        r = readlink_malloc(proc_file, name);
        if (r == -ENOENT)
                return -ESRCH;
        if (r < 0)
                return r;

        return 0;
}

int get_process_exe(pid_t pid, char **name) {
        const char *p;
        char *d;
        int r;

        assert(pid >= 0);

        p = procfs_file_alloca(pid, "exe");
        r = get_process_link_contents(p, name);
        if (r < 0)
                return r;

        d = endswith(*name, " (deleted)");
        if (d)
                *d = '\0';

        return 0;
}

bool pid_is_unwaited(pid_t pid) {
        /* Checks whether a PID is still valid at all, including a zombie */

        if (pid < 0)
                return false;

        if (pid <= 1) /* If we or PID 1 would be dead and have been waited for, this code would not be running */
                return true;

        if (pid == getpid_cached())
                return true;

        if (kill(pid, 0) >= 0)
                return true;

        return errno != ESRCH;
}

bool pid_is_alive(pid_t pid) {
        int r;

        /* Checks whether a PID is still valid and not a zombie */

        if (pid < 0)
                return false;

        if (pid <= 1) /* If we or PID 1 would be a zombie, this code would not be running */
                return true;

        if (pid == getpid_cached())
                return true;

        r = get_process_state(pid);
        if (IN_SET(r, -ESRCH, 'Z'))
                return false;

        return true;
}

pid_t getpid_cached(void) {
        return getpid();
}

int must_be_root(void) {

        if (geteuid() == 0)
                return 0;

        log_error("Need to be root.");
        return -EPERM;
}

