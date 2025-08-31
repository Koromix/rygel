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
#include "mco_common.hh"

namespace K {

struct mco_Authorization {
    enum class Mode: int8_t {
        Complete,
        Partial,
        Mixed
    };

    drd_UnitCode unit;
    int8_t type;
    Mode mode;
    LocalDate dates[2];

    K_HASHTABLE_HANDLER(mco_Authorization, unit);
};

struct mco_AuthorizationSet {
    HeapArray<mco_Authorization> authorizations;
    HashTable<drd_UnitCode, const mco_Authorization *> authorizations_map;
    HeapArray<mco_Authorization> facility_authorizations;

    Span<const mco_Authorization> FindUnit(drd_UnitCode unit) const;
    const mco_Authorization *FindUnit(drd_UnitCode unit, LocalDate date) const;

    bool TestFacilityAuthorization(int8_t auth_type, LocalDate date) const;
};

class mco_AuthorizationSetBuilder {
    K_DELETE_COPY(mco_AuthorizationSetBuilder)

    mco_AuthorizationSet set;

public:
    mco_AuthorizationSetBuilder() = default;

    bool LoadFicum(StreamReader *st);
    bool LoadIni(StreamReader *st);
    bool LoadFiles(Span<const char *const> filenames);

    void Finish(mco_AuthorizationSet *out_set);
};

bool mco_LoadAuthorizationSet(const char *profile_directory,
                              const char *authorization_filename,
                              mco_AuthorizationSet *out_set);

}
