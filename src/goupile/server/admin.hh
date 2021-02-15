// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

int RunInit(Span<const char *> arguments);
int RunMigrate(Span<const char *> arguments);

void HandleInstanceCreate(const http_RequestInfo &request, http_IO *io);
void HandleInstanceDelete(const http_RequestInfo &request, http_IO *io);
void HandleInstanceConfigure(const http_RequestInfo &request, http_IO *io);
void HandleInstanceList(const http_RequestInfo &request, http_IO *io);

void HandleUserCreate(const http_RequestInfo &request, http_IO *io);
void HandleUserEdit(const http_RequestInfo &request, http_IO *io);
void HandleUserDelete(const http_RequestInfo &request, http_IO *io);
void HandleUserAssign(const http_RequestInfo &request, http_IO *io);
void HandleUserList(const http_RequestInfo &request, http_IO *io);

}
