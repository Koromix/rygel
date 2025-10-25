// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "src/core/http/http.hh"

namespace K {

int RunInit(Span<const char *> arguments);
int RunKeys(Span<const char *> arguments);
int RunUnseal(Span<const char *> arguments);

bool ArchiveDomain();

void HandleDomainInfo(http_IO *io);
void HandleDomainConfigure(http_IO *io);
void HandleDomainDemo(http_IO *io);
void HandleDomainRestore(http_IO *io);

void HandleInstanceCreate(http_IO *io);
void HandleInstanceDelete(http_IO *io);
void HandleInstanceConfigure(http_IO *io);
void HandleInstanceList(http_IO *io);
void HandleInstanceAssign(http_IO *io);
void HandleInstancePermissions(http_IO *io);
void HandleInstanceMigrate(http_IO *io);
void HandleInstanceClear(http_IO *io);

void HandleArchiveCreate(http_IO *io);
void HandleArchiveDelete(http_IO *io);
void HandleArchiveList(http_IO *io);
void HandleArchiveDownload(http_IO *io);
void HandleArchiveUpload(http_IO *io);

void HandleUserCreate(http_IO *io);
void HandleUserEdit(http_IO *io);
void HandleUserDelete(http_IO *io);
void HandleUserList(http_IO *io);

}
