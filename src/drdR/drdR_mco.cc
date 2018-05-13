// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libdrd/libdrd.hh"
#include "../common/Rcc.hh"

struct ClassifierInstance {
    mco_TableSet table_set;
    mco_AuthorizationSet authorization_set;
};

// [[Rcpp::export(name = 'drdR')]]
SEXP drdR_Options(SEXP debug = R_NilValue)
{
    if (!Rf_isNull(debug)) {
        enable_debug = Rcpp::as<bool>(debug);
    }

    return Rcpp::List::create(
        Rcpp::Named("debug") = enable_debug
    );
}

// [[Rcpp::export(name = 'mco_init')]]
SEXP drdR_mco_Init(Rcpp::CharacterVector data_dirs = Rcpp::CharacterVector::create(),
                   Rcpp::CharacterVector table_dirs = Rcpp::CharacterVector::create(),
                   Rcpp::CharacterVector table_filenames = Rcpp::CharacterVector::create(),
                   Rcpp::Nullable<Rcpp::String> authorization_filename = R_NilValue)
{
    RCC_SETUP_LOG_HANDLER();

    ClassifierInstance *classifier = new ClassifierInstance;
    DEFER_N(classifier_guard) { delete classifier; };

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
    for (const char *str: table_filenames) {
        table_filenames2.Append(str);
    }
    if (authorization_filename.isNotNull()) {
        authorization_filename2 = authorization_filename.as().get_cstring();
    }

    if (!mco_InitTableSet(data_dirs2, table_dirs2, table_filenames2, &classifier->table_set) ||
            !classifier->table_set.indexes.len)
        Rcc_StopWithLastError();
    if (!mco_InitAuthorizationSet(data_dirs2, authorization_filename2,
                                  &classifier->authorization_set))
        Rcc_StopWithLastError();

    SEXP classifier_xp = R_MakeExternalPtr(classifier, R_NilValue, R_NilValue);
    R_RegisterCFinalizerEx(classifier_xp, [](SEXP classifier_xp) {
        ClassifierInstance *classifier = (ClassifierInstance *)R_ExternalPtrAddr(classifier_xp);
        delete classifier;
    }, TRUE);
    classifier_guard.disable();

    return classifier_xp;
}

struct StaysProxy {
    Size nrow;

    Rcc_Vector<int> id;

    Rcc_Vector<int> admin_id;
    Rcc_Vector<int> bill_id;
    Rcc_Vector<Date> birthdate;
    Rcc_Vector<int> sex;
    Rcc_Vector<Date> entry_date;
    Rcc_Vector<int> entry_mode;
    Rcc_Vector<const char *> entry_origin;
    Rcc_Vector<Date> exit_date;
    Rcc_Vector<int> exit_mode;
    Rcc_Vector<int> exit_destination;
    Rcc_Vector<int> unit;
    Rcc_Vector<int> bed_authorization;
    Rcc_Vector<int> session_count;
    Rcc_Vector<int> igs2;
    Rcc_Vector<int> gestational_age;
    Rcc_Vector<int> newborn_weight;
    Rcc_Vector<Date> last_menstrual_period;

    Rcc_Vector<const char *> main_diagnosis;
    Rcc_Vector<const char *> linked_diagnosis;

    Rcc_Vector<int> confirm;
};

struct DiagnosesProxy {
    Size nrow;

    Rcc_Vector<int> id;

    Rcc_Vector<const char *> diag;
    Rcc_Vector<const char *> type;
};

struct ProceduresProxy {
    Size nrow;

    Rcc_Vector<int> id;

    Rcc_Vector<const char *> proc;
    Rcc_Vector<int> extension;
    Rcc_Vector<int> phase;
    Rcc_Vector<int> activity;
    Rcc_Vector<int> count;
    Rcc_Vector<Date> date;
    Rcc_Vector<const char *> doc;
};

static bool RunClassifier(const ClassifierInstance &classifier,
                          const StaysProxy &stays, Size stays_offset, Size stays_end,
                          const DiagnosesProxy &diagnoses, Size diagnoses_offset, Size diagnoses_end,
                          const ProceduresProxy &procedures, Size procedures_offset, Size procedures_end,
                          unsigned int flags, mco_StaySet *out_stay_set, HeapArray<mco_Result> *out_results)
{
    out_stay_set->stays.Reserve(stays_end - stays_offset);
    out_stay_set->store.diagnoses.Reserve((stays_end - stays_offset) * 2 + diagnoses_end - diagnoses_offset);
    out_stay_set->store.procedures.Reserve(procedures_end - procedures_offset);

    Size j = diagnoses_offset;
    Size k = procedures_offset;
    for (Size i = stays_offset; i < stays_end; i++) {
        mco_Stay stay = {};

        if (UNLIKELY(i && (stays.id[i] < stays.id[i - 1] ||
                          (j < diagnoses_end && diagnoses.id[j] < stays.id[i - 1]) ||
                          (k < procedures_end && procedures.id[k] < stays.id[i - 1]))))
            return false;

        stay.admin_id = Rcc_GetOptional(stays.admin_id, i, 0);
        stay.bill_id = Rcc_GetOptional(stays.bill_id, i, 0);
        stay.birthdate = stays.birthdate[i];
        if (UNLIKELY(stay.birthdate.value && !stay.birthdate.IsValid())) {
            stay.errors |= (int)mco_Stay::Error::MalformedBirthdate;
        }
        if (stays.sex[i] != NA_INTEGER) {
            stay.sex = (int8_t)stays.sex[i];
            if (UNLIKELY(stay.sex != stays.sex[i])) {
                stay.errors |= (int)mco_Stay::Error::MalformedSex;
            }
        }
        stay.entry.date = stays.entry_date[i];
        if (UNLIKELY(stay.entry.date.value && !stay.entry.date.IsValid())) {
            stay.errors |= (int)mco_Stay::Error::MalformedEntryDate;
        }
        stay.entry.mode = (char)('0' + stays.entry_mode[i]);
        {
            const char *origin_str = stays.entry_origin[i].ptr;
            if (origin_str[0] && !origin_str[1]) {
                stay.entry.origin = UpperAscii(origin_str[0]);
            } else if (origin_str != CHAR(NA_STRING)) {
                stay.errors |= (uint32_t)mco_Stay::Error::MalformedEntryOrigin;
            }
        }
        stay.exit.date = stays.exit_date[i];
        if (UNLIKELY(stay.exit.date.value && !stay.exit.date.IsValid())) {
            stay.errors |= (int)mco_Stay::Error::MalformedExitDate;
        }
        stay.exit.mode = (char)('0' + stays.exit_mode[i]);
        stay.exit.destination = (char)('0' + Rcc_GetOptional(stays.exit_destination, i, -'0'));

        stay.unit.number = (int16_t)Rcc_GetOptional(stays.unit, i, 0);
        stay.bed_authorization = (int8_t)Rcc_GetOptional(stays.bed_authorization, i, 0);
        stay.session_count = (int16_t)Rcc_GetOptional(stays.session_count, i, 0);
        stay.igs2 = (int16_t)Rcc_GetOptional(stays.igs2, i, 0);
        stay.gestational_age = (int16_t)stays.gestational_age[i];
        stay.newborn_weight = (int16_t)stays.newborn_weight[i];
        stay.last_menstrual_period = stays.last_menstrual_period[i];
        if (stays.confirm.Len() && stays.confirm[i] && stays.confirm[i] != NA_INTEGER) {
            stay.flags |= (int)mco_Stay::Flag::Confirmed;
        }

        stay.diagnoses.ptr = out_stay_set->store.diagnoses.end();
        if (diagnoses.type.Len()) {
            for (; j < diagnoses_end && diagnoses.id[j] <= stays.id[i]; j++) {
                if (UNLIKELY(diagnoses.id[j] < stays.id[i]))
                    continue;
                if (UNLIKELY(diagnoses.diag[j] == CHAR(NA_STRING)))
                    continue;

                DiagnosisCode diag =
                    DiagnosisCode::FromString(diagnoses.diag[j], (int)ParseFlag::End);
                const char *type_str = diagnoses.type[j].ptr;

                if (LIKELY(type_str[0] && !type_str[1])) {
                    switch (type_str[0]) {
                        case 'p':
                        case 'P': {
                            stay.main_diagnosis = diag;
                            if (UNLIKELY(!stay.main_diagnosis.IsValid())) {
                                stay.errors |= (int)mco_Stay::Error::MalformedMainDiagnosis;
                            }
                        } break;
                        case 'r':
                        case 'R': {
                            stay.linked_diagnosis = diag;
                            if (UNLIKELY(!stay.linked_diagnosis.IsValid())) {
                                stay.errors |= (int)mco_Stay::Error::MalformedLinkedDiagnosis;
                            }
                        } break;
                        case 's':
                        case 'S': {
                            if (LIKELY(diag.IsValid())) {
                                out_stay_set->store.diagnoses.Append(diag);
                            } else {
                                stay.errors |= (int)mco_Stay::Error::MalformedOtherDiagnosis;
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
                stay.main_diagnosis =
                    DiagnosisCode::FromString(stays.main_diagnosis[i], (int)ParseFlag::End);
                if (UNLIKELY(!stay.main_diagnosis.IsValid())) {
                    stay.errors |= (int)mco_Stay::Error::MalformedMainDiagnosis;
                }
            }
            if (stays.linked_diagnosis[i] != CHAR(NA_STRING)) {
                stay.linked_diagnosis =
                    DiagnosisCode::FromString(stays.linked_diagnosis[i], (int)ParseFlag::End);
                if (UNLIKELY(!stay.linked_diagnosis.IsValid())) {
                    stay.errors |= (int)mco_Stay::Error::MalformedLinkedDiagnosis;
                }
            }

            for (; j < diagnoses_end && diagnoses.id[j] <= stays.id[i]; j++) {
                if (UNLIKELY(diagnoses.id[j] < stays.id[i]))
                    continue;
                if (UNLIKELY(diagnoses.diag[j] == CHAR(NA_STRING)))
                    continue;

                DiagnosisCode diag =
                    DiagnosisCode::FromString(diagnoses.diag[j], (int)ParseFlag::End);
                if (UNLIKELY(!diag.IsValid())) {
                    stay.errors |= (int)mco_Stay::Error::MalformedOtherDiagnosis;
                }

                out_stay_set->store.diagnoses.Append(diag);
            }
        }
        if (stay.main_diagnosis.IsValid()) {
            out_stay_set->store.diagnoses.Append(stay.main_diagnosis);
        }
        if (stay.linked_diagnosis.IsValid()) {
            out_stay_set->store.diagnoses.Append(stay.linked_diagnosis);
        }
        stay.diagnoses.len = out_stay_set->store.diagnoses.end() - stay.diagnoses.ptr;

        stay.procedures.ptr = out_stay_set->store.procedures.end();
        for (; k < procedures_end && procedures.id[k] <= stays.id[i]; k++) {
            if (UNLIKELY(procedures.id[k] < stays.id[i]))
                continue;
            if (UNLIKELY(procedures.proc[k] == CHAR(NA_STRING)))
                continue;

            mco_ProcedureRealisation proc = {};

            proc.proc = ProcedureCode::FromString(procedures.proc[k], (int)ParseFlag::End);
            if (procedures.extension.Len() && procedures.extension[k] != NA_INTEGER) {
                int extension = procedures.extension[k];
                if (LIKELY(extension >= 0 && extension < 100)) {
                    proc.extension = (int8_t)extension;
                } else {
                    stay.errors |= (int)mco_Stay::Error::MalformedProcedureExtension;
                }
            }
            proc.phase = (int8_t)Rcc_GetOptional(procedures.phase, k, 0);
            {
                int activities_dec = procedures.activity[k];
                while (activities_dec) {
                    int activity = activities_dec % 10;
                    activities_dec /= 10;
                    proc.activities |= (uint8_t)(1 << activity);
                }
            }
            proc.count = (int16_t)Rcc_GetOptional(procedures.count, k, 0);
            proc.date = procedures.date[k];
            if (procedures.doc.Len()) {
                const char *doc_str = procedures.doc[k].ptr;
                if (doc_str[0] && !doc_str[1]) {
                    proc.doc = doc_str[0];
                } else if (doc_str != CHAR(NA_STRING)) {
                    // Put garbage in doc to trigger classifier error 173
                    proc.doc = '?';
                }
            }

            if (LIKELY(proc.proc.IsValid())) {
                out_stay_set->store.procedures.Append(proc);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedProcedureCode;
            }
        }
        stay.procedures.len = out_stay_set->store.procedures.end() - stay.procedures.ptr;

        out_stay_set->stays.Append(stay);
    }

    // We're already running in parallel, using ClassifyParallel would slow us down,
    // because it has some overhead caused by multi-stays.
    mco_Classify(classifier.table_set, classifier.authorization_set,
             out_stay_set->stays, flags, out_results);

    return true;
}

// [[Rcpp::export(name = '.mco_classify')]]
SEXP drdR_mco_Classify(SEXP classifier_xp, Rcpp::DataFrame stays_df,
                       Rcpp::DataFrame diagnoses_df, Rcpp::DataFrame procedures_df,
                       Rcpp::CharacterVector options = Rcpp::CharacterVector::create(),
                       bool details = true)
{
    RCC_SETUP_LOG_HANDLER();

    static const int task_size = 2048;

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)R_ExternalPtrAddr(classifier_xp);

    unsigned int flags = 0;
    for (const char *opt: options) {
        const OptionDesc *desc = std::find_if(std::begin(mco_ClassifyFlagOptions),
                                              std::end(mco_ClassifyFlagOptions),
                                              [&](const OptionDesc &desc) { return TestStr(desc.name, opt); });
        if (!desc)
            Rcpp::stop("Unknown classifier option '%1'", opt);
        flags |= 1u << (desc - mco_ClassifyFlagOptions);
    }

#define LOAD_OPTIONAL_COLUMN(Var, Name) \
        do { \
            if ((Var ## _df).containsElementNamed(STRINGIFY(Name))) { \
                (Var).Name = (Var ##_df)[STRINGIFY(Name)]; \
            } \
        } while (false)

    LogDebug("Start");

    StaysProxy stays;
    stays.nrow = stays_df.nrow();
    stays.id = stays_df["id"];
    LOAD_OPTIONAL_COLUMN(stays, admin_id);
    LOAD_OPTIONAL_COLUMN(stays, bill_id);
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
    if (!(flags & (int)mco_ClassifyFlag::IgnoreConfirmation)) {
        stays.confirm = stays_df["confirm"];
    }

    DiagnosesProxy diagnoses;
    diagnoses.nrow = diagnoses_df.nrow();
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

    ProceduresProxy procedures;
    procedures.nrow = procedures_df.nrow();
    procedures.id = procedures_df["id"];
    procedures.proc = procedures_df["code"];
    if (!(flags & (int)mco_ClassifyFlag::IgnoreProcedureExtension)) {
        LOAD_OPTIONAL_COLUMN(procedures, extension);
    }
    LOAD_OPTIONAL_COLUMN(procedures, phase);
    procedures.activity = procedures_df["activity"];
    LOAD_OPTIONAL_COLUMN(procedures, count);
    procedures.date = procedures_df["date"];
    if (!(flags & (int)mco_ClassifyFlag::IgnoreProcedureDoc)) {
        LOAD_OPTIONAL_COLUMN(procedures, doc);
    }

#undef LOAD_OPTIONAL_COLUMN

    LogDebug("Classify");

    struct ClassifySet {
        mco_StaySet stay_set;

        HeapArray<mco_Result> results;
        mco_Summary summary;
    };
    HeapArray<ClassifySet> classify_sets;
    classify_sets.Reserve((stays.nrow - 1) / task_size + 1);

    Async async;
    {
        Size stays_offset = 0;
        Size diagnoses_offset = 0;
        Size procedures_offset = 0;
        while (stays_offset < stays.nrow) {
            Size stays_end = std::min(stays.nrow, stays_offset + task_size);
            while (stays_end < stays.nrow &&
                   mco_StaysAreCompatible(stays.bill_id[stays_end - 1], stays.bill_id[stays_end])) {
                stays_end++;
            }

            Size diagnoses_end = diagnoses_offset;
            while (diagnoses_end < diagnoses.nrow &&
                   diagnoses.id[diagnoses_end] <= stays.id[stays_end - 1]) {
                diagnoses_end++;
            }
            Size procedures_end = procedures_offset;
            while (procedures_end < procedures.nrow &&
                   procedures.id[procedures_end] <= stays.id[stays_end - 1]) {
                procedures_end++;
            }

            ClassifySet *classify_set = classify_sets.AppendDefault();

            async.AddTask([=, &stays, &diagnoses, &procedures]() mutable {
                if (!RunClassifier(*classifier, stays, stays_offset, stays_end,
                                   diagnoses, diagnoses_offset, diagnoses_end,
                                   procedures, procedures_offset, procedures_end,
                                   flags, &classify_set->stay_set, &classify_set->results))
                    return false;
                mco_Summarize(classify_set->results, &classify_set->summary);
                return true;
            });

            stays_offset = stays_end;
            diagnoses_offset = diagnoses_end;
            procedures_offset = procedures_end;
        }
    }
    if (!async.Sync()) {
        Rcpp::stop("The 'id' column must be ordered in all data.frames");
    }

    LogDebug("Export");

    mco_Summary summary = {};
    for (const ClassifySet &classify_set: classify_sets) {
        summary += classify_set.summary;
    }

    Rcc_AutoSexp summary_df;
    {
        Rcc_ListBuilder df_builder;
        df_builder.Set("results", (int)summary.results_count);
        df_builder.Set("stays", (int)summary.stays_count);
        df_builder.Set("failures", (int)summary.failures_count);
        df_builder.Set("ghs_cents", (double)summary.ghs_total_cents);
        df_builder.Set("rea_cents", (double)summary.supplement_cents.st.rea);
        df_builder.Set("reasi_cents", (double)summary.supplement_cents.st.reasi);
        df_builder.Set("si_cents", (double)summary.supplement_cents.st.si);
        df_builder.Set("src_cents", (double)summary.supplement_cents.st.src);
        df_builder.Set("nn1_cents", (double)summary.supplement_cents.st.nn1);
        df_builder.Set("nn2_cents", (double)summary.supplement_cents.st.nn2);
        df_builder.Set("nn3_cents", (double)summary.supplement_cents.st.nn3);
        df_builder.Set("rep_cents", (double)summary.supplement_cents.st.rep);
        df_builder.Set("price_cents", (double)summary.total_cents);
        df_builder.Set("rea_days", summary.supplement_days.st.rea);
        df_builder.Set("reasi_days", summary.supplement_days.st.reasi);
        df_builder.Set("si_days", summary.supplement_days.st.si);
        df_builder.Set("src_days", summary.supplement_days.st.src);
        df_builder.Set("nn1_days", summary.supplement_days.st.nn1);
        df_builder.Set("nn2_days", summary.supplement_days.st.nn2);
        df_builder.Set("nn3_days", summary.supplement_days.st.nn3);
        df_builder.Set("rep_days", summary.supplement_days.st.rep);
        summary_df = df_builder.BuildDataFrame();
    }

    Rcc_AutoSexp results_df;
    if (details) {
        char buf[32];

        Rcc_DataFrameBuilder df_builder(summary.results_count);
        Rcc_Vector<int> bill_id = df_builder.Add<int>("bill_id");
        Rcc_Vector<Date> exit_date = df_builder.Add<Date>("exit_date");
        Rcc_Vector<int> stays_count = df_builder.Add<int>("stays_count");
        Rcc_Vector<int> duration = df_builder.Add<int>("duration");
        Rcc_Vector<int> main_stay = df_builder.Add<int>("main_stay");
        Rcc_Vector<const char *> ghm = df_builder.Add<const char *>("ghm");
        Rcc_Vector<int> main_error = df_builder.Add<int>("main_error");
        Rcc_Vector<int> ghs = df_builder.Add<int>("ghs");
        Rcc_Vector<double> ghs_cents = df_builder.Add<double>("ghs_cents");
        Rcc_Vector<double> rea_cents = df_builder.Add<double>("rea_cents");
        Rcc_Vector<double> reasi_cents = df_builder.Add<double>("reasi_cents");
        Rcc_Vector<double> si_cents = df_builder.Add<double>("si_cents");
        Rcc_Vector<double> src_cents = df_builder.Add<double>("src_cents");
        Rcc_Vector<double> nn1_cents = df_builder.Add<double>("nn1_cents");
        Rcc_Vector<double> nn2_cents = df_builder.Add<double>("nn2_cents");
        Rcc_Vector<double> nn3_cents = df_builder.Add<double>("nn3_cents");
        Rcc_Vector<double> rep_cents = df_builder.Add<double>("rep_cents");
        Rcc_Vector<double> price_cents = df_builder.Add<double>("price_cents");
        Rcc_Vector<int> rea_days = df_builder.Add<int>("rea_days");
        Rcc_Vector<int> reasi_days = df_builder.Add<int>("reasi_days");
        Rcc_Vector<int> si_days = df_builder.Add<int>("si_days");
        Rcc_Vector<int> src_days = df_builder.Add<int>("src_days");
        Rcc_Vector<int> nn1_days = df_builder.Add<int>("nn1_days");
        Rcc_Vector<int> nn2_days = df_builder.Add<int>("nn2_days");
        Rcc_Vector<int> nn3_days = df_builder.Add<int>("nn3_days");
        Rcc_Vector<int> rep_days = df_builder.Add<int>("rep_days");

        Size i = 0;
        for (const ClassifySet &classify_set: classify_sets) {
            for (const mco_Result &result: classify_set.results) {
                bill_id[i] = result.stays[0].bill_id;
                exit_date.Set(i, result.stays[result.stays.len - 1].exit.date);
                stays_count[i] = (int)result.stays.len;
                duration[i] = result.duration;
                main_stay[i] = (int)result.main_stay_idx + 1;
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
                rea_days[i] = result.supplement_days.st.rea;
                reasi_days[i] = result.supplement_days.st.reasi;
                si_days[i] = result.supplement_days.st.si;
                src_days[i] = result.supplement_days.st.src;
                nn1_days[i] = result.supplement_days.st.nn1;
                nn2_days[i] = result.supplement_days.st.nn2;
                nn3_days[i] = result.supplement_days.st.nn3;
                rep_days[i] = result.supplement_days.st.rep;

                i++;
            }
        }

        results_df = df_builder.Build();
    } else {
        results_df = R_NilValue;
    }

    Rcc_AutoSexp ret_list;
    {
        Rcc_ListBuilder ret_builder;
        ret_builder.Add("summary", summary_df);
        ret_builder.Add("results", results_df);
        ret_list = ret_builder.BuildList();
    }

    return ret_list;
}

// [[Rcpp::export(name = 'mco_diagnoses')]]
SEXP drdR_mco_Diagnoses(SEXP classifier_xp, SEXP date_xp)
{
    RCC_SETUP_LOG_HANDLER();

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)R_ExternalPtrAddr(classifier_xp);

    Date date = Rcc_Vector<Date>(date_xp).Value();
    if (!date.value)
        Rcc_StopWithLastError();

    const mco_TableIndex *index = classifier->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        Rcc_StopWithLastError();
    }

    Rcc_AutoSexp diagnoses_df;
    {
        Rcc_DataFrameBuilder df_builder(index->diagnoses.len);
        Rcc_Vector<const char *> diag = df_builder.Add<const char *>("diag");
        Rcc_Vector<int> cmd_m = df_builder.Add<int>("cmd_m");
        Rcc_Vector<int> cmd_f = df_builder.Add<int>("cmd_f");

        for (Size i = 0; i < index->diagnoses.len; i++) {
            const mco_DiagnosisInfo &info = index->diagnoses[i];
            char buf[32];

            diag.Set(i, Fmt(buf, "%1", info.diag));
            cmd_m[i] = info.Attributes(1).cmd;
            cmd_f[i] = info.Attributes(2).cmd;
        }

        diagnoses_df = df_builder.Build();
    }

    return diagnoses_df;
}

// [[Rcpp::export(name = 'mco_procedures')]]
SEXP drdR_mco_Procedures(SEXP classifier_xp, SEXP date_xp)
{
    RCC_SETUP_LOG_HANDLER();

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)R_ExternalPtrAddr(classifier_xp);

    Date date = Rcc_Vector<Date>(date_xp).Value();
    if (!date.value)
        Rcc_StopWithLastError();

    const mco_TableIndex *index = classifier->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        Rcc_StopWithLastError();
    }

    Rcc_AutoSexp procedures_df;
    {
        Rcc_DataFrameBuilder df_builder(index->procedures.len);
        Rcc_Vector<const char *> proc = df_builder.Add<const char *>("proc");
        Rcc_Vector<int> phase = df_builder.Add<int>("phase");
        Rcc_Vector<int> activities = df_builder.Add<int>("activities");
        Rcc_Vector<Date> start_date = df_builder.Add<Date>("start_date");
        Rcc_Vector<Date> end_date = df_builder.Add<Date>("end_date");

        for (Size i = 0; i < index->procedures.len; i++) {
            const mco_ProcedureInfo &info = index->procedures[i];
            char buf[32];

            proc.Set(i, Fmt(buf, "%1", info.proc));
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
            start_date.Set(i, info.limit_dates[0]);
            end_date.Set(i, info.limit_dates[1]);
        }

        procedures_df = df_builder.Build();
    }

    return procedures_df;
}

// [[Rcpp::export(name = 'mco_load_stays')]]
SEXP drdR_mco_LoadStays(Rcpp::CharacterVector filenames)
{
    RCC_SETUP_LOG_HANDLER();

    mco_StaySet stay_set;
    {
        mco_StaySetBuilder stay_set_builder;

        bool valid = true;
        for (const char *filename: filenames) {
            valid &= stay_set_builder.LoadFiles(filename);
        }
        if (!valid)
            Rcc_StopWithLastError();

        if (!stay_set_builder.Finish(&stay_set))
            Rcc_StopWithLastError();
    }

    if (stay_set.stays.len >= INT_MAX)
        Rcpp::stop("Cannot load more than %1 stays in data.frame", INT_MAX);

    SEXP stays_df;
    SEXP diagnoses_df;
    SEXP procedures_df;
    {
        Rcc_DataFrameBuilder stays_builder(stay_set.stays.len);
        Rcc_Vector<int> stays_id = stays_builder.Add<int>("id");
        Rcc_Vector<int> stays_admin_id = stays_builder.Add<int>("admin_id");
        Rcc_Vector<int> stays_bill_id = stays_builder.Add<int>("bill_id");
        Rcc_Vector<int> stays_sex = stays_builder.Add<int>("sex");
        Rcc_Vector<Date> stays_birthdate = stays_builder.Add<Date>("birthdate");
        Rcc_Vector<Date> stays_entry_date = stays_builder.Add<Date>("entry_date");
        Rcc_Vector<int> stays_entry_mode = stays_builder.Add<int>("entry_mode");
        Rcc_Vector<const char *> stays_entry_origin = stays_builder.Add<const char *>("entry_origin");
        Rcc_Vector<Date> stays_exit_date = stays_builder.Add<Date>("exit_date");
        Rcc_Vector<int> stays_exit_mode = stays_builder.Add<int>("exit_mode");
        Rcc_Vector<int> stays_exit_destination = stays_builder.Add<int>("exit_destination");
        Rcc_Vector<int> stays_unit = stays_builder.Add<int>("unit");
        Rcc_Vector<int> stays_bed_authorization = stays_builder.Add<int>("bed_authorization");
        Rcc_Vector<int> stays_session_count = stays_builder.Add<int>("session_count");
        Rcc_Vector<int> stays_igs2 = stays_builder.Add<int>("igs2");
        Rcc_Vector<Date> stays_last_menstrual_period = stays_builder.Add<Date>("last_menstrual_period");
        Rcc_Vector<int> stays_gestational_age = stays_builder.Add<int>("gestational_age");
        Rcc_Vector<int> stays_newborn_weight = stays_builder.Add<int>("newborn_weight");
        Rcc_Vector<const char *> stays_main_diagnosis = stays_builder.Add<const char *>("main_diagnosis");
        Rcc_Vector<const char *> stays_linked_diagnosis = stays_builder.Add<const char *>("linked_diagnosis");
        Rcc_Vector<int> stays_confirm = stays_builder.Add<int>("confirm");

        Rcc_DataFrameBuilder diagnoses_builder(stay_set.store.diagnoses.len);
        Rcc_Vector<int> diagnoses_id = diagnoses_builder.Add<int>("id");
        Rcc_Vector<const char *> diagnoses_diag = diagnoses_builder.Add<const char *>("diag");

        Rcc_DataFrameBuilder procedures_builder(stay_set.store.procedures.len);
        Rcc_Vector<int> procedures_id = procedures_builder.Add<int>("id");
        Rcc_Vector<const char *> procedures_proc = procedures_builder.Add<const char *>("code");
        Rcc_Vector<int> procedures_extension = procedures_builder.Add<int>("extension");
        Rcc_Vector<int> procedures_phase = procedures_builder.Add<int>("phase");
        Rcc_Vector<int> procedures_activity = procedures_builder.Add<int>("activity");
        Rcc_Vector<int> procedures_count = procedures_builder.Add<int>("count");
        Rcc_Vector<Date> procedures_date = procedures_builder.Add<Date>("date");
        Rcc_Vector<const char *> procedures_doc = procedures_builder.Add<const char *>("doc");

        Size j = 0, k = 0;
        for (Size i = 0; i < stay_set.stays.len; i++) {
            char buf[32];

            const mco_Stay &stay = stay_set.stays[i];

            stays_id[i] = (int)(i + 1);
            stays_admin_id[i] = LIKELY(stay.admin_id) ? stay.admin_id : NA_INTEGER;
            stays_bill_id[i] = LIKELY(stay.bill_id) ? stay.bill_id : NA_INTEGER;
            stays_sex[i] = LIKELY(stay.sex) ? stay.sex : NA_INTEGER;
            stays_birthdate.Set(i, stay.birthdate);
            stays_entry_date.Set(i, stay.entry.date);
            stays_entry_mode[i] = LIKELY(stay.entry.mode) ? stay.entry.mode - '0' : NA_INTEGER;
            if (stay.entry.origin) {
                stays_entry_origin.Set(i, stay.entry.origin);
            } else {
                stays_entry_origin.Set(i, nullptr);
            }
            stays_exit_date.Set(i, stay.exit.date);
            stays_exit_mode[i] = LIKELY(stay.exit.mode) ? stay.exit.mode - '0' : NA_INTEGER;
            stays_exit_destination[i] = stay.exit.destination ? stay.exit.destination - '0' : NA_INTEGER;
            stays_unit[i] = LIKELY(stay.unit.number) ? stay.unit.number : NA_INTEGER;
            stays_bed_authorization[i] = stay.bed_authorization ? stay.bed_authorization : NA_INTEGER;
            stays_session_count[i] = stay.session_count;
            stays_igs2[i] = stay.igs2 ? stay.igs2 : NA_INTEGER;
            stays_last_menstrual_period.Set(i, stay.last_menstrual_period);
            stays_gestational_age[i] = stay.gestational_age ? stay.gestational_age : NA_INTEGER;
            stays_newborn_weight[i] = stay.newborn_weight ? stay.newborn_weight : NA_INTEGER;
            if (LIKELY(stay.main_diagnosis.IsValid())) {
                stays_main_diagnosis.Set(i, Fmt(buf, "%1", stay.main_diagnosis));
            } else {
                stays_main_diagnosis.Set(i, nullptr);
            }
            if (stay.linked_diagnosis.IsValid()) {
                stays_linked_diagnosis.Set(i, Fmt(buf, "%1", stay.linked_diagnosis));
            } else {
                stays_linked_diagnosis.Set(i, nullptr);
            }
            stays_confirm[i] = !!(stay.flags & (int)mco_Stay::Flag::Confirmed);

            for (DiagnosisCode diag: stay.diagnoses) {
                diagnoses_id[j] = (int)(i + 1);
                diagnoses_diag.Set(j, Fmt(buf, "%1", diag));
                j++;
            }

            for (const mco_ProcedureRealisation &proc: stay.procedures) {
                procedures_id[k] = (int)(i + 1);
                procedures_proc.Set(k, Fmt(buf, "%1", proc.proc));
                procedures_extension[k] = proc.extension ? proc.extension : NA_INTEGER;
                procedures_phase[k] = proc.phase;
                {
                    int activities_dec = 0;
                    for (int i = 1; i < 8; i++) {
                        if (proc.activities & (1 << i)) {
                            activities_dec = (activities_dec * 10) + i;
                        }
                    }
                    procedures_activity[k] = activities_dec;
                }
                procedures_date.Set(k, proc.date);
                procedures_count[k] = LIKELY(proc.count) ? proc.count : NA_INTEGER;
                if (proc.doc) {
                    procedures_doc.Set(k, proc.doc);
                } else {
                    procedures_doc.Set(k, nullptr);
                }
                k++;
            }
        }

        stays_df = stays_builder.Build();
        diagnoses_df = diagnoses_builder.Build();
        procedures_df = procedures_builder.Build();
    }

    SEXP list;
    {
        Rcc_ListBuilder list_builder;
        list_builder.Add("stays", stays_df);
        list_builder.Add("diagnoses", diagnoses_df);
        list_builder.Add("procedures", procedures_df);
        list = list_builder.BuildList();
    }

    return list;
}
