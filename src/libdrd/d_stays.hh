// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "d_common.hh"

struct ProcedureRealisation {
    ProcedureCode proc;
    int8_t phase;
    uint8_t activities;
    int16_t count;
    Date date;
};

struct Stay {
    enum class Error: uint32_t {
        MalformedBirthdate = 1 << 0,
        MalformedSex = 1 << 1,
        MalformedEntryDate = 1 << 2,
        MalformedEntryMode = 1 << 3,
        MalformedEntryOrigin = 1 << 4,
        MalformedExitDate = 1 << 5,
        MalformedExitMode = 1 << 6,
        MalformedExitDestination = 1 << 7,
        MalformedSessionCount = 1 << 8,
        MalformedNewbornWeight = 1 << 9
    };

    int32_t stay_id;
    int32_t bill_id;

    Sex sex;
    Date birthdate;
    struct {
        Date date;
        char mode;
        char origin;
    } entry;
    struct {
        Date date;
        char mode;
        char destination;
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

    // It's 2017, so let's assume 64-bit LE platforms are the majority. Use padding and
    // struct hacking (see StaySetBuilder::LoadPack and StaySet::SavePack) to support dspak
    // files on 32-bit platforms.
    Span<DiagnosisCode> diagnoses;
    Span<ProcedureRealisation> procedures;
#ifndef ARCH_64
    char _pad1[32 - 2 * SIZE(Size) - 2 * SIZE(void *)];
#endif

    uint32_t error_mask;
};

struct StayTest {
    int32_t bill_id;

    uint16_t cluster_len;

    GhmCode ghm;
    int16_t error;

    GhsCode ghs;
    SupplementCounters<int16_t> supplement_days;

    HASH_TABLE_HANDLER(StayTest, bill_id);
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

class StaySetBuilder {
    StaySet set;

public:
    bool LoadJson(StreamReader &st, HashTable<int32_t, StayTest> *out_tests = nullptr);
    bool LoadPack(StreamReader &st, HashTable<int32_t, StayTest> *out_tests = nullptr);
    bool LoadFiles(Span<const char *const> filenames,
                   HashTable<int32_t, StayTest> *out_tests = nullptr);

    bool Finish(StaySet *out_set);
};
