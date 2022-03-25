/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#ifdef __linux__
#include <alloca.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "format-util.h"

#define procfs_file_alloca(pid, field)                                  \
        ({                                                              \
                pid_t _pid_ = (pid);                                    \
                const char *_r_;                                        \
                if (_pid_ == 0) {                                       \
                        _r_ = ("/proc/self/" field);                    \
                } else {                                                \
                        _r_ = alloca(STRLEN("/proc/") + DECIMAL_STR_MAX(pid_t) + 1 + sizeof(field)); \
                        sprintf((char*) _r_, "/proc/"PID_FMT"/" field, _pid_);                       \
                }                                                       \
                _r_;                                                    \
        })

int get_process_state(pid_t pid);
int get_process_comm(pid_t pid, char **name);
int get_process_exe(pid_t pid, char **name);

bool pid_is_alive(pid_t pid);
bool pid_is_unwaited(pid_t pid);

static inline bool pid_is_valid(pid_t p) {
        return p > 0;
}

pid_t getpid_cached(void);

int must_be_root(void);
