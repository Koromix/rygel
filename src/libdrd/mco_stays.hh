// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "../common/kutil.hh"
#include "mco_common.hh"

struct mco_ProcedureRealisation {
    ProcedureCode proc;
    int8_t phase;
    int8_t activity;
    int16_t count;
    Date date;

    char doc;
    int8_t extension;
};

struct mco_Stay {
    enum class Flag {
        Confirmed = 1 << 0,
        Ucd = 1 << 1
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
    int16_t dip_count;

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

// Some paths (e.g. drdR) need to test for this before building Stay
static inline bool mco_SplitTest(int32_t id1, int32_t id2)
{
    return !id1 || id1 != id2;
}

template <typename T>
Span<T> mco_Split(Span<T> mono_stays, Size count, Span<T> *out_remainder = nullptr)
{
    DebugAssert(mono_stays.len > 0);

    Size agg_len = 0;
    while (count && ++agg_len < mono_stays.len) {
        count -= mco_SplitTest(mono_stays[agg_len - 1].bill_id, mono_stays[agg_len].bill_id);
    }

    if (out_remainder) {
        *out_remainder = mono_stays.Take(agg_len, mono_stays.len - agg_len);
    }
    return mono_stays.Take(0, agg_len);
}

struct mco_Test {
    struct SupplementTest {
        int8_t type;
        int16_t days;
    };

    int32_t bill_id;

    uint16_t cluster_len;

    mco_GhmCode ghm;
    int16_t error;

    mco_GhsCode ghs;
    mco_SupplementCounters<int16_t> supplement_days;
    // Also test individual authorization supplements for 16 first stays
    SupplementTest auth_supplements[16];
    int exb_exh;

    HASH_TABLE_HANDLER(mco_Test, bill_id);
};

struct mco_StaySet {
    HeapArray<mco_Stay> stays;

    BlockAllocator diagnoses_alloc {2048 * SIZE(DiagnosisCode)};
    BlockAllocator procedures_alloc {2048 * SIZE(mco_ProcedureRealisation)};

    bool SavePack(StreamWriter &st) const;
    bool SavePack(const char *filename) const;
};

class mco_StaySetBuilder {
    mco_StaySet set;

    struct FichCompData {
        enum class Type {
            Ucd,
            Dip
        };

        Type type;
        int32_t admin_id;
        Date start_date;
        Date end_date;
        int16_t count;
    };

    HeapArray<FichCompData> fichcomps;

public:
    bool LoadPack(StreamReader &st, HashTable<int32_t, mco_Test> *out_tests = nullptr);
    bool LoadRss(StreamReader &st, HashTable<int32_t, mco_Test> *out_tests = nullptr);
    bool LoadRsa(StreamReader &st, HashTable<int32_t, mco_Test> *out_tests = nullptr);
    bool LoadFichComp(StreamReader &st, HashTable<int32_t, mco_Test> *out_tests = nullptr);

    bool LoadFiles(Span<const char *const> filenames,
                   HashTable<int32_t, mco_Test> *out_tests = nullptr);

    bool Finish(mco_StaySet *out_set);

private:
    bool LoadAtih(StreamReader &st,
                  bool (*parse_func)(Span<const char> line, mco_StaySet *out_set,
                                     HashTable<int32_t, mco_Test> *out_tests),
                  HashTable<int32_t, mco_Test> *out_tests);
};
