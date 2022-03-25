/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "sd-bus.h"

enum {
        BUS_MESSAGE_DUMP_WITH_HEADER = 1,
        BUS_MESSAGE_DUMP_SUBTREE_ONLY = 2,
};

int bus_message_dump(sd_bus_message *m, FILE *f, unsigned flags);

int bus_creds_dump(sd_bus_creds *c, FILE *f, bool terse);
