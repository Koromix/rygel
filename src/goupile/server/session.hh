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
    AdminDevelop = 1 << 0,
    AdminPublish = 1 << 1,
    AdminConfigure = 1 << 2,
    AdminAssign = 1 << 3,
    DataList = 1 << 4,
    DataRead = 1 << 5,
    DataCreate = 1 << 6,
    DataModify = 1 << 7,
    DataDelete = 1 << 8,
    DataExport = 1 << 9,
    DataBatch = 1 << 10
};
static const char *const UserPermissionNames[] = {
    "AdminDevelop",
    "AdminPublish",
    "AdminConfigure",
    "AdminAssign",
    "DataList",
    "DataRead",
    "DataCreate",
    "DataModify",
    "DataDelete",
    "DataExport",
    "DataBatch"
};
static const uint32_t UserPermissionMasterMask = 0b000001111u;
static const uint32_t UserPermissionSlaveMask =  0b111110000u;

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
    char local_key[45];

    bool IsAdmin() const { return admin_until && admin_until > GetMonotonicTime(); }
    const InstanceToken *GetToken(const InstanceHolder *instance) const;

    void InvalidateTokens();
};

void InvalidateUserTokens(int64_t userid);

RetainPtr<const Session> GetCheckedSession(const http_RequestInfo &request, http_IO *io);

void HandleSessionLogin(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleSessionLogout(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleSessionProfile(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);

}
