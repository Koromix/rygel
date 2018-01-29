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

    Size stay_size;
    Size stays_len;

    Size diagnoses_len;
    Size procedures_len;
};
#pragma pack(pop)
#define PACK_VERSION 1
#define PACK_SIGNATURE "DRD_STAY_PAK"
StaticAssert(SIZE(PackHeader::signature) == SIZE(PACK_SIGNATURE));

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
    ProcedureRealisation proc;

public:
    StaySet *out_set;

    JsonStayHandler(StaySet *out_set = nullptr)
        : out_set(out_set)
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
                            out_set->store.diagnoses.Append(stay.main_diagnosis);
                        }
                        if (stay.linked_diagnosis.IsValid()) {
                            out_set->store.diagnoses.Append(stay.linked_diagnosis);
                        }
                        stay.diagnoses.len = out_set->store.diagnoses.len - (Size)stay.diagnoses.ptr;
                        stay.procedures.len = out_set->store.procedures.len - (Size)stay.procedures.ptr;
                        out_set->stays.Append(stay);
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
                    SetDate(value, &stay.birthdate);
                } else if (TestStr(key, "entry_date")) {
                    SetDate(value, &stay.entry.date);
                } else if (TestStr(key, "entry_mode")) {
                    if (value.type == JsonValue::Type::Int) {
                        if (value.u.i >= 0 && value.u.i <= 9) {
                            stay.entry.mode = (int8_t)value.u.i;
                        } else {
                            LogError("Invalid entry mode value %1", value.u.i);
                        }
                    } else if (value.type == JsonValue::Type::String) {
                        if (value.u.str.len == 1) {
                            stay.entry.mode = (char)(value.u.str[0] - '0');
                        } else {
                            LogError("Invalid entry mode value '%1'", value.u.str);
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "entry_origin")) {
                    if (value.type == JsonValue::Type::Int) {
                        if (value.u.i >= 0 && value.u.i <= 9) {
                            stay.entry.origin = (int8_t)value.u.i;
                        } else {
                            LogError("Invalid entry origin value %1", value.u.i);
                        }
                    } else if (value.type == JsonValue::Type::String) {
                        if (!value.u.str.len) {
                            stay.entry.origin = 0;
                        } else if (value.u.str.len == 1 &&
                                   ((value.u.str[0] >= '0' && value.u.str[0] <= '9') ||
                                    value.u.str[0] == 'R' || value.u.str[0] == 'r')) {
                            // This is probably incorrect for either 'R' or 'r' but this is what
                            // the machine code in FG2017 does, so keep it that way.
                            stay.entry.origin = (char)(value.u.str[0] - '0');
                        } else {
                            LogError("Invalid entry origin value '%1'", value.u.str);
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "exit_date")) {
                    SetDate(value, &stay.exit.date);
                } else if (TestStr(key, "exit_mode")) {
                    if (value.type == JsonValue::Type::Int) {
                        if (value.u.i >= 0 && value.u.i <= 9) {
                            stay.exit.mode = (int8_t)value.u.i;
                        } else {
                            LogError("Invalid exit mode value %1", value.u.i);
                        }
                    } else if (value.type == JsonValue::Type::String) {
                        if (value.u.str.len == 1) {
                            stay.exit.mode = (char)(value.u.str[0] - '0');
                        } else {
                            LogError("Invalid exit mode value '%1'", value.u.str);
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "exit_destination")) {
                    if (value.type == JsonValue::Type::Int) {
                        if (value.u.i >= 0 && value.u.i <= 9) {
                            stay.exit.destination = (int8_t)value.u.i;
                        } else {
                            LogError("Invalid exit destination value %1", value.u.i);
                        }
                    } else if (value.type == JsonValue::Type::String) {
                        if (!value.u.str.len) {
                            stay.exit.destination = 0;
                        } else if (value.u.str.len == 1 &&
                                   (value.u.str[0] >= '0' && value.u.str[0] <= '9')) {
                            stay.exit.destination = (char)(value.u.str[0] - '0');
                        } else {
                            LogError("Invalid exit destination value '%1'", value.u.str);
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "dp")) {
                    if (value.type == JsonValue::Type::String) {
                        stay.main_diagnosis = DiagnosisCode::FromString(value.u.str.ptr);
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "dr")) {
                    if (value.type == JsonValue::Type::String) {
                        stay.linked_diagnosis = DiagnosisCode::FromString(value.u.str.ptr);
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "gestational_age")) {
                    SetInt(value, &stay.gestational_age);
                } else if (TestStr(key, "igs2")) {
                    SetInt(value, &stay.igs2);
                } else if (TestStr(key, "last_menstrual_period")) {
                    SetDate(value, &stay.last_menstrual_period);
                } else if (TestStr(key, "newborn_weight")) {
                    SetInt(value, &stay.newborn_weight);
                } else if (TestStr(key, "session_count")) {
                    SetInt(value, &stay.session_count);
                } else if (TestStr(key, "sex")) {
                    if (value.type == JsonValue::Type::Int) {
                        if (value.u.i == 1) {
                            stay.sex = Sex::Male;
                        } else if (value.u.i == 2) {
                            stay.sex = Sex::Female;
                        } else {
                            LogError("Invalid sex value %1", value.u.i);
                        }
                    } else if (value.type == JsonValue::Type::String) {
                        if (TestStr(value.u.str, "H") || TestStr(value.u.str, "h") ||
                                TestStr(value.u.str, "M") || TestStr(value.u.str, "m")) {
                            stay.sex = Sex::Male;
                        } else if (TestStr(value.u.str, "F") || TestStr(value.u.str, "f")) {
                            stay.sex = Sex::Female;
                        } else {
                            LogError("Invalid sex value '%1'", value.u.str);
                        }
                    } else {
                        UnexpectedType(value.type);
                    }
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
                    DiagnosisCode diag = DiagnosisCode::FromString(value.u.str.ptr);
                    out_set->store.diagnoses.Append(diag);
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
                    SetDate(value, &proc.date);
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
#ifndef DISABLE_TESTS
                if (TestStr(key, "cluster_len")) {
                    SetInt(value, &stay.test.cluster_len);
                } else if (TestStr(key, "ghm")) {
                    if (value.type == JsonValue::Type::String) {
                        stay.test.ghm = GhmCode::FromString(value.u.str.ptr);
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "error")) {
                    SetInt(value, &stay.test.error);
                } else if (TestStr(key, "ghs")) {
                    SetInt(value, &stay.test.ghs.number);
                } else if (TestStr(key, "rea")) {
                    SetInt(value, &stay.test.supplements.rea);
                } else if (TestStr(key, "reasi")) {
                    SetInt(value, &stay.test.supplements.reasi);
                } else if (TestStr(key, "si")) {
                    SetInt(value, &stay.test.supplements.si);
                } else if (TestStr(key, "src")) {
                    SetInt(value, &stay.test.supplements.src);
                } else if (TestStr(key, "nn1")) {
                    SetInt(value, &stay.test.supplements.nn1);
                } else if (TestStr(key, "nn2")) {
                    SetInt(value, &stay.test.supplements.nn2);
                } else if (TestStr(key, "nn3")) {
                    SetInt(value, &stay.test.supplements.nn3);
                } else if (TestStr(key, "rep")) {
                    SetInt(value, &stay.test.supplements.rep);
                } else {
                    return UnknownAttribute(key);
                }
#endif
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
    }

    void ResetProc()
    {
        proc = {};
        proc.count = 1;
    }
};

bool StaySet::SavePack(StreamWriter &st) const
{
    PackHeader bh = {};

    strcpy(bh.signature, PACK_SIGNATURE);
    bh.version = PACK_VERSION;
    bh.native_size = (uint8_t)SIZE(Size);
    bh.endianness = (int8_t)ARCH_ENDIANNESS;
    bh.stay_size = SIZE(*stays.ptr);
    bh.stays_len = stays.len;
    bh.diagnoses_len = store.diagnoses.len;
    bh.procedures_len = store.procedures.len;

    st.Write(&bh, SIZE(bh));
    st.Write(stays.ptr, stays.len * SIZE(*stays.ptr));
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

    if (!TestStr(extension, ".mpak")) {
        LogError("Unknown packing extension '%1', prefer '.mpak'", extension);
    }

    StreamWriter st(filename, compression_type);
    return SavePack(st);
}

bool StaySetBuilder::LoadJson(StreamReader &st)
{
    Size stays_len = set.stays.len;
    DEFER_NC(set_guard, diagnoses_len = set.store.diagnoses.len,
                        procedures_len = set.store.procedures.len) {
        set.stays.RemoveFrom(stays_len);
        set.store.diagnoses.RemoveFrom(diagnoses_len);
        set.store.procedures.RemoveFrom(procedures_len);
    };

    JsonStayHandler json_handler(&set);
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

bool StaySetBuilder::LoadPack(StreamReader &st)
{
    DEFER_NC(set_guard, stays_len = set.stays.len,
                        diagnoses_len = set.store.diagnoses.len,
                        procedures_len = set.store.procedures.len) {
        set.stays.RemoveFrom(stays_len);
        set.store.diagnoses.RemoveFrom(diagnoses_len);
        set.store.procedures.RemoveFrom(procedures_len);
    };

    PackHeader bh;
    if (st.Read(SIZE(bh), &bh) != SIZE(bh))
        goto error;
    if (bh.version != PACK_VERSION)
        goto error;
    if (bh.native_size != (uint8_t)SIZE(Size))
        goto error;
    if (bh.endianness != (int8_t)ARCH_ENDIANNESS)
        goto error;

    if (bh.stay_size != SIZE(Stay))
        goto error;
    set.stays.Grow(bh.stays_len);
    if (st.Read(SIZE(*set.stays.ptr) * bh.stays_len,
                set.stays.end()) != SIZE(*set.stays.ptr) * bh.stays_len)
        goto error;
    set.stays.len += bh.stays_len;

    set.store.diagnoses.Grow(bh.diagnoses_len);
    if (st.Read(SIZE(*set.store.diagnoses.ptr) * bh.diagnoses_len,
                set.store.diagnoses.end()) != SIZE(*set.store.diagnoses.ptr) * bh.diagnoses_len)
        goto error;
    set.store.procedures.Grow(bh.procedures_len);
    if (st.Read(SIZE(*set.store.procedures.ptr) * bh.procedures_len,
                set.store.procedures.end()) != SIZE(*set.store.procedures.ptr) * bh.procedures_len)
        goto error;

    {
        Size diagnoses_offset = set.store.diagnoses.len;
        Size procedures_offset = set.store.procedures.len;

        for (Size i = set.stays.len - bh.stays_len; i < set.stays.len; i++) {
            Stay *stay = &set.stays[i];

            if (stay->diagnoses.len) {
                stay->diagnoses.ptr = (DiagnosisCode *)(set.store.diagnoses.len - diagnoses_offset);
                set.store.diagnoses.len += stay->diagnoses.len;
            }
            if (stay->procedures.len) {
                stay->procedures.ptr = (ProcedureRealisation *)(set.store.procedures.len - procedures_offset);
                set.store.procedures.len += stay->procedures.len;
            }
        }
    }

    // We assume stays are already sorted in pak files

    set_guard.disable();
    return true;

error:
    LogError("Stay pack file '%1' is malformed", st.filename);
    return false;
}

bool StaySetBuilder::LoadFiles(Span<const char *const> filenames)
{
    for (const char *filename: filenames) {
        LocalArray<char, 16> extension;
        CompressionType compression_type;
        extension.len = GetPathExtension(filename, extension.data, &compression_type);

        bool (StaySetBuilder::*load_func)(StreamReader &st);
        if (TestStr(extension, ".mjson")) {
            load_func = &StaySetBuilder::LoadJson;
        } else if (TestStr(extension, ".mpak")) {
            load_func = &StaySetBuilder::LoadPack;
        } else {
            LogError("Cannot load stays from file '%1' with unknown extension '%2'",
                     filename, extension);
            return false;
        }

        StreamReader st(filename, compression_type);
        if (st.error)
            return false;
        if (!(this->*load_func)(st))
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

    memcpy(out_set, &set, SIZE(set));
    memset(&set, 0, SIZE(set));

    return true;
}
