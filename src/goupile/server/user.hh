// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

enum class UserPermission {
    Develop = 1 << 0,
    New = 1 << 1,
    Edit = 1 << 2,
    Offline = 1 << 3
};
static const char *const UserPermissionNames[] = {
    "Develop",
    "New",
    "Edit",
    "Offline"
};

struct Session: public RetainObject {
    uint32_t permissions;
    char username[];

    bool HasPermission(UserPermission perm) const { return permissions & (int)perm; }
};

RetainPtr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io);

void HandleLogin(const http_RequestInfo &request, http_IO *io);
void HandleLogout(const http_RequestInfo &request, http_IO *io);

}
