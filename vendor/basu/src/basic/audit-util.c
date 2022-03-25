/* SPDX-License-Identifier: LGPL-2.1+ */

#include "alloc-util.h"
#include "audit-util.h"
#include "fileio.h"
#include "parse-util.h"
#include "process-util.h"
#include "user-util.h"
#include "missing.h"

int audit_session_from_pid(pid_t pid, uint32_t *id) {
        _cleanup_free_ char *s = NULL;
        const char *p;
        uint32_t u;
        int r;

        assert(id);

        /* We don't convert ENOENT to ESRCH here, since we can't
         * really distuingish between "audit is not available in the
         * kernel" and "the process does not exist", both which will
         * result in ENOENT. */

        p = procfs_file_alloca(pid, "sessionid");

        r = read_one_line_file(p, &s);
        if (r < 0)
                return r;

        r = safe_atou32(s, &u);
        if (r < 0)
                return r;

        if (!audit_session_is_valid(u))
                return -ENODATA;

        *id = u;
        return 0;
}

int audit_loginuid_from_pid(pid_t pid, uid_t *uid) {
        _cleanup_free_ char *s = NULL;
        const char *p;
        uid_t u;
        int r;

        assert(uid);

        p = procfs_file_alloca(pid, "loginuid");

        r = read_one_line_file(p, &s);
        if (r < 0)
                return r;

        r = parse_uid(s, &u);
        if (r == -ENXIO) /* the UID was -1 */
                return -ENODATA;
        if (r < 0)
                return r;

        *uid = u;
        return 0;
}
