// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "mco_stays.hh"

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
#define PACK_VERSION 9
#define PACK_SIGNATURE "DRD_STAY_PAK"

// This should warn us in most cases when we break dspak files (it's basically a memcpy format)
StaticAssert(SIZE(PackHeader::signature) == SIZE(PACK_SIGNATURE));
StaticAssert(SIZE(mco_Stay) == 104);
StaticAssert(SIZE(DiagnosisCode) == 8);
StaticAssert(SIZE(mco_ProcedureRealisation) == 24);

bool mco_StaySet::SavePack(StreamWriter &st) const
{
    PackHeader bh = {};

    strcpy(bh.signature, PACK_SIGNATURE);
    bh.version = PACK_VERSION;
    bh.native_size = (uint8_t)SIZE(Size);
    bh.endianness = (int8_t)ARCH_ENDIANNESS;
    bh.stays_len = stays.len;
    bh.diagnoses_len = store.diagnoses.len;
    bh.procedures_len = store.procedures.len;

    st.Write(&bh, SIZE(bh));
#ifdef ARCH_64
    st.Write(stays.ptr, stays.len * SIZE(*stays.ptr));
#else
    for (const Stay &stay: stays) {
        mco_Stay stay2;
        memcpy(&stay2, &stay, SIZE(stay));

        union {
            uint8_t raw[32];
            struct {
                int64_t _pad1;
                int64_t diagnoses_len;
                int64_t _pad2;
                int64_t procedures_len;
            } st;
        } u;
        u.st.diagnoses_len = (int64_t)stay.diagnoses.len;
        u.st.procedures_len = (int64_t)stay.procedures.len;
        memcpy(&stay2.diagnoses, u.raw, 32);

        st.Write(&stay2, SIZE(stay2));
    }
#endif
    for (const mco_Stay &stay: stays) {
        st.Write(stay.diagnoses.ptr, stay.diagnoses.len * SIZE(*stay.diagnoses.ptr));
    }
    for (const mco_Stay &stay: stays) {
        st.Write(stay.procedures.ptr, stay.procedures.len * SIZE(*stay.procedures.ptr));
    }
    if (!st.Close())
        return false;

    return true;
}

bool mco_StaySet::SavePack(const char *filename) const
{
    CompressionType compression_type;
    Span<const char> extension = GetPathExtension(filename, &compression_type);

    if (!TestStr(extension, ".dspak")) {
        LogError("Unknown packing extension '%1', prefer '.dspak'", extension);
    }

    StreamWriter st(filename, compression_type);
    return SavePack(st);
}

bool mco_StaySetBuilder::LoadPack(StreamReader &st, HashTable<int32_t, mco_StayTest> *out_tests)
{
    const Size start_stays_len = set.stays.len;
    const Size start_diagnoses_len = set.store.diagnoses.len;
    const Size start_procedures_len = set.store.procedures.len;
    DEFER_N(set_guard) {
        set.stays.RemoveFrom(start_stays_len);
        set.store.diagnoses.RemoveFrom(start_diagnoses_len);
        set.store.procedures.RemoveFrom(start_procedures_len);
    };

    if (out_tests) {
        LogError("Testing is not supported by .dspak files");
    }

    PackHeader bh;
    if (st.Read(SIZE(bh), &bh) != SIZE(bh))
        goto corrupt_error;

    if (strncmp(bh.signature, PACK_SIGNATURE, SIZE(bh.signature)) != 0) {
        LogError("File '%1' does not have dspak signature", st.filename);
        return false;
    }
    if (bh.version != PACK_VERSION) {
        LogError("Cannot load '%1' (dspak version %2), expected version %3", st.filename,
                 bh.version, PACK_VERSION);
        return false;
    }
    if (bh.endianness != (int8_t)ARCH_ENDIANNESS) {
        LogError("File '%1' is not compatible with this platform (endianness issue)",
                 st.filename);
        return false;
    }
    if (bh.stays_len < 0 || bh.diagnoses_len < 0 || bh.procedures_len < 0)
        goto corrupt_error;

    if (bh.stays_len > (LEN_MAX - start_stays_len)
            || bh.diagnoses_len > (LEN_MAX - start_diagnoses_len)
            || bh.procedures_len > (LEN_MAX - start_procedures_len)) {
        LogError("Too much data to load in '%1'", st.filename);
        return false;
    }

    set.stays.Grow((Size)bh.stays_len);
    if (st.Read(SIZE(*set.stays.ptr) * (Size)bh.stays_len,
                set.stays.end()) != SIZE(*set.stays.ptr) * (Size)bh.stays_len)
        goto corrupt_error;
    set.stays.len += (Size)bh.stays_len;

    set.store.diagnoses.Grow((Size)bh.diagnoses_len);
    if (st.Read(SIZE(*set.store.diagnoses.ptr) * (Size)bh.diagnoses_len,
                set.store.diagnoses.end()) != SIZE(*set.store.diagnoses.ptr) * (Size)bh.diagnoses_len)
        goto corrupt_error;
    set.store.procedures.Grow((Size)bh.procedures_len);
    if (st.Read(SIZE(*set.store.procedures.ptr) * (Size)bh.procedures_len,
                set.store.procedures.end()) != SIZE(*set.store.procedures.ptr) * (Size)bh.procedures_len)
        goto corrupt_error;

    {
        Size store_diagnoses_len = set.store.diagnoses.len;
        Size store_procedures_len = set.store.procedures.len;

        for (Size i = set.stays.len - (Size)bh.stays_len; i < set.stays.len; i++) {
            mco_Stay *stay = &set.stays[i];

#ifndef ARCH_64
            union {
                uint8_t raw[32];
                struct {
                    int64_t _pad1;
                    int64_t diagnoses_len;
                    int64_t _pad2;
                    int64_t procedures_len;
                } st;
            } u;
            memcpy(u.raw, &stay->diagnoses, 32);
            stay->diagnoses.len = (Size)u.st.diagnoses_len;
            stay->procedures.len = (Size)u.st.procedures_len;
#endif

            if (stay->diagnoses.len) {
                if (UNLIKELY(stay->diagnoses.len < 0))
                    goto corrupt_error;
                stay->diagnoses.ptr = (DiagnosisCode *)store_diagnoses_len;
                store_diagnoses_len += stay->diagnoses.len;
                if (UNLIKELY(store_diagnoses_len <= 0 ||
                             store_diagnoses_len > start_diagnoses_len + bh.diagnoses_len))
                    goto corrupt_error;
            }
            if (stay->procedures.len) {
                if (UNLIKELY(stay->procedures.len < 0))
                    goto corrupt_error;
                stay->procedures.ptr = (mco_ProcedureRealisation *)store_procedures_len;
                store_procedures_len += stay->procedures.len;
                if (UNLIKELY(store_procedures_len <= 0 ||
                             store_procedures_len > start_procedures_len + bh.procedures_len))
                    goto corrupt_error;
            }
        }

        set.store.diagnoses.len = store_diagnoses_len;
        set.store.procedures.len = store_procedures_len;
    }


    // We assume stays are already sorted in pak files

    set_guard.disable();
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
    DebugAssert(str.len > 0);

    if (str[0] == ' ')
        return true;
    if (UNLIKELY((unsigned int)(str[0] - '0') > 9))
        return false;

    std::pair<T, bool> ret = ParseDec<T>(str, 0, &str);
    if (LIKELY(ret.second && (!str.len || str[0] == ' '))) {
        *out_value = ret.first;
    }
    return ret.second;
}

static bool ParsePmsiDate(Span<const char> str, Date *out_date)
{
    DebugAssert(str.len == 8);

    if (str[0] == ' ')
        return true;
    if (UNLIKELY(!IsAsciiDigit(str[0]) || !IsAsciiDigit(str[1]) || !IsAsciiDigit(str[2]) ||
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

static bool ParseRssLine(Span<const char> line, mco_StaySet *out_set,
                         HashTable<int32_t, mco_StayTest> *out_tests)
{
    if (UNLIKELY(line.len < 12)) {
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
    if (UNLIKELY(version < 16 || version > 18)) {
        stay.errors |= (int)mco_Stay::Error::UnknownRumVersion;
        out_set->stays.Append(stay);
        return true;
    }
    if (UNLIKELY(line.len < offset + 165)) {
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
    if (LIKELY(line[offset] != ' ')) {
        ParsePmsiInt(line.Take(offset, 2), &das_count) ||
            SetErrorFlag(mco_Stay::Error::MalformedOtherDiagnosesCount);
    } else {
        SetErrorFlag(mco_Stay::Error::MissingOtherDiagnosesCount);
    }
    offset += 2;
    if (LIKELY(line[offset] != ' ')) {
        ParsePmsiInt(line.Take(offset, 2), &dad_count) ||
            SetErrorFlag(mco_Stay::Error::MalformedOtherDiagnosesCount);
    } else {
        SetErrorFlag(mco_Stay::Error::MissingOtherDiagnosesCount);
    }
    offset += 2;
    if (LIKELY(line[offset] != ' ')) {
        ParsePmsiInt(line.Take(offset, 3), &procedures_count) ||
            SetErrorFlag(mco_Stay::Error::MalformedProceduresCount);
    } else {
        SetErrorFlag(mco_Stay::Error::MissingProceduresCount);
    }
    offset += 3;
    if (LIKELY(line[offset] != ' ')) {
        stay.main_diagnosis =
            DiagnosisCode::FromString(line.Take(offset, 8), (int)ParseFlag::End);
        if (UNLIKELY(!stay.main_diagnosis.IsValid())) {
            stay.errors |= (int)mco_Stay::Error::MalformedMainDiagnosis;
        }
    }
    offset += 8;
    if (line[offset] != ' ') {
        stay.linked_diagnosis =
            DiagnosisCode::FromString(line.Take(offset, 8), (int)ParseFlag::End);
        if (UNLIKELY(!stay.linked_diagnosis.IsValid())) {
            stay.errors |= (int)mco_Stay::Error::MalformedLinkedDiagnosis;
        }
    }
    offset += 8;
    ParsePmsiInt(ReadFragment(3), &stay.igs2) || SetErrorFlag(mco_Stay::Error::MalformedIgs2);
    if (line[offset] == '1') {
        stay.flags |= (int)mco_Stay::Flag::Confirmed;
    } else if (UNLIKELY(line[offset] != ' ')) {
        // According to the GenRSA manual and what the official FG does, confirmation
        // code '2' is supposed to be okay... but why? I don't accept it here.
        stay.errors |= (int)mco_Stay::Error::MalformedConfirmation;
    }
    offset += 33; // Skip a bunch of fields

    if (LIKELY(das_count >= 0 && dad_count >=0 && procedures_count >= 0)) {
        if (UNLIKELY(line.len < offset + 8 * das_count + 8 * dad_count +
                                (version >= 17 ? 29 : 26) * procedures_count)) {
            LogError("Truncated RUM line");
            return false;
        }

        stay.diagnoses.ptr = (DiagnosisCode *)out_set->store.diagnoses.len;
        if (LIKELY(stay.main_diagnosis.IsValid())) {
            out_set->store.diagnoses.Append(stay.main_diagnosis);
        }
        if (stay.linked_diagnosis.IsValid()) {
            out_set->store.diagnoses.Append(stay.linked_diagnosis);
        }
        for (int i = 0; i < das_count; i++) {
            DiagnosisCode diag =
                DiagnosisCode::FromString(ReadFragment(8), (int)ParseFlag::End);
            if (LIKELY(diag.IsValid())) {
                out_set->store.diagnoses.Append(diag);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedOtherDiagnosis;
            }
        }
        stay.diagnoses.len = out_set->store.diagnoses.len - (Size)stay.diagnoses.ptr;
        offset += 8 * dad_count; // Skip documentary diagnoses

        stay.procedures.ptr = (mco_ProcedureRealisation *)out_set->store.procedures.len;
        for (int i = 0; i < procedures_count; i++) {
            mco_ProcedureRealisation proc = {};

            ParsePmsiDate(ReadFragment(8), &proc.date);
            proc.proc = ProcedureCode::FromString(ReadFragment(7), (int)ParseFlag::End);
            if (version >= 17) {
                if (line[offset] != ' ') {
                    if (UNLIKELY(line[offset] != '-' ||
                                 !ParsePmsiInt(line.Take(offset + 1, 2), &proc.extension))) {
                        SetErrorFlag(mco_Stay::Error::MalformedProcedureExtension);
                    }
                }
                offset += 3;
            }
            ParsePmsiInt(ReadFragment(1), &proc.phase);
            {
                int activity = 0;
                ParsePmsiInt(ReadFragment(1), &activity);
                proc.activities = (uint8_t)(1 << activity);
            }
            if (line[offset] != ' ') {
                proc.doc = UpperAscii(line[offset]);
            }
            offset += 1;
            offset += 6; // Skip modifiers, etc.
            ParsePmsiInt(ReadFragment(2), &proc.count);

            if (LIKELY(proc.proc.IsValid())) {
                out_set->store.procedures.Append(proc);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedProcedureCode;
            }
        }
        stay.procedures.len = out_set->store.procedures.len - (Size)stay.procedures.ptr;
    }

    if (out_tests && tests) {
        mco_StayTest test = {};

        bool valid = true;
        test.bill_id = stay.bill_id;
        test.ghm = mco_GhmCode::FromString(line.Take(2, 6));
        valid &= test.ghm.IsValid();
        valid &= ParsePmsiInt(line.Take(12, 3), &test.error);

        if (valid) {
            mco_StayTest *it = out_tests->Append(test).first;
            it->cluster_len++;
        } else {
            mco_StayTest *it = out_tests->Find(test.bill_id);
            if (it) {
                it->cluster_len++;
            }
        }
    }

    out_set->stays.Append(stay);
    return true;
}

static bool ParseRsaLine(Span<const char> line, mco_StaySet *out_set,
                         HashTable<int32_t, mco_StayTest> *out_tests)
{
    if (UNLIKELY(line.len < 12)) {
        LogError("Truncated RSA line");
        return false;
    }

    mco_Stay rsa = {};
    mco_StayTest test = {};
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
    if (UNLIKELY(version < 220 || version > 224)) {
        SetErrorFlag(mco_Stay::Error::UnknownRumVersion);
        out_set->stays.Append(rsa);
        return true;
    }
    if (UNLIKELY(line.len < (version >= 222 ? 174 : 182))) {
        LogError("Truncated RSA line");
        return false;
    }

    ParsePmsiInt(ReadFragment(10), &rsa.bill_id) || SetErrorFlag(mco_Stay::Error::MalformedBillId);
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
        if (UNLIKELY(!valid)) {
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
    {
        int last_period_delay;
        if (ParsePmsiInt(ReadFragment(3), &last_period_delay) && rsa.entry.date.IsValid()) {
            rsa.last_menstrual_period = rsa.entry.date - last_period_delay;
        } else {
            SetErrorFlag(mco_Stay::Error::MalformedLastMenstrualPeriod);
        }
    }
    ParsePmsiInt(ReadFragment(2), &rsa.session_count) || SetErrorFlag(mco_Stay::Error::MalformedSessionCount);
    ParsePmsiInt(ReadFragment(4), &test.ghs.number);
    offset += 13; // Skip many fields
    if (line[offset] == '1') {
        rsa.flags |= (int)mco_Stay::Flag::Confirmed;
    } else if (UNLIKELY(line[offset] != ' ')) {
        SetErrorFlag(mco_Stay::Error::MalformedConfirmation);
    }
    offset++;
    ParsePmsiInt(ReadFragment(1), &global_auth_count);
    offset += 21; // Skip many fields
    ParsePmsiInt(ReadFragment(1), &radiotherapy_supp_count);
    offset += (version >= 222) ? 18 : 26;
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
        offset += 64;
    } else if (version >= 222) {
        offset += 49;
    } else {
        offset += 41;
    }
    offset += 2 * global_auth_count;
    offset += 7 * radiotherapy_supp_count;

    if (UNLIKELY(offset + test.cluster_len * (version >= 221 ? 60 : 58) > line.len)) {
        LogError("Truncated RSA line");
        return false;
    }

    Size das_count = 0;
    Size procedures_count = 0;
    for (Size i = 0; i < test.cluster_len; i++) {
        mco_Stay stay = rsa;

        offset += 14; // Skip many fields
        if (LIKELY(line[offset] != ' ')) {
            stay.main_diagnosis =
                DiagnosisCode::FromString(line.Take(offset, 6), (int)ParseFlag::End);
            if (UNLIKELY(!stay.main_diagnosis.IsValid())) {
                stay.errors |= (int)mco_Stay::Error::MalformedMainDiagnosis;
            }
        }
        offset += 6;
        if (line[offset] != ' ') {
            stay.linked_diagnosis =
                DiagnosisCode::FromString(line.Take(offset, 6), (int)ParseFlag::End);
            if (UNLIKELY(!stay.linked_diagnosis.IsValid())) {
                stay.errors |= (int)mco_Stay::Error::MalformedLinkedDiagnosis;
            }
        }
        offset += 6;
        ParsePmsiInt(ReadFragment(3), &stay.igs2);
        if (version >= 221) {
            ParsePmsiInt(ReadFragment(2), &stay.gestational_age);
        }
        ParsePmsiInt(ReadFragment(2), &stay.diagnoses.len);
        ParsePmsiInt(ReadFragment(3), &stay.procedures.len);
        if (i) {
            stay.entry.date = out_set->stays[out_set->stays.len - 1].exit.date;
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
        offset += 18; // Skip many fields

        out_set->stays.Append(stay);
        stay.entry.date = stay.exit.date;

        das_count += stay.diagnoses.len;
        procedures_count += stay.procedures.len;
    }

    if (UNLIKELY(offset + das_count * 6 +
                 procedures_count * (version >= 222 ? 24 : 22) > line.len)) {
        LogError("Truncated RSA line");
        return false;
    }

    for (Size i = out_set->stays.len - test.cluster_len; i < out_set->stays.len; i++) {
        mco_Stay &stay =out_set->stays[i];

        stay.diagnoses.ptr = (DiagnosisCode *)out_set->store.diagnoses.len;
        for (Size j = 0; j < stay.diagnoses.len; j++) {
            DiagnosisCode diag =
                DiagnosisCode::FromString(ReadFragment(6), (int)ParseFlag::End);
            if (LIKELY(diag.IsValid())) {
                out_set->store.diagnoses.Append(diag);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedOtherDiagnosis;
            }
        }
        if (LIKELY(stay.main_diagnosis.IsValid())) {
            out_set->store.diagnoses.Append(stay.main_diagnosis);
            stay.diagnoses.len++;
        }
        if (stay.linked_diagnosis.IsValid()) {
            out_set->store.diagnoses.Append(stay.linked_diagnosis);
            stay.diagnoses.len++;
        }
    }

    for (Size i = out_set->stays.len - test.cluster_len; i < out_set->stays.len; i++) {
        mco_Stay &stay = out_set->stays[i];

        stay.procedures.ptr = (mco_ProcedureRealisation *)out_set->store.procedures.len;
        for (Size j = 0; j < stay.procedures.len; j++) {
            mco_ProcedureRealisation proc = {};

            {
                int proc_delay;
                if (ParsePmsiInt(ReadFragment(3), &proc_delay)) {
                    proc.date = rsa.entry.date + proc_delay;
                }
            }
            proc.proc = ProcedureCode::FromString(ReadFragment(7), (int)ParseFlag::End);
            if (version >= 222) {
                if (line[offset] != ' ') {
                    ParsePmsiInt(line.Take(offset, 2), &proc.extension) ||
                        SetErrorFlag(mco_Stay::Error::MalformedProcedureExtension);
                }
                offset += 2;
            }
            ParsePmsiInt(ReadFragment(1), &proc.phase);
            {
                int activity;
                ParsePmsiInt(ReadFragment(1), &activity);
                proc.activities = (uint8_t)(1 << activity);
            }
            ParsePmsiChar(line[offset++], &proc.doc);
            offset += 6; // Skip modifiers, doc extension, etc.
            ParsePmsiInt(ReadFragment(2), &proc.count);
            offset += 1; // Skip date compatibility flag

            if (LIKELY(proc.proc.IsValid())) {
                out_set->store.procedures.Append(proc);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedProcedureCode;
            }
        }
    }

    if (out_tests) {
        out_tests->Append(test);
    }

    return true;
}

bool mco_StaySetBuilder::LoadAtih(StreamReader &st,
                                  bool (*parse_func)(Span<const char> line, mco_StaySet *out_set,
                                                     HashTable<int32_t, mco_StayTest> *out_tests),
                                  HashTable<int32_t, mco_StayTest> *out_tests)
{
    Size stays_len = set.stays.len;
    DEFER_NC(set_guard, diagnoses_len = set.store.diagnoses.len,
                        procedures_len = set.store.procedures.len) {
        set.stays.RemoveFrom(stays_len);
        set.store.diagnoses.RemoveFrom(diagnoses_len);
        set.store.procedures.RemoveFrom(procedures_len);
    };

    Size errors = 0;
    {
        LineReader reader(&st);

        reader.PushLogHandler();
        DEFER { PopLogHandler(); };

        Span<const char> line;
        while (reader.Next(&line)) {
            errors += !parse_func(line, &set, out_tests);
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

    set_guard.disable();
    return true;
}

bool mco_StaySetBuilder::LoadRss(StreamReader &st, HashTable<int32_t, mco_StayTest> *out_tests)
{
    return LoadAtih(st, ParseRssLine, out_tests);
}

bool mco_StaySetBuilder::LoadRsa(StreamReader &st, HashTable<int32_t, mco_StayTest> *out_tests)
{
    static std::atomic_flag gave_rsa_warning = ATOMIC_FLAG_INIT;
    if (!gave_rsa_warning.test_and_set()) {
        LogError("RSA files contain partial information that can lead to classifier errors"
                 " (such as procedure date errors)");
    }

    return LoadAtih(st, ParseRsaLine, out_tests);
}

bool mco_StaySetBuilder::LoadFiles(Span<const char *const> filenames,
                                   HashTable<int32_t, mco_StayTest> *out_tests)
{
    bool success = true;

    for (const char *filename: filenames) {
        CompressionType compression_type;
        Span<const char> extension = GetPathExtension(filename, &compression_type);

        bool (mco_StaySetBuilder::*load_func)(StreamReader &st,
                                              HashTable<int32_t, mco_StayTest> *out_tests);
        if (extension == ".dspak") {
            load_func = &mco_StaySetBuilder::LoadPack;
        } else if (extension == ".grp" || extension == ".rss") {
            load_func = &mco_StaySetBuilder::LoadRss;
        } else if (extension == ".rsa") {
            load_func = &mco_StaySetBuilder::LoadRsa;
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
    for (mco_Stay &stay: set.stays) {
#define FIX_SPAN(SpanName) \
            stay.SpanName.ptr = set.store.SpanName.ptr + (Size)stay.SpanName.ptr

        FIX_SPAN(diagnoses);
        FIX_SPAN(procedures);

#undef FIX_SPAN
    }

    SwapMemory(out_set, &set, SIZE(set));
    return true;
}
