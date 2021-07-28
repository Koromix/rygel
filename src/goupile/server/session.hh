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
#include "../../core/libnet/libnet.hh"

namespace RG {

class InstanceHolder;

enum class UserPermission {
    AdminCode = 1 << 0,
    AdminPublish = 1 << 1,
    AdminConfig = 1 << 2,
    AdminAssign = 1 << 3,
    DataLoad = 1 << 4,
    DataSave = 1 << 5,
    DataExport = 1 << 6,
    DataBatch = 1 << 7,
    DataMessage = 1 << 8
};
static const char *const UserPermissionNames[] = {
    "AdminCode",
    "AdminPublish",
    "AdminConfig",
    "AdminAssign",
    "DataLoad",
    "DataSave",
    "DataExport",
    "DataBatch",
    "DataMessage"
};
static const uint32_t UserPermissionMasterMask = 0b000001111u;
static const uint32_t UserPermissionSlaveMask =  0b111110000u;

enum class SessionType {
    Login,
    Token,
    Key,
    Auto
};

struct SessionStamp {
    int64_t unique;

    bool authorized;
    uint32_t permissions;
    const char *ulid;

    bool HasPermission(UserPermission perm) const { return permissions & (int)perm; };

    RG_HASHTABLE_HANDLER(SessionStamp, unique);
};

enum class SessionConfirm {
    None,
    SMS,
    TOTP,
    QRcode // Init TOTP
};

class SessionInfo: public RetainObject {
    mutable std::shared_mutex mutex;

    mutable BucketArray<SessionStamp, 8> stamps;
    mutable HashTable<int64_t, SessionStamp *> stamps_map;

    mutable BlockAllocator str_alloc;

public:
    SessionType type;
    int64_t userid;
    const char *username;
    int64_t admin_until;
    char local_key[45];

    std::atomic<SessionConfirm> confirm;
    std::atomic<const char *> secret;

    bool IsAdmin() const;
    bool HasPermission(const InstanceHolder *instance, UserPermission perm) const;

    const SessionStamp *GetStamp(const InstanceHolder *instance) const;
    void InvalidateStamps();

    void AuthorizeInstance(const InstanceHolder *instance, uint32_t permissions, const char *ulid = nullptr);

    void UpdateSecret();
};

void InvalidateUserStamps(int64_t userid);

RetainPtr<const SessionInfo> GetCheckedSession(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void PruneSessions();

bool HashPassword(Span<const char> password, char out_hash[crypto_pwhash_STRBYTES]);

void HandleSessionLogin(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
bool HandleSessionToken(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
bool HandleSessionKey(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleSessionConfirm(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleSessionLogout(const http_RequestInfo &request, http_IO *io);
void HandleSessionProfile(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleSessionQRcode(const http_RequestInfo &request, http_IO *io);

void HandleChangePassword(const http_RequestInfo &request, http_IO *io);
void HandleChangeTOTP1(const http_RequestInfo &request, http_IO *io);
void HandleChangeTOTP2(const http_RequestInfo &request, http_IO *io);

}
