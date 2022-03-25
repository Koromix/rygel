/* SPDX-License-Identifier: LGPL-2.1+ */

#include <string.h>
#include <syslog.h>

#include "macro.h"
#include "string-table.h"
#include "syslog-util.h"

static const char *const log_level_table[] = {
        [LOG_EMERG] = "emerg",
        [LOG_ALERT] = "alert",
        [LOG_CRIT] = "crit",
        [LOG_ERR] = "err",
        [LOG_WARNING] = "warning",
        [LOG_NOTICE] = "notice",
        [LOG_INFO] = "info",
        [LOG_DEBUG] = "debug"
};

DEFINE_STRING_TABLE_LOOKUP_WITH_FALLBACK(log_level, int, LOG_DEBUG);
