// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../core/libcc/libcc.hh"
#include "mco_common.hh"
#include "mco_tables.hh"

namespace RG {

struct mco_GhmConstraint {
    enum class Warning {
        PreferCmd28 = 1 << 0
    };

    mco_GhmCode ghm;

    uint32_t cmds;
    uint32_t durations;
    uint32_t raac_durations;
    uint32_t warnings;

    RG_HASHTABLE_HANDLER(mco_GhmConstraint, ghm);
};

bool mco_ComputeGhmConstraints(const mco_TableIndex &index,
                               HashTable<mco_GhmCode, mco_GhmConstraint> *out_constraints);

}
