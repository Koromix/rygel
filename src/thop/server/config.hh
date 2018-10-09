// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libdrd/libdrd.hh"

struct User {
    const char *name;
    const char *password_hash;

    bool allow_default;
    Span<const char *> allow;
    Span<const char *> deny;

    unsigned int dispense_modes;

    HASH_TABLE_HANDLER(User, name);
};

struct UserSet {
    HeapArray<User> users;
    HashTable<const char *, const User *> map;

    BlockAllocator allow_alloc {Kibibytes(4)};
    BlockAllocator deny_alloc {Kibibytes(4)};
    BlockAllocator str_alloc {Kibibytes(16)};

    const User *FindUser(const char *name) const { return map.FindValue(name, nullptr); }
};

class UserSetBuilder {
    UserSet set;
    HashMap<const char *, Size> map;

public:
    bool LoadIni(StreamReader &st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(UserSet *out_set);
};

struct Unit {
    UnitCode unit;
    const char *path;
};

struct Structure {
    const char *name;
    HeapArray<Unit> units;
};

struct StructureSet {
    mco_DispenseMode dispense_mode;
    HeapArray<Structure> structures;

    BlockAllocator str_alloc {Kibibytes(16)};
};

class StructureSetBuilder {
    StructureSet set;
    HashSet<const char *> map;

public:
    bool LoadIni(StreamReader &st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(StructureSet *out_set);
};
