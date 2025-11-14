// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "lib/native/base/base.hh"
#include "mco_common.hh"
#include "mco_table.hh"

namespace K {

struct mco_GhmConstraint {
    enum class Warning {
        PreferCmd28 = 1 << 0
    };

    mco_GhmCode ghm;

    uint32_t cmds;
    uint32_t durations;
    uint32_t raac_durations;
    uint32_t warnings;

    K_HASHTABLE_HANDLER(mco_GhmConstraint, ghm);
};

bool mco_ComputeGhmConstraints(const mco_TableIndex &index,
                               HashTable<mco_GhmCode, mco_GhmConstraint> *out_constraints);

}
