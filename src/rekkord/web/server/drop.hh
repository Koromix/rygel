// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"

namespace K {

bool InitDrop();

void HandleDropCreate(http_IO *io);

void HandleDropUpload(http_IO *io);
void HandleDropMark(http_IO *io);

void HandleDropInfo(http_IO *io);
void HandleDropFragment(http_IO *io);

}
