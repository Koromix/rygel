/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "kutil.hh"
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
#define PACK_SIGNATURE "MOYASTAYPACK"
StaticAssert(SIZE(PackHeader::signature) == SIZE(PACK_SIGNATURE));

// TODO: Flag errors and translate to FG errors
class JsonStayHandler: public BaseJsonHandler<JsonStayHandler> {
    enum class State {
        Default,

        // Stay objects
        StayArray,
        StayObject,
        StayBedAuthorization,
        StayBillId,
        StayBirthdate,
        StayEntryDate,
        StayEntryMode,
        StayEntryOrigin,
        StayExitDate,
        StayExitMode,
        StayExitDestination,
        StayGestationalAge,
        StayStayId,
        StayIgs2,
        StayLastMenstrualPeriod,
        StayNewbornWeight,
        StaySessionCount,
        StaySex,
        StayUnit,
        StayMainDiagnosis,
        StayLinkedDiagnosis,
        StayAssociatedDiagnoses,
        StayProcedures,
        StayTest,

        // Associated diagnosis objects
        AssociatedDiagnosisArray,

        // Procedure realisation objects
        ProcedureArray,
        ProcedureObject,
        ProcedureCode,
        ProcedureDate,
        ProcedurePhase,
        ProcedureActivity,
        ProcedureCount,

        // Test values
        TestObject,
        TestClusterLen,
        TestGhm,
        TestError,
        TestGhs,
        TestRea,
        TestReaSi,
        TestSi,
        TestSrc,
        TestNn1,
        TestNn2,
        TestNn3,
        TestRep
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

    bool StartArray()
    {
        switch (state) {
            case State::Default: { state = State::StayArray; } break;
            case State::StayAssociatedDiagnoses:
                { state = State::AssociatedDiagnosisArray; } break;
            case State::StayProcedures: { state = State::ProcedureArray; } break;

            default: {
                LogError("Unexpected array");
                return false;
            } break;
        }

        return true;
    }
    bool EndArray(rapidjson::SizeType)
    {
        switch (state) {
            case State::StayArray: { state = State::Default; } break;
            case State::AssociatedDiagnosisArray: { state = State::StayObject; } break;
            case State::ProcedureArray: { state = State::StayObject; } break;

            default: {
                LogError("Unexpected end of array");
                return false;
            } break;
        }

        return true;
    }

    bool StartObject()
    {
        switch (state) {
            case State::StayArray: { state = State::StayObject; } break;
            case State::ProcedureArray: { state = State::ProcedureObject; } break;
            case State::StayTest: { state = State::TestObject; } break;

            default: {
                LogError("Unexpected object");
                return false;
            } break;
        }

        return true;
    }
    bool EndObject(rapidjson::SizeType)
    {
        switch (state) {
            case State::StayObject: {
                state = State::StayArray;

                stay.diagnoses.len = out_set->store.diagnoses.len - (Size)stay.diagnoses.ptr;
                stay.procedures.len = out_set->store.procedures.len - (Size)stay.procedures.ptr;
                out_set->stays.Append(stay);
                ResetStay();
            } break;

            case State::ProcedureObject: {
                state = State::ProcedureArray;

                out_set->store.procedures.Append(proc);
                ResetProc();
            } break;

            case State::TestObject: { state = State::StayObject; } break;

            default: {
                LogError("Unexpected end of object");
                return false;
            } break;
        }

        return true;
    }

    bool Key(const char *key, rapidjson::SizeType, bool) {
#define HANDLE_KEY(Key, State) \
            do { \
                if (TestStr(key, (Key))) { \
                    state = State; \
                    return true; \
                } \
             } while (false)

        switch (state) {
            case State::StayObject: {
                HANDLE_KEY("bed_authorization", State::StayBedAuthorization);
                HANDLE_KEY("bill_id", State::StayBillId);
                HANDLE_KEY("birthdate", State::StayBirthdate);
                HANDLE_KEY("entry_date", State::StayEntryDate);
                HANDLE_KEY("entry_mode", State::StayEntryMode);
                HANDLE_KEY("entry_origin", State::StayEntryOrigin);
                HANDLE_KEY("exit_date", State::StayExitDate);
                HANDLE_KEY("exit_mode", State::StayExitMode);
                HANDLE_KEY("exit_destination", State::StayExitDestination);
                HANDLE_KEY("dp", State::StayMainDiagnosis);
                HANDLE_KEY("dr", State::StayLinkedDiagnosis);
                HANDLE_KEY("das", State::StayAssociatedDiagnoses);
                HANDLE_KEY("gestational_age", State::StayGestationalAge);
                HANDLE_KEY("igs2", State::StayIgs2);
                HANDLE_KEY("last_menstrual_period", State::StayLastMenstrualPeriod);
                HANDLE_KEY("newborn_weight", State::StayNewbornWeight);
                HANDLE_KEY("procedures", State::StayProcedures);
                HANDLE_KEY("session_count", State::StaySessionCount);
                HANDLE_KEY("sex", State::StaySex);
                HANDLE_KEY("stay_id", State::StayStayId);
                HANDLE_KEY("unit", State::StayUnit);
                HANDLE_KEY("test", State::StayTest);

                LogError("Unknown stay attribute '%1'", key);
                return false;
            } break;

            case State::ProcedureObject: {
                HANDLE_KEY("code", State::ProcedureCode);
                HANDLE_KEY("date", State::ProcedureDate);
                HANDLE_KEY("phase", State::ProcedurePhase);
                HANDLE_KEY("activity", State::ProcedureActivity);
                HANDLE_KEY("count", State::ProcedureCount);

                LogError("Unknown procedure attribute '%1'", key);
                return false;
            } break;

            case State::TestObject: {
                HANDLE_KEY("cluster_len", State::TestClusterLen);
                HANDLE_KEY("ghm", State::TestGhm);
                HANDLE_KEY("error", State::TestError);
                HANDLE_KEY("ghs", State::TestGhs);
                HANDLE_KEY("rea", State::TestRea);
                HANDLE_KEY("reasi", State::TestReaSi);
                HANDLE_KEY("si", State::TestSi);
                HANDLE_KEY("src", State::TestSrc);
                HANDLE_KEY("nn1", State::TestNn1);
                HANDLE_KEY("nn2", State::TestNn2);
                HANDLE_KEY("nn3", State::TestNn3);
                HANDLE_KEY("rep", State::TestRep);

                LogError("Unknown test attribute '%1'", key);
                return false;
            } break;

            default: {
                LogError("Unexpected key token '%1'", key);
                return false;
            } break;
        }

#undef HANDLE_KEY
    }

    bool Int(int i)
    {
        switch (state) {
            // Stay attributes
            case State::StayStayId: { SetInt(&stay.stay_id, i); } break;
            case State::StayBedAuthorization: { SetInt(&stay.bed_authorization, i); } break;
            case State::StayBillId: { SetInt(&stay.bill_id, i); } break;
            case State::StaySex: {
                if (i == 1) {
                    stay.sex = Sex::Male;
                } else if (i == 2) {
                    stay.sex = Sex::Female;
                } else {
                    LogError("Invalid sex value %1", i);
                }
            } break;
            case State::StayEntryMode: {
                if (i >= 0 && i <= 9) {
                    stay.entry.mode = (int8_t)i;
                } else {
                    LogError("Invalid entry mode value %1", i);
                }
            } break;
            case State::StayEntryOrigin: {
                if (i >= 0 && i <= 9) {
                    stay.entry.origin = (int8_t)i;
                } else {
                    LogError("Invalid entry origin value %1", i);
                }
            } break;
            case State::StayExitMode: {
                if (i >= 0 && i <= 9) {
                    stay.exit.mode = (int8_t)i;
                } else {
                    LogError("Invalid exit mode value %1", i);
                }
            } break;
            case State::StayExitDestination: {
                if (i >= 0 && i <= 9) {
                    stay.exit.destination = (int8_t)i;
                } else {
                    LogError("Invalid exit destination value %1", i);
                }
            } break;
            case State::StayUnit: { SetInt(&stay.unit.number, i);} break;
            case State::StaySessionCount: { SetInt(&stay.session_count, i); } break;
            case State::StayIgs2: { SetInt(&stay.igs2, i); } break;
            case State::StayGestationalAge: { SetInt(&stay.gestational_age, i); } break;
            case State::StayNewbornWeight: { SetInt(&stay.newborn_weight, i); } break;

            // Procedure attributes
            case State::ProcedurePhase: { SetInt(&proc.phase, i); } break;
            case State::ProcedureActivity: {
                if (LIKELY(i >= 0)) {
                    int activities_dec = i;
                    while (activities_dec) {
                        int activity = activities_dec % 10;
                        activities_dec /= 10;
                        if (LIKELY(activity < 8)) {
                            proc.activities |= (uint8_t)(1 << activity);
                        } else {
                            LogError("Procedure activity %1 outside of %2 - %3", i, 0, 7);
                        }
                    }
                } else {
                    LogError("Procedure activity %1 cannot be a negative value", i);
                }
            } break;
            case State::ProcedureCount: { SetInt(&proc.count, i); } break;

            // Test attributes
#ifndef DISABLE_TESTS
            case State::TestClusterLen: { SetInt(&stay.test.cluster_len, i); } break;
            case State::TestError: { SetInt(&stay.test.error, i); } break;
            case State::TestGhs: {
                // TODO: Use GhsCode() constructor to validate number
                SetInt(&stay.test.ghs.number, i);
            } break;
            case State::TestRea: { SetInt(&stay.test.supplements.rea, i); } break;
            case State::TestReaSi: { SetInt(&stay.test.supplements.reasi, i); } break;
            case State::TestSi: { SetInt(&stay.test.supplements.si, i); } break;
            case State::TestSrc: { SetInt(&stay.test.supplements.src, i); } break;
            case State::TestNn1: { SetInt(&stay.test.supplements.nn1, i); } break;
            case State::TestNn2: { SetInt(&stay.test.supplements.nn2, i); } break;
            case State::TestNn3: { SetInt(&stay.test.supplements.nn3, i); } break;
            case State::TestRep: { SetInt(&stay.test.supplements.rep, i); } break;
#else
            case State::TestClusterLen: {} break;
            case State::TestError: {} break;
            case State::TestGhs: {} break;
            case State::TestRea: {} break;
            case State::TestReaSi: {} break;
            case State::TestSi: {} break;
            case State::TestSrc: {} break;
            case State::TestNn1: {} break;
            case State::TestNn2: {} break;
            case State::TestNn3: {} break;
            case State::TestRep: {} break;
#endif

            default: {
                LogError("Unexpected integer value %1", i);
                return false;
            } break;
        }

        return HandleValueEnd();
    }
    bool String(const char *str, rapidjson::SizeType, bool)
    {
        switch (state) {
            // Stay attributes
            case State::StaySex: {
                if (TestStr(str, "H") || TestStr(str, "h") ||
                        TestStr(str, "M") || TestStr(str, "m")) {
                    stay.sex = Sex::Male;
                } else if (TestStr(str, "F") || TestStr(str, "f")) {
                    stay.sex = Sex::Female;
                } else {
                    LogError("Invalid sex value '%1'", str);
                }
            } break;
            case State::StayBirthdate:
                { SetDateOrError(&stay.birthdate, str, Stay::Error::MalformedBirthdate); } break;
            case State::StayEntryDate: { SetDate(&stay.dates[0], str); } break;
            case State::StayEntryMode: {
                if (str[0] && !str[1]) {
                    stay.entry.mode = (char)(str[0] - '0');
                } else {
                    LogError("Invalid entry mode value '%1'", str);
                }
            } break;
            case State::StayEntryOrigin: {
                if (!str[0]) {
                    stay.entry.origin = 0;
                } else if (((str[0] >= '0' && str[0] <= '9')
                            || str[0] == 'R' || str[0] == 'r') && !str[1]) {
                    // This is probably incorrect for either 'R' or 'r' but this is what
                    // the machine code in FG2017 does, so keep it that way.
                    stay.entry.origin = (char)(str[0] - '0');
                } else {
                    LogError("Invalid entry origin value '%1'", str);
                }
            } break;
            case State::StayExitDate: { SetDate(&stay.dates[1], str); } break;
            case State::StayExitMode: {
                if (str[0] && !str[1]) {
                    stay.exit.mode = (char)(str[0] - '0');
                } else {
                    LogError("Invalid exit mode value '%1'", str);
                }
            } break;
            case State::StayExitDestination: {
                if (!str[0]) {
                    stay.exit.destination = 0;
                } else if ((str[0] >= '0' && str[0] <= '9') && !str[1]) {
                    stay.exit.destination = (char)(str[0] - '0');
                } else {
                    LogError("Invalid exit destination value '%1'", str);
                }
            } break;
            case State::StayLastMenstrualPeriod:
                { SetDate(&stay.last_menstrual_period, str); } break;

            // Diagnoses (part of Stay, separated for clarity)
            case State::StayMainDiagnosis: {
                stay.main_diagnosis = DiagnosisCode::FromString(str);
                out_set->store.diagnoses.Append(stay.main_diagnosis);
            } break;
            case State::StayLinkedDiagnosis: {
                stay.linked_diagnosis = DiagnosisCode::FromString(str);
                out_set->store.diagnoses.Append(stay.linked_diagnosis);
            } break;
            case State::AssociatedDiagnosisArray: {
                DiagnosisCode diag = DiagnosisCode::FromString(str);
                out_set->store.diagnoses.Append(diag);
            } break;

            // Procedure attributes
            case State::ProcedureCode: { proc.proc = ProcedureCode::FromString(str); } break;
            case State::ProcedureDate: { SetDate(&proc.date, str); } break;

            // Test attributes
#ifndef DISABLE_TESTS
            case State::TestGhm: { stay.test.ghm = GhmCode::FromString(str); } break;
#else
            case State::TestGhm: {} break;
#endif

            default: {
                LogError("Unexpected string value '%1'", str);
                return false;
            } break;
        }

        return HandleValueEnd();
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

    template <typename T>
    void SetIntOrError(T *dest, int i, Stay::Error error_flag)
    {
        if (!SetInt(dest, i)) {
            stay.error_mask |= (uint32_t)error_flag;
        }
    }

    void SetDateOrError(Date *dest, const char *date_str, Stay::Error error_flag)
    {
        if (!SetDate(dest, date_str)) {
            stay.error_mask |= (uint32_t)error_flag;
        }
    }

    bool HandleValueEnd()
    {
        if ((int)state >= (int)State::TestObject) {
            state = State::TestObject;
            return true;
        } else if ((int)state >= (int)State::ProcedureArray) {
            state = State::ProcedureObject;
            return true;
        } else if ((int)state >= (int)State::AssociatedDiagnosisArray) {
            return true;
        } else if ((int)state >= (int)State::StayArray) {
            state = State::StayObject;
            return true;
        } else {
            LogError("Unexpected value");
            return false;
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

static bool LoadStayPack(StreamReader &st, StaySet *out_set)
{
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
    out_set->stays.Grow(bh.stays_len);
    if (st.Read(SIZE(*out_set->stays.ptr) * bh.stays_len,
                out_set->stays.end()) != SIZE(*out_set->stays.ptr) * bh.stays_len)
        goto error;
    out_set->stays.len += bh.stays_len;

    out_set->store.diagnoses.Grow(bh.diagnoses_len);
    if (st.Read(SIZE(*out_set->store.diagnoses.ptr) * bh.diagnoses_len,
                out_set->store.diagnoses.end()) != SIZE(*out_set->store.diagnoses.ptr) * bh.diagnoses_len)
        goto error;
    out_set->store.procedures.Grow(bh.procedures_len);
    if (st.Read(SIZE(*out_set->store.procedures.ptr) * bh.procedures_len,
                out_set->store.procedures.end()) != SIZE(*out_set->store.procedures.ptr) * bh.procedures_len)
        goto error;

    {
        Size diagnoses_offset = out_set->store.diagnoses.len;
        Size procedures_offset = out_set->store.procedures.len;

        for (Size i = out_set->stays.len - bh.stays_len; i < out_set->stays.len; i++) {
            Stay *stay = &out_set->stays[i];

            if (stay->diagnoses.len) {
                stay->diagnoses.ptr = (DiagnosisCode *)(out_set->store.diagnoses.len - diagnoses_offset);
                out_set->store.diagnoses.len += stay->diagnoses.len;
            }
            if (stay->procedures.len) {
                stay->procedures.ptr = (ProcedureRealisation *)(out_set->store.procedures.len - procedures_offset);
                out_set->store.procedures.len += stay->procedures.len;
            }
        }
    }

    return true;

error:
    LogError("Error while reading stay pack file '%1'", st.filename);
    return false;
}

bool StaySetBuilder::Load(StreamReader &st, StaySetDataType type)
{
    DEFER_NC(set_guard, stays_len = set.store.diagnoses.len,
                        diagnoses_len = set.store.diagnoses.len,
                        procedures_len = set.store.procedures.len) {
        set.stays.RemoveFrom(stays_len);
        set.store.diagnoses.RemoveFrom(diagnoses_len);
        set.store.procedures.RemoveFrom(procedures_len);
    };

    switch (type) {
        case StaySetDataType::Json: {
            Size start_len = set.stays.len;

            JsonStayHandler json_handler(&set);
            if (!ParseJsonFile(st, &json_handler))
                return false;

            std::stable_sort(set.stays.begin() + start_len, set.stays.end(),
                             [](const Stay &stay1, const Stay &stay2) {
                return MultiCmp(stay1.stay_id - stay2.stay_id,
                                stay1.bill_id - stay2.bill_id) < 0;
            });
        } break;

        case StaySetDataType::Pack: {
            if (!LoadStayPack(st, &set))
                return false;

            // Assume stays are already sorted in pak files
        } break;
    }

    set_guard.disable();
    return true;
}

bool StaySetBuilder::LoadFiles(Span<const char *const> filenames)
{
    for (const char *filename: filenames) {
        LocalArray<char, 16> extension;
        CompressionType compression_type;
        extension.len = GetPathExtension(filename, extension.data, &compression_type);

        StaySetDataType data_type;
        if (TestStr(extension, ".mjson")) {
            data_type = StaySetDataType::Json;
        } else if (TestStr(extension, ".mpak")) {
            data_type = StaySetDataType::Pack;
        } else {
            LogError("Cannot load stays from file '%1' with unknown extension '%2'",
                     filename, extension);
            return false;
        }

        StreamReader st(filename, compression_type);
        if (st.error)
            return false;
        if (!Load(st, data_type))
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
