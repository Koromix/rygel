// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

class InstanceHolder;

enum class UserPermission {
    Develop = 1 << 0,
    New = 1 << 1,
    Edit = 1 << 2,
    Deploy = 1 << 3,
    Validate = 1 << 4,
    Export = 1 << 5,
    Recompute = 1 << 6
};
static const char *const UserPermissionNames[] = {
    "Develop",
    "New",
    "Edit",
    "Deploy",
    "Validate",
    "Export",
    "Recompute"
};

struct Token {
    const char *zone;
    uint32_t permissions;

    bool HasPermission(UserPermission perm) const { return permissions & (int)perm; };
};

class Session: public RetainObject {
    mutable std::shared_mutex tokens_lock;
    mutable HashMap<int64_t, Token> tokens_map;
    mutable BlockAllocator tokens_alloc;

public:
    const char *username;
    bool admin;
    bool demo;

    const Token *GetToken(const InstanceHolder *instance) const;
};

RetainPtr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io);

void HandleUserLogin(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleUserLogout(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleUserProfile(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);

}
