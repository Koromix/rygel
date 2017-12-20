// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../common/kutil.hh"
#include "d_codes.hh"

struct Authorization {
    UnitCode unit;
    Date dates[2];
    int8_t type;

    HASH_SET_HANDLER(Authorization, unit);
};

struct AuthorizationSet {
    HeapArray<Authorization> authorizations;
    HashSet<UnitCode, const Authorization *> authorizations_map;

    Span<const Authorization> FindUnit(UnitCode unit) const;
    const Authorization *FindUnit(UnitCode unit, Date date) const;
};

bool LoadAuthorizationFile(const char *filename, AuthorizationSet *out_set);
