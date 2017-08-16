#pragma once

#include "kutil.hh"
#include "stays.hh"
#include "tables.hh"

struct ResultError {
    int code;
};

struct Result {
    ArrayRef<Stay> stays;
    Stay synth_stay;

    GhmCode ghm_code;
    ArrayRef<ResultError> errors;
};

struct ResultSet {
    HeapArray<Result> results;

    struct {
        HeapArray<ResultError> errors;
    } store;
};

bool Classify(const ClassifierSet &classifier_set, const StaySet &stay_set,
              ResultSet *out_result_set);
