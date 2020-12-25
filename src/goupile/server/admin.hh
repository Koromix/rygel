// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

extern char admin_etag[33];
extern HashMap<const char *, const AssetInfo *> admin_assets_map;

void InitAdminAssets();

int RunInit(Span<const char *> arguments);
int RunMigrate(Span<const char *> arguments);

void HandleCreateInstance(const http_RequestInfo &request, http_IO *io);
void HandleDeleteInstance(const http_RequestInfo &request, http_IO *io);
void HandleConfigureInstance(const http_RequestInfo &request, http_IO *io);
void HandleListInstances(const http_RequestInfo &request, http_IO *io);

void HandleCreateUser(const http_RequestInfo &request, http_IO *io);
void HandleDeleteUser(const http_RequestInfo &request, http_IO *io);
void HandleAssignUser(const http_RequestInfo &request, http_IO *io);
void HandleListUsers(const http_RequestInfo &request, http_IO *io);

}
