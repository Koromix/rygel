/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

int readlinkat_malloc(int fd, const char *p, char **ret);
int readlink_malloc(const char *p, char **r);

