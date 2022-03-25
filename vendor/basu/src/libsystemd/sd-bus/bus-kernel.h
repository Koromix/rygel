/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include "sd-bus.h"

#define MEMFD_CACHE_MAX 32

/* This determines at which minimum size we prefer sending memfds over
 * sending vectors */
#define MEMFD_MIN_SIZE (512*1024)

struct memfd_cache {
        int fd;
        void *address;
        size_t mapped;
        size_t allocated;
};

void close_and_munmap(int fd, void *address, size_t size);
void bus_flush_memfd(sd_bus *bus);
