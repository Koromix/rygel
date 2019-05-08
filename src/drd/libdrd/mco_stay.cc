// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libcc/libcc.hh"
#include "mco_stay.hh"

namespace RG {

#pragma pack(push, 1)
struct PackHeader {
    char signature[13];
    int8_t version;
    int8_t native_size;
    int8_t endianness;

    int64_t stays_len;
    int64_t diagnoses_len;
    int64_t procedures_len;
};
#pragma pack(pop)
#define PACK_VERSION 16
#define PACK_SIGNATURE "DRD_MCO_PACK"

// This should warn us in most cases when we break dspak files (it's basically a memcpy format)
RG_STATIC_ASSERT(RG_SIZE(PackHeader::signature) == RG_SIZE(PACK_SIGNATURE));
RG_STATIC_ASSERT(RG_SIZE(mco_Stay) == 112);
RG_STATIC_ASSERT(RG_SIZE(drd_DiagnosisCode) == 8);
RG_STATIC_ASSERT(RG_SIZE(mco_ProcedureRealisation) == 24);

bool mco_StaySet::SavePack(StreamWriter &st) const
{
    PackHeader bh = {};

    strcpy(bh.signature, PACK_SIGNATURE);
    bh.version = PACK_VERSION;
    bh.native_size = (uint8_t)RG_SIZE(Size);
    bh.endianness = (int8_t)RG_ARCH_ENDIANNESS;
    bh.stays_len = stays.len;
    for (const mco_Stay &stay: stays) {
        bh.diagnoses_len += stay.other_diagnoses.len;
        bh.procedures_len += stay.procedures.len;
    }

    st.Write(&bh, RG_SIZE(bh));
#ifdef RG_ARCH_64
    st.Write(stays.ptr, stays.len * RG_SIZE(*stays.ptr));
#else
    for (const mco_Stay &stay: stays) {
        mco_Stay stay2;
        memcpy(&stay2, &stay, SIZE(stay));

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
        memcpy(&stay2.other_diagnoses, u.raw, 32);

        st.Write(&stay2, SIZE(stay2));
    }
#endif
    for (const mco_Stay &stay: stays) {
        st.Write(stay.other_diagnoses.ptr, stay.other_diagnoses.len * RG_SIZE(*stay.other_diagnoses.ptr));
    }
    for (const mco_Stay &stay: stays) {
        st.Write(stay.procedures.ptr, stay.procedures.len * RG_SIZE(*stay.procedures.ptr));
    }
    if (!st.Close())
        return false;

    return true;
}

bool mco_StaySet::SavePack(const char *filename) const
{
    CompressionType compression_type;
    Span<const char> extension = GetPathExtension(filename, &compression_type);

    if (!TestStr(extension, ".dmpak")) {
        LogError("Unknown packing extension '%1', prefer '.dmpak'", extension);
    }

    StreamWriter st(filename, compression_type);
    return SavePack(st);
}

bool mco_StaySetBuilder::LoadPack(StreamReader &st, HashTable<int32_t, mco_Test> *out_tests)
{
    const Size start_stays_len = set.stays.len;
    RG_DEFER_N(set_guard) { set.stays.RemoveFrom(start_stays_len); };

    if (out_tests) {
        LogError("Testing is not supported by '.dmpak' files");
    }

    HeapArray<drd_DiagnosisCode> other_diagnoses(&other_diagnoses_alloc);
    HeapArray<mco_ProcedureRealisation> procedures(&procedures_alloc);

    PackHeader bh;
    if (st.Read(RG_SIZE(bh), &bh) != RG_SIZE(bh))
        goto corrupt_error;

    if (strncmp(bh.signature, PACK_SIGNATURE, RG_SIZE(bh.signature)) != 0) {
        LogError("File '%1' does not have dspak signature", st.filename);
        return false;
    }
    if (bh.version != PACK_VERSION) {
        LogError("Cannot load '%1' (dspak version %2), expected version %3", st.filename,
                 bh.version, PACK_VERSION);
        return false;
    }
    if (bh.endianness != (int8_t)RG_ARCH_ENDIANNESS) {
        LogError("File '%1' is not compatible with this platform (endianness issue)",
                 st.filename);
        return false;
    }
    if (bh.stays_len < 0 || bh.diagnoses_len < 0 || bh.procedures_len < 0)
        goto corrupt_error;

    if (bh.stays_len > (RG_SIZE_MAX - start_stays_len)) {
        LogError("Too much data to load in '%1'", st.filename);
        return false;
    }

    set.stays.Grow((Size)bh.stays_len);
    if (st.Read(RG_SIZE(*set.stays.ptr) * (Size)bh.stays_len,
                set.stays.end()) != RG_SIZE(*set.stays.ptr) * (Size)bh.stays_len)
        goto corrupt_error;
    set.stays.len += (Size)bh.stays_len;

    other_diagnoses.Reserve((Size)bh.diagnoses_len);
    if (st.Read(RG_SIZE(*other_diagnoses.ptr) * (Size)bh.diagnoses_len,
                other_diagnoses.ptr) != RG_SIZE(*other_diagnoses.ptr) * (Size)bh.diagnoses_len)
        goto corrupt_error;
    other_diagnoses.len += (Size)bh.diagnoses_len;

    procedures.Grow((Size)bh.procedures_len);
    if (st.Read(RG_SIZE(*procedures.ptr) * (Size)bh.procedures_len,
                procedures.ptr) != RG_SIZE(*procedures.ptr) * (Size)bh.procedures_len)
        goto corrupt_error;
    procedures.len += (Size)bh.procedures_len;

    // Fix Stay diagnosis and procedure pointers
    {
        Size diagnoses_offset = 0;
        Size procedures_offset = 0;

        for (Size i = set.stays.len - (Size)bh.stays_len; i < set.stays.len; i++) {
            mco_Stay *stay = &set.stays[i];

#ifndef RG_ARCH_64
            union {
                uint8_t raw[32];
                struct {
                    int64_t _pad1;
                    int64_t other_diagnoses_len;
                    int64_t _pad2;
                    int64_t procedures_len;
                } st;
            } u;
            memcpy(u.raw, &stay->other_diagnoses, 32);
            stay->other_diagnoses.len = (Size)u.st.other_diagnoses_len;
            stay->procedures.len = (Size)u.st.procedures_len;
#endif

            if (stay->other_diagnoses.len) {
                if (RG_UNLIKELY(stay->other_diagnoses.len < 0))
                    goto corrupt_error;
                stay->other_diagnoses.ptr = &other_diagnoses[diagnoses_offset];
                diagnoses_offset += stay->other_diagnoses.len;
                if (RG_UNLIKELY(diagnoses_offset <= 0 || diagnoses_offset > bh.diagnoses_len))
                    goto corrupt_error;
            }

            if (stay->procedures.len) {
                if (RG_UNLIKELY(stay->procedures.len < 0))
                    goto corrupt_error;
                stay->procedures.ptr = &procedures[procedures_offset];
                procedures_offset += stay->procedures.len;
                if (RG_UNLIKELY(procedures_offset <= 0 || procedures_offset > bh.procedures_len))
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
    LogError("Stay pack file '%1' appears to be corrupt or truncated", st.filename);
    return false;
}

static bool ParsePmsiChar(char c, char *out_value)
{
    if (c == ' ')
        return true;
    if (c < 32 || (uint8_t)c > 127)
        return false;

    *out_value = c;
    return true;
}

template <typename T>
static bool ParsePmsiInt(Span<const char> str, T *out_value)
{
    RG_ASSERT_DEBUG(str.len > 0);

    if (str[0] == ' ')
        return true;
    if (RG_UNLIKELY((unsigned int)(str[0] - '0') > 9))
        return false;

    T value;
    if (RG_LIKELY(ParseDec<T>(str, &value, 0, &str))) {
        *out_value = value;
        return true;
    } else {
        return (!str.len || str[0] == ' ');
    }
}

static bool ParsePmsiDate(Span<const char> str, Date *out_date)
{
    RG_ASSERT_DEBUG(str.len == 8);

    if (str[0] == ' ')
        return true;
    if (RG_UNLIKELY(!IsAsciiDigit(str[0]) || !IsAsciiDigit(str[1]) || !IsAsciiDigit(str[2]) ||
                    !IsAsciiDigit(str[3]) || !IsAsciiDigit(str[4]) || !IsAsciiDigit(str[5]) ||
                    !IsAsciiDigit(str[6]) || !IsAsciiDigit(str[7])))
        return false;

    Date date;
    date.st.day = (int8_t)((str[0] - '0') * 10 + (str[1] - '0'));
    date.st.month = (int8_t)((str[2] - '0') * 10 + (str[3] - '0'));
    date.st.year = (int16_t)((str[4] - '0') * 1000 + (str[5] - '0') * 100 +
                             (str[6] - '0') * 10 + (str[7] - '0'));

    *out_date = date;
    return true;
}

bool mco_StaySetBuilder::ParseRssLine(Span<const char> line, HashTable<int32_t, mco_Test> *out_tests)
{
    if (RG_UNLIKELY(line.len < 12)) {
        LogError("Truncated RUM line");
        return false;
    }

    mco_Stay stay = {};
    int das_count = -1;
    int dad_count = -1;
    int procedures_count = -1;

    // Declaring (simple) lambdas inside loops does not seem to impact performance
    Size offset = 9;
    const auto ReadFragment = [&](Size len) {
        Span<const char> frag = line.Take(offset, len);
        offset += len;
        return frag;
    };
    const auto SetErrorFlag = [&stay](mco_Stay::Error flag) {
        stay.errors |= (uint32_t)flag;
        return true;
    };

    bool tests = false;
    int16_t version = 0;
    ParsePmsiInt(ReadFragment(3), &version);
    if (version > 100) {
        tests = true;
        version = (int16_t)(version - 100);
        offset += 15;
    }
    if (RG_UNLIKELY(version < 16 || version > 19)) {
        stay.errors |= (int)mco_Stay::Error::UnknownRumVersion;
        set.stays.Append(stay);
        return true;
    }
    if (RG_UNLIKELY(line.len < offset + 165)) {
        LogError("Truncated RUM line");
        return false;
    }

    ParsePmsiInt(ReadFragment(20), &stay.bill_id) || SetErrorFlag(mco_Stay::Error::MalformedBillId);
    ParsePmsiInt(ReadFragment(20), &stay.admin_id);
    offset += 10; // Skip RUM id
    ParsePmsiDate(ReadFragment(8), &stay.birthdate) || SetErrorFlag(mco_Stay::Error::MalformedBirthdate);
    ParsePmsiInt(ReadFragment(1), &stay.sex) || SetErrorFlag(mco_Stay::Error::MalformedSex);
    ParsePmsiInt(ReadFragment(4), &stay.unit.number);
    ParsePmsiInt(ReadFragment(2), &stay.bed_authorization);
    ParsePmsiDate(ReadFragment(8), &stay.entry.date) || SetErrorFlag(mco_Stay::Error::MalformedEntryDate);
    ParsePmsiChar(line[offset++], &stay.entry.mode);
    ParsePmsiChar(line[offset++], &stay.entry.origin);
    ParsePmsiDate(ReadFragment(8), &stay.exit.date) || SetErrorFlag(mco_Stay::Error::MalformedExitDate);
    ParsePmsiChar(line[offset++], &stay.exit.mode);
    ParsePmsiChar(line[offset++], &stay.exit.destination);
    offset += 5; // Skip postal code
    ParsePmsiInt(ReadFragment(4), &stay.newborn_weight) || SetErrorFlag(mco_Stay::Error::MalformedNewbornWeight);
    ParsePmsiInt(ReadFragment(2), &stay.gestational_age) || SetErrorFlag(mco_Stay::Error::MalformedGestationalAge);
    ParsePmsiDate(ReadFragment(8), &stay.last_menstrual_period) || SetErrorFlag(mco_Stay::Error::MalformedLastMenstrualPeriod);
    ParsePmsiInt(ReadFragment(2), &stay.session_count) || SetErrorFlag(mco_Stay::Error::MalformedSessionCount);
    if (RG_LIKELY(line[offset] != ' ')) {
        ParsePmsiInt(line.Take(offset, 2), &das_count) ||
            SetErrorFlag(mco_Stay::Error::MalformedOtherDiagnosesCount);
    } else {
        SetErrorFlag(mco_Stay::Error::MissingOtherDiagnosesCount);
    }
    offset += 2;
    if (RG_LIKELY(line[offset] != ' ')) {
        ParsePmsiInt(line.Take(offset, 2), &dad_count) ||
            SetErrorFlag(mco_Stay::Error::MalformedOtherDiagnosesCount);
    } else {
        SetErrorFlag(mco_Stay::Error::MissingOtherDiagnosesCount);
    }
    offset += 2;
    if (RG_LIKELY(line[offset] != ' ')) {
        ParsePmsiInt(line.Take(offset, 3), &procedures_count) ||
            SetErrorFlag(mco_Stay::Error::MalformedProceduresCount);
    } else {
        SetErrorFlag(mco_Stay::Error::MissingProceduresCount);
    }
    offset += 3;
    if (RG_LIKELY(line[offset] != ' ')) {
        stay.main_diagnosis =
            drd_DiagnosisCode::FromString(line.Take(offset, 8), (int)ParseFlag::End);
        if (RG_UNLIKELY(!stay.main_diagnosis.IsValid())) {
            stay.errors |= (int)mco_Stay::Error::MalformedMainDiagnosis;
        }
    }
    offset += 8;
    if (line[offset] != ' ') {
        stay.linked_diagnosis =
            drd_DiagnosisCode::FromString(line.Take(offset, 8), (int)ParseFlag::End);
        if (RG_UNLIKELY(!stay.linked_diagnosis.IsValid())) {
            stay.errors |= (int)mco_Stay::Error::MalformedLinkedDiagnosis;
        }
    }
    offset += 8;
    ParsePmsiInt(ReadFragment(3), &stay.igs2) || SetErrorFlag(mco_Stay::Error::MalformedIgs2);
    if (line[offset] == '1') {
        stay.flags |= (int)mco_Stay::Flag::Confirmed;
    } else if (RG_UNLIKELY(line[offset] != ' ')) {
        // According to the GenRSA manual and what the official FG does, confirmation
        // code '2' is supposed to be okay... but why? I don't accept it here.
        stay.errors |= (int)mco_Stay::Error::MalformedConfirmation;
    }
    offset += 18; // Skip a bunch of fields
    if (version >= 19) {
        if (line[offset] == '1') {
            stay.flags |= (int)mco_Stay::Flag::Conversion;
        } else if (line[offset] == '2') {
            stay.flags |= (int)mco_Stay::Flag::NoConversion;
        } else if (RG_UNLIKELY(line[offset] != ' ')) {
            stay.errors |= (int)mco_Stay::Error::MalformedConversion;
        }
        if (line[offset + 1] == '1') {
            stay.flags |= (int)mco_Stay::Flag::RAAC;
        } else if (RG_UNLIKELY(line[offset + 1] != '2' && line[offset + 1] != ' ')) {
            stay.errors |= (int)mco_Stay::Error::MalformedRAAC;
        }
    }
    offset += 15; // Skip a bunch of fields

    HeapArray<drd_DiagnosisCode> other_diagnoses(&other_diagnoses_alloc);
    HeapArray<mco_ProcedureRealisation> procedures(&procedures_alloc);
    if (RG_LIKELY(das_count >= 0 && dad_count >=0 && procedures_count >= 0)) {
        if (RG_UNLIKELY(line.len < offset + 8 * das_count + 8 * dad_count +
                                   (version >= 17 ? 29 : 26) * procedures_count)) {
            LogError("Truncated RUM line");
            return false;
        }

        for (int i = 0; i < das_count; i++) {
            drd_DiagnosisCode diag =
                drd_DiagnosisCode::FromString(ReadFragment(8), (int)ParseFlag::End);
            if (RG_LIKELY(diag.IsValid())) {
                other_diagnoses.Append(diag);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedOtherDiagnosis;
            }
        }
        offset += 8 * dad_count; // Skip documentary diagnoses

        for (int i = 0; i < procedures_count; i++) {
            mco_ProcedureRealisation proc = {};

            ParsePmsiDate(ReadFragment(8), &proc.date);
            proc.proc = drd_ProcedureCode::FromString(ReadFragment(7), (int)ParseFlag::End);
            if (version >= 17) {
                if (line[offset] != ' ') {
                    if (RG_UNLIKELY(line[offset] != '-' ||
                                    !ParsePmsiInt(line.Take(offset + 1, 2), &proc.extension))) {
                        SetErrorFlag(mco_Stay::Error::MalformedProcedureExtension);
                    }
                }
                offset += 3;
            }
            ParsePmsiInt(ReadFragment(1), &proc.phase);
            ParsePmsiInt(ReadFragment(1), &proc.activity);
            if (line[offset] != ' ') {
                proc.doc = UpperAscii(line[offset]);
            }
            offset += 1;
            offset += 6; // Skip modifiers, etc.
            ParsePmsiInt(ReadFragment(2), &proc.count);

            if (RG_LIKELY(proc.proc.IsValid())) {
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
        test.ghm = mco_GhmCode::FromString(line.Take(2, 6));
        valid &= test.ghm.IsValid();
        valid &= ParsePmsiInt(line.Take(12, 3), &test.error);

        if (valid) {
            mco_Test *it = out_tests->Append(test).first;
            it->cluster_len++;
        } else {
            mco_Test *it = out_tests->Find(test.bill_id);
            if (it) {
                it->cluster_len++;
            }
        }
    }

    set.stays.Append(stay);
    return true;
}

bool mco_StaySetBuilder::ParseRsaLine(Span<const char> line, HashTable<int32_t, mco_Test> *out_tests)
{
    if (RG_UNLIKELY(line.len < 12)) {
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
    const auto ReadFragment = [&](Size len) {
        Span<const char> frag = line.Take(offset, len);
        offset += len;
        return frag;
    };
    const auto SetErrorFlag = [&rsa](mco_Stay::Error flag) {
        rsa.errors |= (uint32_t)flag;
        return true;
    };

    int16_t version = 0;
    ParsePmsiInt(ReadFragment(3), &version);
    if (RG_UNLIKELY(version < 220 || version > 224)) {
        SetErrorFlag(mco_Stay::Error::UnknownRumVersion);
        set.stays.Append(rsa);
        return true;
    }
    if (RG_UNLIKELY(line.len < (version >= 222 ? 174 : 182))) {
        LogError("Truncated RSA line");
        return false;
    }

    ParsePmsiInt(ReadFragment(10), &rsa.bill_id) || SetErrorFlag(mco_Stay::Error::MalformedBillId);
    rsa.admin_id = rsa.bill_id;
    test.bill_id = rsa.bill_id;
    offset += 19; // Skip more version info, first GHM
    test.ghm = mco_GhmCode::FromString(ReadFragment(6));
    ParsePmsiInt(ReadFragment(3), &test.error);
    ParsePmsiInt(ReadFragment(2), &test.cluster_len);
    ParsePmsiInt(ReadFragment(3), &age) || SetErrorFlag(mco_Stay::Error::MalformedBirthdate);
    ParsePmsiInt(ReadFragment(3), &age_days) || SetErrorFlag(mco_Stay::Error::MalformedBirthdate);
    ParsePmsiInt(ReadFragment(1), &rsa.sex) || SetErrorFlag(mco_Stay::Error::MalformedSex);
    ParsePmsiChar(line[offset++], &rsa.entry.mode);
    ParsePmsiChar(line[offset++], &rsa.entry.origin);
    {
        bool valid = true;
        valid &= ParsePmsiInt(ReadFragment(2), &rsa.exit.date.st.month);
        valid &= ParsePmsiInt(ReadFragment(4), &rsa.exit.date.st.year);
        if (RG_UNLIKELY(!valid)) {
            SetErrorFlag(mco_Stay::Error::MalformedExitDate);
        }
        rsa.exit.date.st.day = 1;
    }
    ParsePmsiChar(line[offset++], &rsa.exit.mode);
    ParsePmsiChar(line[offset++], &rsa.exit.destination);
    offset += 1; // Skip stay type
    {
        int duration = 0;
        if (ParsePmsiInt(ReadFragment(4), &duration) && rsa.exit.date.IsValid()) {
            rsa.entry.date = rsa.exit.date - duration;
            if (age) {
                rsa.birthdate = Date((int16_t)(rsa.entry.date.st.year - age), 1, 1);
            } else {
                rsa.birthdate = rsa.entry.date - age_days;
            }
        } else {
            SetErrorFlag(mco_Stay::Error::MalformedEntryDate);
        }
    }
    offset += 5; // Skip geography code
    ParsePmsiInt(ReadFragment(4), &rsa.newborn_weight) || SetErrorFlag(mco_Stay::Error::MalformedNewbornWeight);
    ParsePmsiInt(ReadFragment(2), &rsa.gestational_age) || SetErrorFlag(mco_Stay::Error::MalformedGestationalAge);
    if (line[offset] != ' ') {
        int last_period_delay;
        if (ParsePmsiInt(line.Take(offset, 3), &last_period_delay) && rsa.entry.date.IsValid()) {
            rsa.last_menstrual_period = rsa.entry.date - last_period_delay;
        } else {
            SetErrorFlag(mco_Stay::Error::MalformedLastMenstrualPeriod);
        }
    }
    offset += 3;
    ParsePmsiInt(ReadFragment(2), &rsa.session_count) || SetErrorFlag(mco_Stay::Error::MalformedSessionCount);
    if (line[offset] == ' ' && line[offset + 1] == 'D') {
        offset += 2;
        ParsePmsiInt(ReadFragment(2), &test.ghs.number);
        test.ghs.number += 20000;
    } else {
        ParsePmsiInt(ReadFragment(4), &test.ghs.number);
    }
    {
        int exh = 0;
        int exb = 0;
        ParsePmsiInt(ReadFragment(4), &exh);
        offset += 1;
        ParsePmsiInt(ReadFragment(2), &exb);
        test.exb_exh = exh - exb;
    }
    offset += 6; // Skip dialysis, UHCD
    if (line[offset] == '1') {
        rsa.flags |= (int)mco_Stay::Flag::Confirmed;
    } else if (RG_UNLIKELY(line[offset] != ' ')) {
        SetErrorFlag(mco_Stay::Error::MalformedConfirmation);
    }
    offset++;
    ParsePmsiInt(ReadFragment(1), &global_auth_count);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.dia);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.ent1);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.ent2);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.ent3);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.aph);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.rap);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.ant);
    ParsePmsiInt(ReadFragment(1), &radiotherapy_supp_count);
    offset += (version >= 222) ? 14 : 22;
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.ohb);
    offset++; // Skip prestation type
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.rea);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.reasi);
    {
        int stf = 0;
        ParsePmsiInt(ReadFragment(3), &stf);
        test.supplement_days.st.si = (int16_t)(stf - test.supplement_days.st.reasi);
    }
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.src);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.nn1);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.nn2);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.nn3);
    ParsePmsiInt(ReadFragment(3), &test.supplement_days.st.rep);
    if (line[offset++] > '0') {
        rsa.bed_authorization = 8;
    }

    // Skip a whole bunch of stuff we don't care about
    if (version >= 223) {
        offset += 17;
        ParsePmsiInt(ReadFragment(1), &test.supplement_days.st.sdc);
        offset += 46;
    } else if (version >= 222) {
        offset += 49;
    } else {
        offset += 41;
    }
    offset += 2 * global_auth_count;
    offset += 7 * radiotherapy_supp_count;

    if (RG_UNLIKELY(offset + test.cluster_len * (version >= 221 ? 60 : 58) > line.len)) {
        LogError("Truncated RSA line");
        return false;
    }

    Size das_count = 0;
    Size procedures_count = 0;
    for (Size i = 0; i < test.cluster_len; i++) {
        mco_Stay stay = rsa;

        offset += 14; // Skip many fields
        if (RG_LIKELY(line[offset] != ' ')) {
            stay.main_diagnosis =
                drd_DiagnosisCode::FromString(line.Take(offset, 6), (int)ParseFlag::End);
            if (RG_UNLIKELY(!stay.main_diagnosis.IsValid())) {
                stay.errors |= (int)mco_Stay::Error::MalformedMainDiagnosis;
            }
        }
        offset += 6;
        if (line[offset] != ' ') {
            stay.linked_diagnosis =
                drd_DiagnosisCode::FromString(line.Take(offset, 6), (int)ParseFlag::End);
            if (RG_UNLIKELY(!stay.linked_diagnosis.IsValid())) {
                stay.errors |= (int)mco_Stay::Error::MalformedLinkedDiagnosis;
            }
        }
        offset += 6;
        ParsePmsiInt(ReadFragment(3), &stay.igs2);
        if (version >= 221) {
            ParsePmsiInt(ReadFragment(2), &stay.gestational_age);
        }
        ParsePmsiInt(ReadFragment(2), &stay.other_diagnoses.len);
        ParsePmsiInt(ReadFragment(3), &stay.procedures.len);
        if (i) {
            stay.entry.date = set.stays[set.stays.len - 1].exit.date;
            stay.entry.mode = '6';
            stay.entry.origin = '1';
        }
        {
            int duration;
            if (ParsePmsiInt(ReadFragment(4), &duration)) {
                stay.exit.date = stay.entry.date + duration;
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedExitDate;
            }
        }
        if (i < test.cluster_len - 1) {
            stay.exit.mode = '6';
            stay.exit.destination = '1';
        }
        ParsePmsiInt(ReadFragment(2), &stay.unit.number);
        stay.unit.number = (int16_t)(stay.unit.number + 10000);
        offset += 2; // Skip end of UM type (A/B, H/P)

        if (RG_LIKELY(i < RG_LEN(test.auth_supplements))) {
            int type = 0;
            ParsePmsiInt(ReadFragment(2), &type);
            ParsePmsiInt(ReadFragment(4), &test.auth_supplements[i].days);
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

    if (RG_UNLIKELY(offset + das_count * 6 +
                    procedures_count * (version >= 222 ? 24 : 22) > line.len)) {
        LogError("Truncated RSA line");
        return false;
    }

    for (Size i = set.stays.len - test.cluster_len; i < set.stays.len; i++) {
        mco_Stay &stay = set.stays[i];

        HeapArray<drd_DiagnosisCode> other_diagnoses(&other_diagnoses_alloc);
        for (Size j = 0; j < stay.other_diagnoses.len; j++) {
            drd_DiagnosisCode diag =
                drd_DiagnosisCode::FromString(ReadFragment(6), (int)ParseFlag::End);
            if (RG_LIKELY(diag.IsValid())) {
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
                if (ParsePmsiInt(ReadFragment(3), &proc_delay)) {
                    proc.date = rsa.entry.date + proc_delay;
                }
            }
            proc.proc = drd_ProcedureCode::FromString(ReadFragment(7), (int)ParseFlag::End);
            if (version >= 222) {
                if (line[offset] != ' ') {
                    ParsePmsiInt(line.Take(offset, 2), &proc.extension) ||
                        SetErrorFlag(mco_Stay::Error::MalformedProcedureExtension);
                }
                offset += 2;
            }
            ParsePmsiInt(ReadFragment(1), &proc.phase);
            ParsePmsiInt(ReadFragment(1), &proc.activity);
            ParsePmsiChar(line[offset++], &proc.doc);
            offset += 6; // Skip modifiers, doc extension, etc.
            ParsePmsiInt(ReadFragment(2), &proc.count);
            offset += 1; // Skip date compatibility flag

            if (RG_LIKELY(proc.proc.IsValid())) {
                procedures.Append(proc);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedProcedureCode;
            }
        }

        stay.procedures = procedures.TrimAndLeak();
    }

    if (out_tests) {
        out_tests->Append(test);
    }

    return true;
}

bool mco_StaySetBuilder::LoadAtih(StreamReader &st,
                                  bool (mco_StaySetBuilder::*parse_func)(Span<const char> line,
                                                                         HashTable<int32_t, mco_Test> *out_tests),
                                  HashTable<int32_t, mco_Test> *out_tests)
{
    Size stays_len = set.stays.len;
    RG_DEFER_N(set_guard) { set.stays.RemoveFrom(stays_len); };

    Size errors = 0;
    {
        LineReader reader(&st);

        reader.PushLogHandler();
        RG_DEFER { PopLogHandler(); };

        Span<const char> line;
        while (reader.Next(&line)) {
            errors += !(this->*parse_func)(line, out_tests);
        }
        if (reader.error)
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

bool mco_StaySetBuilder::LoadRss(StreamReader &st, HashTable<int32_t, mco_Test> *out_tests)
{
    return LoadAtih(st, &mco_StaySetBuilder::ParseRssLine, out_tests);
}

bool mco_StaySetBuilder::LoadRsa(StreamReader &st, HashTable<int32_t, mco_Test> *out_tests)
{
    static std::atomic_flag gave_rsa_warning = ATOMIC_FLAG_INIT;
    if (!gave_rsa_warning.test_and_set()) {
        LogError("RSA files contain partial information that can lead to classifier errors"
                 " (such as procedure date errors)");
    }

    return LoadAtih(st, &mco_StaySetBuilder::ParseRsaLine, out_tests);
}

bool mco_StaySetBuilder::LoadFichComp(StreamReader &st, HashTable<int32_t, mco_Test> *)
{
    Size lines = 0;
    Size errors = 0;
    {
        LineReader reader(&st);
        reader.PushLogHandler();
        RG_DEFER { PopLogHandler(); };

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

                    if (RG_LIKELY(valid)) {
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

                    if (RG_LIKELY(valid)) {
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
        if (reader.error)
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

        bool (mco_StaySetBuilder::*load_func)(StreamReader &st,
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
        if (st.error) {
            success = false;
            continue;
        }
        success &= (this->*load_func)(st, out_tests);
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
        fichcomps_map.Append(fc.admin_id, &fc);
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
                            if (RG_UNLIKELY(sub_stays[0].dip_count)) {
                                LogError("Overwriting DIP count for stay %1", sub_stays[0].bill_id);
                            }
                            sub_stays[0].dip_count = fc->count;
                        } break;
                    }

                    matched_fichcomps.Append(fc - fichcomps.ptr);
                }

                fc++;
            }
        }
    }
    if (matched_fichcomps.table.count < fichcomps.len) {
        LogError("Some FICHCOMP entries (%1) have no matching stay",
                 fichcomps.len - matched_fichcomps.table.count);
    }

    SwapMemory(out_set, &set, RG_SIZE(set));
    return true;
}

}
