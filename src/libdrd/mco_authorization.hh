// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "mco_common.hh"

struct mco_Authorization {
    UnitCode unit;
    int8_t type;
    Date dates[2];

    HASH_TABLE_HANDLER(mco_Authorization, unit);
};

struct mco_AuthorizationSet {
    HeapArray<mco_Authorization> authorizations;
    HashTable<UnitCode, const mco_Authorization *> authorizations_map;
    HeapArray<mco_Authorization> facility_authorizations;

    Span<const mco_Authorization> FindUnit(UnitCode unit) const;
    const mco_Authorization *FindUnit(UnitCode unit, Date date) const;

    int8_t GetAuthorizationType(UnitCode unit, Date date) const;
    bool TestAuthorization(UnitCode unit, Date date, int8_t auth_type) const;
};

class mco_AuthorizationSetBuilder {
    mco_AuthorizationSet set;

public:
    bool LoadFicum(StreamReader &stt);
    bool LoadIni(StreamReader &st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(mco_AuthorizationSet *out_set);
};

bool mco_LoadAuthorizationSet(const char *profile_directory,
                              const char *authorization_filename,
                              mco_AuthorizationSet *out_set);
