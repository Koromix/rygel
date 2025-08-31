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
#include "mco_common.hh"

namespace K {

struct alignas(8) mco_ProcedureRealisation {
    drd_ProcedureCode proc;
    int8_t phase;
    int8_t activity;
    int16_t count;
    LocalDate date;

    char doc;
    int8_t extension;
};

struct mco_Stay {
    enum class Flag {
        Confirmed = 1 << 0,
        UCD = 1 << 1,
        NoConversion = 1 << 2,
        Conversion = 1 << 3,
        RAAC = 1 << 4,
        Context = 1 << 5,
        HospitalUse = 1 << 6,
        Rescript = 1 << 7
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
        MalformedConversion = 1 << 16,
        MalformedRAAC = 1 << 17,
        MalformedContext = 1 << 18,
        MalformedHospitalUse = 1 << 19,
        MalformedRescript = 1 << 20,
        MalformedMainDiagnosis = 1 << 21,
        MalformedLinkedDiagnosis = 1 << 22,
        MissingOtherDiagnosesCount = 1 << 23,
        MalformedOtherDiagnosesCount = 1 << 24,
        MalformedOtherDiagnosis = 1 << 25,
        MissingProceduresCount = 1 << 26,
        MalformedProceduresCount = 1 << 27,
        MalformedProcedureCode = 1 << 28,
        MalformedProcedureExtension = 1 << 29
    };

    uint32_t flags;
    uint32_t errors;

    int32_t admin_id;
    int32_t bill_id;

    int8_t sex;
    LocalDate birthdate;
    struct {
        LocalDate date;
        char mode;
        char origin;
    } entry;
    struct {
        LocalDate date;
        char mode;
        char destination;
    } exit;
    drd_UnitCode unit;
    int8_t bed_authorization;
    int16_t session_count;
    int16_t igs2;
    LocalDate last_menstrual_period;
    int16_t gestational_age;
    int16_t newborn_weight;
    int16_t dip_count;
    char interv_category;

    drd_DiagnosisCode main_diagnosis;
    drd_DiagnosisCode linked_diagnosis;

    // It's 2017, so let's assume 64-bit LE platforms are the majority. Use padding and
    // struct hacking (see StaySetBuilder::LoadPack and StaySet::SavePack) to support dspak
    // files on 32-bit platforms.
    Span<drd_DiagnosisCode> other_diagnoses;
    Span<mco_ProcedureRealisation> procedures;
#if K_SIZE_MAX < INT64_MAX
    char _pad1[32 - 2 * K_SIZE(Size) - 2 * K_SIZE(void *)];
#endif
};

// Some paths (e.g. drdR) need to test for this before building Stay
static inline bool mco_SplitTest(int32_t id1, int32_t id2)
{
    return !id1 || id1 != id2;
}

template <typename T>
Span<T> mco_Split(Span<T> mono_stays, Size split_len, Span<T> *out_remainder = nullptr)
{
    K_ASSERT(mono_stays.len >= split_len);

    while (split_len < mono_stays.len &&
           !mco_SplitTest(mono_stays[split_len - 1].bill_id, mono_stays[split_len].bill_id)) {
        split_len++;
    }

    if (out_remainder) {
        *out_remainder = mono_stays.Take(split_len, mono_stays.len - split_len);
    }
    return mono_stays.Take(0, split_len);
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

    K_HASHTABLE_HANDLER(mco_Test, bill_id);
};

struct mco_StaySet {
    HeapArray<mco_Stay> stays;
    LinkedAllocator array_alloc;

    bool SavePack(StreamWriter *st) const;
    bool SavePack(const char *filename) const;
};

class mco_StaySetBuilder {
    K_DELETE_COPY(mco_StaySetBuilder)

    mco_StaySet set;

    BlockAllocator other_diagnoses_alloc { 2048 * K_SIZE(drd_DiagnosisCode) };
    BlockAllocator procedures_alloc { 2048 * K_SIZE(mco_ProcedureRealisation) };

    struct FichCompData {
        enum class Type {
            UCD,
            DIP
        };

        Type type;
        int32_t admin_id;
        LocalDate start_date;
        LocalDate end_date;
        int16_t count;
    };

    HeapArray<FichCompData> fichcomps;

public:
    mco_StaySetBuilder() = default;

    bool LoadPack(StreamReader *st, HashTable<int32_t, mco_Test> *out_tests = nullptr);
    bool LoadRss(StreamReader *st, HashTable<int32_t, mco_Test> *out_tests = nullptr);
    bool LoadRsa(StreamReader *st, HashTable<int32_t, mco_Test> *out_tests = nullptr);
    bool LoadFichComp(StreamReader *st, HashTable<int32_t, mco_Test> *out_tests = nullptr);

    bool LoadFiles(Span<const char *const> filenames,
                   HashTable<int32_t, mco_Test> *out_tests = nullptr);

    bool Finish(mco_StaySet *out_set);

private:
    bool LoadAtih(StreamReader *st,
                  bool (mco_StaySetBuilder::*parse_func)(Span<const char> line,
                                                         HashTable<int32_t, mco_Test> *out_tests),
                  HashTable<int32_t, mco_Test> *out_tests);
    bool ParseRssLine(Span<const char> line, HashTable<int32_t, mco_Test> *out_tests);
    bool ParseRsaLine(Span<const char> line, HashTable<int32_t, mco_Test> *out_tests);
};

}
