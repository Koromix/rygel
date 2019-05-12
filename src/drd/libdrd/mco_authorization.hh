// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../libcc/libcc.hh"
#include "mco_common.hh"

namespace RG {

struct mco_Authorization {
    enum class Mode: int8_t {
        Complete,
        Partial,
        Mixed
    };

    drd_UnitCode unit;
    int8_t type;
    Mode mode;
    Date dates[2];

    RG_HASH_TABLE_HANDLER(mco_Authorization, unit);
};

struct mco_AuthorizationSet {
    HeapArray<mco_Authorization> authorizations;
    HashTable<drd_UnitCode, const mco_Authorization *> authorizations_map;
    HeapArray<mco_Authorization> facility_authorizations;

    Span<const mco_Authorization> FindUnit(drd_UnitCode unit) const;
    const mco_Authorization *FindUnit(drd_UnitCode unit, Date date) const;

    bool TestFacilityAuthorization(int8_t auth_type, Date date) const;
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

}
