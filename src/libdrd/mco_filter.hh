// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../libcc/libcc.hh"
#include "mco_classifier.hh"

class mco_FilterRunner {
    HeapArray<char> filter_buf;

    class mco_WrenRunner *wren = nullptr;
    Size wren_count;

public:
    ~mco_FilterRunner();

    bool Init(const char *filter);

    bool IsValid() const { return wren; }

    bool Process(Span<const mco_Result> results, Span<const mco_Result> mono_results,
                 HeapArray<const mco_Result *> *out_results,
                 HeapArray<const mco_Result *> *out_mono_results,
                 mco_StaySet *out_stay_set = nullptr);
    bool Process(Span<const mco_Result> results,
                 HeapArray<const mco_Result *> *out_results,
                 mco_StaySet *out_stay_set = nullptr)
        { return Process(results, {}, out_results, nullptr, out_stay_set); }

private:
    bool ResetRunner();
};

static inline bool mco_Filter(const char *filter,
                              Span<const mco_Result> results, Span<const mco_Result> mono_results,
                              HeapArray<const mco_Result *> *out_results,
                              HeapArray<const mco_Result *> *out_mono_results,
                              mco_StaySet *out_stay_set = nullptr)
{
    mco_FilterRunner runner;
    return runner.Init(filter) &&
           runner.Process(results, mono_results, out_results, out_mono_results, out_stay_set);
}

static inline bool mco_Filter(const char *filter,
                              Span<const mco_Result> results,
                              HeapArray<const mco_Result *> *out_results,
                              mco_StaySet *out_stay_set = nullptr)
{
    mco_FilterRunner runner;
    return runner.Init(filter) &&
           runner.Process(results, out_results, out_stay_set);
}
