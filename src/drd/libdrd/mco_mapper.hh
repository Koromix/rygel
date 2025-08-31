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
