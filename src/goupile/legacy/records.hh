// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "src/core/http/http.hh"

namespace K {

class InstanceHolder;

void HandleLegacyLoad(http_IO *io, InstanceHolder *instance);
void HandleLegacySave(http_IO *io, InstanceHolder *instance);
void HandleLegacyExport(http_IO *io, InstanceHolder *instance);

}
