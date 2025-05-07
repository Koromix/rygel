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
#include "src/drd/libdrd/libdrd.hh"
#include "src/core/http/http.hh"

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
struct User: public RetainObject<User> {
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

const User *CheckSessionUser(http_IO *io);
void PruneSessions();

void HandleLogin(http_IO *io, const User *user);
void HandleLogout(http_IO *io, const User *user);

}
