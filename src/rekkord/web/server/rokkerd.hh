// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "lib/native/kid/kid.hh"
#include "lib/native/sqlite/sqlite.hh"
#include "config.hh"

namespace K {

extern Config config;
extern sq_Database db;

enum class KIDType {
    Drop = 0
};
static const char *const KIDTypeNames[] = {
    "Drop"
};

static inline void FillKID(KIDType type, KID *out_kid) { return FillKID((int8_t)type, out_kid); }
static inline bool ParseKID(Span<const char> str, KIDType type, KID *out_kid) { return ParseKID(str, (int8_t)type, out_kid); }

}
