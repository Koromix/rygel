// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "src/core/base/base.hh"
#include "src/core/http/http.hh"

namespace RG {

class InstanceHolder;

void HandleRecordList(http_IO *io, InstanceHolder *instance);
void HandleRecordGet(http_IO *io, InstanceHolder *instance);
void HandleRecordAudit(http_IO *io, InstanceHolder *instance);

void HandleRecordSave(http_IO *io, InstanceHolder *instance);
void HandleRecordDelete(http_IO *io, InstanceHolder *instance);
void HandleRecordLock(http_IO *io, InstanceHolder *instance);
void HandleRecordUnlock(http_IO *io, InstanceHolder *instance);
void HandleRecordPublic(http_IO *io, InstanceHolder *instance);

void HandleExportCreate(http_IO *io, InstanceHolder *instance);
void HandleExportList(http_IO *io, InstanceHolder *instance);
void HandleExportDownload(http_IO *io, InstanceHolder *instance);

void HandleBlobGet(http_IO *io, InstanceHolder *instance);
void HandleBlobPost(http_IO *io, InstanceHolder *instance);

}
