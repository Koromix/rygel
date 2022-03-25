/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

bool uid_is_valid(uid_t uid);

static inline bool gid_is_valid(gid_t gid) {
        return uid_is_valid((uid_t) gid);
}

int parse_uid(const char *s, uid_t* ret_uid);

char* uid_to_name(uid_t uid);

#define UID_INVALID ((uid_t) -1)

#define UID_NOBODY ((uid_t) 65534U)

bool synthesize_nobody(void);

