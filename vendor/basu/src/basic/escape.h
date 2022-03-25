/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stddef.h>

char *cescape(const char *s);
char *cescape_length(const char *s, size_t n);
int cescape_char(char c, char *buf);
