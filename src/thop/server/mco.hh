// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#pragma once

#include "src/core/base/base.hh"
#include "user.hh"
#include "src/core/http/http.hh"

namespace K {

struct McoCacheSet {
    HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint>> constraints_set;
    HashMap<const void *, HashTable<mco_GhmCode, mco_GhmConstraint> *> index_to_constraints;

    HashMap<const void *, HeapArray<mco_ReadableGhmNode>> readable_nodes;

    BlockAllocator str_alloc;
};

extern mco_TableSet mco_table_set;
extern McoCacheSet mco_cache_set;

extern mco_AuthorizationSet mco_authorization_set;

extern mco_StaySet mco_stay_set;
extern LocalDate mco_stay_set_dates[2];

extern HeapArray<mco_Result> mco_results;
extern HeapArray<mco_Result> mco_mono_results;
extern HashMap<mco_GhmRootCode, Span<const mco_Result *>> mco_results_by_ghm_root;
extern HashMap<const void *, const mco_Result *> mco_results_to_mono;

bool InitMcoTables(Span<const char *const> table_directories);
bool InitMcoProfile(const char *profile_directory, const char *authorization_filename);
bool InitMcoStays(Span<const char *const> stay_directories, Span<const char *const> stay_filenames);

class McoResultProvider {
    K_DELETE_COPY(McoResultProvider)

    // Parameters
    LocalDate min_date = {};
    LocalDate max_date = {};
    const char *filter = nullptr;
    bool allow_mutation = false;
    mco_GhmRootCode ghm_root = {};

public:
    McoResultProvider() = default;

    void SetDateRange(LocalDate min_date, LocalDate max_date)
    {
        this->min_date = min_date;
        this->max_date = max_date;
    }
    void SetFilter(const char *filter, bool allow_mutation) {
        this->filter = filter;
        this->allow_mutation = allow_mutation;
    }
    void SetGhmRoot(mco_GhmRootCode ghm_root) { this->ghm_root = ghm_root; }

    bool Run(FunctionRef<void(Span<const mco_Result>, Span<const mco_Result>)> func);

private:
    bool RunFilter(FunctionRef<void(Span<const mco_Result>, Span<const mco_Result>)> func);
    bool RunIndex(FunctionRef<void(Span<const mco_Result>, Span<const mco_Result>)> func);
    bool RunDirect(FunctionRef<void(Span<const mco_Result>, Span<const mco_Result>)> func);
};

}
