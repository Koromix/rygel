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

namespace K {

class InstanceHolder;

enum class UserPermission {
    BuildCode = 1 << 0,
    BuildPublish = 1 << 1,
    BuildAdmin = 1 << 2,
    BuildBatch = 1 << 3,
    DataRead = 1 << 4,
    DataSave = 1 << 5,
    DataDelete = 1 << 6,
    DataAudit = 1 << 7,
    DataAnnotate = 1 << 8,
    ExportCreate = 1 << 9,
    ExportDownload = 1 << 10,
    MiscMail = 1 << 11,
    MiscTexto = 1 << 12,
    MiscOffline = 1 << 13
};
static const char *const UserPermissionNames[] = {
    "BuildCode",
    "BuildPublish",
    "BuildAdmin",
    "BuildBatch",
    "DataRead",
    "DataSave",
    "DataDelete",
    "DataAudit",
    "DataAnnotate",
    "ExportCreate",
    "ExportDownload",
    "MiscMail",
    "MiscTexto",
    "MiscOffline"
};
static const uint32_t UserPermissionMasterMask = 0b00000000001111u;
static const uint32_t UserPermissionSlaveMask =  0b11111111110000u;
static const uint32_t LegacyPermissionMask =     0b11101000110111u;

static const int PasswordHashBytes = 128;

enum class SessionType {
    Login,
    Token,
    Auto
};

struct SessionStamp {
    int64_t unique;

    bool authorized;
    std::atomic_bool develop;
    uint32_t permissions;
    bool single;
    const char *lock;

    bool HasPermission(UserPermission perm) const { return permissions & (int)perm; };

    K_HASHTABLE_HANDLER(SessionStamp, unique);
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
    mutable BlockAllocator stamps_alloc;

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

    void AuthorizeInstance(const InstanceHolder *instance, uint32_t permissions,
                           bool single = false, const char *lock = nullptr);
};

void ExportProfile(const SessionInfo *session, const InstanceHolder *instance, json_Writer *json);
Span<const char> ExportProfile(const SessionInfo *session, const InstanceHolder *instance, Allocator *alloc);

void InvalidateUserStamps(int64_t userid);

RetainPtr<const SessionInfo> GetNormalSession(http_IO *io, InstanceHolder *instance);
RetainPtr<const SessionInfo> GetAdminSession(http_IO *io, InstanceHolder *instance);

void PruneSessions();

bool HashPassword(Span<const char> password, char out_hash[PasswordHashBytes]);

void HandleSessionLogin(http_IO *io, InstanceHolder *instance);
void HandleSessionToken(http_IO *io, InstanceHolder *instance);
void HandleSessionConfirm(http_IO *io, InstanceHolder *instance);
void HandleSessionLogout(http_IO *io);
void HandleSessionProfile(http_IO *io, InstanceHolder *instance);

void HandleChangePassword(http_IO *io, InstanceHolder *instance);
void HandleChangeQRcode(http_IO *io, const char *title);
void HandleChangeTOTP(http_IO *io);
void HandleChangeMode(http_IO *io, InstanceHolder *instance);
void HandleChangeExportKey(http_IO *io, InstanceHolder *instance);

int64_t CreateInstanceUser(InstanceHolder *instance, const char *username);
RetainPtr<const SessionInfo> MigrateGuestSession(http_IO *io, InstanceHolder *instance, const char *username);

}
