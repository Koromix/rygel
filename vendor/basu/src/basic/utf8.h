/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <uchar.h>

#include "macro.h"

bool unichar_is_valid(char32_t c);

char *utf8_is_valid(const char *s) _pure_;
char *ascii_is_valid_n(const char *str, size_t len);

int utf8_encoded_valid_unichar(const char *str);
int utf8_encoded_to_unichar(const char *str, char32_t *ret_unichar);
