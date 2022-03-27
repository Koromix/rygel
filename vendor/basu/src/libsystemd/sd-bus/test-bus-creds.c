/* SPDX-License-Identifier: LGPL-2.1+ */

#include "sd-bus.h"

#include "bus-dump.h"
#include "tests.h"
#include "errno.h"
#include "log.h"

int main(int argc, char *argv[]) {
        _cleanup_(sd_bus_creds_unrefp) sd_bus_creds *creds = NULL;
        int r;

        test_setup_logging(LOG_DEBUG);

        r = sd_bus_creds_new_from_pid(&creds, 0, _SD_BUS_CREDS_ALL);
        log_full_errno(r < 0 ? LOG_ERR : LOG_DEBUG, r, "sd_bus_creds_new_from_pid: %m");
        assert_se(r >= 0);

        bus_creds_dump(creds, NULL, true);

        creds = sd_bus_creds_unref(creds);

        r = sd_bus_creds_new_from_pid(&creds, 1, _SD_BUS_CREDS_ALL);
        if (r != -EACCES) {
                assert_se(r >= 0);
                putchar('\n');
                bus_creds_dump(creds, NULL, true);
        }

        return 0;
}
