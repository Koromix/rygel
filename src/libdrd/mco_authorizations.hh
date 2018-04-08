// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_common.hh"

struct mco_Authorization {
    UnitCode unit;
    Date dates[2];
    int8_t type;

    HASH_TABLE_HANDLER(mco_Authorization, unit);
};

struct mco_AuthorizationSet {
    HeapArray<mco_Authorization> authorizations;
    HashTable<UnitCode, const mco_Authorization *> authorizations_map;

    Span<const mco_Authorization> FindUnit(UnitCode unit) const;
    const mco_Authorization *FindUnit(UnitCode unit, Date date) const;
};

bool mco_LoadAuthorizationFile(const char *filename, mco_AuthorizationSet *out_set);
