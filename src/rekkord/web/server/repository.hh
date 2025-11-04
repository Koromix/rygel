// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "src/core/native/http/http.hh"

namespace K {

bool CheckRepositories();

void HandleRepositoryList(http_IO *io);
void HandleRepositoryGet(http_IO *io);
void HandleRepositorySave(http_IO *io);
void HandleRepositoryDelete(http_IO *io);
void HandleRepositorySnapshots(http_IO *io);

}
