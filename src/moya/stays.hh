/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "kutil.hh"
#include "codes.hh"

struct Authorization {
    UnitCode unit;
    Date dates[2];
    int8_t type;

    HASH_SET_HANDLER(Authorization, unit);
};

struct AuthorizationSet {
    HeapArray<Authorization> authorizations;
    HashSet<UnitCode, const Authorization *> authorizations_map;

    ArrayRef<const Authorization> FindUnit(UnitCode unit_code) const;
    const Authorization *FindUnit(UnitCode unit_code, Date date) const;
};

struct Procedure {
    ProcedureCode code;
    int8_t phase;
    uint8_t activities;
    int16_t count;
    Date date;
};

struct Stay {
    enum class Error: uint32_t {
        MalformedBirthdate = 0x1
    };

    int32_t stay_id;
    int32_t bill_id;

    Sex sex;
    Date birthdate;
    Date dates[2];
    struct {
        int8_t mode;
        int8_t origin;
    } entry;
    struct {
        int8_t mode;
        int8_t destination;
    } exit;
    UnitCode unit_code;
    int8_t bed_authorization;
    int16_t session_count;
    int16_t igs2;
    Date last_menstrual_period;
    int16_t gestational_age;
    int16_t newborn_weight;

    DiagnosisCode main_diagnosis;
    DiagnosisCode linked_diagnosis;
    ArrayRef<DiagnosisCode> diagnoses;

    ArrayRef<Procedure> procedures;

#ifndef DISABLE_TESTS
    struct {
        GhmCode ghm;
        int16_t error;
        uint16_t cluster_len;
    } test;
#endif

    uint32_t error_mask;
};

struct StaySet {
    HeapArray<Stay> stays;

    struct {
        HeapArray<DiagnosisCode> diagnoses;
        HeapArray<Procedure> procedures;
    } store;
};

bool LoadAuthorizationFile(const char *filename, AuthorizationSet *out_set);

class StaySetBuilder {
    StaySet set;

public:
    bool LoadFile(ArrayRef<const char *const> filenames);
    bool Finish(StaySet *out_set);
};
