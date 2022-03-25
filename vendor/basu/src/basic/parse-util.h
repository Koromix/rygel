/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "macro.h"

int parse_boolean(const char *v) _pure_;
int parse_pid(const char *s, pid_t* ret_pid);

int safe_atou_full(const char *s, unsigned base, unsigned *ret_u);

static inline int safe_atou(const char *s, unsigned *ret_u) {
        return safe_atou_full(s, 0, ret_u);
}

int safe_atoi(const char *s, int *ret_i);
int safe_atollu(const char *s, unsigned long long *ret_u);
int safe_atolli(const char *s, long long int *ret_i);

int safe_atou8(const char *s, uint8_t *ret);

int safe_atou16_full(const char *s, unsigned base, uint16_t *ret);

static inline int safe_atou16(const char *s, uint16_t *ret) {
        return safe_atou16_full(s, 0, ret);
}

int safe_atoi16(const char *s, int16_t *ret);

static inline int safe_atou32(const char *s, uint32_t *ret_u) {
        assert_cc(sizeof(uint32_t) == sizeof(unsigned));
        return safe_atou(s, (unsigned*) ret_u);
}

static inline int safe_atoi32(const char *s, int32_t *ret_i) {
        assert_cc(sizeof(int32_t) == sizeof(int));
        return safe_atoi(s, (int*) ret_i);
}

static inline int safe_atou64(const char *s, uint64_t *ret_u) {
        assert_cc(sizeof(uint64_t) == sizeof(unsigned long long));
        return safe_atollu(s, (unsigned long long*) ret_u);
}

static inline int safe_atoi64(const char *s, int64_t *ret_i) {
        assert_cc(sizeof(int64_t) == sizeof(long long int));
        return safe_atolli(s, (long long int*) ret_i);
}

#if LONG_MAX == INT_MAX
static inline int safe_atolu(const char *s, unsigned long *ret_u) {
        assert_cc(sizeof(unsigned long) == sizeof(unsigned));
        return safe_atou(s, (unsigned*) ret_u);
}
#else
static inline int safe_atolu(const char *s, unsigned long *ret_u) {
        assert_cc(sizeof(unsigned long) == sizeof(unsigned long long));
        return safe_atollu(s, (unsigned long long*) ret_u);
}
#endif

int safe_atod(const char *s, double *ret_d);
