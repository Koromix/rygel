// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../../common/kutil.hh"
#include "d_codes.hh"

struct ProcedureRealisation {
    ProcedureCode proc;
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
    struct {
        Date date;
        int8_t mode;
        int8_t origin;
    } entry;
    struct {
        Date date;
        int8_t mode;
        int8_t destination;
    } exit;
    UnitCode unit;
    int8_t bed_authorization;
    int16_t session_count;
    int16_t igs2;
    Date last_menstrual_period;
    int16_t gestational_age;
    int16_t newborn_weight;

    DiagnosisCode main_diagnosis;
    DiagnosisCode linked_diagnosis;
    Span<DiagnosisCode> diagnoses;

    Span<ProcedureRealisation> procedures;

#ifndef DISABLE_TESTS
    struct {
        uint16_t cluster_len;

        GhmCode ghm;
        int16_t error;

        GhsCode ghs;
        struct {
            int16_t rea;
            int16_t reasi;
            int16_t si;
            int16_t src;
            int16_t nn1;
            int16_t nn2;
            int16_t nn3;
            int16_t rep;
        } supplements;
    } test;
#endif

    uint32_t error_mask;
};

struct StaySet {
    HeapArray<Stay> stays;

    struct {
        HeapArray<DiagnosisCode> diagnoses;
        HeapArray<ProcedureRealisation> procedures;
    } store;

    bool SavePack(StreamWriter &st) const;
    bool SavePack(const char *filename) const;
};

enum class StaySetDataType {
    Json,
    Pack
};

class StaySetBuilder {
    StaySet set;

public:
    bool Load(StreamReader &st, StaySetDataType type);
    bool LoadFiles(Span<const char *const> filenames);

    bool Finish(StaySet *out_set);
};
