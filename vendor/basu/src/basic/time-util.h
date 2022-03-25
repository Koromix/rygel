/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <time.h>

#include "macro.h"

typedef uint64_t usec_t;
typedef uint64_t nsec_t;

#define PRI_USEC PRIu64
#define USEC_FMT "%" PRI_USEC

#define USEC_INFINITY ((usec_t) -1)

#define USEC_PER_SEC  ((usec_t) 1000000ULL)
#define USEC_PER_MSEC ((usec_t) 1000ULL)
#define NSEC_PER_USEC ((nsec_t) 1000ULL)

#define USEC_PER_MINUTE ((usec_t) (60ULL*USEC_PER_SEC))
#define USEC_PER_HOUR ((usec_t) (60ULL*USEC_PER_MINUTE))
#define USEC_PER_DAY ((usec_t) (24ULL*USEC_PER_HOUR))
#define USEC_PER_WEEK ((usec_t) (7ULL*USEC_PER_DAY))
#define USEC_PER_MONTH ((usec_t) (2629800ULL*USEC_PER_SEC))
#define USEC_PER_YEAR ((usec_t) (31557600ULL*USEC_PER_SEC))

#define TIME_T_MAX (time_t)((UINTMAX_C(1) << ((sizeof(time_t) << 3) - 1)) - 1)

usec_t now(clockid_t clock);

usec_t timespec_load(const struct timespec *ts) _pure_;
struct timespec *timespec_store(struct timespec *ts, usec_t u);

int parse_sec(const char *t, usec_t *usec);
int parse_time(const char *t, usec_t *usec, usec_t default_unit);

static inline usec_t usec_add(usec_t a, usec_t b) {
        usec_t c;

        /* Adds two time values, and makes sure USEC_INFINITY as input results as USEC_INFINITY in output, and doesn't
         * overflow. */

        c = a + b;
        if (c < a || c < b) /* overflow check */
                return USEC_INFINITY;

        return c;
}

static inline usec_t usec_sub_unsigned(usec_t timestamp, usec_t delta) {

        if (timestamp == USEC_INFINITY) /* Make sure infinity doesn't degrade */
                return USEC_INFINITY;
        if (timestamp < delta)
                return 0;

        return timestamp - delta;
}
