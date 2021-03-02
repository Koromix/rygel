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

struct InstanceToken {
    uint32_t permissions;
    const char *title;
    const char *url;

    bool HasPermission(UserPermission perm) const { return permissions & (int)perm; };
};

class Session: public RetainObject {
    mutable std::shared_mutex tokens_lock;
    mutable HashMap<int64_t, InstanceToken> tokens_map;
    mutable BlockAllocator tokens_alloc;

public:
    int64_t userid;
    const char *username;
    int64_t admin_until;
    bool demo;
    char local_key[64];

    bool IsAdmin() const { return admin_until && admin_until > GetMonotonicTime(); }
    const InstanceToken *GetToken(const InstanceHolder *instance) const;
};

RetainPtr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io);

void HandleUserLogin(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleUserLogout(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleUserProfile(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);

void HandleUserBackup(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);

}
