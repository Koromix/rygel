/* SPDX-License-Identifier: LGPL-2.1+ */

#include <string.h>

#include "alloc-util.h"
#include "cap-list.h"
#include "missing.h"

#if HAVE_LIBCAP
#include <linux/capability.h>
#endif

#include "cap-to-name.h"

const char *capability_to_name(int id) {

        if (id < 0)
                return NULL;

        if (id >= (int) ELEMENTSOF(capability_names))
                return NULL;

        return capability_names[id];
}
