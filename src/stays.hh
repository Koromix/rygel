#pragma once

#include "kutil.hh"
#include "tables.hh"

struct UnitCode {
    uint32_t value;

    UnitCode() = default;
    explicit UnitCode(uint32_t code) : value(code) {}

    bool IsValid() const { return value; }

    bool operator==(const UnitCode &other) const { return value == other.value; }
    bool operator!=(const UnitCode &other) const { return value != other.value; }

    operator FmtArg() const { return FmtArg(value); }
};

struct UnitInfo {
    UnitCode code;
    Date dates[2];
    int32_t facility_id;
};

struct Procedure {
    ProcedureCode code;
    int8_t phase;
    int8_t activity;
    int16_t count;
    Date date;
};

struct Stay {
    enum class Error {
        Load,
        Incoherent
    };

    int32_t stay_id;

    Sex sex;
    Date birthdate;
    Date dates[2];
    struct {
        int8_t mode;
        int8_t origin;
    } entry;
    struct {
        int8_t mode;
        int8_t destination;
    } exit;
    UnitCode unit_code;
    int16_t session_count;
    int16_t igs2;
    Date last_menstrual_period;
    int16_t gestational_age;
    int16_t newborn_weight;

    DiagnosisCode main_diagnosis;
    DiagnosisCode linked_diagnosis;
    ArrayRef<DiagnosisCode> diagnoses;

    ArrayRef<Procedure> procedures;

    uint32_t error_mask;
};

struct StaySet {
    HeapArray<Stay> stays;

    struct {
        HeapArray<DiagnosisCode> diagnoses;
        HeapArray<Procedure> procedures;
    } store;
};

class StaySetBuilder {
    StaySet set;

public:
    bool LoadJson(ArrayRef<const char *const> filenames);

    bool Finish(StaySet *out_set);
};
