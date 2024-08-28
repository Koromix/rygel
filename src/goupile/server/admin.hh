// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

#pragma once

#include "src/core/base/base.hh"
#include "src/core/http/http.hh"

namespace RG {

int RunInit(Span<const char *> arguments);
int RunMigrate(Span<const char *> arguments);
int RunKeys(Span<const char *> arguments);

int RunUnseal(Span<const char *> arguments);

void HandleDemo(http_IO *io);

void HandleInstanceCreate(http_IO *io);
void HandleInstanceDelete(http_IO *io);
void HandleInstanceConfigure(http_IO *io);
void HandleInstanceList(http_IO *io);
void HandleInstanceAssign(http_IO *io);
void HandleInstancePermissions(http_IO *io);

bool ArchiveDomain();

void HandleArchiveCreate(http_IO *io);
void HandleArchiveDelete(http_IO *io);
void HandleArchiveList(http_IO *io);
void HandleArchiveDownload(http_IO *io);
void HandleArchiveUpload(http_IO *io);
void HandleArchiveRestore(http_IO *io);

void HandleUserCreate(http_IO *io);
void HandleUserEdit(http_IO *io);
void HandleUserDelete(http_IO *io);
void HandleUserList(http_IO *io);

}
