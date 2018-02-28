// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libdrd/libdrd.hh"
#include "../common/rcpp.hh"

struct ClassifierSet {
    TableSet table_set;
    AuthorizationSet authorization_set;
};

static inline void ParseEntryExitCharacter(SEXP sexp, Stay::Error error_flag,
                                           char *out_dest, uint32_t *out_error_mask)
{
    const char *str = CHAR(sexp);
    if (str[0] && !str[1]) {
        *out_dest = UpperAscii(str[0]);
    } else if (sexp != NA_STRING) {
        *out_error_mask |= (uint32_t)error_flag;
    }
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
           Rcpp::CharacterVector price_filenames = Rcpp::CharacterVector::create(),
           Rcpp::Nullable<Rcpp::String> authorization_filename = R_NilValue)
{
    SETUP_RCPP_LOG_HANDLER();

    ClassifierSet *set = new ClassifierSet;
    DEFER_N(set_guard) { delete set; };

    HeapArray<const char *> data_dirs2;
    HeapArray<const char *> table_dirs2;
    HeapArray<const char *> table_filenames2;
    const char *authorization_filename2 = nullptr;
    for (const char *str: data_dirs) {
        data_dirs2.Append(str);
    }
    for (const char *str: table_dirs) {
        table_dirs2.Append(str);
    }
    for (const char *str: price_filenames) {
        table_filenames2.Append(str);
    }
    if (authorization_filename.isNotNull()) {
        authorization_filename2 = authorization_filename.as().get_cstring();
    }

    if (!InitTableSet(data_dirs2, table_dirs2, table_filenames2, &set->table_set) ||
            !set->table_set.indexes.len)
        StopRcppWithLastMessage();
    if (!InitAuthorizationSet(data_dirs2, authorization_filename2,
                              &set->authorization_set))
        StopRcppWithLastMessage();

    set_guard.disable();
    return Rcpp::XPtr<ClassifierSet>(set, true);
}

// [[Rcpp::export(name = '.classify')]]
Rcpp::DataFrame R_Classify(SEXP classifier_set_xp,
                           Rcpp::DataFrame stays_df, Rcpp::DataFrame diagnoses_df,
                           Rcpp::DataFrame procedures_df)
{
    SETUP_RCPP_LOG_HANDLER();

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
        RcppDateVector birthdate;
        Rcpp::CharacterVector sex;
        RcppDateVector entry_date;
        Rcpp::CharacterVector entry_mode;
        Rcpp::CharacterVector entry_origin;
        RcppDateVector exit_date;
        Rcpp::CharacterVector exit_mode;
        Rcpp::CharacterVector exit_destination;
        Rcpp::IntegerVector unit;
        Rcpp::IntegerVector bed_authorization;
        Rcpp::IntegerVector session_count;
        Rcpp::IntegerVector igs2;
        Rcpp::IntegerVector gestational_age;
        Rcpp::IntegerVector newborn_weight;
        RcppDateVector last_menstrual_period;

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
        RcppDateVector date;
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

            stay.bill_id = GetRcppOptionalValue(stays.bill_id, i, 0);
            stay.stay_id = GetRcppOptionalValue(stays.stay_id, i, 0);
            stay.birthdate = stays.birthdate[i];
            {
                const char *sex_str = stays.sex[i];
                if (TestStr(sex_str, "1") || TestStr(sex_str, "M") || TestStr(sex_str, "m") ||
                        TestStr(sex_str, "H") || TestStr(sex_str, "h")) {
                    stay.sex = Sex::Male;
                } else if (TestStr(sex_str, "2") || TestStr(sex_str, "F") || TestStr(sex_str, "f")) {
                    stay.sex = Sex::Female;
                } else if (stays.sex[i] != NA_STRING) {
                    LogError("Unexpected sex '%1' on row %2", sex_str, i + 1);
                    stay.error_mask &= (int)Stay::Error::MalformedSex;
                }
            }

            stay.entry.date = stays.entry_date[i];
            if (UNLIKELY(!stay.entry.date.value && !stays.entry_date.IsNA(i))) {
                stay.error_mask |= (int)Stay::Error::MalformedEntryDate;
            }
            ParseEntryExitCharacter(stays.entry_mode[i], Stay::Error::MalformedEntryMode,
                                    &stay.entry.mode, &stay.error_mask);
            ParseEntryExitCharacter(stays.entry_origin[i], Stay::Error::MalformedEntryOrigin,
                                    &stay.entry.origin, &stay.error_mask);
            stay.exit.date = stays.exit_date[i];
            if (UNLIKELY(!stay.exit.date.value && !stays.exit_date.IsNA(i))) {
                stay.error_mask |= (int)Stay::Error::MalformedExitDate;
            }
            ParseEntryExitCharacter(stays.exit_mode[i], Stay::Error::MalformedExitMode,
                                    &stay.exit.mode, &stay.error_mask);
            ParseEntryExitCharacter(stays.exit_destination[i], Stay::Error::MalformedExitDestination,
                                    &stay.exit.destination, &stay.error_mask);

            stay.unit.number = (int16_t)GetRcppOptionalValue(stays.unit, i, 0);
            stay.bed_authorization = (int8_t)GetRcppOptionalValue(stays.bed_authorization, i, 0);
            stay.session_count = (int16_t)GetRcppOptionalValue(stays.session_count, i, 0);
            stay.igs2 = (int16_t)GetRcppOptionalValue(stays.igs2, i, 0);
            stay.gestational_age = (int16_t)stays.gestational_age[i];
            stay.newborn_weight = (int16_t)stays.newborn_weight[i];
            stay.last_menstrual_period = stays.last_menstrual_period[i];

            stay.main_diagnosis =
                DiagnosisCode::FromString(GetRcppOptionalValue(stays.main_diagnosis, i, ""));
            stay.linked_diagnosis =
                DiagnosisCode::FromString(GetRcppOptionalValue(stays.linked_diagnosis, i, ""));
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
                proc.phase = (int8_t)GetRcppOptionalValue(procedures.phase, k, 0);
                {
                    int activities_dec = procedures.activity[k];
                    while (activities_dec) {
                        int activity = activities_dec % 10;
                        activities_dec /= 10;
                        proc.activities |= (uint8_t)(1 << activity);
                    }
                }
                proc.count = (int16_t)GetRcppOptionalValue(procedures.count, k, 1);
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

    HeapArray<ClassifyResult> results;
    Classify(classifier_set->table_set, classifier_set->authorization_set,
             stay_set.stays, ClusterMode::BillId, &results);

    LogDebug("Export");

    Rcpp::DataFrame retval;
    {
        char buf[32];

        Rcpp::IntegerVector bill_id(results.len);
        Rcpp::CharacterVector exit_date(results.len);
        Rcpp::CharacterVector ghm(results.len);
        Rcpp::IntegerVector ghs(results.len);
        Rcpp::NumericVector ghs_cents(results.len);
        Rcpp::NumericVector rea_cents(results.len);
        Rcpp::NumericVector reasi_cents(results.len);
        Rcpp::NumericVector si_cents(results.len);
        Rcpp::NumericVector src_cents(results.len);
        Rcpp::NumericVector nn1_cents(results.len);
        Rcpp::NumericVector nn2_cents(results.len);
        Rcpp::NumericVector nn3_cents(results.len);
        Rcpp::NumericVector rep_cents(results.len);
        Rcpp::NumericVector price_cents(results.len);
        // FIXME: Work around Rcpp limitation with 20 columns
        /* Rcpp::IntegerVector rea_days(results.len);
        Rcpp::IntegerVector reasi_days(results.len);
        Rcpp::IntegerVector si_days(results.len);
        Rcpp::IntegerVector src_days(results.len);
        Rcpp::IntegerVector nn1_days(results.len);
        Rcpp::IntegerVector nn2_days(results.len);
        Rcpp::IntegerVector nn3_days(results.len);
        Rcpp::IntegerVector rep_days(results.len); */

        for (Size i = 0; i < results.len; i++) {
            const ClassifyResult &result = results[i];

            bill_id[i] = result.stays[0].bill_id;
            exit_date[i] = Fmt(buf, "%1", result.stays[result.stays.len - 1].exit.date).ptr;
            ghm[i] = Fmt(buf, "%1", result.ghm).ptr;
            ghs[i] = result.ghs.number;
            ghs_cents[i] = (double)result.ghs_price_cents;
            rea_cents[i] = result.supplement_cents.st.rea;
            reasi_cents[i] = result.supplement_cents.st.reasi;
            si_cents[i] = result.supplement_cents.st.si;
            src_cents[i] = result.supplement_cents.st.src;
            nn1_cents[i] = result.supplement_cents.st.nn1;
            nn2_cents[i] = result.supplement_cents.st.nn2;
            nn3_cents[i] = result.supplement_cents.st.nn3;
            rep_cents[i] = result.supplement_cents.st.rep;
            price_cents[i] = result.price_cents;
            /* rea_days[i] = result.supplement_days.st.rea;
            reasi_days[i] = result.supplement_days.st.reasi;
            si_days[i] = result.supplement_days.st.si;
            src_days[i] = result.supplement_days.st.src;
            nn1_days[i] = result.supplement_days.st.nn1;
            nn2_days[i] = result.supplement_days.st.nn2;
            nn3_days[i] = result.supplement_days.st.nn3;
            rep_days[i] = result.supplement_days.st.rep; */

            if (i % 1024 == 0) {
                Rcpp::checkUserInterrupt();
            }
        }

        retval = Rcpp::DataFrame::create(
            Rcpp::Named("bill_id") = bill_id,
            Rcpp::Named("exit_date") = exit_date,
            Rcpp::Named("ghm") = ghm,
            Rcpp::Named("ghs") = ghs, Rcpp::Named("ghs_cents") = ghs_cents,
            Rcpp::Named("rea_cents") = rea_cents, Rcpp::Named("reasi_cents") = reasi_cents,
            Rcpp::Named("si_cents") = si_cents, Rcpp::Named("src_cents") = src_cents,
            Rcpp::Named("nn1_cents") = nn1_cents, Rcpp::Named("nn2_cents") = nn2_cents,
            Rcpp::Named("nn3_cents") = nn3_cents, Rcpp::Named("rep_cents") = rep_cents,
            Rcpp::Named("price_cents") = price_cents,
            /* Rcpp::Named("rea_days") = rea_days, Rcpp::Named("reasi") = reasi_days,
            Rcpp::Named("si_days") = si_days, Rcpp::Named("src_days") = src_days,
            Rcpp::Named("nn1_days") = nn1_days, Rcpp::Named("nn2_days") = nn2_days,
            Rcpp::Named("nn3_days") = nn3_days, Rcpp::Named("rep_days") = rep_days, */
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
    SETUP_RCPP_LOG_HANDLER();

    const ClassifierSet *classifier_set = Rcpp::XPtr<ClassifierSet>(classifier_set_xp).get();
    Date date = RcppDateVector(date_xp).Value();
    if (!date.value)
        StopRcppWithLastMessage();

    const TableIndex *index = classifier_set->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        StopRcppWithLastMessage();
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
    SETUP_RCPP_LOG_HANDLER();

    const ClassifierSet *classifier_set = Rcpp::XPtr<ClassifierSet>(classifier_set_xp).get();
    Date date = RcppDateVector(date_xp).Value();
    if (!date.value)
        StopRcppWithLastMessage();

    const TableIndex *index = classifier_set->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        StopRcppWithLastMessage();
    }

    Rcpp::DataFrame retval;
    {
        Rcpp::CharacterVector proc(index->procedures.len);
        Rcpp::IntegerVector phase(index->procedures.len);
        Rcpp::IntegerVector activities(index->procedures.len);
        Rcpp::newDateVector start_date((int)index->procedures.len);
        Rcpp::newDateVector end_date((int)index->procedures.len);

        for (Size i = 0; i < index->procedures.len; i++) {
            const ProcedureInfo &info = index->procedures[i];
            char buf[32];

            proc[i] = Fmt(buf, "%1", info.proc).ptr;
            phase[i] = info.phase;
            {
                int activities_dec = 0;
                for (int activities_bin = info.activities, i = 0; activities_bin; i++) {
                    if (activities_bin & 0x1) {
                        activities_dec = (activities_dec * 10) + i;
                    }
                    activities_bin >>= 1;
                }
                activities[i] = activities_dec;
            }
            start_date[i] = Rcpp::Date((unsigned int)info.limit_dates[0].st.month,
                                       (unsigned int)info.limit_dates[0].st.day,
                                       (unsigned int)info.limit_dates[0].st.year);
            end_date[i] = Rcpp::Date((unsigned int)info.limit_dates[1].st.month,
                                     (unsigned int)info.limit_dates[1].st.day,
                                     (unsigned int)info.limit_dates[1].st.year);
        }

        retval = Rcpp::DataFrame::create(
            Rcpp::Named("proc") = proc,
            Rcpp::Named("phase") = phase,
            Rcpp::Named("activities") = activities,
            Rcpp::Named("start_date") = start_date,
            Rcpp::Named("end_date") = end_date,
            Rcpp::Named("stringsAsFactors") = false
        );
    }

    return retval;
}
