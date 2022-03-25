/* SPDX-License-Identifier: LGPL-2.1+ */

#include <langinfo.h>
#include <locale.h>
#include "locale-util.h"
#include "string-util.h"
#include "strv.h"

bool is_locale_utf8(void) {
        const char *set;
        static int cached_answer = -1;

        /* Note that we default to 'true' here, since today UTF8 is
         * pretty much supported everywhere. */

        if (cached_answer >= 0)
                goto out;

        if (!setlocale(LC_ALL, "")) {
                cached_answer = true;
                goto out;
        }

        set = nl_langinfo(CODESET);
        if (!set) {
                cached_answer = true;
                goto out;
        }

        if (streq(set, "UTF-8")) {
                cached_answer = true;
                goto out;
        }

        /* For LC_CTYPE=="C" return true, because CTYPE is effectly
         * unset and everything can do to UTF-8 nowadays. */
        set = setlocale(LC_CTYPE, NULL);
        if (!set) {
                cached_answer = true;
                goto out;
        }

        /* Check result, but ignore the result if C was set
         * explicitly. */
        cached_answer =
                STR_IN_SET(set, "C", "POSIX") &&
                !getenv("LC_ALL") &&
                !getenv("LC_CTYPE") &&
                !getenv("LANG");

out:
        return (bool) cached_answer;
}

const char *special_glyph(SpecialGlyph code) {

        /* A list of a number of interesting unicode glyphs we can use to decorate our output. It's probably wise to be
         * conservative here, and primarily stick to the glyphs defined in the eurlatgr font, so that display still
         * works reasonably well on the Linux console. For details see:
         *
         * http://git.altlinux.org/people/legion/packages/kbd.git?p=kbd.git;a=blob;f=data/consolefonts/README.eurlatgr
         */

        static const char* const draw_table[2][_SPECIAL_GLYPH_MAX] = {
                /* ASCII fallback */
                [false] = {
                        [TREE_VERTICAL]      = "| ",
                        [TREE_BRANCH]        = "|-",
                        [TREE_RIGHT]         = "`-",
                        [TREE_SPACE]         = "  ",
                        [TRIANGULAR_BULLET]  = ">",
                        [BLACK_CIRCLE]       = "*",
                        [ARROW]              = "->",
                        [MDASH]              = "-",
                        [ELLIPSIS]           = "...",
                        [MU]                 = "u",
                },

                /* UTF-8 */
                [true] = {
                        [TREE_VERTICAL]      = "\342\224\202 ",            /* │  */
                        [TREE_BRANCH]        = "\342\224\234\342\224\200", /* ├─ */
                        [TREE_RIGHT]         = "\342\224\224\342\224\200", /* └─ */
                        [TREE_SPACE]         = "  ",                       /*    */
                        [TRIANGULAR_BULLET]  = "\342\200\243",             /* ‣ */
                        [BLACK_CIRCLE]       = "\342\227\217",             /* ● */
                        [ARROW]              = "\342\206\222",             /* → */
                        [MDASH]              = "\342\200\223",             /* – */
                        [ELLIPSIS]           = "\342\200\246",             /* … */
                        [MU]                 = "\316\274",                 /* μ */
                },
        };

        return draw_table[is_locale_utf8()][code];
}

