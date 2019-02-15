// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "thop.hh"
#include "../../wrappers/http.hh"

extern mco_TableSet mco_table_set;
extern HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint>> mco_constraints_set;
extern HeapArray<HashTable<mco_GhmCode, mco_GhmConstraint> *> mco_index_to_constraints;

extern mco_AuthorizationSet mco_authorization_set;

extern mco_StaySet mco_stay_set;
extern Date mco_stay_set_dates[2];

extern HeapArray<mco_Result> mco_results;
extern HeapArray<mco_Result> mco_mono_results;
extern HashMap<mco_GhmRootCode, Span<const mco_Result *>> mco_results_by_ghm_root;
extern HashMap<const void *, const mco_Result *> mco_results_to_mono;

bool InitMcoTables(Span<const char *const> table_directories);
bool InitMcoProfile(const char *profile_directory, const char *authorization_filename);
bool InitMcoStays(Span<const char *const> stay_directories, Span<const char *const> stay_filenames);

class McoResultProvider {
    // Parameters
    Date min_date = {};
    Date max_date = {};
    const char *filter = nullptr;
    bool allow_mutation = false;
    mco_GhmRootCode ghm_root = {};

public:
    void SetDateRange(Date min_date, Date max_date)
    {
        this->min_date = min_date;
        this->max_date = max_date;
    }
    void SetFilter(const char *filter, bool allow_mutation) {
        this->filter = filter;
        this->allow_mutation = allow_mutation;
    }
    void SetGhmRoot(mco_GhmRootCode ghm_root) { this->ghm_root = ghm_root; }

    bool Run(std::function<void(Span<const mco_Result>, Span<const mco_Result>)> func);

private:
    bool RunFilter(std::function<void(Span<const mco_Result>, Span<const mco_Result>)> func);
    bool RunIndex(std::function<void(Span<const mco_Result>, Span<const mco_Result>)> func);
    bool RunDirect(std::function<void(Span<const mco_Result>, Span<const mco_Result>)> func);
};

int ProduceMcoSettings(const ConnectionInfo *conn, const char *url, http_Response *out_response);
