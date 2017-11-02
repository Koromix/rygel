/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "kutil.hh"
#include "data.hh"
#include "tables.hh"
#include "../../lib/rapidjson/reader.h"
#include "../../lib/rapidjson/error/en.h"

// TODO: Version this data structure, deal with endianness
struct BundleHeader {
    size_t stays_len;
    size_t diagnoses_len;
    size_t procedures_len;
};

template <typename T>
class JsonHandler: public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, JsonHandler<T>> {
public:
    bool Uint(unsigned int u)
    {
        if (u <= INT_MAX) {
            return (T *)this->Int((int)u);
        } else {
            return (T *)this->Default();
        }
    }

    bool Default()
    {
        LogError("Unsupported value type (not a string or 32-bit integer)");
        return false;
    }

    template <typename U>
    bool SetInt(U *dest, int i)
    {
        U value = (U)i;
        if (value != i) {
            LogError("Value %1 outside of range %2 - %3",
                     i, (int)std::numeric_limits<U>::min(), (int)std::numeric_limits<U>::max());
            return false;
        }
        *dest = value;
        return true;
    }

    bool SetDate(Date *dest, const char *date_str)
    {
        Date date = Date::FromString(date_str, false);
        if (!date.value)
            return false;
        *dest = date;
        return true;
    }
};

class JsonAuthorizationHandler: public JsonHandler<JsonAuthorizationHandler> {
    enum class State {
        Default,

        AuthArray,
        AuthObject,
        AuthAuthorization,
        AuthBeginDate,
        AuthEndDate,
        AuthUnit
    };

    State state = State::Default;

    Authorization auth = {};

public:
    HeapArray<Authorization> *out_authorizations;

    JsonAuthorizationHandler(HeapArray<Authorization> *out_authorizations = nullptr)
        : out_authorizations(out_authorizations) {}

    bool StartArray()
    {
        if (state != State::Default) {
            LogError("Unexpected array");
            return false;
        }

        state = State::AuthArray;
        return true;
    }
    bool EndArray(rapidjson::SizeType)
    {
        if (state != State::AuthArray) {
            LogError("Unexpected end of array");
            return false;
        }

        state = State::Default;
        return true;
    }

    bool StartObject()
    {
        if (state != State::AuthArray) {
            LogError("Unexpected object");
            return false;
        }

        state = State::AuthObject;
        return true;
    }
    bool EndObject(rapidjson::SizeType)
    {
        if (state != State::AuthObject) {
            LogError("Unexpected end of object");
            return false;
        }

        if (!auth.dates[1].value) {
            static const Date default_end_date = ConvertDate1980(UINT16_MAX);
            auth.dates[1] = default_end_date;
        }

        out_authorizations->Append(auth);
        auth = {};

        state = State::AuthArray;
        return true;
    }

    bool Key(const char *key, rapidjson::SizeType, bool) {
#define HANDLE_KEY(Key, State) \
            do { \
                if (StrTest(key, (Key))) { \
                    state = State; \
                    return true; \
                } \
             } while (false)

        if (state != State::AuthObject) {
            LogError("Unexpected key token '%1'", key);
            return false;
        }

        HANDLE_KEY("authorization", State::AuthAuthorization);
        HANDLE_KEY("begin_date", State::AuthBeginDate);
        HANDLE_KEY("end_date", State::AuthEndDate);
        HANDLE_KEY("unit", State::AuthUnit);

        LogError("Unknown authorization attribute '%1'", key);
        return false;

#undef HANDLE_KEY
    }

    bool Int(int i)
    {
        switch (state) {
            case State::AuthAuthorization: {
                if (i >= 0 && i < 100) {
                    auth.type = (int8_t)i;
                } else {
                    LogError("Invalid authorization type %1", i);
                }
            } break;
            case State::AuthUnit: {
                if (i >= 0 && i < 10000) {
                    auth.unit.number = (int16_t)i;
                } else {
                    LogError("Invalid unit code %1", i);
                }
            } break;

            default: {
                LogError("Unexpected integer value %1", i);
                return false;
            } break;
        }

        state = State::AuthObject;
        return true;
    }
    bool String(const char *str, rapidjson::SizeType, bool)
    {
        switch (state) {
            case State::AuthAuthorization: {
                int8_t type;
                if (sscanf(str, "%" SCNd8, &type) == 1 && type >= 0 && type < 100) {
                    auth.type = type;
                } else {
                    LogError("Invalid authorization type '%1'", str);
                }
            } break;
            case State::AuthBeginDate: { SetDate(&auth.dates[0], str); } break;
            case State::AuthEndDate: { SetDate(&auth.dates[1], str); } break;
            case State::AuthUnit: {
                int16_t unit;
                if (StrTest(str, "facility")) {
                    auth.unit.number = INT16_MAX;
                } else if (sscanf(str, "%" SCNd16, &unit) == 1 && unit >= 0 && unit < 10000) {
                    auth.unit.number = unit;
                } else {
                    LogError("Invalid unit code '%1'", str);
                }
            } break;

            default: {
                LogError("Unexpected string value '%1'", str);
                return false;
            } break;
        }

        state = State::AuthObject;
        return true;
    }

    // FIXME: Why do I need that?
    bool Uint(unsigned int u)
    {
        if (u <= INT_MAX) {
            return Int((int)u);
        } else {
            return Default();
        }
    }
};

// TODO: Flag errors and translate to FG errors
class JsonStayHandler: public JsonHandler<JsonStayHandler> {
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
    ProcedureRealisation proc = {};

public:
    StaySet *out_set;

    JsonStayHandler(StaySet *out_set = nullptr)
        : out_set(out_set)
    {
        ResetStay();
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

                stay.diagnoses.len = out_set->store.diagnoses.len - (size_t)stay.diagnoses.ptr;
                stay.procedures.len = out_set->store.procedures.len - (size_t)stay.procedures.ptr;
                out_set->stays.Append(stay);
                ResetStay();
            } break;

            case State::ProcedureObject: {
                state = State::ProcedureArray;

                out_set->store.procedures.Append(proc);
                proc = {};
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
                if (StrTest(key, (Key))) { \
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
                if (i >= 0 && i < 8) {
                    proc.activities = (uint8_t)(1 << i);
                } else {
                    LogError("Procedure activity %1 outside of %2 - %3", i, 0, 7);
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
                if (StrTest(str, "H") || StrTest(str, "h")) {
                    stay.sex = Sex::Male;
                } else if (StrTest(str, "F") || StrTest(str, "f")) {
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
                    stay.entry.mode = str[0] - '0';
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
                    // the machine code in FG2017.exe does, so keep it that way.
                    stay.entry.origin = str[0] - '0';
                } else {
                    LogError("Invalid entry origin value '%1'", str);
                }
            } break;
            case State::StayExitDate: { SetDate(&stay.dates[1], str); } break;
            case State::StayExitMode: {
                if (str[0] && !str[1]) {
                    stay.exit.mode = str[0] - '0';
                } else {
                    LogError("Invalid exit mode value '%1'", str);
                }
            } break;
            case State::StayExitDestination: {
                if (!str[0]) {
                    stay.exit.destination = 0;
                } else if ((str[0] >= '0' && str[0] <= '9') && !str[1]) {
                    stay.exit.destination = str[0] - '0';
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

    // FIXME: Why do I need that?
    bool Uint(unsigned int u)
    {
        if (u <= INT_MAX) {
            return Int((int)u);
        } else {
            return Default();
        }
    }

private:
    void ResetStay()
    {
        stay = {};
        stay.diagnoses.ptr = (DiagnosisCode *)out_set->store.diagnoses.len;
        stay.procedures.ptr = (ProcedureRealisation *)out_set->store.procedures.len;
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

bool StaySet::SaveBundle(FILE *fp, const char *filename) const
{
    BundleHeader bh;

    bh.stays_len = stays.len;
    bh.diagnoses_len = store.diagnoses.len;
    bh.procedures_len = store.procedures.len;

    clearerr(fp);
    fwrite(&bh, 1, sizeof(bh), fp);
    fwrite(stays.ptr, sizeof(*stays.ptr), stays.len, fp);
    for (const Stay &stay: stays) {
        fwrite(stay.diagnoses.ptr, sizeof(*stay.diagnoses.ptr), stay.diagnoses.len, fp);
    }
    for (const Stay &stay: stays) {
        fwrite(stay.procedures.ptr, sizeof(*stay.procedures.ptr), stay.procedures.len, fp);
    }
    if (ferror(fp)) {
        LogError("Error while writing stay bundle to '%1'", filename ? filename : "?");
        return false;
    }

    return true;
}

template <typename T>
bool ParseJsonFile(const char *filename, T *json_handler)
{
    FILE *fp = fopen(filename, "rb" FOPEN_COMMON_FLAGS);
    if (!fp) {
        LogError("Cannot open '%1': %2", filename, strerror(errno));
        return false;
    }
    DEFER { fclose(fp); };

    // This is mostly copied from RapidJSON's FileReadStream, but this
    // version calculates current line number and offset.
    class FileReadStreamEx
    {
        FILE *fp;

        LocalArray<char, 1024 * 1024> buffer;
        size_t buffer_offset = 0;
        size_t file_offset = 0;

    public:
        typedef char Ch MAYBE_UNUSED;

        size_t line_number = 1;
        size_t line_offset = 1;

        FileReadStreamEx(FILE *fp)
            : fp(fp)
        {
            Read();
        }

        char Peek() const { return buffer[buffer_offset]; }
        char Take()
        {
            char c = buffer[buffer_offset];
            if (c == '\n') {
                line_number++;
                line_offset = 1;
            } else {
                line_offset++;
            }
            Read();
            return c;
        }
        size_t Tell() const { return file_offset + buffer_offset; }

        // Not implemented
        void Put(char) {}
        void Flush() {}
        char *PutBegin() { return 0; }
        size_t PutEnd(char *) { return 0; }

        // For encoding detection only
        const char *Peek4() const
        {
            if (buffer.len - buffer_offset < 4)
                return 0;
            return buffer.data + buffer_offset;
        }

    private:
        void Read()
        {
            if (buffer_offset + 1 < buffer.len) {
                buffer_offset++;
            } else if (fp) {
                file_offset += buffer.len;
                buffer.len = fread(buffer.data, 1, sizeof(buffer.data), fp);
                buffer_offset = 0;

                if (buffer.len < sizeof(buffer.data)) {
                    buffer.Append('\0');
                    fp = nullptr;
                }
            }
        }
    };

    {
        FileReadStreamEx json_stream(fp);
        PushLogHandler([&](FILE *fp) {
            Print(fp, "%1(%2:%3): ", filename, json_stream.line_number, json_stream.line_offset);
        });
        DEFER { PopLogHandler(); };

        rapidjson::Reader json_reader;
        if (!json_reader.Parse(json_stream, *json_handler)) {
            rapidjson::ParseErrorCode err_code = json_reader.GetParseErrorCode();
            LogError("%1 (%2)", GetParseError_En(err_code), json_reader.GetErrorOffset());
            return false;
        }
    }

    return true;
}

bool LoadAuthorizationFile(const char *filename, AuthorizationSet *out_set)
{
    DEFER_NC(out_guard, len = out_set->authorizations.len)
        { out_set->authorizations.RemoveFrom(len); };

    JsonAuthorizationHandler json_handler(&out_set->authorizations);
    if (!ParseJsonFile(filename, &json_handler))
        return false;

    for (const Authorization &auth: out_set->authorizations) {
        out_set->authorizations_map.Append(&auth);
    }

    out_guard.disable();
    return true;
}

ArrayRef<const Authorization> AuthorizationSet::FindUnit(UnitCode unit) const
{
    ArrayRef<const Authorization> auths;
    auths.ptr = authorizations_map.FindValue(unit, nullptr);
    if (!auths.ptr)
        return {};

    {
        const Authorization *end_auth = auths.ptr + 1;
        while (end_auth < authorizations.end() &&
               end_auth->unit == unit) {
            end_auth++;
        }
        auths.len = (size_t)(end_auth - auths.ptr);
    }

    return auths;
}

const Authorization *AuthorizationSet::FindUnit(UnitCode unit, Date date) const
{
    const Authorization *auth = authorizations_map.FindValue(unit, nullptr);
    if (!auth)
        return nullptr;

    do {
        if (date >= auth->dates[0] && date < auth->dates[1])
            return auth;
    } while (++auth < authorizations.ptr + authorizations.len &&
             auth->unit == unit);

    return nullptr;
}

static bool LoadStayBundle(const char *filename, StaySet *out_set)
{
    FILE *fp = fopen(filename, "rb" FOPEN_COMMON_FLAGS);
    if (!fp) {
        LogError("Cannot open '%1': %2", filename, strerror(errno));
        return false;
    }
    DEFER { fclose(fp); };

    size_t diagnoses_offset;
    size_t procedures_offset;

    BundleHeader bh;
    if (fread(&bh, sizeof(bh), 1, fp) != 1)
        goto error;

    out_set->stays.Grow(bh.stays_len);
    if (fread(out_set->stays.end(), sizeof(*out_set->stays.ptr), bh.stays_len, fp) != bh.stays_len)
        goto error;
    out_set->stays.len += bh.stays_len;

    out_set->store.diagnoses.Grow(bh.diagnoses_len);
    if (fread(out_set->store.diagnoses.end(), sizeof(*out_set->store.diagnoses.ptr),
              bh.diagnoses_len, fp) != bh.diagnoses_len)
        goto error;
    out_set->store.procedures.Grow(bh.procedures_len);
    if (fread(out_set->store.procedures.end(), sizeof(*out_set->store.procedures.ptr),
              bh.procedures_len, fp) != bh.procedures_len)
        goto error;

    diagnoses_offset = out_set->store.diagnoses.len;
    procedures_offset = out_set->store.procedures.len;
    for (size_t i = out_set->stays.len - bh.stays_len; i < out_set->stays.len; i++) {
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

    return true;

error:
    // TODO: Adapt message depending on error code
    LogError("Stay bundle file '%1' is corrupted", filename);
    return false;
}

bool StaySetBuilder::LoadFile(ArrayRef<const char *const> filenames)
{
    DEFER_NC(set_guard, stays_len = set.store.diagnoses.len,
                        diagnoses_len = set.store.diagnoses.len,
                        procedures_len = set.store.procedures.len) {
        set.stays.RemoveFrom(stays_len);
        set.store.diagnoses.RemoveFrom(diagnoses_len);
        set.store.procedures.RemoveFrom(procedures_len);
    };

    for (const char *filename: filenames) {
        const char *ext = strrchr(filename, '.');
        if (!ext || strpbrk(ext, PATH_SEPARATORS)) {
            LogError("Cannot load stays from file '%1' without extension", filename);
            return false;
        }

        if (StrTest(ext, ".json")) {
            JsonStayHandler json_handler(&set);
            if (!ParseJsonFile(filename, &json_handler))
                return false;
        } else if (StrTest(ext, ".stb")) {
            if (!LoadStayBundle(filename, &set))
                return false;
        } else {
            LogError("Cannot load stays from file '%1' with unsupported format '%2'",
                     filename, ext);
            return false;
        }
    }

    set_guard.disable();
    return true;
}

bool StaySetBuilder::Finish(StaySet *out_set)
{
    std::stable_sort(set.stays.begin(), set.stays.end(),
              [](const Stay &stay1, const Stay &stay2) {
        return stay1.stay_id < stay2.stay_id;
    });

    for (Stay &stay: set.stays) {
#define FIX_ARRAYREF(ArrayRefName) \
            stay.ArrayRefName.ptr = set.store.ArrayRefName.ptr + \
                                    (size_t)stay.ArrayRefName.ptr

        FIX_ARRAYREF(diagnoses);
        FIX_ARRAYREF(procedures);

#undef FIX_ARRAYREF
    }

    memcpy(out_set, &set, sizeof(set));
    memset(&set, 0, sizeof(set));

    return true;
}
