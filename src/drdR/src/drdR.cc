// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../libdrd/libdrd.hh"
#include <Rcpp.h>

struct ClassifierSet {
    TableSet table_set;
    PricingSet pricing_set;
    AuthorizationSet authorization_set;
};

static thread_local DynamicQueue<const char *> log_messages;
static thread_local bool log_missing_messages = false;

#define SETUP_LOG_HANDLER() \
    PushLogHandler([](LogLevel level, const char *ctx, \
                      const char *fmt, Span<const FmtArg> args) { \
        switch (level) { \
            case LogLevel::Error: { \
                const char *msg = FmtFmt(log_messages.bucket_allocator, fmt, args).ptr; \
                log_messages.Append(msg); \
                if (log_messages.len > 100) { \
                    log_messages.RemoveFirst(); \
                    log_missing_messages = true; \
                } \
            } break; \
 \
            case LogLevel::Info: \
            case LogLevel::Debug: { \
                Print("%1", ctx); \
                PrintFmt(stdout, fmt, args); \
                PrintLn(); \
            } break; \
        } \
    }); \
    DEFER { \
        DumpWarnings(); \
        PopLogHandler(); \
    };

static void DumpWarnings()
{
    for (const char *msg: log_messages) {
        Rcpp::warning(msg);
    }
    log_messages.Clear();

    if (log_missing_messages) {
        Rcpp::warning("There were too many warnings, some have been lost");
        log_missing_messages = false;
    }
}

static void StopWithLastMessage()
{
    if (log_messages.len) {
        std::string error_msg = log_messages[log_messages.len - 1];
        log_messages.RemoveLast();
        DumpWarnings();
        Rcpp::stop(error_msg);
    } else {
        Rcpp::stop("Unknown error");
    }
}

class FlexibleDateVector {
    enum class Type {
        Character,
        Date
    };

    Type type;
    struct { // FIXME: I want union
        Rcpp::CharacterVector chr;
        Rcpp::NumericVector num;
    } u;

public:
    Size len = 0;

    FlexibleDateVector() = default;
    FlexibleDateVector(SEXP xp)
    {
        if (Rcpp::is<Rcpp::CharacterVector>(xp)) {
            type = Type::Character;
            u.chr = xp;
            len = (Size)u.chr.size();
        } else if ((Rcpp::is<Rcpp::NumericVector>(xp) || Rcpp::is<Rcpp::IntegerVector>(xp)) &&
                   Rf_inherits(xp, "Date")) {
            type = Type::Date;
            u.num = xp;
            len = (Size)u.num.size();
        } else {
            Rcpp::stop("Date vector uses unsupported type (must be Date or date-like string)");
        }
    }

    Date operator[](int idx) const
    {
        switch (type) {
            case Type::Character: {
                SEXP str = u.chr[idx].get();
                if (str != NA_STRING) {
                    Date date = Date::FromString(CHAR(str));
                    if (!date.value)
                        StopWithLastMessage();
                    return date;
                }
            } break;

            case Type::Date: {
                double value = u.num[idx];
                if (value != NA_REAL) {
                    Rcpp::Datetime dt = value * 86400;
                    Date date(dt.getYear(), dt.getMonth(), dt.getDay());
                    DebugAssert(date.IsValid());
                    return date;
                }
            } break;
        }

        return {};
    }

    Date Value() const
    {
        if (UNLIKELY(len != 1)) {
            LogError("Date or date-like vector must have one value (no more, no less)");
            StopWithLastMessage();
        }

        return (*this)[0];
    }
};

template <int RTYPE, typename T>
T GetOptionalValue(Rcpp::Vector<RTYPE> &vec, R_xlen_t i, T default_value)
{
    if (UNLIKELY(i >= vec.size()))
        return default_value;
    auto value = vec[i % vec.size()];
    if (vec.is_na(value))
        return default_value;
    return value;
}

static inline int8_t ParseEntryExitCharacter(const char *str)
{
    if (str[0] < '0' || str[1])
        return 0;
    return str[0] - '0';
}

// [[Rcpp::export(name = 'drd.options')]]
SEXP R_Options(SEXP debug = R_NilValue)
{
    if (!Rf_isNull(debug)) {
        enable_debug = Rcpp::as<bool>(debug);
    }

    return Rcpp::List::create(
        Rcpp::Named("debug") = enable_debug
    );
}

// [[Rcpp::export(name = 'drd')]]
SEXP R_Drd(Rcpp::CharacterVector data_dirs = Rcpp::CharacterVector::create(),
            Rcpp::CharacterVector table_dirs = Rcpp::CharacterVector::create(),
            Rcpp::Nullable<Rcpp::String> pricing_filename = R_NilValue,
            Rcpp::Nullable<Rcpp::String> authorization_filename = R_NilValue)
{
    SETUP_LOG_HANDLER();

    ClassifierSet *set = new ClassifierSet;
    DEFER_N(set_guard) { delete set; };

    HeapArray<const char *> data_dirs2;
    HeapArray<const char *> table_dirs2;
    const char *pricing_filename2 = nullptr;
    const char *authorization_filename2 = nullptr;
    for (const char *str: data_dirs) {
        data_dirs2.Append(str);
    }
    for (const char *str: table_dirs) {
        table_dirs2.Append(str);
    }
    if (pricing_filename.isNotNull()) {
        pricing_filename2 = pricing_filename.as().get_cstring();
    }
    if (authorization_filename.isNotNull()) {
        authorization_filename2 = authorization_filename.as().get_cstring();
    }

    if (!InitTableSet(data_dirs2, table_dirs2, &set->table_set) ||
            !set->table_set.indexes.len)
        StopWithLastMessage();
    if (!InitPricingSet(data_dirs2, pricing_filename2,
                        &set->pricing_set)) // Tolerate empty pricing sets
        StopWithLastMessage();
    if (!InitAuthorizationSet(data_dirs2, authorization_filename2,
                              &set->authorization_set)) // Tolerate missing authorizations
        StopWithLastMessage();

    set_guard.disable();
    return Rcpp::XPtr<ClassifierSet>(set, true);
}

// [[Rcpp::export(name = '.classify')]]
Rcpp::DataFrame R_Classify(SEXP classifier_set_xp,
                           Rcpp::DataFrame stays_df, Rcpp::DataFrame diagnoses_df,
                           Rcpp::DataFrame procedures_df)
{
    SETUP_LOG_HANDLER();

#define LOAD_OPTIONAL_COLUMN(Var, Name) \
        do { \
            if ((Var ## _df).containsElementNamed(STRINGIFY(Name))) { \
                (Var).Name = (Var ##_df)[STRINGIFY(Name)]; \
            } \
        } while (false)

    const ClassifierSet *classifier_set = Rcpp::XPtr<ClassifierSet>(classifier_set_xp).get();

    struct {
        Rcpp::IntegerVector id;

        Rcpp::IntegerVector bill_id;
        Rcpp::IntegerVector stay_id;
        FlexibleDateVector birthdate;
        Rcpp::CharacterVector sex;
        FlexibleDateVector entry_date;
        Rcpp::CharacterVector entry_mode;
        Rcpp::CharacterVector entry_origin;
        FlexibleDateVector exit_date;
        Rcpp::CharacterVector exit_mode;
        Rcpp::CharacterVector exit_destination;
        Rcpp::IntegerVector unit;
        Rcpp::IntegerVector bed_authorization;
        Rcpp::IntegerVector session_count;
        Rcpp::IntegerVector igs2;
        Rcpp::IntegerVector gestational_age;
        Rcpp::IntegerVector newborn_weight;
        FlexibleDateVector last_menstrual_period;

        Rcpp::CharacterVector main_diagnosis;
        Rcpp::CharacterVector linked_diagnosis;
    } stays;

    struct {
        Rcpp::IntegerVector id;

        Rcpp::CharacterVector diag;
        Rcpp::CharacterVector type;
    } diagnoses;

    struct {
        Rcpp::IntegerVector id;

        Rcpp::CharacterVector proc;
        Rcpp::IntegerVector phase;
        Rcpp::IntegerVector activity;
        Rcpp::IntegerVector count;
        FlexibleDateVector date;
    } procedures;

    LogDebug("Start");

    stays.id = stays_df["id"];
    LOAD_OPTIONAL_COLUMN(stays, bill_id);
    LOAD_OPTIONAL_COLUMN(stays, stay_id);
    stays.birthdate = stays_df["birthdate"];
    stays.sex = stays_df["sex"];
    stays.entry_date = stays_df["entry_date"];
    stays.entry_mode = stays_df["entry_mode"];
    LOAD_OPTIONAL_COLUMN(stays, entry_origin);
    stays.exit_date = stays_df["exit_date"];
    stays.exit_mode = stays_df["exit_mode"];
    LOAD_OPTIONAL_COLUMN(stays, exit_destination);
    LOAD_OPTIONAL_COLUMN(stays, unit);
    LOAD_OPTIONAL_COLUMN(stays, bed_authorization);
    LOAD_OPTIONAL_COLUMN(stays, session_count);
    LOAD_OPTIONAL_COLUMN(stays, igs2);
    LOAD_OPTIONAL_COLUMN(stays, gestational_age);
    LOAD_OPTIONAL_COLUMN(stays, newborn_weight);
    LOAD_OPTIONAL_COLUMN(stays, last_menstrual_period);
    LOAD_OPTIONAL_COLUMN(stays, main_diagnosis);
    LOAD_OPTIONAL_COLUMN(stays, linked_diagnosis);

    diagnoses.id = diagnoses_df["id"];
    diagnoses.diag = diagnoses_df["diag"];
    LOAD_OPTIONAL_COLUMN(diagnoses, type);

    procedures.id = procedures_df["id"];
    procedures.proc = procedures_df["code"];
    LOAD_OPTIONAL_COLUMN(procedures, phase);
    procedures.activity = procedures_df["activity"];
    LOAD_OPTIONAL_COLUMN(procedures, count);
    procedures.date = procedures_df["date"];

    LogDebug("Copy");

    // TODO: Don't require sorted id column (id)
    StaySet stay_set;
    {
        stay_set.stays.Reserve(stays_df.nrow());
        stay_set.store.diagnoses.Reserve(diagnoses_df.nrow() + 2 * stays_df.nrow());
        stay_set.store.procedures.Reserve(procedures_df.nrow());

        int j = 0, k = 0;
        for (int i = 0; i < stays_df.nrow(); i++) {
            Stay stay = {};

            stay.bill_id = GetOptionalValue(stays.bill_id, i, 0);
            stay.stay_id = GetOptionalValue(stays.stay_id, i, 0);
            stay.birthdate = stays.birthdate[i];
            {
                const char *sex = stays.sex[i];
                if (TestStr(sex, "1") || TestStr(sex, "M") || TestStr(sex, "m") ||
                        TestStr(sex, "H") || TestStr(sex, "h")) {
                    stay.sex = Sex::Male;
                } else if (TestStr(sex, "2") || TestStr(sex, "F") || TestStr(sex, "f")) {
                    stay.sex = Sex::Female;
                } else {
                    LogError("Unexpected sex '%1' on row %2", sex, i + 1);
                }
            }
            stay.entry.date = stays.entry_date[i];
            // TODO: Harmonize who deals with format errors (for example sex is dealt with here, not modes)
            stay.entry.date = stays.entry_date[i];
            stay.entry.mode = ParseEntryExitCharacter(stays.entry_mode[i]);
            stay.entry.origin = ParseEntryExitCharacter(GetOptionalValue(stays.entry_origin, i, ""));
            stay.exit.date = stays.exit_date[i];
            stay.exit.mode = ParseEntryExitCharacter(stays.exit_mode[i]);
            stay.exit.destination = ParseEntryExitCharacter(GetOptionalValue(stays.exit_destination, i, ""));
            stay.unit.number = GetOptionalValue(stays.unit, i, 0);
            stay.bed_authorization = GetOptionalValue(stays.bed_authorization, i, 0);
            stay.session_count = GetOptionalValue(stays.session_count, i, 0);
            stay.igs2 = GetOptionalValue(stays.igs2, i, 0);
            stay.gestational_age = stays.gestational_age[i];
            stay.newborn_weight = stays.newborn_weight[i];
            stay.last_menstrual_period = stays.last_menstrual_period[i];
            stay.main_diagnosis =
                DiagnosisCode::FromString(GetOptionalValue(stays.main_diagnosis, i, ""));
            stay.linked_diagnosis =
                DiagnosisCode::FromString(GetOptionalValue(stays.linked_diagnosis, i, ""));

            stay.diagnoses.ptr = stay_set.store.diagnoses.end();
            while (j < diagnoses_df.nrow() && diagnoses.id[j] == stays.id[i]) {
                DiagnosisCode diag = DiagnosisCode::FromString(diagnoses.diag[j]);

                if (diagnoses.type.size()) {
                    const char *type = diagnoses.type[j];
                    if (TestStr(type, "P") || TestStr(type, "p")) {
                        stay.main_diagnosis = diag;
                    } else if (TestStr(type, "R") || TestStr(type, "r")) {
                        stay.linked_diagnosis = diag;
                    } else if (TestStr(type, "S") || TestStr(type, "s")) {
                        stay_set.store.diagnoses.Append(diag);
                    } else if (TestStr(type, "D") || TestStr(type, "d")) {
                        // Ignore documentary diagnoses
                    } else {
                        LogError("Unexpected diagnosis type '%1' on row %2", type, j + 1);
                    }
                } else {
                    stay_set.store.diagnoses.Append(diag);
                }
                j++;
            }
            if (stay.main_diagnosis.IsValid()) {
                stay_set.store.diagnoses.Append(stay.main_diagnosis);
            }
            if (stay.linked_diagnosis.IsValid()) {
                stay_set.store.diagnoses.Append(stay.linked_diagnosis);
            }
            stay.diagnoses.len = stay_set.store.diagnoses.end() - stay.diagnoses.ptr;

            stay.procedures.ptr = stay_set.store.procedures.end();
            while (k < procedures_df.nrow() && procedures.id[k] == stays.id[i]) {
                ProcedureRealisation proc = {};

                proc.proc = ProcedureCode::FromString(procedures.proc[k]);
                proc.phase = GetOptionalValue(procedures.phase, k, 0);
                {
                    unsigned int activities_dec = (unsigned int)procedures.activity[k];
                    while (activities_dec) {
                        int activity = activities_dec % 10;
                        activities_dec /= 10;
                        proc.activities |= (1 << activity);
                    }
                }
                proc.count = GetOptionalValue(procedures.count, k, 1);
                proc.date = procedures.date[k];

                stay_set.store.procedures.Append(proc);
                k++;
            }
            stay.procedures.len = stay_set.store.procedures.end() - stay.procedures.ptr;

            stay_set.stays.Append(stay);

            if (i % 1024 == 0) {
                Rcpp::checkUserInterrupt();
            }
        }
    }

    LogDebug("Classify");

    ClassifyResultSet result_set = {};
    Classify(classifier_set->table_set, classifier_set->authorization_set,
             classifier_set->pricing_set,stay_set.stays, ClusterMode::BillId,
             &result_set);

    LogDebug("Export");

    Rcpp::DataFrame retval;
    {
        char buf[32];

        Rcpp::IntegerVector bill_id(result_set.results.len);
        Rcpp::CharacterVector exit_date(result_set.results.len);
        Rcpp::IntegerVector duration(result_set.results.len);
        Rcpp::CharacterVector ghm(result_set.results.len);
        Rcpp::IntegerVector ghs(result_set.results.len);
        Rcpp::NumericVector ghs_price(result_set.results.len);
        Rcpp::IntegerVector rea(result_set.results.len);
        Rcpp::IntegerVector reasi(result_set.results.len);
        Rcpp::IntegerVector si(result_set.results.len);
        Rcpp::IntegerVector src(result_set.results.len);
        Rcpp::IntegerVector nn1(result_set.results.len);
        Rcpp::IntegerVector nn2(result_set.results.len);
        Rcpp::IntegerVector nn3(result_set.results.len);
        Rcpp::IntegerVector rep(result_set.results.len);

        for (Size i = 0; i < result_set.results.len; i++) {
            const ClassifyResult &result = result_set.results[i];

            bill_id[i] = result.stays[0].bill_id;
            exit_date[i] = Fmt(buf, "%1", result.stays[result.stays.len - 1].exit.date).ptr;
            duration[i] = result.duration;
            ghm[i] = Fmt(buf, "%1", result.ghm).ptr;
            ghs[i] = result.ghs.number;
            ghs_price[i] = (double)result.ghs_price_cents / 100.0;
            rea[i] = result.supplements.rea;
            reasi[i] = result.supplements.reasi;
            si[i] = result.supplements.si;
            src[i] = result.supplements.src;
            nn1[i] = result.supplements.nn1;
            nn2[i] = result.supplements.nn2;
            nn3[i] = result.supplements.nn3;
            rep[i] = result.supplements.rep;

            if (i % 1024 == 0) {
                Rcpp::checkUserInterrupt();
            }
        }

        retval = Rcpp::DataFrame::create(
            Rcpp::Named("bill_id") = bill_id,
            Rcpp::Named("exit_date") = exit_date, Rcpp::Named("duration") = duration,
            Rcpp::Named("ghm") = ghm,
            Rcpp::Named("ghs") = ghs, Rcpp::Named("ghs_price") = ghs_price,
            Rcpp::Named("rea") = rea, Rcpp::Named("reasi") = reasi, Rcpp::Named("si") = si,
            Rcpp::Named("src") = src, Rcpp::Named("nn1") = nn1, Rcpp::Named("nn2") = nn2,
            Rcpp::Named("nn3") = nn3, Rcpp::Named("rep") = rep,
            Rcpp::Named("stringsAsFactors") = false
        );
    }

    LogDebug("Done");

#undef LOAD_OPTIONAL_COLUMN

    return retval;
}

// [[Rcpp::export(name = 'diagnoses')]]
Rcpp::DataFrame R_Diagnoses(SEXP classifier_set_xp, SEXP date_xp)
{
    SETUP_LOG_HANDLER();

    const ClassifierSet *classifier_set = Rcpp::XPtr<ClassifierSet>(classifier_set_xp).get();
    Date date = FlexibleDateVector(date_xp).Value();
    if (!date.value)
        StopWithLastMessage();

    const TableIndex *index = classifier_set->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        StopWithLastMessage();
    }

    Rcpp::DataFrame retval;
    {
        Rcpp::CharacterVector diag(index->diagnoses.len);
        Rcpp::IntegerVector cmd_m(index->diagnoses.len);
        Rcpp::IntegerVector cmd_f(index->diagnoses.len);

        for (Size i = 0; i < index->diagnoses.len; i++) {
            const DiagnosisInfo &info = index->diagnoses[i];
            char buf[32];

            diag[i] = Fmt(buf, "%1", info.diag).ptr;
            cmd_m[i] = info.Attributes(Sex::Male).cmd;
            cmd_f[i] = info.Attributes(Sex::Female).cmd;
        }

        retval = Rcpp::DataFrame::create(
            Rcpp::Named("diag") = diag,
            Rcpp::Named("cmd_m") = cmd_m,
            Rcpp::Named("cmd_f") = cmd_f,
            Rcpp::Named("stringsAsFactors") = false
        );
    }

    return retval;
}

// [[Rcpp::export(name = 'procedures')]]
Rcpp::DataFrame R_Procedures(SEXP classifier_set_xp, SEXP date_xp)
{
    SETUP_LOG_HANDLER();

    const ClassifierSet *classifier_set = Rcpp::XPtr<ClassifierSet>(classifier_set_xp).get();
    Date date = FlexibleDateVector(date_xp).Value();
    if (!date.value)
        StopWithLastMessage();

    const TableIndex *index = classifier_set->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        StopWithLastMessage();
    }

    Rcpp::DataFrame retval;
    {
        Rcpp::CharacterVector proc(index->procedures.len);
        Rcpp::IntegerVector phase(index->procedures.len);
        Rcpp::IntegerVector activities(index->procedures.len);

        for (Size i = 0; i < index->procedures.len; i++) {
            const ProcedureInfo &info = index->procedures[i];
            char buf[32];

            proc[i] = Fmt(buf, "%1", info.proc).ptr;
            phase[i] = info.phase;
            // FIXME: Fill activities correctly
            activities[i] = 1;
        }

        retval = Rcpp::DataFrame::create(
            Rcpp::Named("proc") = proc,
            Rcpp::Named("phase") = phase,
            Rcpp::Named("activities") = activities,
            Rcpp::Named("stringsAsFactors") = false
        );
    }

    return retval;
}

#undef SETUP_LOG_HANDLER
