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

#include "src/core/libcc/libcc.hh"
#include "src/core/libnet/libnet.hh"

namespace RG {

class InstanceHolder;

enum class UserPermission {
    BuildCode = 1 << 0,
    BuildPublish = 1 << 1,
    BuildAdmin = 1 << 2,
    BuildBatch = 1 << 3,
    DataLoad = 1 << 4,
    DataNew = 1 << 5,
    DataEdit = 1 << 6,
    DataDelete = 1 << 7,
    DataAudit = 1 << 8,
    DataAnnotate = 1 << 9,
    DataExport = 1 << 10,
    MiscMail = 1 << 11,
    MiscTexto = 1 << 12,
    MiscOffline = 1 << 13
};
static const char *const UserPermissionNames[] = {
    "BuildCode",
    "BuildPublish",
    "BuildAdmin",
    "BuildBatch",
    "DataLoad",
    "DataNew",
    "DataEdit",
    "DataDelete",
    "DataAudit",
    "DataAnnotate",
    "DataExport",
    "MiscMail",
    "MiscTexto",
    "MiscOffline"
};
static const uint32_t UserPermissionMasterMask = 0b000000001111u;
static const uint32_t UserPermissionSlaveMask =  0b111111110000u;

static const int PasswordHashBytes = 128;

enum class SessionType {
    Login,
    Token,
    Key,
    Auto
};

struct SessionStamp {
    int64_t unique;

    bool authorized;
    std::atomic_bool develop;
    uint32_t permissions;

    bool HasPermission(UserPermission perm) const { return permissions & (int)perm; };

    RG_HASHTABLE_HANDLER(SessionStamp, unique);
};

enum class SessionConfirm {
    None,
    Mail,
    SMS,
    TOTP,
    QRcode // Init TOTP
};

class SessionInfo: public RetainObject<SessionInfo> {
    mutable BucketArray<SessionStamp, 8> stamps;
    mutable HashTable<int64_t, SessionStamp *> stamps_map;

public:
    mutable std::shared_mutex mutex;

    SessionType type;
    int64_t userid;
    bool is_root;
    bool is_admin;
    std::atomic<int64_t> admin_until;
    char local_key[45];
    bool change_password;
    std::atomic<SessionConfirm> confirm;
    char secret[33]; // Lock mutex to change
    char username[];

    bool IsAdmin() const;
    bool IsRoot() const;
    bool HasPermission(const InstanceHolder *instance, UserPermission perm) const;

    SessionStamp *GetStamp(const InstanceHolder *instance) const;
    void InvalidateStamps();

    void AuthorizeInstance(const InstanceHolder *instance, uint32_t permissions);
};

void InvalidateUserStamps(int64_t userid);

RetainPtr<const SessionInfo> GetNormalSession(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
RetainPtr<const SessionInfo> GetAdminSession(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);

void PruneSessions();

bool HashPassword(Span<const char> password, char out_hash[PasswordHashBytes]);

void HandleSessionLogin(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleSessionToken(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleSessionKey(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleSessionConfirm(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleSessionLogout(const http_RequestInfo &request, http_IO *io);
void HandleSessionProfile(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);

void HandleChangePassword(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleChangeQRcode(const http_RequestInfo &request, http_IO *io);
void HandleChangeTOTP(const http_RequestInfo &request, http_IO *io);
void HandleChangeMode(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);
void HandleChangeExportKey(InstanceHolder *instance, const http_RequestInfo &request, http_IO *io);

RetainPtr<const SessionInfo> MigrateGuestSession(const SessionInfo &guest, InstanceHolder *instance,
                                                 const http_RequestInfo &request, http_IO *io);


}
