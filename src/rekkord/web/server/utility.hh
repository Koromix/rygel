// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"

namespace K {

bool IsMailValid(Span<const char> str);

bool IsEnumValid(Span<const char> str, Span<const char *const> values);
bool IsStringValid(Span<const char> str);

}
