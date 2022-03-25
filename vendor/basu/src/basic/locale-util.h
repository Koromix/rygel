/* SPDX-License-Identifier: LGPL-2.1+ */
#pragma once

#include <stdbool.h>
#include <locale.h>

#include "macro.h"

bool is_locale_utf8(void);

typedef enum {
        TREE_VERTICAL,
        TREE_BRANCH,
        TREE_RIGHT,
        TREE_SPACE,
        TRIANGULAR_BULLET,
        BLACK_CIRCLE,
        ARROW,
        MDASH,
        ELLIPSIS,
        MU,
        _SPECIAL_GLYPH_MAX
} SpecialGlyph;

const char *special_glyph(SpecialGlyph code) _const_;

static inline void freelocalep(locale_t *p) {
        if (*p == (locale_t) 0)
                return;

        freelocale(*p);
}
