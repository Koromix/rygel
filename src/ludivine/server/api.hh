// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "src/core/native/http/http.hh"

namespace K {

void HandleRegister(http_IO *io);
void HandleToken(http_IO *io);
void HandleProtect(http_IO *io);
void HandlePassword(http_IO *io);

void HandleDownload(http_IO *io);
void HandleUpload(http_IO *io);

void HandleRemind(http_IO *io);
void HandlePublish(http_IO *io);

}
