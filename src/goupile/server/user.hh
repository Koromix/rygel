// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "src/core/http/http.hh"

namespace K {

class InstanceHolder;

enum class PasswordComplexity {
    Easy,
    Moderate,
    Hard
};
static const char *const PasswordComplexityNames[] = {
    "Easy",
    "Moderate",
    "Hard"
};

enum class UserPermission {
    BuildCode = 1 << 0,
    BuildPublish = 1 << 1,
    BuildAdmin = 1 << 2,
    DataRead = 1 << 3,
    DataSave = 1 << 4,
    DataDelete = 1 << 5,
    DataAudit = 1 << 6,
    DataOffline = 1 << 7,
    ExportCreate = 1 << 8,
    ExportDownload = 1 << 9,
    MessageMail = 1 << 10,
    MessageText = 1 << 11
};
static const char *const UserPermissionNames[] = {
    "BuildCode",
    "BuildPublish",
    "BuildAdmin",
    "DataRead",
    "DataSave",
    "DataDelete",
    "DataAudit",
    "DataOffline",
    "ExportCreate",
    "ExportDownload",
    "MessageMail",
    "MessageText"
};
static const uint32_t UserPermissionMasterMask = 0b000000000111u;
static const uint32_t UserPermissionSlaveMask =  0b111111111000u;
static const uint32_t LegacyPermissionMask =     0b110100011111u;

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

bool CheckPasswordComplexity(const char *password, const char *username, PasswordComplexity treshold);
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
