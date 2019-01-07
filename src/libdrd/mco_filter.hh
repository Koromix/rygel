// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/util.hh"
#include "mco_classifier.hh"

bool mco_Filter(Span<const mco_Stay> stays, Span<const char> filter,
                std::function<Size(Span<const mco_Stay> stays,
                                   mco_Result out_results[],
                                   mco_Result out_mono_results[])> func,
                HeapArray<mco_Stay> *out_stays, HeapArray<mco_Result> *out_results,
                HeapArray<mco_Result> *out_mono_results = nullptr);
