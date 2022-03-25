/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stddef.h>

#include "macro.h"

char octchar(int x) _const_;

int undecchar(char c) _const_;

char hexchar(int x) _const_;
int unhexchar(char c) _const_;

char *hexmem(const void *p, size_t l);
int unhexmem(const char *p, size_t l, void **mem, size_t *len);
