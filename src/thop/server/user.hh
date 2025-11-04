// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/native/base/base.hh"
#include "src/drd/libdrd/libdrd.hh"
#include "src/core/native/http/http.hh"

namespace K {

struct StructureEntity;
struct StructureSet;

enum class UserPermission {
    McoCasemix = 1 << 0,
    McoResults = 1 << 1,
    McoFilter = 1 << 2,
    McoMutate = 1 << 3
};
static const char *const UserPermissionNames[] = {
    "McoCasemix",
    "McoResults",
    "McoFilter",
    "McoMutate"
};

// We don't need any extra session information for connected users, so we can
// use this as the user data pointer stored in session manager.
struct User: public RetainObject<User> {
    const char *name;
    const char *password_hash;

    unsigned int permissions;
    unsigned int mco_dispense_modes;
    HashSet<drd_UnitCode> mco_allowed_units;

    bool CheckPermission(UserPermission permission) const { return permissions & (int)permission; }
    bool CheckMcoDispenseMode(mco_DispenseMode dispense_mode) const
        { return mco_dispense_modes & (1 << (int)dispense_mode); }

    K_HASHTABLE_HANDLER(User, name);
};

struct UserSet {
    HeapArray<User> users;
    HashTable<const char *, const User *> map;

    BlockAllocator str_alloc;

    const User *FindUser(const char *name) const { return map.FindValue(name, nullptr); }
};

bool InitUsers(const char *profile_directory);

const User *CheckSessionUser(http_IO *io);
void PruneSessions();

void HandleLogin(http_IO *io, const User *user);
void HandleLogout(http_IO *io, const User *user);

}
