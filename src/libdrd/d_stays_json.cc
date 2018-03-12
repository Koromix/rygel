// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../common/kutil.hh"
#include "../common/json.hh"
#include "d_stays.hh"

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
                if (TestStr(key, "admin_id")) {
                    SetInt(value, &stay.admin_id);
                } else if (TestStr(key, "bed_authorization")) {
                    SetInt(value, &stay.bed_authorization);
                } else if (TestStr(key, "bill_id")) {
                    SetInt(value, &stay.bill_id);
                } else if (TestStr(key, "birthdate")) {
                    bool success = SetDate(value, (int)ParseFlag::End, &stay.birthdate);
                    SetErrorFlag(Stay::Error::MalformedBirthdate, !success);
                } else if (TestStr(key, "entry_date")) {
                    bool success = SetDate(value, (int)ParseFlag::End, &stay.entry.date);
                    SetErrorFlag(Stay::Error::MalformedEntryDate, !success);
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
                    bool success = SetDate(value, (int)ParseFlag::End, &stay.exit.date);
                    SetErrorFlag(Stay::Error::MalformedExitDate, !success);
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
                        stay.main_diagnosis = DiagnosisCode::FromString(value.u.str, false);
                        SetErrorFlag(Stay::Error::MalformedMainDiagnosis,
                                     !stay.main_diagnosis.IsValid());
                    } else {
                        UnexpectedType(value.type);
                    }
                } else if (TestStr(key, "dr")) {
                    if (value.type == JsonValue::Type::String) {
                        stay.linked_diagnosis = DiagnosisCode::FromString(value.u.str, false);
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
                    SetDate(value, &stay.last_menstrual_period);
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
                } else if (TestStr(key, "unit")) {
                    SetInt(value, &stay.unit.number);
                } else {
                    return UnknownAttribute(key);
                }
            } break;

            case State::AssociatedDiagnosisArray: {
                if (value.type == JsonValue::Type::String) {
                    DiagnosisCode diag = DiagnosisCode::FromString(value.u.str, false);
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
                        proc.proc = ProcedureCode::FromString(value.u.str);
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
                if (TestStr(key, "cluster_len")) {
                    SetInt(value, &test.cluster_len);
                } else if (TestStr(key, "ghm")) {
                    if (value.type == JsonValue::Type::String) {
                        test.ghm = GhmCode::FromString(value.u.str);
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
        return MultiCmp(stay1.admin_id - stay2.admin_id,
                        stay1.bill_id - stay2.bill_id) < 0;
    });

    set_guard.disable();
    return true;
}
