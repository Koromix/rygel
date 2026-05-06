// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"
#include "lib/native/http/http.hh"

namespace K {

bool InitDrop();

bool PruneDrops();

void HandleDropList(http_IO *io);
void HandleDropCreate(http_IO *io);
void HandleDropUpload(http_IO *io);
void HandleDropMark(http_IO *io);
void HandleDropDelete(http_IO *io);
void HandleDropInfo(http_IO *io);
void HandleDropFragment(http_IO *io);

}
