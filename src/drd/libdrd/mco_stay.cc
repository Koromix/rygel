// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

#include "src/core/native/base/base.hh"
#include "mco_stay.hh"

namespace K {

#pragma pack(push, 1)
struct PackHeader {
    char signature[13];
    int8_t version;
    int8_t native_size;
    char _pad1[1];

    int64_t stays_len;
    int64_t diagnoses_len;
    int64_t procedures_len;
};
#pragma pack(pop)
#define PACK_VERSION 18
#define PACK_SIGNATURE "DRD_MCO_PACK"

// This should warn us in most cases when we break dspak files (it's basically a memcpy format)
static_assert(K_SIZE(PackHeader::signature) == K_SIZE(PACK_SIGNATURE));
static_assert(K_SIZE(mco_Stay) == 112);
static_assert(K_SIZE(drd_DiagnosisCode) == 8);
static_assert(K_SIZE(mco_ProcedureRealisation) == 24);

bool mco_StaySet::SavePack(StreamWriter *st) const
{
    PackHeader bh = {};

    CopyString(PACK_SIGNATURE, bh.signature);
    bh.version = PACK_VERSION;
    bh.native_size = (uint8_t)K_SIZE(Size);
    bh.stays_len = stays.len;
    for (const mco_Stay &stay: stays) {
        bh.diagnoses_len += stay.other_diagnoses.len;
        bh.procedures_len += stay.procedures.len;
    }

    st->Write(&bh, K_SIZE(bh));
#if K_SIZE_MAX == INT64_MAX
    st->Write(stays.ptr, stays.len * K_SIZE(*stays.ptr));
#else
    for (const mco_Stay &stay: stays) {
        mco_Stay stay2;
        MemCpy(&stay2, &stay, K_SIZE(stay));

        union {
            uint8_t raw[32];
            struct {
                int64_t _pad1;
                int64_t other_diagnoses_len;
                int64_t _pad2;
                int64_t procedures_len;
            } st;
        } u;
        u.st.other_diagnoses_len = (int64_t)stay.other_diagnoses.len;
        u.st.procedures_len = (int64_t)stay.procedures.len;
        MemCpy(&stay2.other_diagnoses, u.raw, 32);

        st->Write(&stay2, K_SIZE(stay2));
    }
#endif
    for (const mco_Stay &stay: stays) {
        st->Write(stay.other_diagnoses.ptr, stay.other_diagnoses.len * K_SIZE(*stay.other_diagnoses.ptr));
    }
    for (const mco_Stay &stay: stays) {
        st->Write(stay.procedures.ptr, stay.procedures.len * K_SIZE(*stay.procedures.ptr));
    }

    return st->Close();
}

bool mco_StaySet::SavePack(const char *filename) const
{
    CompressionType compression_type;
    Span<const char> extension = GetPathExtension(filename, &compression_type);

    if (!TestStr(extension, ".dmpak")) {
        LogError("Unknown packing extension '%1', prefer '.dmpak'", extension);
    }

    StreamWriter st(filename, 0, compression_type);
    return SavePack(&st);
}

bool mco_StaySetBuilder::LoadPack(StreamReader *st, HashTable<int32_t, mco_Test> *out_tests)
{
    const Size start_stays_len = set.stays.len;
    K_DEFER_N(set_guard) { set.stays.RemoveFrom(start_stays_len); };

    if (out_tests) {
        LogError("Testing is not supported by '.dmpak' files");
    }

    HeapArray<drd_DiagnosisCode> other_diagnoses(&other_diagnoses_alloc);
    HeapArray<mco_ProcedureRealisation> procedures(&procedures_alloc);

    PackHeader bh;
    if (st->ReadFill(K_SIZE(bh), &bh) != K_SIZE(bh))
        goto corrupt_error;

    if (strncmp(bh.signature, PACK_SIGNATURE, K_SIZE(bh.signature)) != 0) {
        LogError("File '%1' does not have dspak signature", st->GetFileName());
        return false;
    }
    if (bh.version != PACK_VERSION) {
        LogError("Cannot load '%1' (dspak version %2), expected version %3", st->GetFileName(),
                 bh.version, PACK_VERSION);
        return false;
    }
    if (bh.stays_len < 0 || bh.diagnoses_len < 0 || bh.procedures_len < 0)
        goto corrupt_error;

    if (bh.stays_len > (K_SIZE_MAX - start_stays_len)) {
        LogError("Too much data to load in '%1'", st->GetFileName());
        return false;
    }

    set.stays.Grow((Size)bh.stays_len);
    if (st->ReadFill(K_SIZE(*set.stays.ptr) * (Size)bh.stays_len,
                     set.stays.end()) != K_SIZE(*set.stays.ptr) * (Size)bh.stays_len)
        goto corrupt_error;
    set.stays.len += (Size)bh.stays_len;

    other_diagnoses.Reserve((Size)bh.diagnoses_len);
    if (st->ReadFill(K_SIZE(*other_diagnoses.ptr) * (Size)bh.diagnoses_len,
                     other_diagnoses.ptr) != K_SIZE(*other_diagnoses.ptr) * (Size)bh.diagnoses_len)
        goto corrupt_error;
    other_diagnoses.len += (Size)bh.diagnoses_len;

    procedures.Grow((Size)bh.procedures_len);
    if (st->ReadFill(K_SIZE(*procedures.ptr) * (Size)bh.procedures_len,
                     procedures.ptr) != K_SIZE(*procedures.ptr) * (Size)bh.procedures_len)
        goto corrupt_error;
    procedures.len += (Size)bh.procedures_len;

    // Fix Stay diagnosis and procedure pointers
    {
        Size diagnoses_offset = 0;
        Size procedures_offset = 0;

        for (Size i = set.stays.len - (Size)bh.stays_len; i < set.stays.len; i++) {
            mco_Stay *stay = &set.stays[i];

#if K_SIZE_MAX < INT64_MAX
            union {
                uint8_t raw[32];
                struct {
                    int64_t _pad1;
                    int64_t other_diagnoses_len;
                    int64_t _pad2;
                    int64_t procedures_len;
                } st;
            } u;
            MemCpy(u.raw, &stay->other_diagnoses, 32);
            stay->other_diagnoses.len = (Size)u.st.other_diagnoses_len;
            stay->procedures.len = (Size)u.st.procedures_len;
#endif

            if (stay->other_diagnoses.len) {
                if (stay->other_diagnoses.len < 0) [[unlikely]]
                    goto corrupt_error;
                stay->other_diagnoses.ptr = &other_diagnoses[diagnoses_offset];
                diagnoses_offset += stay->other_diagnoses.len;
                if (diagnoses_offset <= 0 || diagnoses_offset > bh.diagnoses_len) [[unlikely]]
                    goto corrupt_error;
            }

            if (stay->procedures.len) {
                if (stay->procedures.len < 0) [[unlikely]]
                    goto corrupt_error;
                stay->procedures.ptr = &procedures[procedures_offset];
                procedures_offset += stay->procedures.len;
                if (procedures_offset <= 0 || procedures_offset > bh.procedures_len) [[unlikely]]
                    goto corrupt_error;
            }
        }
    }

    other_diagnoses.Leak();
    procedures.Leak();

    // We assume stays are already sorted in pak files

    set_guard.Disable();
    return true;

corrupt_error:
    LogError("Stay pack file '%1' appears to be corrupt or truncated", st->GetFileName());
    return false;
}

static bool ParsePmsiChar(char c, char *out_value)
{
    if (c == ' ')
        return true;
    if (IsAsciiControl(c) || (unsigned int)c >= 128)
        return false;

    *out_value = c;
    return true;
}

template <typename T>
static bool ParsePmsiInt(Span<const char> str, T *out_value)
{
    K_ASSERT(str.len > 0);

    if (str[0] == ' ')
        return true;
    if ((unsigned int)(str[0] - '0') > 9) [[unlikely]]
        return false;

    T value;
    if (ParseInt<T>(str, &value, 0, &str)) [[likely]] {
        *out_value = value;
        return true;
    } else {
        return (!str.len || str[0] == ' ');
    }
}

static bool ParsePmsiDate(Span<const char> str, LocalDate *out_date)
{
    K_ASSERT(str.len == 8);

    if (str[0] == ' ')
        return true;
    if (!IsAsciiDigit(str[0]) || !IsAsciiDigit(str[1]) || !IsAsciiDigit(str[2]) ||
            !IsAsciiDigit(str[3]) || !IsAsciiDigit(str[4]) || !IsAsciiDigit(str[5]) ||
            !IsAsciiDigit(str[6]) || !IsAsciiDigit(str[7])) [[unlikely]]
        return false;

    LocalDate date;
    date.st.day = (int8_t)((str[0] - '0') * 10 + (str[1] - '0'));
    date.st.month = (int8_t)((str[2] - '0') * 10 + (str[3] - '0'));
    date.st.year = (int16_t)((str[4] - '0') * 1000 + (str[5] - '0') * 100 +
                             (str[6] - '0') * 10 + (str[7] - '0'));

    *out_date = date;
    return true;
}

static bool ParsePmsiFlag(char c, int flag1, int flag2, uint32_t *out_flags)
{
    switch (c) {
        case '1': { *out_flags |= flag1; } break;
        case '2': { *out_flags |= flag2; } break;
        case ' ': {} break;

        default: { return false; } break;
    }

    return true;
}

bool mco_StaySetBuilder::ParseRssLine(Span<const char> line, HashTable<int32_t, mco_Test> *out_tests)
{
    if (line.len < 12) [[unlikely]] {
        LogError("Truncated RUM line");
        return false;
    }

    mco_Stay stay = {};
    int das_count = -1;
    int dad_count = -1;
    int procedures_count = -1;

    // Declaring (simple) lambdas inside loops does not seem to impact performance
    Size offset = 9;
    const auto read_fragment = [&](Size len) {
        Span<const char> frag = line.Take(offset, len);
        offset += len;
        return frag;
    };
    const auto set_error_flag = [&stay](mco_Stay::Error flag) {
        stay.errors |= (uint32_t)flag;
        return true;
    };

    bool tests = false;
    int16_t version = 0;
    ParsePmsiInt(read_fragment(3), &version);
    if (version > 100) {
        tests = true;
        version = (int16_t)(version - 100);
        offset += 15;
    }
    if (version < 16 || version > 20) [[unlikely]] {
        stay.errors |= (int)mco_Stay::Error::UnknownRumVersion;
        set.stays.Append(stay);
        return true;
    }
    if (line.len < offset + 165) [[unlikely]] {
        LogError("Truncated RUM line");
        return false;
    }

    ParsePmsiInt(read_fragment(20), &stay.bill_id) || set_error_flag(mco_Stay::Error::MalformedBillId);
    ParsePmsiInt(read_fragment(20), &stay.admin_id);
    offset += 10; // Skip RUM id
    ParsePmsiDate(read_fragment(8), &stay.birthdate) || set_error_flag(mco_Stay::Error::MalformedBirthdate);
    ParsePmsiInt(read_fragment(1), &stay.sex) || set_error_flag(mco_Stay::Error::MalformedSex);
    ParsePmsiInt(read_fragment(4), &stay.unit.number);
    ParsePmsiInt(read_fragment(2), &stay.bed_authorization);
    ParsePmsiDate(read_fragment(8), &stay.entry.date) || set_error_flag(mco_Stay::Error::MalformedEntryDate);
    ParsePmsiChar(line[offset++], &stay.entry.mode);
    ParsePmsiChar(line[offset++], &stay.entry.origin);
    ParsePmsiDate(read_fragment(8), &stay.exit.date) || set_error_flag(mco_Stay::Error::MalformedExitDate);
    ParsePmsiChar(line[offset++], &stay.exit.mode);
    ParsePmsiChar(line[offset++], &stay.exit.destination);
    offset += 5; // Skip postal code
    ParsePmsiInt(read_fragment(4), &stay.newborn_weight) || set_error_flag(mco_Stay::Error::MalformedNewbornWeight);
    ParsePmsiInt(read_fragment(2), &stay.gestational_age) || set_error_flag(mco_Stay::Error::MalformedGestationalAge);
    ParsePmsiDate(read_fragment(8), &stay.last_menstrual_period) || set_error_flag(mco_Stay::Error::MalformedLastMenstrualPeriod);
    ParsePmsiInt(read_fragment(2), &stay.session_count) || set_error_flag(mco_Stay::Error::MalformedSessionCount);
    if (line[offset] != ' ') [[likely]] {
        ParsePmsiInt(line.Take(offset, 2), &das_count) ||
            set_error_flag(mco_Stay::Error::MalformedOtherDiagnosesCount);
    } else {
        set_error_flag(mco_Stay::Error::MissingOtherDiagnosesCount);
    }
    offset += 2;
    if (line[offset] != ' ') [[likely]] {
        ParsePmsiInt(line.Take(offset, 2), &dad_count) ||
            set_error_flag(mco_Stay::Error::MalformedOtherDiagnosesCount);
    } else {
        set_error_flag(mco_Stay::Error::MissingOtherDiagnosesCount);
    }
    offset += 2;
    if (line[offset] != ' ') [[likely]] {
        ParsePmsiInt(line.Take(offset, 3), &procedures_count) ||
            set_error_flag(mco_Stay::Error::MalformedProceduresCount);
    } else {
        set_error_flag(mco_Stay::Error::MissingProceduresCount);
    }
    offset += 3;
    if (line[offset] != ' ') [[likely]] {
        stay.main_diagnosis =
            drd_DiagnosisCode::Parse(line.Take(offset, 8), (int)ParseFlag::End);
        if (!stay.main_diagnosis.IsValid()) [[likely]] {
            stay.errors |= (int)mco_Stay::Error::MalformedMainDiagnosis;
        }
    }
    offset += 8;
    if (line[offset] != ' ') {
        stay.linked_diagnosis =
            drd_DiagnosisCode::Parse(line.Take(offset, 8), (int)ParseFlag::End);
        if (!stay.linked_diagnosis.IsValid()) [[likely]] {
            stay.errors |= (int)mco_Stay::Error::MalformedLinkedDiagnosis;
        }
    }
    offset += 8;
    ParsePmsiInt(read_fragment(3), &stay.igs2) || set_error_flag(mco_Stay::Error::MalformedIgs2);
    ParsePmsiFlag(line[offset++], (int)mco_Stay::Flag::Confirmed, 0, &stay.flags) || set_error_flag(mco_Stay::Error::MalformedConfirmation);
    offset += 17; // Skip a bunch of fields
    if (version >= 19) {
        ParsePmsiFlag(line[offset++], (int)mco_Stay::Flag::Conversion, (int)mco_Stay::Flag::NoConversion, &stay.flags) ||
            set_error_flag(mco_Stay::Error::MalformedConversion);
        ParsePmsiFlag(line[offset++], (int)mco_Stay::Flag::RAAC, 0, &stay.flags) || set_error_flag(mco_Stay::Error::MalformedRAAC);

        if (version >= 20) {
            ParsePmsiFlag(line[offset++], (int)mco_Stay::Flag::Context, 0, &stay.flags) || set_error_flag(mco_Stay::Error::MalformedContext);
            ParsePmsiFlag(line[offset++], (int)mco_Stay::Flag::HospitalUse, 0, &stay.flags) || set_error_flag(mco_Stay::Error::MalformedHospitalUse);
            ParsePmsiFlag(line[offset++], (int)mco_Stay::Flag::Rescript, 0, &stay.flags) || set_error_flag(mco_Stay::Error::MalformedRescript);
            ParsePmsiChar(line[offset++], &stay.interv_category);

            offset += 9; // Skip a bunch of fields
        } else {
            offset += 13; // Skip a bunch of fields
        }
    } else {
        offset += 15; // Skip a bunch of fields
    }

    HeapArray<drd_DiagnosisCode> other_diagnoses(&other_diagnoses_alloc);
    HeapArray<mco_ProcedureRealisation> procedures(&procedures_alloc);
    if (das_count >= 0 && dad_count >=0 && procedures_count >= 0) [[likely]] {
        if (line.len < offset + 8 * das_count + 8 * dad_count +
                       (version >= 17 ? 29 : 26) * procedures_count) [[unlikely]] {
            LogError("Truncated RUM line");
            return false;
        }

        for (int i = 0; i < das_count; i++) {
            drd_DiagnosisCode diag =
                drd_DiagnosisCode::Parse(read_fragment(8), (int)ParseFlag::End);
            if (diag.IsValid()) [[likely]] {
                other_diagnoses.Append(diag);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedOtherDiagnosis;
            }
        }
        offset += 8 * dad_count; // Skip documentary diagnoses

        for (int i = 0; i < procedures_count; i++) {
            mco_ProcedureRealisation proc = {};

            ParsePmsiDate(read_fragment(8), &proc.date);
            proc.proc = drd_ProcedureCode::Parse(read_fragment(7), (int)ParseFlag::End);
            if (version >= 17) {
                if (line[offset] != ' ') {
                    if (line[offset] != '-' ||
                            !ParsePmsiInt(line.Take(offset + 1, 2), &proc.extension)) [[unlikely]] {
                        set_error_flag(mco_Stay::Error::MalformedProcedureExtension);
                    }
                }
                offset += 3;
            }
            ParsePmsiInt(read_fragment(1), &proc.phase);
            ParsePmsiInt(read_fragment(1), &proc.activity);
            if (line[offset] != ' ') {
                proc.doc = UpperAscii(line[offset]);
            }
            offset += 1;
            offset += 6; // Skip modifiers, etc.
            ParsePmsiInt(read_fragment(2), &proc.count);

            if (proc.proc.IsValid()) [[likely]] {
                procedures.Append(proc);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedProcedureCode;
            }
        }

        stay.other_diagnoses = other_diagnoses.TrimAndLeak();
        stay.procedures = procedures.TrimAndLeak();
    }

    if (out_tests && tests) {
        mco_Test test = {};

        bool valid = true;
        test.bill_id = stay.bill_id;
        test.ghm = mco_GhmCode::Parse(line.Take(2, 6));
        valid &= test.ghm.IsValid();
        valid &= ParsePmsiInt(line.Take(12, 3), &test.error);

        if (valid) {
            mco_Test *ptr = out_tests->TrySet(test);
            ptr->cluster_len++;
        } else {
            mco_Test *ptr = out_tests->Find(test.bill_id);
            if (ptr) {
                ptr->cluster_len++;
            }
        }
    }

    set.stays.Append(stay);
    return true;
}

bool mco_StaySetBuilder::ParseRsaLine(Span<const char> line, HashTable<int32_t, mco_Test> *out_tests)
{
    if (line.len < 12) [[unlikely]] {
        LogError("Truncated RSA line");
        return false;
    }

    mco_Stay rsa = {};
    mco_Test test = {};
    int age = 0;
    int age_days = 0;
    int global_auth_count = 0;
    int radiotherapy_supp_count = 0;

    // Declaring (simple) lambdas inside loops does not seem to impact performance
    Size offset = 9;
    const auto read_fragment = [&](Size len) {
        Span<const char> frag = line.Take(offset, len);
        offset += len;
        return frag;
    };
    const auto set_error_flag = [&rsa](mco_Stay::Error flag) {
        rsa.errors |= (uint32_t)flag;
        return true;
    };

    int16_t version = 0;
    ParsePmsiInt(read_fragment(3), &version);
    if (version < 220 || version > 225) [[unlikely]] {
        set_error_flag(mco_Stay::Error::UnknownRumVersion);
        set.stays.Append(rsa);
        return true;
    }
    if (line.len < (version >= 222 ? 174 : 182)) [[unlikely]] {
        LogError("Truncated RSA line");
        return false;
    }

    ParsePmsiInt(read_fragment(10), &rsa.bill_id) || set_error_flag(mco_Stay::Error::MalformedBillId);
    rsa.admin_id = rsa.bill_id;
    test.bill_id = rsa.bill_id;
    offset += 19; // Skip more version info, first GHM
    test.ghm = mco_GhmCode::Parse(read_fragment(6));
    ParsePmsiInt(read_fragment(3), &test.error);
    ParsePmsiInt(read_fragment(2), &test.cluster_len);
    ParsePmsiInt(read_fragment(3), &age) || set_error_flag(mco_Stay::Error::MalformedBirthdate);
    ParsePmsiInt(read_fragment(3), &age_days) || set_error_flag(mco_Stay::Error::MalformedBirthdate);
    ParsePmsiInt(read_fragment(1), &rsa.sex) || set_error_flag(mco_Stay::Error::MalformedSex);
    ParsePmsiChar(line[offset++], &rsa.entry.mode);
    ParsePmsiChar(line[offset++], &rsa.entry.origin);
    {
        bool valid = true;
        valid &= ParsePmsiInt(read_fragment(2), &rsa.exit.date.st.month);
        valid &= ParsePmsiInt(read_fragment(4), &rsa.exit.date.st.year);
        if (!valid) [[unlikely]] {
            set_error_flag(mco_Stay::Error::MalformedExitDate);
        }
        rsa.exit.date.st.day = 1;
    }
    ParsePmsiChar(line[offset++], &rsa.exit.mode);
    ParsePmsiChar(line[offset++], &rsa.exit.destination);
    offset += 1; // Skip stay type
    {
        int duration = 0;
        if (ParsePmsiInt(read_fragment(4), &duration) && rsa.exit.date.IsValid()) {
            rsa.entry.date = rsa.exit.date - duration;
            if (age) {
                rsa.birthdate = LocalDate((int16_t)(rsa.entry.date.st.year - age), 1, 1);
            } else {
                rsa.birthdate = rsa.entry.date - age_days;
            }
        } else {
            set_error_flag(mco_Stay::Error::MalformedEntryDate);
        }
    }
    offset += 5; // Skip geography code
    ParsePmsiInt(read_fragment(4), &rsa.newborn_weight) || set_error_flag(mco_Stay::Error::MalformedNewbornWeight);
    ParsePmsiInt(read_fragment(2), &rsa.gestational_age) || set_error_flag(mco_Stay::Error::MalformedGestationalAge);
    if (line[offset] != ' ') {
        int last_period_delay;
        if (ParsePmsiInt(line.Take(offset, 3), &last_period_delay) && rsa.entry.date.IsValid()) {
            rsa.last_menstrual_period = rsa.entry.date - last_period_delay;
        } else {
            set_error_flag(mco_Stay::Error::MalformedLastMenstrualPeriod);
        }
    }
    offset += 3;
    ParsePmsiInt(read_fragment(2), &rsa.session_count) || set_error_flag(mco_Stay::Error::MalformedSessionCount);
    if (line[offset] == ' ' && line[offset + 1] == 'D') {
        offset += 2;
        ParsePmsiInt(read_fragment(2), &test.ghs.number);
        test.ghs.number += 20000;
    } else {
        ParsePmsiInt(read_fragment(4), &test.ghs.number);
    }
    {
        int exh = 0;
        int exb = 0;
        ParsePmsiInt(read_fragment(4), &exh);
        offset += 1;
        ParsePmsiInt(read_fragment(2), &exb);
        test.exb_exh = exh - exb;
    }
    offset += 6; // Skip dialysis, UHCD
    if (line[offset] == '1') {
        rsa.flags |= (int)mco_Stay::Flag::Confirmed;
    } else if (line[offset] != ' ') [[unlikely]] {
        set_error_flag(mco_Stay::Error::MalformedConfirmation);
    }
    offset++;
    ParsePmsiInt(read_fragment(1), &global_auth_count);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.dia);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.ent1);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.ent2);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.ent3);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.aph);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.rap);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.ant);
    ParsePmsiInt(read_fragment(1), &radiotherapy_supp_count);
    offset += (version >= 222) ? 14 : 22;
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.ohb);
    offset++; // Skip prestation type
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.rea);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.reasi);
    {
        int stf = 0;
        ParsePmsiInt(read_fragment(3), &stf);
        test.supplement_days.st.si = (int16_t)(stf - test.supplement_days.st.reasi);
    }
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.src);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.nn1);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.nn2);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.nn3);
    ParsePmsiInt(read_fragment(3), &test.supplement_days.st.rep);
    if (line[offset++] > '0') {
        rsa.bed_authorization = 8;
    }

    if (version >= 225) {
        offset += 17;
        ParsePmsiInt(read_fragment(1), &test.supplement_days.st.sdc);
        switch (line[offset++]) {
            case '1': { rsa.flags |= (int)mco_Stay::Flag::Conversion; } break;
            case '2': { rsa.flags |= (int)mco_Stay::Flag::NoConversion; } break;
            case ' ': {} break;
            default: { set_error_flag(mco_Stay::Error::MalformedConversion); } break;
        }
        switch (line[offset++]) {
            case '1': { rsa.flags |= (int)mco_Stay::Flag::RAAC; } break;
            case '2':
            case '0':
            case ' ': {} break;
            default: { set_error_flag(mco_Stay::Error::MalformedRAAC); } break;
        }
        offset += 44;
    } else if (version >= 223) {
        offset += 17;
        ParsePmsiInt(read_fragment(1), &test.supplement_days.st.sdc);
        offset += 46;
    } else if (version >= 222) {
        offset += 49;
    } else {
        offset += 41;
    }
    offset += 2 * global_auth_count;
    offset += 7 * radiotherapy_supp_count;

    if (offset + test.cluster_len * (version >= 221 ? 60 : 58) > line.len) [[unlikely]] {
        LogError("Truncated RSA line");
        return false;
    }

    Size das_count = 0;
    Size procedures_count = 0;
    for (Size i = 0; i < test.cluster_len; i++) {
        mco_Stay stay = rsa;

        offset += 14; // Skip many fields
        if (line[offset] != ' ') [[likely]] {
            stay.main_diagnosis =
                drd_DiagnosisCode::Parse(line.Take(offset, 6), (int)ParseFlag::End);
            if (!stay.main_diagnosis.IsValid()) [[unlikely]] {
                stay.errors |= (int)mco_Stay::Error::MalformedMainDiagnosis;
            }
        }
        offset += 6;
        if (line[offset] != ' ') {
            stay.linked_diagnosis =
                drd_DiagnosisCode::Parse(line.Take(offset, 6), (int)ParseFlag::End);
            if (!stay.linked_diagnosis.IsValid()) [[unlikely]] {
                stay.errors |= (int)mco_Stay::Error::MalformedLinkedDiagnosis;
            }
        }
        offset += 6;
        ParsePmsiInt(read_fragment(3), &stay.igs2);
        if (version >= 221) {
            ParsePmsiInt(read_fragment(2), &stay.gestational_age);
        }
        ParsePmsiInt(read_fragment(2), &stay.other_diagnoses.len);
        ParsePmsiInt(read_fragment(3), &stay.procedures.len);
        if (i) {
            stay.entry.date = set.stays[set.stays.len - 1].exit.date;
            stay.entry.mode = '6';
            stay.entry.origin = '1';
        }
        {
            int duration;
            if (ParsePmsiInt(read_fragment(4), &duration)) {
                stay.exit.date = stay.entry.date + duration;
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedExitDate;
            }
        }
        if (i < test.cluster_len - 1) {
            stay.exit.mode = '6';
            stay.exit.destination = '1';
        }
        ParsePmsiInt(read_fragment(2), &stay.unit.number);
        stay.unit.number = (int16_t)(stay.unit.number + 10000);
        offset += 1; // Skip end of UM type (A/B)
        switch (line[offset++]) {
            case 'C': {} break;
            case 'P': {
                stay.unit.number = (int16_t)(stay.unit.number + 1000);
                // This will prevent error 152 from popping up on all conversions
                stay.flags &= ~(uint32_t)mco_Stay::Flag::Conversion;
            } break;
            case 'M': { stay.unit.number = (int16_t)(stay.unit.number + 2000); } break;
        }

        if (i < K_LEN(test.auth_supplements)) [[likely]] {
            int type = 0;
            ParsePmsiInt(read_fragment(2), &type);
            ParsePmsiInt(read_fragment(4), &test.auth_supplements[i].days);
            if (!test.auth_supplements[i].days) {
                type = 0;
            }

            switch (type) {
                case 0: {
                    test.auth_supplements[i].type = 0;
                    test.auth_supplements[i].days = 0;
                } break;

                case 1: { test.auth_supplements[i].type = (int8_t)mco_SupplementType::Rea; } break;
                case 2: {
                    if (stay.unit.number == 10002 || stay.unit.number == 10016 ||
                            stay.unit.number == 10018) {
                        test.auth_supplements[i].type = (int8_t)mco_SupplementType::Si;
                    } else {
                        test.auth_supplements[i].type = (int8_t)mco_SupplementType::Reasi;
                    }
                } break;
                case 3: { test.auth_supplements[i].type = (int8_t)mco_SupplementType::Src; } break;
                case 4: { test.auth_supplements[i].type = (int8_t)mco_SupplementType::Nn1; } break;
                case 5: { test.auth_supplements[i].type = (int8_t)mco_SupplementType::Nn2; } break;
                case 6: { test.auth_supplements[i].type = (int8_t)mco_SupplementType::Nn3; } break;
                case 13: { test.auth_supplements[i].type = (int8_t)mco_SupplementType::Rep; } break;

                default: {
                    LogError("Unrecognized supplement type %1", type);
                    test.auth_supplements[i].type = 0;
                    test.auth_supplements[i].days = 0;
                } break;
            }

            offset += 10; // Skip many fields
        } else {
            offset += 16;
        }

        set.stays.Append(stay);

        stay.entry.date = stay.exit.date;
        das_count += stay.other_diagnoses.len;
        procedures_count += stay.procedures.len;
    }

    if (offset + das_count * 6 +
                 procedures_count * (version >= 222 ? 24 : 22) > line.len) [[unlikely]] {
        LogError("Truncated RSA line");
        return false;
    }

    for (Size i = set.stays.len - test.cluster_len; i < set.stays.len; i++) {
        mco_Stay &stay = set.stays[i];

        HeapArray<drd_DiagnosisCode> other_diagnoses(&other_diagnoses_alloc);
        for (Size j = 0; j < stay.other_diagnoses.len; j++) {
            drd_DiagnosisCode diag =
                drd_DiagnosisCode::Parse(read_fragment(6), (int)ParseFlag::End);
            if (diag.IsValid()) [[likely]] {
                other_diagnoses.Append(diag);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedOtherDiagnosis;
            }
        }

        stay.other_diagnoses = other_diagnoses.TrimAndLeak();
    }

    for (Size i = set.stays.len - test.cluster_len; i < set.stays.len; i++) {
        mco_Stay &stay = set.stays[i];

        HeapArray<mco_ProcedureRealisation> procedures(&procedures_alloc);
        for (Size j = 0; j < stay.procedures.len; j++) {
            mco_ProcedureRealisation proc = {};

            {
                int proc_delay;
                if (ParsePmsiInt(read_fragment(3), &proc_delay)) {
                    proc.date = rsa.entry.date + proc_delay;
                }
            }
            proc.proc = drd_ProcedureCode::Parse(read_fragment(7), (int)ParseFlag::End);
            if (version >= 222) {
                if (line[offset] != ' ') {
                    ParsePmsiInt(line.Take(offset, 2), &proc.extension) ||
                        set_error_flag(mco_Stay::Error::MalformedProcedureExtension);
                }
                offset += 2;
            }
            ParsePmsiInt(read_fragment(1), &proc.phase);
            ParsePmsiInt(read_fragment(1), &proc.activity);
            ParsePmsiChar(line[offset++], &proc.doc);
            offset += 6; // Skip modifiers, doc extension, etc.
            ParsePmsiInt(read_fragment(2), &proc.count);
            offset += 1; // Skip date compatibility flag

            if (proc.proc.IsValid()) [[likely]] {
                procedures.Append(proc);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedProcedureCode;
            }
        }

        stay.procedures = procedures.TrimAndLeak();
    }

    if (out_tests) {
        out_tests->Set(test);
    }

    return true;
}

bool mco_StaySetBuilder::LoadAtih(StreamReader *st,
                                  bool (mco_StaySetBuilder::*parse_func)(Span<const char> line,
                                                                         HashTable<int32_t, mco_Test> *out_tests),
                                  HashTable<int32_t, mco_Test> *out_tests)
{
    Size stays_len = set.stays.len;
    K_DEFER_N(set_guard) { set.stays.RemoveFrom(stays_len); };

    Size errors = 0;
    {
        LineReader reader(st);

        reader.PushLogFilter();
        K_DEFER { PopLogFilter(); };

        Span<const char> line;
        while (reader.Next(&line)) {
            errors += !(this->*parse_func)(line, out_tests);
        }
        if (!reader.IsValid())
            return false;
    }
    if (errors && set.stays.len == stays_len)
        return false;

    std::stable_sort(set.stays.begin() + stays_len, set.stays.end(),
                     [](const mco_Stay &stay1, const mco_Stay &stay2) {
        return MultiCmp(stay1.admin_id - stay2.admin_id,
                        stay1.bill_id - stay2.bill_id) < 0;
    });

    set_guard.Disable();
    return true;
}

bool mco_StaySetBuilder::LoadRss(StreamReader *st, HashTable<int32_t, mco_Test> *out_tests)
{
    return LoadAtih(st, &mco_StaySetBuilder::ParseRssLine, out_tests);
}

bool mco_StaySetBuilder::LoadRsa(StreamReader *st, HashTable<int32_t, mco_Test> *out_tests)
{
    static std::atomic_flag gave_rsa_warning = ATOMIC_FLAG_INIT;
    if (!gave_rsa_warning.test_and_set()) {
        LogError("RSA files contain partial information that can lead to errors"
                 " (such as procedure date errors)");
    }

    return LoadAtih(st, &mco_StaySetBuilder::ParseRsaLine, out_tests);
}

bool mco_StaySetBuilder::LoadFichComp(StreamReader *st, HashTable<int32_t, mco_Test> *)
{
    Size lines = 0;
    Size errors = 0;
    {
        LineReader reader(st);
        reader.PushLogFilter();
        K_DEFER { PopLogFilter(); };

        Span<const char> line;
        while (reader.Next(&line)) {
            lines++;

            if (line.len < 92) {
                LogError("Truncated FICHCOMP line");
                errors++;
                continue;
            }

            int type = 0;
            ParsePmsiInt(line.Take(9, 2), &type);

            switch (type) {
                case 6:
                case 9:
                case 10: {
                    FichCompData fc = {};
                    bool valid = true;

                    fc.type = FichCompData::Type::UCD;
                    valid &= ParsePmsiInt(line.Take(11, 20), &fc.admin_id) && fc.admin_id;
                    valid &= ParsePmsiDate(line.Take(31, 8), &fc.start_date) && fc.start_date.value;

                    if (valid) [[likely]] {
                        fichcomps.Append(fc);
                    } else {
                        LogError("Malformed DIP (FICHCOMP) line");
                        errors++;
                    }
                } break;

                case 7: {
                    FichCompData fc = {};
                    bool valid = true;

                    fc.type = FichCompData::Type::DIP;
                    valid &= ParsePmsiInt(line.Take(11, 20), &fc.admin_id) && fc.admin_id;
                    valid &= ParsePmsiDate(line.Take(41, 8), &fc.start_date) && fc.start_date.value;
                    valid &= ParsePmsiDate(line.Take(49, 8), &fc.end_date) && fc.end_date.value;
                    valid &= ParsePmsiInt(line.Take(72, 10), &fc.count) && fc.count;
                    valid &= (line.Take(57, 15) == "            DIP");

                    if (valid) [[likely]] {
                        fichcomps.Append(fc);
                    } else {
                        LogError("Malformed MED (FICHCOMP) line");
                        errors++;
                    }
                } break;

                case 2:
                case 3:
                case 4:
                case 99: {} break;

                default: {
                    LogError("Unknown or invalid FICHCOMP type %1", type);
                    errors++;
                } break;
            }
        }
        if (!reader.IsValid())
            return false;
    }
    if (errors && errors == lines)
        return false;

    return true;
}

bool mco_StaySetBuilder::LoadFiles(Span<const char *const> filenames,
                                   HashTable<int32_t, mco_Test> *out_tests)
{
    bool success = true;

    for (const char *filename: filenames) {
        CompressionType compression_type;
        Span<const char> extension = GetPathExtension(filename, &compression_type);

        bool (mco_StaySetBuilder::*load_func)(StreamReader *st,
                                              HashTable<int32_t, mco_Test> *out_tests);
        if (extension == ".dmpak") {
            load_func = &mco_StaySetBuilder::LoadPack;
        } else if (extension == ".grp" || extension == ".rss") {
            load_func = &mco_StaySetBuilder::LoadRss;
        } else if (extension == ".rsa") {
            load_func = &mco_StaySetBuilder::LoadRsa;
        } else if (extension == ".txt") {
            load_func = &mco_StaySetBuilder::LoadFichComp;
        } else {
            LogError("Cannot load stays from file '%1' with unknown extension '%2'",
                     filename, extension);
            success = false;
            continue;
        }

        StreamReader st(filename, compression_type);
        if (!st.IsValid()) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(&st, out_tests);
    }

    return success;
}

bool mco_StaySetBuilder::Finish(mco_StaySet *out_set)
{
    // Build FICHCOMP map
    HashMap<int32_t, const FichCompData *> fichcomps_map;
    std::sort(fichcomps.begin(), fichcomps.end(),
              [](const FichCompData &fc1, const FichCompData &fc2) {
        return fc1.admin_id < fc2.admin_id;
    });
    for (const FichCompData &fc: fichcomps) {
        fichcomps_map.TrySet(fc.admin_id, &fc);
    }

    // Add FICHCOMP data to stays
    HashSet<Size> matched_fichcomps;
    {
        Span<mco_Stay> stays2 = set.stays;
        while (stays2.len) {
            Span<mco_Stay> sub_stays = mco_Split(stays2, 1, &stays2);

            const FichCompData *fc = fichcomps_map.FindValue(sub_stays[0].admin_id, nullptr);
            while (fc && fc < fichcomps.end() && fc->admin_id == sub_stays[0].admin_id) {
                if (fc->start_date >= sub_stays[0].entry.date &&
                        (!fc->end_date.value || fc->end_date <= sub_stays[sub_stays.len - 1].exit.date)) {
                    switch (fc->type) {
                        case FichCompData::Type::UCD: {
                            sub_stays[0].flags |= (int)mco_Stay::Flag::UCD;
                        } break;

                        case FichCompData::Type::DIP: {
                            if (sub_stays[0].dip_count) [[unlikely]] {
                                LogError("Overwriting DIP count for stay %1", sub_stays[0].bill_id);
                            }
                            sub_stays[0].dip_count = fc->count;
                        } break;
                    }

                    matched_fichcomps.Set(fc - fichcomps.ptr);
                }

                fc++;
            }
        }
    }
    if (matched_fichcomps.table.count < fichcomps.len) {
        LogError("Some FICHCOMP entries (%1) have no matching stay",
                 fichcomps.len - matched_fichcomps.table.count);
    }

    other_diagnoses_alloc.GiveTo(&set.array_alloc);
    procedures_alloc.GiveTo(&set.array_alloc);
    set.stays.Trim();

    std::swap(*out_set, set);

    return true;
}

}
