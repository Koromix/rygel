// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "d_stays.hh"

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
#define PACK_VERSION 5
#define PACK_SIGNATURE "DRD_STAY_PAK"

// This should warn us in most cases when we break dspak files (it's basically a memcpy format)
StaticAssert(SIZE(PackHeader::signature) == SIZE(PACK_SIGNATURE));
StaticAssert(SIZE(Stay) == 104);
StaticAssert(SIZE(DiagnosisCode) == 8);
StaticAssert(SIZE(ProcedureRealisation) == 16);

bool StaySet::SavePack(StreamWriter &st) const
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
        Stay stay2;
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
    for (const Stay &stay: stays) {
        st.Write(stay.diagnoses.ptr, stay.diagnoses.len * SIZE(*stay.diagnoses.ptr));
    }
    for (const Stay &stay: stays) {
        st.Write(stay.procedures.ptr, stay.procedures.len * SIZE(*stay.procedures.ptr));
    }
    if (!st.Close())
        return false;

    return true;
}

bool StaySet::SavePack(const char *filename) const
{
    LocalArray<char, 16> extension;
    CompressionType compression_type;

    extension.len = GetPathExtension(filename, extension.data, &compression_type);

    if (!TestStr(extension, ".dspak")) {
        LogError("Unknown packing extension '%1', prefer '.dspak'", extension);
    }

    StreamWriter st(filename, compression_type);
    return SavePack(st);
}

bool StaySetBuilder::LoadPack(StreamReader &st, HashTable<int32_t, StayTest> *out_tests)
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
            Stay *stay = &set.stays[i];

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
                stay->procedures.ptr = (ProcedureRealisation *)store_procedures_len;
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

bool StaySetBuilder::LoadRssOrGrp(StreamReader &st, bool grp,
                                  HashTable<int32_t, StayTest> *out_tests)
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

        while (!reader.eof) {
            Span<const char> line = reader.GetLine();
            if (reader.error)
                return false;

            Size offset = grp ? 24 : 9;
            if (UNLIKELY(line.len < offset + 168)) {
                LogError("Truncated RUM line %1 in '%2'", reader.line_number, st.filename);
                errors++;
                continue;
            }

            Stay stay = {};
            int das_count = -1;
            int dad_count = -1;
            int procedures_count = -1;

            // Declaring (simple) lambdas inside loops does not seem to impact performance
            const auto ReadFragment = [&](Size len) {
                Span<const char> frag = line.Take(offset, len);
                offset += len;
                return frag;
            };
            const auto SetErrorFlag = [&stay](Stay::Error flag) {
                stay.error_mask |= (uint32_t)flag;
                return true;
            };

            int16_t version = 0;
            ParsePmsiInt(ReadFragment(3), &version);
            if (UNLIKELY(version < 16 || version > 18)) {
                LogError("Unsupported RUM version %1 in '%2'", version, st.filename);
                errors++;
                continue;
            }

            ParsePmsiInt(ReadFragment(20), &stay.bill_id);
            ParsePmsiInt(ReadFragment(20), &stay.admin_id);
            offset += 10; // Skip RUM id
            ParsePmsiDate(ReadFragment(8), &stay.birthdate) || SetErrorFlag(Stay::Error::MalformedBirthdate);
            ParsePmsiInt(ReadFragment(1), &stay.sex) || SetErrorFlag(Stay::Error::MalformedSex);
            ParsePmsiInt(ReadFragment(4), &stay.unit.number);
            ParsePmsiInt(ReadFragment(2), &stay.bed_authorization);
            ParsePmsiDate(ReadFragment(8), &stay.entry.date) || SetErrorFlag(Stay::Error::MalformedEntryDate);
            if (LIKELY(line[offset] != ' ')) {
                stay.entry.mode = line[offset];
            }
            offset += 1;
            if (line[offset] != ' ') {
                stay.entry.origin = line[offset];
            }
            offset += 1;
            ParsePmsiDate(ReadFragment(8), &stay.exit.date) || SetErrorFlag(Stay::Error::MalformedExitDate);
            if (LIKELY(line[offset] != ' ')) {
                stay.exit.mode = line[offset];
            }
            offset += 1;
            if (line[offset] != ' ') {
                stay.exit.destination = line[offset];
            }
            offset += 1;
            offset += 5; // Skip postal code
            ParsePmsiInt(ReadFragment(4), &stay.newborn_weight) || SetErrorFlag(Stay::Error::MalformedNewbornWeight);
            ParsePmsiInt(ReadFragment(2), &stay.gestational_age);
            ParsePmsiDate(ReadFragment(8), &stay.last_menstrual_period);
            ParsePmsiInt(ReadFragment(2), &stay.session_count) || SetErrorFlag(Stay::Error::MalformedSessionCount);
            if (LIKELY(line[offset] != ' ')) {
                ParsePmsiInt(line.Take(offset, 2), &das_count) ||
                    SetErrorFlag(Stay::Error::MalformedOtherDiagnosesCount);
            } else {
                SetErrorFlag(Stay::Error::MissingOtherDiagnosesCount);
            }
            offset += 2;
            if (LIKELY(line[offset] != ' ')) {
                ParsePmsiInt(line.Take(offset, 2), &dad_count) ||
                    SetErrorFlag(Stay::Error::MalformedOtherDiagnosesCount);
            } else {
                SetErrorFlag(Stay::Error::MissingOtherDiagnosesCount);
            }
            offset += 2;
            if (LIKELY(line[offset] != ' ')) {
                ParsePmsiInt(line.Take(offset, 3), &procedures_count) ||
                    SetErrorFlag(Stay::Error::MalformedProceduresCount);
            } else {
                SetErrorFlag(Stay::Error::MissingProceduresCount);
            }
            offset += 3;
            if (LIKELY(line[offset] != ' ')) {
                stay.main_diagnosis =
                    DiagnosisCode::FromString(line.Take(offset, 8), (int)ParseFlag::End);
                if (UNLIKELY(!stay.main_diagnosis.IsValid())) {
                    stay.error_mask |= (int)Stay::Error::MalformedMainDiagnosis;
                }
            }
            offset += 8;
            if (line[offset] != ' ') {
                stay.linked_diagnosis =
                    DiagnosisCode::FromString(line.Take(offset, 8), (int)ParseFlag::End);
                if (UNLIKELY(!stay.linked_diagnosis.IsValid())) {
                    stay.error_mask |= (int)Stay::Error::MalformedLinkedDiagnosis;
                }
            }
            offset += 8;
            ParsePmsiInt(ReadFragment(3), &stay.igs2) || SetErrorFlag(Stay::Error::MalformedIgs2);
            offset += 33; // Skip a bunch of fields

            if (LIKELY(das_count >= 0 && dad_count >=0 && procedures_count >= 0)) {
                if (UNLIKELY(line.len < offset + 8 * das_count + 8 * dad_count +
                                        (version >= 17 ? 29 : 26) * procedures_count)) {
                    LogError("Truncated RUM line %1 in '%2'", reader.line_number, st.filename);
                    errors++;
                    continue;
                }

                stay.diagnoses.ptr = (DiagnosisCode *)set.store.diagnoses.len;
                if (LIKELY(stay.main_diagnosis.IsValid())) {
                    set.store.diagnoses.Append(stay.main_diagnosis);
                }
                if (stay.linked_diagnosis.IsValid()) {
                    set.store.diagnoses.Append(stay.linked_diagnosis);
                }
                for (int i = 0; i < das_count; i++) {
                    DiagnosisCode diag =
                        DiagnosisCode::FromString(ReadFragment(8), (int)ParseFlag::End);
                    if (LIKELY(diag.IsValid())) {
                        set.store.diagnoses.Append(diag);
                    } else {
                        stay.error_mask |= (int)Stay::Error::MalformedOtherDiagnosis;
                    }
                }
                stay.diagnoses.len = set.store.diagnoses.len - (Size)stay.diagnoses.ptr;
                offset += 8 * dad_count; // Skip documentary diagnoses

                stay.procedures.ptr = (ProcedureRealisation *)set.store.procedures.len;
                for (int i = 0; i < procedures_count; i++) {
                    ProcedureRealisation proc = {};
                    ParsePmsiDate(ReadFragment(8), &proc.date);
                    proc.proc = ProcedureCode::FromString(ReadFragment(7), (int)ParseFlag::End);
                    if (UNLIKELY(!proc.proc.IsValid())) {
                        stay.error_mask |= (int)Stay::Error::MalformedProcedureCode;
                    }
                    if (version >= 17) {
                        offset += 3; // Skip CCAM extension
                    }
                    ParsePmsiInt(ReadFragment(1), &proc.phase);
                    {
                        int activity = 0;
                        ParsePmsiInt(ReadFragment(1), &activity);
                        proc.activities = (uint8_t)(1 << activity);
                    }
                    offset += 7; // Skip extension, modifiers, etc.
                    ParsePmsiInt(ReadFragment(2), &proc.count);
                    set.store.procedures.Append(proc);
                }
                stay.procedures.len = set.store.procedures.len - (Size)stay.procedures.ptr;
            }

            if (out_tests && grp) {
                StayTest test = {};

                bool valid = true;
                test.bill_id = stay.bill_id;
                test.ghm = GhmCode::FromString(line.Take(2, 6));
                valid &= test.ghm.IsValid();
                valid &= ParsePmsiInt(line.Take(12, 3), &test.error);

                if (valid) {
                    StayTest *it = out_tests->Append(test).first;
                    it->cluster_len++;
                } else {
                    StayTest *it = out_tests->Find(test.bill_id);
                    if (it) {
                        it->cluster_len++;
                    }
                }
            }

            set.stays.Append(stay);
        }
    }
    if (errors && set.stays.len == stays_len)
        return false;

    std::stable_sort(set.stays.begin() + stays_len, set.stays.end(),
                     [](const Stay &stay1, const Stay &stay2) {
        return MultiCmp(stay1.admin_id - stay2.admin_id,
                        stay1.bill_id - stay2.bill_id) < 0;
    });

    set_guard.disable();
    return true;
}

bool StaySetBuilder::LoadRsa(StreamReader &st, HashTable<int32_t, StayTest> *out_tests)
{
    LogError("RSA files are not supported yet");
    return false;
}

bool StaySetBuilder::LoadFiles(Span<const char *const> filenames,
                               HashTable<int32_t, StayTest> *out_tests)
{
    for (const char *filename: filenames) {
        LocalArray<char, 16> extension;
        CompressionType compression_type;
        extension.len = GetPathExtension(filename, extension.data, &compression_type);

        bool (StaySetBuilder::*load_func)(StreamReader &st,
                                          HashTable<int32_t, StayTest> *out_tests);
        if (TestStr(extension, ".dspak")) {
            load_func = &StaySetBuilder::LoadPack;
        } else if (TestStr(extension, ".grp")) {
            load_func = &StaySetBuilder::LoadGrp;
        } else if (TestStr(extension, ".rss")) {
            load_func = &StaySetBuilder::LoadRss;
        } else if (TestStr(extension, ".rsa")) {
            load_func = &StaySetBuilder::LoadRsa;
        } else {
            LogError("Cannot load stays from file '%1' with unknown extension '%2'",
                     filename, extension);
            return false;
        }

        StreamReader st(filename, compression_type);
        if (st.error)
            return false;
        if (!(this->*load_func)(st, out_tests))
            return false;
    }

    return true;
}

bool StaySetBuilder::Finish(StaySet *out_set)
{
    for (Stay &stay: set.stays) {
#define FIX_SPAN(SpanName) \
            stay.SpanName.ptr = set.store.SpanName.ptr + (Size)stay.SpanName.ptr

        FIX_SPAN(diagnoses);
        FIX_SPAN(procedures);

#undef FIX_SPAN
    }

    SwapMemory(out_set, &set, SIZE(set));
    return true;
}
