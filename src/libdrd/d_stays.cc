// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "../common/json.hh"
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
#define PACK_VERSION 4
#define PACK_SIGNATURE "DRD_STAY_PAK"

// This should warn us in most cases when we break dspak files (it's basically a memcpy format)
StaticAssert(SIZE(PackHeader::signature) == SIZE(PACK_SIGNATURE));
StaticAssert(SIZE(Stay) == 104);
StaticAssert(SIZE(DiagnosisCode) == 8);
StaticAssert(SIZE(ProcedureRealisation) == 16);

// TODO: Flag errors and translate to FG errors
class JsonStayHandler: public BaseJsonHandler<JsonStayHandler> {
    enum class State {
        Default,
        StayArray,
        StayObject,
        AssociatedDiagnosisArray,
        ProcedureArray,
        ProcedureObject,
        TestObject
    };

    State state = State::Default;

    Stay stay;
    StayTest test;
    ProcedureRealisation proc;

public:
    StaySet *out_set;
    HashTable<int32_t, StayTest> *out_tests;

    JsonStayHandler(StaySet *out_set, HashTable<int32_t, StayTest> *out_tests)
        : out_set(out_set), out_tests(out_tests)
    {
        ResetStay();
        ResetProc();
    }

    bool Branch(JsonBranchType type, const char *key)
    {
        switch (state) {
            case State::Default: {
                switch (type) {
                    case JsonBranchType::Array: { state = State::StayArray; } break;
                    default: { return UnexpectedBranch(type); } break;
                }
            } break;

            case State::StayArray: {
                switch (type) {
                    case JsonBranchType::Object: { state = State::StayObject; } break;
                    case JsonBranchType::EndArray: { state = State::Default; } break;
                    default: { return UnexpectedBranch(type); } break;
                }
            } break;

            case State::StayObject: {
                switch (type) {
                    case JsonBranchType::EndObject: {
                        if (LIKELY(stay.main_diagnosis.IsValid())) {
                            if (UNLIKELY(out_set->store.diagnoses.len == LEN_MAX)) {
                                LogError("Too much data to load");
                                return false;
                            }
                            out_set->store.diagnoses.Append(stay.main_diagnosis);
                        }
                        if (stay.linked_diagnosis.IsValid()) {
                            if (UNLIKELY(out_set->store.diagnoses.len == LEN_MAX)) {
                                LogError("Too much data to load");
                                return false;
                            }
                            out_set->store.diagnoses.Append(stay.linked_diagnosis);
                        }

                        if (UNLIKELY(out_set->stays.len == LEN_MAX)) {
                            LogError("Too much data to load");
                            return false;
                        }
                        stay.diagnoses.len = out_set->store.diagnoses.len - (Size)stay.diagnoses.ptr;
                        stay.procedures.len = out_set->store.procedures.len - (Size)stay.procedures.ptr;
                        out_set->stays.Append(stay);

                        if (out_tests && (test.ghm.value || test.ghs.number)) {
                            test.bill_id = stay.bill_id;
                            out_tests->Append(test);
                        }

                        ResetStay();

                        state = State::StayArray;
                    } break;
                    case JsonBranchType::Object: {
                        if (TestStr(key, "test")) {
                            state = State::TestObject;
                        } else {
                            return UnexpectedBranch(type);
                        }
                    } break;
                    case JsonBranchType::Array: {
                        if (TestStr(key, "das")) {
                            state = State::AssociatedDiagnosisArray;
                        } else if (TestStr(key, "procedures")) {
                            state = State::ProcedureArray;
                        } else {
                            return UnexpectedBranch(type);
                        }
                    } break;
                    default: { return UnexpectedBranch(type); } break;
                }
            } break;

            case State::TestObject: {
                switch (type) {
                    case JsonBranchType::EndObject: { state = State::StayObject; } break;
                    default: { return UnexpectedBranch(type); } break;
                }
            } break;

            case State::AssociatedDiagnosisArray: {
                switch (type) {
                    case JsonBranchType::EndArray: { state = State::StayObject; } break;
                    default: { return UnexpectedBranch(type); } break;
                }
            } break;

            case State::ProcedureArray: {
                switch (type) {
                    case JsonBranchType::Object: { state = State::ProcedureObject; } break;
                    case JsonBranchType::EndArray: { state = State::StayObject; } break;
                    default: { return UnexpectedBranch(type); } break;
                }
            } break;

            case State::ProcedureObject: {
                switch (type) {
                    case JsonBranchType::EndObject: {
                        if (UNLIKELY(out_set->store.procedures.len == LEN_MAX)) {
                            LogError("Too much data to load");
                            return false;
                        }
                        out_set->store.procedures.Append(proc);
                        ResetProc();

                        state = State::ProcedureArray;
                    } break;
                    default: { return UnexpectedBranch(type); } break;
                }
            } break;
        }

        return true;
    }

    bool Value(const char *key, const JsonValue &value)
    {
        switch (state) {
            case State::StayObject: {
                if (TestStr(key, "bed_authorization")) {
                    SetInt(value, &stay.bed_authorization);
                } else if (TestStr(key, "bill_id")) {
                    SetInt(value, &stay.bill_id);
                } else if (TestStr(key, "birthdate")) {
                    SetErrorFlag(Stay::Error::MalformedBirthdate,
                                 !SetDate(value, false, &stay.birthdate));
                } else if (TestStr(key, "entry_date")) {
                    SetErrorFlag(Stay::Error::MalformedEntryDate,
                                 !SetDate(value, false, &stay.entry.date));
                } else if (TestStr(key, "entry_mode")) {
                    bool valid = false;
                    if (value.type == JsonValue::Type::Int) {
                        if (value.u.i >= 0 && value.u.i <= 9) {
                            stay.entry.mode = (int8_t)('0' + value.u.i);
                            valid = true;
                        } else {
                            LogError("Invalid entry mode value %1", value.u.i);
                        }
                    } else if (value.type == JsonValue::Type::String) {
                        if (value.u.str.len == 1) {
                            stay.entry.mode = UpperAscii(value.u.str[0]);
                            valid = true;
                        } else {
                            LogError("Invalid entry mode value '%1'", value.u.str);
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
                    SetErrorFlag(Stay::Error::MalformedEntryMode, !valid);
                } else if (TestStr(key, "entry_origin")) {
                    bool valid = false;
                    if (value.type == JsonValue::Type::Int) {
                        if (value.u.i >= 0 && value.u.i <= 9) {
                            stay.entry.origin = (int8_t)('0' + value.u.i);
                            valid = true;
                        } else {
                            LogError("Invalid entry origin value %1", value.u.i);
                        }
                    } else if (value.type == JsonValue::Type::String) {
                        if (!value.u.str.len) {
                            stay.entry.origin = 0;
                            valid = true;
                        } else if (value.u.str.len == 1) {
                            stay.entry.origin = UpperAscii(value.u.str[0]);
                            valid = true;
                        } else {
                            LogError("Invalid entry origin value '%1'", value.u.str);
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
                    SetErrorFlag(Stay::Error::MalformedEntryOrigin, !valid);
                } else if (TestStr(key, "exit_date")) {
                    SetErrorFlag(Stay::Error::MalformedExitDate,
                                 !SetDate(value, false, &stay.exit.date));
                } else if (TestStr(key, "exit_mode")) {
                    bool valid = false;
                    if (value.type == JsonValue::Type::Int) {
                        if (value.u.i >= 0 && value.u.i <= 9) {
                            stay.exit.mode = (int8_t)('0' + value.u.i);
                            valid = true;
                        } else {
                            LogError("Invalid exit mode value %1", value.u.i);
                        }
                    } else if (value.type == JsonValue::Type::String) {
                        if (value.u.str.len == 1) {
                            stay.exit.mode = UpperAscii(value.u.str[0]);
                            valid = true;
                        } else {
                            LogError("Invalid exit mode value '%1'", value.u.str);
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
                    SetErrorFlag(Stay::Error::MalformedExitMode, !valid);
                } else if (TestStr(key, "exit_destination")) {
                    bool valid = false;
                    if (value.type == JsonValue::Type::Int) {
                        if (value.u.i >= 0 && value.u.i <= 9) {
                            stay.exit.destination = (int8_t)('0' + value.u.i);
                            valid = true;
                        } else {
                            LogError("Invalid exit destination value %1", value.u.i);
                        }
                    } else if (value.type == JsonValue::Type::String) {
                        if (!value.u.str.len) {
                            stay.exit.destination = 0;
                            valid = true;
                        } else if (value.u.str.len == 1) {
                            stay.exit.destination = UpperAscii(value.u.str[0]);
                            valid = true;
                        } else {
                            LogError("Invalid exit destination value '%1'", value.u.str);
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
                    SetErrorFlag(Stay::Error::MalformedExitDestination, !valid);
                } else if (TestStr(key, "dp")) {
                    if (value.type == JsonValue::Type::String) {
                        stay.main_diagnosis = DiagnosisCode::FromString(value.u.str.ptr, false);
                        SetErrorFlag(Stay::Error::MalformedMainDiagnosis,
                                     !stay.main_diagnosis.IsValid());
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "dr")) {
                    if (value.type == JsonValue::Type::String) {
                        stay.linked_diagnosis = DiagnosisCode::FromString(value.u.str.ptr, false);
                        SetErrorFlag(Stay::Error::MalformedLinkedDiagnosis,
                                     !stay.linked_diagnosis.IsValid());
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "gestational_age")) {
                    SetInt(value, &stay.gestational_age);
                } else if (TestStr(key, "igs2")) {
                    SetInt(value, &stay.igs2);
                } else if (TestStr(key, "last_menstrual_period")) {
                    SetDate(value, false, &stay.last_menstrual_period);
                } else if (TestStr(key, "newborn_weight")) {
                    SetErrorFlag(Stay::Error::MalformedNewbornWeight,
                                 !SetInt(value, &stay.newborn_weight));
                } else if (TestStr(key, "session_count")) {
                    SetErrorFlag(Stay::Error::MalformedSessionCount,
                                 !SetInt(value, &stay.session_count));
                } else if (TestStr(key, "sex")) {
                    bool valid = false;
                    if (value.type == JsonValue::Type::Int) {
                        if (value.u.i == 1) {
                            stay.sex = Sex::Male;
                            valid = true;
                        } else if (value.u.i == 2) {
                            stay.sex = Sex::Female;
                            valid = true;
                        } else {
                            LogError("Invalid sex value '%1'", value.u.i);
                        }
                    } else if (value.type == JsonValue::Type::String) {
                        if (TestStr(value.u.str, "H") || TestStr(value.u.str, "h") ||
                                TestStr(value.u.str, "M") || TestStr(value.u.str, "m")) {
                            stay.sex = Sex::Male;
                            valid = true;
                        } else if (TestStr(value.u.str, "F") || TestStr(value.u.str, "f")) {
                            stay.sex = Sex::Female;
                            valid = true;
                        } else {
                            LogError("Invalid sex value '%1'", value.u.str);
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
                    SetErrorFlag(Stay::Error::MalformedSex, !valid);
                } else if (TestStr(key, "stay_id")) {
                    SetInt(value, &stay.stay_id);
                } else if (TestStr(key, "unit")) {
                    SetInt(value, &stay.unit.number);
                } else {
                    return UnknownAttribute(key);
                }
            } break;

            case State::AssociatedDiagnosisArray: {
                if (value.type == JsonValue::Type::String) {
                    DiagnosisCode diag = DiagnosisCode::FromString(value.u.str.ptr, false);
                    if (UNLIKELY(!diag.IsValid())) {
                        stay.error_mask |= (int)Stay::Error::MalformedAssociatedDiagnosis;
                    } else if (UNLIKELY(out_set->store.diagnoses.len == LEN_MAX)) {
                        LogError("Too much data to load");
                        return false;
                    } else {
                        out_set->store.diagnoses.Append(diag);
                    }
                } else {
                    UnexpectedType(value.type);
                }
            } break;

            case State::ProcedureObject: {
                if (TestStr(key, "code")) {
                    if (value.type == JsonValue::Type::String) {
                        proc.proc = ProcedureCode::FromString(value.u.str.ptr);
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "date")) {
                    SetDate(value, false, &proc.date);
                } else if (TestStr(key, "phase")) {
                    SetInt(value, &proc.phase);
                } else if (TestStr(key, "activity")) {
                    if (value.type == JsonValue::Type::Int) {
                        int64_t activities_dec = value.u.i;
                        if (UNLIKELY(activities_dec < 0)) {
                            LogError("Procedure activity %1 cannot be a negative value", value.u.i);
                            activities_dec = 0;
                        }
                        while (activities_dec) {
                            int activity = (int)(activities_dec % 10);
                            activities_dec /= 10;
                            if (LIKELY(activity < 8)) {
                                proc.activities = (uint8_t)(proc.activities | (1 << activity));
                            } else {
                                LogError("Procedure activity %1 outside of %2 - %3", activity, 0, 7);
                            }
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "count")) {
                    SetInt(value, &proc.count);
                } else {
                    return UnknownAttribute(key);
                }
            } break;

            case State::TestObject: {
                if (TestStr(key, "cluster_len")) {
                    SetInt(value, &test.cluster_len);
                } else if (TestStr(key, "ghm")) {
                    if (value.type == JsonValue::Type::String) {
                        test.ghm = GhmCode::FromString(value.u.str.ptr);
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "error")) {
                    SetInt(value, &test.error);
                } else if (TestStr(key, "ghs")) {
                    SetInt(value, &test.ghs.number);
                } else if (TestStr(key, "rea")) {
                    SetInt(value, &test.supplement_days.st.rea);
                } else if (TestStr(key, "reasi")) {
                    SetInt(value, &test.supplement_days.st.reasi);
                } else if (TestStr(key, "si")) {
                    SetInt(value, &test.supplement_days.st.si);
                } else if (TestStr(key, "src")) {
                    SetInt(value, &test.supplement_days.st.src);
                } else if (TestStr(key, "nn1")) {
                    SetInt(value, &test.supplement_days.st.nn1);
                } else if (TestStr(key, "nn2")) {
                    SetInt(value, &test.supplement_days.st.nn2);
                } else if (TestStr(key, "nn3")) {
                    SetInt(value, &test.supplement_days.st.nn3);
                } else if (TestStr(key, "rep")) {
                    SetInt(value, &test.supplement_days.st.rep);
                } else {
                    return UnknownAttribute(key);
                }
            } break;

            default: { return UnexpectedValue(); } break;
        }

        return true;
    }

private:
    void ResetStay()
    {
        stay = {};
        stay.diagnoses.ptr = (DiagnosisCode *)out_set->store.diagnoses.len;
        stay.procedures.ptr = (ProcedureRealisation *)out_set->store.procedures.len;
        test = {};
    }

    void ResetProc()
    {
        proc = {};
        proc.count = 1;
    }

    void SetErrorFlag(Stay::Error flag, bool error)
    {
        if (UNLIKELY(error)) {
            stay.error_mask |= (uint32_t)flag;
        } else {
            stay.error_mask &= ~(uint32_t)flag;
        }
    }
};

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

bool StaySetBuilder::LoadJson(StreamReader &st, HashTable<int32_t, StayTest> *out_tests)
{
    Size stays_len = set.stays.len;
    DEFER_NC(set_guard, diagnoses_len = set.store.diagnoses.len,
                        procedures_len = set.store.procedures.len) {
        set.stays.RemoveFrom(stays_len);
        set.store.diagnoses.RemoveFrom(diagnoses_len);
        set.store.procedures.RemoveFrom(procedures_len);
    };

    JsonStayHandler json_handler(&set, out_tests);
    if (!ParseJsonFile(st, &json_handler))
        return false;

    std::stable_sort(set.stays.begin() + stays_len, set.stays.end(),
                     [](const Stay &stay1, const Stay &stay2) {
        return MultiCmp(stay1.stay_id - stay2.stay_id,
                        stay1.bill_id - stay2.bill_id) < 0;
    });

    set_guard.disable();
    return true;
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

bool StaySetBuilder::LoadFiles(Span<const char *const> filenames,
                               HashTable<int32_t, StayTest> *out_tests)
{
    for (const char *filename: filenames) {
        LocalArray<char, 16> extension;
        CompressionType compression_type;
        extension.len = GetPathExtension(filename, extension.data, &compression_type);

        bool (StaySetBuilder::*load_func)(StreamReader &st,
                                          HashTable<int32_t, StayTest> *out_tests);
        if (TestStr(extension, ".dsjson")) {
            load_func = &StaySetBuilder::LoadJson;
        } else if (TestStr(extension, ".dspak")) {
            load_func = &StaySetBuilder::LoadPack;
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
