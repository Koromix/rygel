/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdint.h>

int memfd_set_sealed(int fd);

int memfd_get_size(int fd, uint64_t *sz);
int memfd_set_size(int fd, uint64_t sz);
