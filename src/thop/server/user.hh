// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "structure.hh"
#include "../../libdrd/libdrd.hh"
#include "../../wrappers/http.hh"

struct StructureEntity;

enum class UserPermission {
    FullResults = 1 << 0,
    UseFilter = 1 << 1,
    MutateFilter = 1 << 2
};
static const char *const UserPermissionNames[] = {
    "FullResults",
    "UseFilter",
    "MutateFilter"
};

struct User {
    const char *name;
    const char *password_hash;

    unsigned int permissions;

    unsigned int mco_dispense_modes;
    HashSet<UnitCode> mco_allowed_units;

    bool CheckMcoDispenseMode(mco_DispenseMode dispense_mode) const
        { return (mco_dispense_modes & (1 << (int)dispense_mode)); }

    HASH_TABLE_HANDLER(User, name);
};

struct UserSet {
    HeapArray<User> users;
    HashTable<const char *, const User *> map;

    BlockAllocator str_alloc;

    const User *FindUser(const char *name) const { return map.FindValue(name, nullptr); }
};

class UserSetBuilder {
    struct UnitRuleSet {
        bool allow_default;
        Span<const char *> allow;
        Span<const char *> deny;
    };

    UserSet set;
    HashMap<const char *, Size> map;

    HeapArray<UnitRuleSet> rule_sets;
    BlockAllocator allow_alloc;
    BlockAllocator deny_alloc;

public:
    bool LoadIni(StreamReader &st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(const StructureSet &structure_set, UserSet *out_set);

private:
    bool CheckUnitPermission(const UnitRuleSet &rule_set, const StructureEntity &ent);
};

bool LoadUserSet(Span<const char *const> filenames, const StructureSet &structure_set,
                 UserSet *out_set);

const User *CheckSessionUser(MHD_Connection *conn, bool *out_mismatch = nullptr);
void DeleteSessionCookies(http_Response *response);

int HandleConnect(const http_Request &request, const User *user, http_Response *out_response);
int HandleDisconnect(const http_Request &request, const User *user, http_Response *out_response);
