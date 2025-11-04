// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "src/core/native/http/http.hh"

namespace K {

bool IsProjectNameSafe(const char *name);

void HandleViews(http_IO *io);
void HandleEntities(http_IO *io);

void HandleMark(http_IO *io);

}
