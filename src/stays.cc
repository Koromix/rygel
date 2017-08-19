#include "kutil.hh"
#include "stays.hh"
#include "../lib/rapidjson/reader.h"
#include "../lib/rapidjson/error/en.h"

using namespace rapidjson;

class JsonStayHandler: public BaseReaderHandler<UTF8<>, JsonStayHandler> {
    enum class State {
        Default,

        // Stay objects
        StayArray,
        StayObject,
        StayBirthdate,
        StayEntryDate,
        StayEntryMode,
        StayEntryOrigin,
        StayExitDate,
        StayExitMode,
        StayExitDestination,
        StayGestationalAge,
        StayIdentifier,
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
        StayTestGhm,
        StayTestRssLen,

        // Associated diagnosis objects
        AssociatedDiagnosisArray,

        // Procedure objects
        ProcedureArray,
        ProcedureObject,
        ProcedureCode,
        ProcedureDate,
        ProcedurePhase,
        ProcedureActivity,
        ProcedureCount
    };

    State state = State::Default;

    Stay stay;
    Procedure proc = {};

public:
    StaySet *out_set;
    size_t minor_errors = 0;

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
    bool EndArray(SizeType)
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

            default: {
                LogError("Unexpected object");
                return false;
            } break;
        }

        return true;
    }
    bool EndObject(SizeType)
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

            default: {
                LogError("Unexpected end of object");
                return false;
            } break;
        }

        return true;
    }

    bool Key(const char *key, SizeType, bool) {
#define HANDLE_KEY(Key, State) \
            do { \
                if (!strcmp(key, (Key))) { \
                    state = State; \
                    return true; \
                } \
             } while (false)

        switch (state) {
            case State::StayObject: {
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
                HANDLE_KEY("stay_id", State::StayIdentifier);
                HANDLE_KEY("unit", State::StayUnit);
                HANDLE_KEY("test_ghm", State::StayTestGhm);
                HANDLE_KEY("test_rss_len", State::StayTestRssLen);

                LogError("Unknown stay attribute '%1'", key);
                SetErrorFlag();

                return false;
            } break;

            case State::ProcedureObject: {
                HANDLE_KEY("code", State::ProcedureCode);
                HANDLE_KEY("date", State::ProcedureDate);
                HANDLE_KEY("phase", State::ProcedurePhase);
                HANDLE_KEY("activity", State::ProcedureActivity);
                HANDLE_KEY("count", State::ProcedureCount);

                LogError("Unknown procedure attribute '%1'", key);
                SetErrorFlag();

                return true;
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
            case State::StayIdentifier: { SetInt(&stay.stay_id, i); } break;
            case State::StaySex: {
                if (i == 1) {
                    stay.sex = Sex::Male;
                } else if (i == 2) {
                    stay.sex = Sex::Female;
                } else {
                    LogError("Invalid sex value %1", i);
                    SetErrorFlag();
                }
            } break;
            case State::StayEntryMode: {
                if (i >= 0 && i <= 9) {
                    stay.entry.mode = i;
                } else {
                    LogError("Invalid entry mode value %1", i);
                    SetErrorFlag();
                }
            } break;
            case State::StayEntryOrigin: {
                if (i >= 0 && i <= 9) {
                    stay.entry.origin = i;
                } else {
                    LogError("Invalid entry origin value %1", i);
                    SetErrorFlag();
                }
            } break;
            case State::StayExitMode: {
                if (i >= 0 && i <= 9) {
                    stay.exit.mode = i;
                } else {
                    LogError("Invalid exit mode value %1", i);
                    SetErrorFlag();
                }
            } break;
            case State::StayExitDestination: {
                if (i >= 0 && i <= 9) {
                    stay.exit.destination = i;
                } else {
                    LogError("Invalid exit destination value %1", i);
                    SetErrorFlag();
                }
            } break;
            case State::StayUnit: { SetInt(&stay.unit_code.value, i);} break;
            case State::StaySessionCount: { SetInt(&stay.session_count, i); } break;
            case State::StayIgs2: { SetInt(&stay.igs2, i); } break;
            case State::StayGestationalAge: { SetInt(&stay.gestational_age, i); } break;
            case State::StayNewbornWeight: { SetInt(&stay.newborn_weight, i); } break;
#ifdef TESTING
            case State::StayTestRssLen: { SetInt(&stay.test.rss_len, i); } break;
#else
            case State::StayTestRssLen: {} break;
#endif

            // Procedure attributes
            case State::ProcedurePhase: { SetInt(&proc.phase, i); } break;
            case State::ProcedureActivity: {
                if (i >= 0 && i < 8) {
                    proc.activities = 1 << i;
                } else {
                    LogError("Procedure activity %1 outside of %2 - %3", i, 0, 7);
                    SetErrorFlag();
                }
            } break;
            case State::ProcedureCount: { SetInt(&proc.count, i); } break;

            default: {
                LogError("Unexpected integer value %1", i);
                SetErrorFlag();
            } break;
        }

        return HandleValueEnd();
    }
    bool String(const char *str, SizeType, bool)
    {
        switch (state) {
            // Stay attributes
            case State::StaySex: {
                if (!strcmp(str, "H") || !strcmp(str, "h")) {
                    stay.sex = Sex::Male;
                } else if (!strcmp(str, "F") || !strcmp(str, "f")) {
                    stay.sex = Sex::Female;
                } else {
                    LogError("Invalid sex value '%1'", str);
                    SetErrorFlag();
                }
            } break;
            case State::StayBirthdate: { SetDate(&stay.birthdate, str); } break;
            case State::StayEntryDate: { SetDate(&stay.dates[0], str); } break;
            case State::StayEntryMode: {
                if (str[0] && !str[1]) {
                    stay.entry.mode = str[0] - '0';
                } else {
                    LogError("Invalid entry mode value '%1'", str);
                    SetErrorFlag();
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
                    SetErrorFlag();
                }
            } break;
            case State::StayExitDate: { SetDate(&stay.dates[1], str); } break;
            case State::StayExitMode: {
                if (str[0] && !str[1]) {
                    stay.exit.mode = str[0] - '0';
                } else {
                    LogError("Invalid exit mode value '%1'", str);
                    SetErrorFlag();
                }
            } break;
            case State::StayExitDestination: {
                if (!str[0]) {
                    stay.exit.destination = 0;
                } else if ((str[0] >= '0' && str[0] <= '9') && !str[1]) {
                    stay.exit.destination = str[0] - '0';
                } else {
                    LogError("Invalid exit destination value '%1'", str);
                    SetErrorFlag();
                }
            } break;
            case State::StayLastMenstrualPeriod:
                { SetDate(&stay.last_menstrual_period, str); } break;
#ifdef TESTING
            case State::StayTestGhm: { stay.test.ghm = GhmCode::FromString(str); } break;
#else
            case State::StayTestGhm: {} break;
#endif

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
            case State::ProcedureCode: { proc.code = ProcedureCode::FromString(str); } break;
            case State::ProcedureDate: { SetDate(&proc.date, str); } break;

            default: {
                LogError("Unexpected string value '%1'", str);
                SetErrorFlag();
            } break;
        }

        return HandleValueEnd();
    }

    bool Uint(unsigned int u)
    {
        if (u <= INT_MAX) {
            return Int((int)u);
        } else {
            return Default();
        }
    }

    bool Default()
    {
        LogError("Unsupported value type (not a string or 32-bit integer)");
        SetErrorFlag();
        return HandleValueEnd();
    }

private:
    void ResetStay()
    {
        stay = {};
        stay.diagnoses.ptr = (DiagnosisCode *)out_set->store.diagnoses.len;
        stay.procedures.ptr = (Procedure *)out_set->store.procedures.len;
    }

    void SetErrorFlag()
    {
        minor_errors++;
        stay.error_mask |= MaskEnum(Stay::Error::Load);
    }

    template <typename T>
    void SetInt(T *dest, int i)
    {
        *dest = i;
        if (*dest != i) {
            LogError("Value %1 outside of range %2 - %3",
                     i, (int)std::numeric_limits<T>::min(), (int)std::numeric_limits<T>::max());
            SetErrorFlag();
            return;
        }

        *dest = i;
    }
    void SetDate(Date *dest, const char *date_str)
    {
        *dest = Date::FromString(date_str);
        if (!dest->value) {
            LogError("Invalid date string '%1'", date_str);
            SetErrorFlag();
        }
    }

    bool HandleValueEnd()
    {
        if ((int)state >= (int)State::ProcedureArray) {
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

        LocalArray<char, 65536> buffer;
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

    FileReadStreamEx json_stream(fp);

    PushLogHandler([&](FILE *fp) {
        Print(fp, "%1(%2:%3): ", filename, json_stream.line_number, json_stream.line_offset);
    });
    DEFER { PopLogHandler(); };

    {
        Reader json_reader;
        if (!json_reader.Parse(json_stream, *json_handler)) {
            ParseErrorCode err_code = json_reader.GetParseErrorCode();
            LogError("%1 (%2)", GetParseError_En(err_code), json_reader.GetErrorOffset());
            return false;
        }
    }

    return true;
}

bool StaySetBuilder::LoadJson(ArrayRef<const char *const> filenames)
{
    DEFER_NC(set_guard, stays_len = set.store.diagnoses.len,
                        diagnoses_len = set.store.diagnoses.len,
                        procedures_len = set.store.procedures.len) {
        set.stays.RemoveFrom(stays_len);
        set.store.diagnoses.RemoveFrom(diagnoses_len);
        set.store.procedures.RemoveFrom(procedures_len);
    };

    bool success = true;
    for (const char *filename: filenames) {
        JsonStayHandler json_handler(&set);
        if (!ParseJsonFile(filename, &json_handler))
            return false;
        success &= !json_handler.minor_errors;
    }

    set_guard.disable();
    return success;
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
