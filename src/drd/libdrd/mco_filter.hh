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
#include "mco_classifier.hh"

namespace K {

class mco_FilterRunner {
    K_DELETE_COPY(mco_FilterRunner)

    HeapArray<char> filter_buf;

    class mco_WrenRunner *wren = nullptr;
    Size wren_count;

public:
  mco_FilterRunner() = default;
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

}
