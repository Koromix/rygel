// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libdrd/libdrd.hh"
#include "../common/rcpp.hh"

struct ClassifierSet {
    TableSet table_set;
    AuthorizationSet authorization_set;
};

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
        RStopWithLastError();
    if (!InitAuthorizationSet(data_dirs2, authorization_filename2,
                              &set->authorization_set))
        RStopWithLastError();

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
        RVectorView<const int> id;

        RVectorView<int> bill_id;
        RVectorView<int> stay_id;
        RVectorView<Date> birthdate;
        RVectorView<int> sex;
        RVectorView<Date> entry_date;
        RVectorView<int> entry_mode;
        RVectorView<const char *> entry_origin;
        RVectorView<Date> exit_date;
        RVectorView<int> exit_mode;
        RVectorView<int> exit_destination;
        RVectorView<int> unit;
        RVectorView<int> bed_authorization;
        RVectorView<int> session_count;
        RVectorView<int> igs2;
        RVectorView<int> gestational_age;
        RVectorView<int> newborn_weight;
        RVectorView<Date> last_menstrual_period;

        RVectorView<const char *> main_diagnosis;
        RVectorView<const char *> linked_diagnosis;
    } stays;
    int stays_nrow = stays_df.nrow();

    struct {
        RVectorView<int> id;

        RVectorView<const char *> diag;
        RVectorView<const char *> type;
    } diagnoses;
    int diagnoses_nrow = diagnoses_df.nrow();

    struct {
        RVectorView<int> id;

        RVectorView<const char *> proc;
        RVectorView<int> phase;
        RVectorView<int> activity;
        RVectorView<int> count;
        RVectorView<Date> date;
    } procedures;
    int procedures_nrow = procedures_df.nrow();

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

    diagnoses.id = diagnoses_df["id"];
    diagnoses.diag = diagnoses_df["diag"];
    if (diagnoses_df.containsElementNamed("type")) {
        diagnoses.type = diagnoses_df["type"];

        if (stays_df.containsElementNamed("main_diagnosis") ||
                stays_df.containsElementNamed("linked_diagnosis")) {
            LogError("Columns 'main_diagnosis' and 'linked_diagnosis' are ignored when the "
                     "diagnoses table has a type column");
        }
    } else {
        stays.main_diagnosis = stays_df["main_diagnosis"];
        stays.linked_diagnosis = stays_df["linked_diagnosis"];
    }

    procedures.id = procedures_df["id"];
    procedures.proc = procedures_df["code"];
    LOAD_OPTIONAL_COLUMN(procedures, phase);
    procedures.activity = procedures_df["activity"];
    LOAD_OPTIONAL_COLUMN(procedures, count);
    procedures.date = procedures_df["date"];

    LogDebug("Copy");

    StaySet stay_set;
    {
        stay_set.stays.Reserve(stays_nrow);
        stay_set.store.diagnoses.Reserve(diagnoses_nrow + 2 * stays_nrow);
        stay_set.store.procedures.Reserve(procedures_nrow);

        int j = 0, k = 0;
        for (int i = 0; i < stays_nrow; i++) {
            Stay stay = {};

            stay.bill_id = RGetOptionalValue(stays.bill_id, i, 0);
            stay.stay_id = RGetOptionalValue(stays.stay_id, i, 0);
            stay.birthdate = stays.birthdate[i];
            if (UNLIKELY(!stay.birthdate.value && !stays.birthdate.IsNA(stay.birthdate))) {
                stay.error_mask |= (int)Stay::Error::MalformedBirthdate;
            }
            switch (stays.sex[i]) {
                case 1: { stay.sex = Sex::Male; } break;
                case 2: { stay.sex = Sex::Female; } break;

                default: {
                    if (stays.sex[i] != NA_INTEGER) {
                        LogError("Unexpected sex %1 on row %2", stays.sex[i], i + 1);
                        stay.error_mask |= (int)Stay::Error::MalformedSex;
                    }
                } break;
            }
            stay.entry.date = stays.entry_date[i];
            if (UNLIKELY(!stay.entry.date.value && !stays.entry_date.IsNA(stay.entry.date))) {
                stay.error_mask |= (int)Stay::Error::MalformedEntryDate;
            }
            stay.entry.mode = (char)('0' + stays.entry_mode[i]);
            {
                const char *origin_str = stays.entry_origin[i];
                if (origin_str[0] && !origin_str[1]) {
                    stay.entry.origin = UpperAscii(origin_str[0]);
                } else if (origin_str != CHAR(NA_STRING)) {
                    stay.error_mask |= (uint32_t)Stay::Error::MalformedEntryOrigin;
                }
            }
            stay.exit.date = stays.exit_date[i];
            if (UNLIKELY(!stay.exit.date.value && !stays.exit_date.IsNA(stay.exit.date))) {
                stay.error_mask |= (int)Stay::Error::MalformedExitDate;
            }
            stay.exit.mode = (char)('0' + stays.exit_mode[i]);
            stay.exit.destination = (char)('0' + RGetOptionalValue(stays.exit_destination, i, -'0'));

            stay.unit.number = (int16_t)RGetOptionalValue(stays.unit, i, 0);
            stay.bed_authorization = (int8_t)RGetOptionalValue(stays.bed_authorization, i, 0);
            stay.session_count = (int16_t)RGetOptionalValue(stays.session_count, i, 0);
            stay.igs2 = (int16_t)RGetOptionalValue(stays.igs2, i, 0);
            stay.gestational_age = (int16_t)stays.gestational_age[i];
            stay.newborn_weight = (int16_t)stays.newborn_weight[i];
            stay.last_menstrual_period = stays.last_menstrual_period[i];

            stay.diagnoses.ptr = stay_set.store.diagnoses.end();
            if (diagnoses.type.Len()) {
                for (; j < diagnoses_nrow && diagnoses.id[j] == stays.id[i]; j++) {
                    if (UNLIKELY(diagnoses.diag[j] == CHAR(NA_STRING)))
                        continue;

                    DiagnosisCode diag = DiagnosisCode::FromString(diagnoses.diag[j], false);
                    const char *type_str = diagnoses.type[j];

                    if (LIKELY(type_str[0] && !type_str[1])) {
                        switch (type_str[0]) {
                            case 'p':
                            case 'P': {
                                stay.main_diagnosis = diag;
                                if (UNLIKELY(!stay.main_diagnosis.IsValid())) {
                                    stay.error_mask |= (int)Stay::Error::MalformedMainDiagnosis;
                                }
                            } break;
                            case 'r':
                            case 'R': {
                                stay.linked_diagnosis = diag;
                                if (UNLIKELY(!stay.linked_diagnosis.IsValid())) {
                                    stay.error_mask |= (int)Stay::Error::MalformedLinkedDiagnosis;
                                }
                            } break;
                            case 's':
                            case 'S': {
                                if (LIKELY(diag.IsValid())) {
                                    stay_set.store.diagnoses.Append(diag);
                                } else {
                                    stay.error_mask |= (int)Stay::Error::MalformedAssociatedDiagnosis;
                                }
                            } break;
                            case 'd':
                            case 'D': { /* Ignore documentary diagnoses */ } break;

                            default: {
                                LogError("Unexpected diagnosis type '%1' on row %2", type_str, i + 1);
                            } break;
                        }
                    } else {
                        LogError("Malformed diagnosis type '%1' on row %2", type_str, i + 1);
                    }
                }
            } else {
                if (LIKELY(stays.main_diagnosis[i] != CHAR(NA_STRING))) {
                    stay.main_diagnosis = DiagnosisCode::FromString(stays.main_diagnosis[i], false);
                    if (UNLIKELY(!stay.main_diagnosis.IsValid())) {
                        stay.error_mask |= (int)Stay::Error::MalformedMainDiagnosis;
                    }
                }
                if (stays.linked_diagnosis[i] != CHAR(NA_STRING)) {
                    stay.linked_diagnosis = DiagnosisCode::FromString(stays.linked_diagnosis[i], false);
                    if (UNLIKELY(!stay.linked_diagnosis.IsValid())) {
                        stay.error_mask |= (int)Stay::Error::MalformedLinkedDiagnosis;
                    }
                }

                for (; j < diagnoses_nrow && diagnoses.id[j] == stays.id[i]; j++) {
                    if (UNLIKELY(diagnoses.diag[j] == CHAR(NA_STRING)))
                        continue;

                    DiagnosisCode diag = DiagnosisCode::FromString(diagnoses.diag[j], false);
                    if (UNLIKELY(!diag.IsValid())) {
                        stay.error_mask |= (int)Stay::Error::MalformedAssociatedDiagnosis;
                    }

                    stay_set.store.diagnoses.Append(diag);
                }
            }
            if (stay.main_diagnosis.IsValid()) {
                stay_set.store.diagnoses.Append(stay.main_diagnosis);
            }
            if (stay.linked_diagnosis.IsValid()) {
                stay_set.store.diagnoses.Append(stay.linked_diagnosis);
            }
            stay.diagnoses.len = stay_set.store.diagnoses.end() - stay.diagnoses.ptr;

            stay.procedures.ptr = stay_set.store.procedures.end();
            for (; k < procedures_nrow && procedures.id[k] == stays.id[i]; k++) {
                ProcedureRealisation proc = {};

                proc.proc = ProcedureCode::FromString(procedures.proc[k]);
                proc.phase = (int8_t)RGetOptionalValue(procedures.phase, k, 0);
                {
                    int activities_dec = procedures.activity[k];
                    while (activities_dec) {
                        int activity = activities_dec % 10;
                        activities_dec /= 10;
                        proc.activities |= (uint8_t)(1 << activity);
                    }
                }
                proc.count = (int16_t)RGetOptionalValue(procedures.count, k, 1);
                proc.date = procedures.date[k];

                stay_set.store.procedures.Append(proc);
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

        RVectorView<int> bill_id(Rf_allocVector(INTSXP, results.len));
        RVectorView<const char *> exit_date(Rf_allocVector(STRSXP, results.len));
        RVectorView<const char *> ghm(Rf_allocVector(STRSXP, results.len));
        RVectorView<int> main_error(Rf_allocVector(INTSXP, results.len));
        RVectorView<int> ghs(Rf_allocVector(INTSXP, results.len));
        RVectorView<double> ghs_cents(Rf_allocVector(REALSXP, results.len));
        RVectorView<double> rea_cents(Rf_allocVector(REALSXP, results.len));
        RVectorView<double> reasi_cents(Rf_allocVector(REALSXP, results.len));
        RVectorView<double> si_cents(Rf_allocVector(REALSXP, results.len));
        RVectorView<double> src_cents(Rf_allocVector(REALSXP, results.len));
        RVectorView<double> nn1_cents(Rf_allocVector(REALSXP, results.len));
        RVectorView<double> nn2_cents(Rf_allocVector(REALSXP, results.len));
        RVectorView<double> nn3_cents(Rf_allocVector(REALSXP, results.len));
        RVectorView<double> rep_cents(Rf_allocVector(REALSXP, results.len));
        RVectorView<double> price_cents(Rf_allocVector(REALSXP, results.len));
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
            exit_date.Set(i, Fmt(buf, "%1", result.stays[result.stays.len - 1].exit.date));
            ghm.Set(i, Fmt(buf, "%1", result.ghm));
            main_error[i] = result.main_error;
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
            Rcpp::Named("ghm") = ghm, Rcpp::Named("main_error") = main_error,
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
    Date date = RVectorView<Date>(date_xp).Value();
    if (!date.value)
        RStopWithLastError();

    const TableIndex *index = classifier_set->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        RStopWithLastError();
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
    Date date = RVectorView<Date>(date_xp).Value();
    if (!date.value)
        RStopWithLastError();

    const TableIndex *index = classifier_set->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        RStopWithLastError();
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
