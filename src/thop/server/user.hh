// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "../../drd/libdrd/libdrd.hh"
#include "../../web/libhttp/libhttp.hh"

namespace RG {

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
struct User: public RetainObject {
    const char *name;
    const char *password_hash;

    unsigned int permissions;
    unsigned int mco_dispense_modes;
    HashSet<drd_UnitCode> mco_allowed_units;

    bool CheckPermission(UserPermission permission) const { return permissions & (int)permission; }
    bool CheckMcoDispenseMode(mco_DispenseMode dispense_mode) const
        { return mco_dispense_modes & (1 << (int)dispense_mode); }

    RG_HASHTABLE_HANDLER(User, name);
};

struct UserSet {
    HeapArray<User> users;
    HashTable<const char *, const User *> map;

    BlockAllocator str_alloc;

    const User *FindUser(const char *name) const { return map.FindValue(name, nullptr); }
};

bool InitUsers(const char *profile_directory);

const User *CheckSessionUser(const http_RequestInfo &request, http_IO *io);

void HandleLogin(const http_RequestInfo &request, const User *user, http_IO *io);
void HandleLogout(const http_RequestInfo &request, const User *user, http_IO *io);

}
