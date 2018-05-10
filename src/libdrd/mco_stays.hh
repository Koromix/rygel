// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_common.hh"

struct mco_ProcedureRealisation {
    ProcedureCode proc;
    int8_t phase;
    uint8_t activities;
    int16_t count;
    Date date;

    char doc;
    int8_t extension;
};

struct mco_Stay {
    enum class Flag {
        Confirmed = 1 << 0,
    };

    enum class Error {
        UnknownRumVersion = 1 << 0,
        MalformedBillId = 1 << 1,
        MalformedBirthdate = 1 << 2,
        MalformedSex = 1 << 3,
        MalformedEntryDate = 1 << 4,
        MalformedEntryMode = 1 << 5,
        MalformedEntryOrigin = 1 << 6,
        MalformedExitDate = 1 << 7,
        MalformedExitMode = 1 << 8,
        MalformedExitDestination = 1 << 9,
        MalformedSessionCount = 1 << 10,
        MalformedGestationalAge = 1 << 11,
        MalformedNewbornWeight = 1 << 12,
        MalformedLastMenstrualPeriod = 1 << 13,
        MalformedIgs2 = 1 << 14,
        MalformedConfirmation = 1 << 15,
        MalformedMainDiagnosis = 1 << 16,
        MalformedLinkedDiagnosis = 1 << 17,
        MissingOtherDiagnosesCount = 1 << 18,
        MalformedOtherDiagnosesCount = 1 << 19,
        MalformedOtherDiagnosis = 1 << 20,
        MissingProceduresCount = 1 << 21,
        MalformedProceduresCount = 1 << 22,
        MalformedProcedureCode = 1 << 23,
        MalformedProcedureExtension = 1 << 24
    };

    uint32_t flags;
    uint32_t errors;

    int32_t admin_id;
    int32_t bill_id;

    int8_t sex;
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
    Span<mco_ProcedureRealisation> procedures;
#ifndef ARCH_64
    char _pad1[32 - 2 * SIZE(Size) - 2 * SIZE(void *)];
#endif
};

struct mco_StayTest {
    int32_t bill_id;

    uint16_t cluster_len;

    mco_GhmCode ghm;
    int16_t error;

    mco_GhsCode ghs;
    mco_SupplementCounters<int16_t> supplement_days;

    HASH_TABLE_HANDLER(mco_StayTest, bill_id);
};

struct mco_StaySet {
    HeapArray<mco_Stay> stays;

    struct {
        HeapArray<DiagnosisCode> diagnoses;
        HeapArray<mco_ProcedureRealisation> procedures;
    } store;

    bool SavePack(StreamWriter &st) const;
    bool SavePack(const char *filename) const;
};

class mco_StaySetBuilder {
    mco_StaySet set;

public:
    bool LoadPack(StreamReader &st, HashTable<int32_t, mco_StayTest> *out_tests = nullptr);
    bool LoadRss(StreamReader &st, HashTable<int32_t, mco_StayTest> *out_tests = nullptr);
    bool LoadRsa(StreamReader &st, HashTable<int32_t, mco_StayTest> *out_tests = nullptr);

    bool LoadFiles(Span<const char *const> filenames,
                   HashTable<int32_t, mco_StayTest> *out_tests = nullptr);

    bool Finish(mco_StaySet *out_set);

private:
    bool LoadAtih(StreamReader &st,
                  bool (*parse_func)(Span<const char> line, mco_StaySet *out_set,
                                     HashTable<int32_t, mco_StayTest> *out_tests),
                  HashTable<int32_t, mco_StayTest> *out_tests);
};
