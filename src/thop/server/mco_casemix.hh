// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "user.hh"
#include "src/core/http/http.hh"

namespace K {

void ProduceMcoAggregate(http_IO *io, const User *user);
void ProduceMcoResults(http_IO *io, const User *user);

}
