// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_classifier.hh"

enum class mco_DispenseMode {
    E,
    Ex,
    Ex2,
    J,
    ExJ,
    ExJ2
};
static const struct OptionDesc mco_DispenseModeOptions[] = {
    {"e", "E"},
    {"ex", "Ex"},
    {"ex2", "Ex'"},
    {"j", "J"},
    {"exj", "ExJ"},
    {"exj2", "Ex'J"}
};

struct mco_Due {
    UnitCode unit;
    mco_Summary summary;
};

void mco_Dispense(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                  mco_DispenseMode dispense_mode, HeapArray<mco_Due> *out_dues,
                  HashMap<UnitCode, Size> *out_dues_map);
void mco_Dispense(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                  mco_DispenseMode dispense_mode, HeapArray<mco_Due> *out_dues);
