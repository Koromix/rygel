#pragma once

#include "kutil.hh"
#include "data_common.hh"

struct UnitCode {
    uint32_t value;

    bool operator==(const UnitCode &other) const { return value == other.value; }
    bool operator!=(const UnitCode &other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(value); }
};

struct UnitInfo {
    UnitCode code;
    Date dates[2];
    uint32_t facility_id;
};

enum class Sex: uint8_t {
    Male = 1,
    Female
};
static const char *const SexNames[] = {
    "Male",
    "Female"
};

struct Procedure {
    ProcedureCode code;
    int8_t phase;
    int16_t count;
    Date date;
};

struct Stay {
    enum class Error {
        Load,
        Incoherent
    };

    uint32_t stay_id;

    Sex sex;
    Date birthdate;
    Date dates[2];
    struct {
        char mode;
        char origin;
    } entry;
    struct {
        char mode;
        char destination;
    } exit;
    UnitCode unit_code;
    uint16_t session_count;
    uint8_t igs2;
    Date last_menstrual_period;
    uint16_t gestational_age;
    uint16_t newborn_weight;

    DiagnosisCode main_diagnosis;
    DiagnosisCode linked_diagnosis;
    ArrayRef<DiagnosisCode> diagnoses;

    ArrayRef<Procedure> procedures;

    uint32_t error_mask;
};

struct StaySet {
    DynamicArray<Stay> stays;
    DynamicArray<DiagnosisCode> diagnoses;
    DynamicArray<Procedure> procedures;
};

class StaySetBuilder {
    StaySet set;

public:
    bool LoadJson(ArrayRef<const char *const> filenames);

    bool Finish(StaySet *out_set);
};
