/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"
#include "data.hh"
#include "tables.hh"

struct GhmConstraint {
    GhmCode ghm;

    uint64_t duration_mask;

    HASH_SET_HANDLER(GhmConstraint, ghm);
};

bool ComputeGhmConstraints(const TableIndex &index,
                           HashSet<GhmCode, GhmConstraint> *out_constraints);
