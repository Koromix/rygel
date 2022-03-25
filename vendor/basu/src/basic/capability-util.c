/* SPDX-License-Identifier: LGPL-2.1+ */

#include <sys/capability.h>
#include <sys/prctl.h>

#include "alloc-util.h"
#include "capability-util.h"
#include "fileio.h"
#include "parse-util.h"

unsigned long cap_last_cap(void) {
        static thread_local unsigned long saved;
        static thread_local bool valid = false;
        _cleanup_free_ char *content = NULL;
        unsigned long p = 0;
        int r;

        if (valid)
                return saved;

        /* available since linux-3.2 */
        r = read_one_line_file("/proc/sys/kernel/cap_last_cap", &content);
        if (r >= 0) {
                r = safe_atolu(content, &p);
                if (r >= 0) {
                        saved = p;
                        valid = true;
                        return p;
                }
        }

        /* fall back to syscall-probing for pre linux-3.2 */
        p = (unsigned long) CAP_LAST_CAP;

        if (prctl(PR_CAPBSET_READ, p) < 0) {

                /* Hmm, look downwards, until we find one that
                 * works */
                for (p--; p > 0; p --)
                        if (prctl(PR_CAPBSET_READ, p) >= 0)
                                break;

        } else {

                /* Hmm, look upwards, until we find one that doesn't
                 * work */
                for (;; p++)
                        if (prctl(PR_CAPBSET_READ, p+1) < 0)
                                break;
        }

        saved = p;
        valid = true;

        return p;
}
