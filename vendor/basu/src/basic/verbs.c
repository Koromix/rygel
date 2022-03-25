/* SPDX-License-Identifier: LGPL-2.1+ */

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <string.h>

#include "log.h"
#include "process-util.h"
#include "string-util.h"
#include "verbs.h"

int dispatch_verb(int argc, char *argv[], const Verb verbs[], void *userdata) {
        const Verb *verb;
        const char *name;
        unsigned i;
        int left, r;

        assert(verbs);
        assert(verbs[0].dispatch);
        assert(argc >= 0);
        assert(argv);
        assert(argc >= optind);

        left = argc - optind;
        argv += optind;
        optind = 0;
        name = argv[0];

        for (i = 0;; i++) {
                bool found;

                /* At the end of the list? */
                if (!verbs[i].dispatch) {
                        if (name)
                                log_error("Unknown operation %s.", name);
                        else
                                log_error("Requires operation parameter.");
                        return -EINVAL;
                }

                if (name)
                        found = streq(name, verbs[i].verb);
                else
                        found = verbs[i].flags & VERB_DEFAULT;

                if (found) {
                        verb = &verbs[i];
                        break;
                }
        }

        assert(verb);

        if (!name)
                left = 1;

        if (verb->min_args != VERB_ANY &&
            (unsigned) left < verb->min_args) {
                log_error("Too few arguments.");
                return -EINVAL;
        }

        if (verb->max_args != VERB_ANY &&
            (unsigned) left > verb->max_args) {
                log_error("Too many arguments.");
                return -EINVAL;
        }

        if (verb->flags & VERB_MUST_BE_ROOT) {
                r = must_be_root();
                if (r < 0)
                        return r;
        }

        if (name)
                return verb->dispatch(left, argv, userdata);
        else {
                char* fake[2] = {
                        (char*) verb->verb,
                        NULL
                };

                return verb->dispatch(1, fake, userdata);
        }
}
