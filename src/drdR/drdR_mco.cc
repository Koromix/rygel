// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../libdrd/libdrd.hh"
#include "../common/Rcc.hh"

struct ClassifierInstance {
    mco_TableSet table_set;
    mco_AuthorizationSet authorization_set;
};

RcppExport SEXP drdR_Options(SEXP debug = R_NilValue)
{
    if (!Rf_isNull(debug)) {
        enable_debug = Rcpp::as<bool>(debug);
    }

    return Rcpp::List::create(
        Rcpp::Named("debug") = enable_debug
    );
}

RcppExport SEXP drdR_mco_Init(SEXP data_dirs_xp, SEXP table_dirs_xp, SEXP table_filenames_xp,
                              SEXP authorization_filename_xp)
{
    BEGIN_RCPP
    RCC_SETUP_LOG_HANDLER();

    Rcc_Vector<const char *> data_dirs(data_dirs_xp);
    Rcc_Vector<const char *> table_dirs(table_dirs_xp);
    Rcc_Vector<const char *> table_filenames(table_filenames_xp);
    Rcc_Vector<const char *> authorization_filename(authorization_filename_xp);
    if (authorization_filename.Len() > 1)
        Rcpp::stop("Cannot load more than one authorization file");

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
    if (authorization_filename.Len()) {
        authorization_filename2 = authorization_filename[0].ptr;
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

    END_RCPP
}

struct StaysProxy {
    Size nrow;

    Rcc_NumericVector<int> id;

    Rcc_NumericVector<int> admin_id;
    Rcc_NumericVector<int> bill_id;
    Rcc_Vector<Date> birthdate;
    Rcc_NumericVector<int> sex;
    Rcc_Vector<Date> entry_date;
    Rcc_NumericVector<int> entry_mode;
    Rcc_Vector<const char *> entry_origin;
    Rcc_Vector<Date> exit_date;
    Rcc_NumericVector<int> exit_mode;
    Rcc_NumericVector<int> exit_destination;
    Rcc_NumericVector<int> unit;
    Rcc_NumericVector<int> bed_authorization;
    Rcc_NumericVector<int> session_count;
    Rcc_NumericVector<int> igs2;
    Rcc_NumericVector<int> gestational_age;
    Rcc_NumericVector<int> newborn_weight;
    Rcc_Vector<Date> last_menstrual_period;

    Rcc_Vector<const char *> main_diagnosis;
    Rcc_Vector<const char *> linked_diagnosis;

    Rcc_NumericVector<int> confirm;
};

struct DiagnosesProxy {
    Size nrow;

    Rcc_NumericVector<int> id;

    Rcc_Vector<const char *> diag;
    Rcc_Vector<const char *> type;
};

struct ProceduresProxy {
    Size nrow;

    Rcc_NumericVector<int> id;

    Rcc_Vector<const char *> proc;
    Rcc_NumericVector<int> extension;
    Rcc_NumericVector<int> phase;
    Rcc_NumericVector<int> activity;
    Rcc_NumericVector<int> count;
    Rcc_Vector<Date> date;
    Rcc_Vector<const char *> doc;
};

static bool RunClassifier(const ClassifierInstance &classifier,
                          const StaysProxy &stays, Size stays_offset, Size stays_end,
                          const DiagnosesProxy &diagnoses, Size diagnoses_offset, Size diagnoses_end,
                          const ProceduresProxy &procedures, Size procedures_offset, Size procedures_end,
                          unsigned int flags, mco_StaySet *out_stay_set,
                          HeapArray<mco_Result> *out_results, HeapArray<mco_Result> *out_mono_results)
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
        stay.bill_id = stays.bill_id[i];
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
    mco_Classify(classifier.table_set, classifier.authorization_set, out_stay_set->stays,
                 flags, out_results, out_mono_results);

    return true;
}

static void MakeSupplementColumnName(const char *supplement_type, const char *suffix,
                                     char out_buf[32])
{
    Size i;
    for (i = 0; i < 16 && supplement_type[i]; i++) {
        out_buf[i] = LowerAscii(supplement_type[i]);
    }
    strcpy(out_buf + i, suffix);
}

static SEXP ExportResultsDataFrame(Span<const HeapArray<mco_Result>> result_sets,
                                   Span<const HeapArray<mco_Pricing>> pricing_sets,
                                   bool export_units, bool apply_coefficient)
{
    Size results_count = 0;
    for (const HeapArray<mco_Result> &results: result_sets) {
        results_count += results.len;
    }

    Rcc_DataFrameBuilder df_builder(results_count);
    Rcc_Vector<int> bill_id = df_builder.Add<int>("bill_id");
    Rcc_Vector<int> unit;
    if (export_units) {
        unit = df_builder.Add<int>("unit");
    }
    Rcc_Vector<Date> exit_date = df_builder.Add<Date>("exit_date");
    Rcc_Vector<int> stays_count = df_builder.Add<int>("stays_count");
    Rcc_Vector<int> duration = df_builder.Add<int>("duration");
    Rcc_Vector<int> main_stay = df_builder.Add<int>("main_stay");
    Rcc_Vector<const char *> ghm = df_builder.Add<const char *>("ghm");
    Rcc_Vector<int> main_error = df_builder.Add<int>("main_error");
    Rcc_Vector<int> ghs = df_builder.Add<int>("ghs");
    Rcc_Vector<double> total_cents = df_builder.Add<double>("total_cents");
    Rcc_Vector<double> price_cents = df_builder.Add<double>("price_cents");
    Rcc_Vector<double> ghs_cents = df_builder.Add<double>("ghs_cents");
    Rcc_Vector<double> ghs_coefficient = df_builder.Add<double>("ghs_coefficient");
    Rcc_Vector<int> exb_exh = df_builder.Add<int>("exb_exh");
    Rcc_Vector<double> supplement_cents[ARRAY_SIZE(mco_SupplementTypeNames)];
    Rcc_Vector<int> supplement_count[ARRAY_SIZE(mco_SupplementTypeNames)];
    for (Size i = 0; i < ARRAY_SIZE(mco_SupplementTypeNames); i++) {
        char name_buf[32];
        MakeSupplementColumnName(mco_SupplementTypeNames[i], "_cents", name_buf);
        supplement_cents[i] = df_builder.Add<double>(name_buf);
    }
    for (Size i = 0; i < ARRAY_SIZE(mco_SupplementTypeNames); i++) {
        char name_buf[32];
        MakeSupplementColumnName(mco_SupplementTypeNames[i], "_count", name_buf);
        supplement_count[i] = df_builder.Add<int>(name_buf);
    }

    Size k = 0;
    for (Size i = 0; i < result_sets.len; i++) {
        Span<const mco_Result> results = result_sets[i];
        Span<const mco_Pricing> pricings = pricing_sets[i];

        for (Size j = 0; j < results.len; j++) {
            const mco_Result &result = results[j];
            mco_Pricing pricing = apply_coefficient ? pricings[j].WithCoefficient() : pricings[j];

            char buf[32];

            bill_id[k] = result.stays[0].bill_id;
            if (export_units) {
                DebugAssert(result.stays.len == 1);
                unit[k] = result.stays[0].unit.number;
            }
            exit_date.Set(k, result.stays[result.stays.len - 1].exit.date);
            stays_count[k] = (int)result.stays.len;
            duration[k] = (result.duration >= 0) ? result.duration : NA_INTEGER;
            main_stay[k] = (int)result.main_stay_idx + 1;
            if (result.ghm.IsValid()) {
                ghm.Set(k, result.ghm.ToString(buf));
                main_error[k] = result.main_error;
            } else {
                ghm.Set(k, nullptr);
                main_error[k] = NA_INTEGER;
            }
            ghs[k] = result.ghs.number;
            total_cents[k] = pricing.total_cents;
            price_cents[k] = pricing.price_cents;
            ghs_cents[k] = (double)pricing.ghs_cents;
            ghs_coefficient[k] = (double)pricing.ghs_coefficient;
            exb_exh[k] = pricing.exb_exh;
            for (Size l = 0; l < ARRAY_SIZE(mco_SupplementTypeNames); l++) {
                supplement_cents[l][k] = pricing.supplement_cents.values[l];
                supplement_count[l][k] = result.supplement_days.values[l];
            }

            k++;
        }
    }

    return df_builder.Build();
}

RcppExport SEXP drdR_mco_Classify(SEXP classifier_xp, SEXP stays_xp, SEXP diagnoses_xp,
                                  SEXP procedures_xp, SEXP options_xp, SEXP details_xp,
                                  SEXP dispense_mode_xp, SEXP apply_coefficient_xp)
{
    BEGIN_RCPP
    RCC_SETUP_LOG_HANDLER();

    static const int task_size = 2048;

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)Rcc_GetPointerSafe(classifier_xp);
    Rcpp::DataFrame stays_df(stays_xp);
    Rcpp::DataFrame diagnoses_df(diagnoses_xp);
    Rcpp::DataFrame procedures_df(procedures_xp);
    Rcpp::CharacterVector options_vec(options_xp);
    bool details = Rcpp::as<bool>(details_xp);
    const char *dispense_mode_str = !Rf_isNull(dispense_mode_xp) ?
                                    Rcpp::as<const char *>(dispense_mode_xp) : nullptr;
    bool apply_coefficient = Rcpp::as<bool>(apply_coefficient_xp);

    unsigned int flags = 0;
    for (const char *opt: options_vec) {
        const OptionDesc *desc = std::find_if(std::begin(mco_ClassifyFlagOptions),
                                              std::end(mco_ClassifyFlagOptions),
                                              [&](const OptionDesc &desc) { return TestStr(desc.name, opt); });
        if (desc == std::end(mco_ClassifyFlagOptions))
            Rcpp::stop("Unknown classifier option '%1'", opt);
        flags |= 1u << (desc - mco_ClassifyFlagOptions);
    }

    int dispense_mode = -1;
    if (dispense_mode_str) {
        const OptionDesc *desc = std::find_if(std::begin(mco_DispenseModeOptions),
                                              std::end(mco_DispenseModeOptions),
                                              [&](const OptionDesc &desc) { return TestStr(desc.name, dispense_mode_str); });
        if (desc == std::end(mco_DispenseModeOptions)) {
            LogError("Unknown dispensation mode '%1'", dispense_mode_str);
            Rcc_StopWithLastError();
        }
        dispense_mode = (int)(desc - mco_DispenseModeOptions);
        flags |= (int)mco_ClassifyFlag::MonoResults;
    } else {
        flags &= (int)mco_ClassifyFlag::MonoResults;
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
    stays.bill_id = stays_df["bill_id"];
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

    Size sets_count = (stays.nrow - 1) / task_size + 1;
    HeapArray<mco_StaySet> stay_sets(sets_count);
    HeapArray<HeapArray<mco_Result>> result_sets(sets_count);
    HeapArray<HeapArray<mco_Pricing>> pricing_sets(sets_count);
    HeapArray<HeapArray<mco_Result>> mono_result_sets;
    HeapArray<HeapArray<mco_Pricing>> mono_pricing_sets(sets_count);
    if (flags & (int)mco_ClassifyFlag::MonoResults) {
        mono_result_sets.Reserve(sets_count);
        mono_pricing_sets.Reserve(sets_count);
    }
    HeapArray<mco_Pricing> summaries(sets_count);

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

            mco_StaySet *task_stay_set = stay_sets.AppendDefault();
            HeapArray<mco_Result> *task_results = result_sets.AppendDefault();
            HeapArray<mco_Pricing> *task_pricings = pricing_sets.AppendDefault();
            HeapArray<mco_Result> *task_mono_results = nullptr;
            HeapArray<mco_Pricing> *task_mono_pricings = nullptr;
            if (flags & (int)mco_ClassifyFlag::MonoResults) {
                task_mono_results = mono_result_sets.AppendDefault();
                task_mono_pricings = mono_pricing_sets.AppendDefault();
            }
            mco_Pricing *task_summary = summaries.AppendDefault();

            async.AddTask([=, &stays, &diagnoses, &procedures]() mutable {
                if (!RunClassifier(*classifier, stays, stays_offset, stays_end,
                                   diagnoses, diagnoses_offset, diagnoses_end,
                                   procedures, procedures_offset, procedures_end,
                                   flags, task_stay_set, task_results, task_mono_results))
                    return false;

                if (details || dispense_mode >= 0) {
                    mco_Price(*task_results, apply_coefficient, task_pricings);
                    if (dispense_mode >= 0) {
                        mco_Dispense(*task_pricings, *task_mono_results,
                                     (mco_DispenseMode)dispense_mode, task_mono_pricings);
                    }
                    mco_Summarize(*task_pricings, task_summary);
                } else {
                    mco_PriceTotal(*task_results, apply_coefficient, task_summary);
                }

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

    mco_Pricing summary = {};
    mco_Summarize(summaries, &summary);

    Rcc_AutoSexp summary_df;
    {
        Rcc_ListBuilder df_builder;
        df_builder.Set("results", (int)summary.results_count);
        df_builder.Set("stays", (int)summary.stays_count);
        df_builder.Set("failures", (int)summary.failures_count);
        df_builder.Set("total_cents", (double)summary.total_cents);
        df_builder.Set("price_cents", (double)summary.price_cents);
        df_builder.Set("ghs_cents", (double)summary.ghs_cents);
        for (Size i = 0; i < ARRAY_SIZE(mco_SupplementTypeNames); i++) {
            char name_buf[32];
            MakeSupplementColumnName(mco_SupplementTypeNames[i], "_cents", name_buf);
            df_builder.Set(name_buf, (double)summary.supplement_cents.values[i]);
        }
        for (Size i = 0; i < ARRAY_SIZE(mco_SupplementTypeNames); i++) {
            char name_buf[32];
            MakeSupplementColumnName(mco_SupplementTypeNames[i], "_count", name_buf);
            df_builder.Set(name_buf, (int)summary.supplement_days.values[i]);
        }
        summary_df = df_builder.BuildDataFrame();
    }

    Rcc_AutoSexp results_df;
    if (details) {
        results_df = ExportResultsDataFrame(result_sets, pricing_sets, false, apply_coefficient);
    }

    Rcc_AutoSexp mono_results_df;
    if (flags & (int)mco_ClassifyFlag::MonoResults) {
        mono_results_df = ExportResultsDataFrame(mono_result_sets, mono_pricing_sets, true,
                                                 apply_coefficient);
    }

    Rcc_AutoSexp ret_list;
    {
        Rcc_ListBuilder ret_builder;
        ret_builder.Add("summary", summary_df);
        if (results_df) {
            ret_builder.Add("results", results_df);
        }
        if (mono_results_df) {
            ret_builder.Add("mono_results", mono_results_df);
        }
        ret_list = ret_builder.BuildList();
    }

    return ret_list;

    END_RCPP
}

RcppExport SEXP drdR_mco_Diagnoses(SEXP classifier_xp, SEXP date_xp)
{
    BEGIN_RCPP
    RCC_SETUP_LOG_HANDLER();

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)Rcc_GetPointerSafe(classifier_xp);

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

            diag.Set(i, info.diag.str);
            cmd_m[i] = info.Attributes(1).cmd;
            cmd_f[i] = info.Attributes(2).cmd;
        }

        diagnoses_df = df_builder.Build();
    }

    return diagnoses_df;

    END_RCPP
}

RcppExport SEXP drdR_mco_Procedures(SEXP classifier_xp, SEXP date_xp)
{
    BEGIN_RCPP
    RCC_SETUP_LOG_HANDLER();

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)Rcc_GetPointerSafe(classifier_xp);

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
        Rcc_Vector<Date> start_date = df_builder.Add<Date>("start_date");
        Rcc_Vector<Date> end_date = df_builder.Add<Date>("end_date");
        Rcc_Vector<int> activities = df_builder.Add<int>("activities");
        Rcc_Vector<int> extensions = df_builder.Add<int>("extensions");

        for (Size i = 0; i < index->procedures.len; i++) {
            const mco_ProcedureInfo &proc_info = index->procedures[i];

            proc.Set(i, proc_info.proc.str);
            phase[i] = proc_info.phase;
            start_date.Set(i, proc_info.limit_dates[0]);
            end_date.Set(i, proc_info.limit_dates[1]);
            activities[i] = proc_info.ActivitiesToDec();
            extensions[i] = proc_info.ExtensionsToDec();
        }

        procedures_df = df_builder.Build();
    }

    return procedures_df;

    END_RCPP
}

RcppExport SEXP drdR_mco_LoadStays(SEXP filenames_xp)
{
    BEGIN_RCPP
    RCC_SETUP_LOG_HANDLER();

    Rcc_Vector<const char *> filenames(filenames_xp);

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
                stays_main_diagnosis.Set(i, stay.main_diagnosis.str);
            } else {
                stays_main_diagnosis.Set(i, nullptr);
            }
            if (stay.linked_diagnosis.IsValid()) {
                stays_linked_diagnosis.Set(i, stay.linked_diagnosis.str);
            } else {
                stays_linked_diagnosis.Set(i, nullptr);
            }
            stays_confirm[i] = !!(stay.flags & (int)mco_Stay::Flag::Confirmed);

            for (DiagnosisCode diag: stay.diagnoses) {
                diagnoses_id[j] = (int)(i + 1);
                diagnoses_diag.Set(j, diag.str);
                j++;
            }

            for (const mco_ProcedureRealisation &proc: stay.procedures) {
                procedures_id[k] = (int)(i + 1);
                procedures_proc.Set(k, proc.proc.str);
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

    END_RCPP
}

RcppExport SEXP drdR_mco_SupplementTypes()
{
    Rcc_Vector<const char *> types(ARRAY_SIZE(mco_SupplementTypeNames));
    for (Size i = 0; i < ARRAY_SIZE(mco_SupplementTypeNames); i++) {
        types.Set(i, mco_SupplementTypeNames[i]);
    }

    return types;
}

RcppExport void R_init_drdR(DllInfo *dll) {
    static const R_CallMethodDef call_entries[] = {
        {"drdR_Options", (DL_FUNC)&drdR_Options, 1},
        {"drdR_mco_Init", (DL_FUNC)&drdR_mco_Init, 4},
        {"drdR_mco_Classify", (DL_FUNC)&drdR_mco_Classify, 8},
        // {"drdR_mco_Dispense", (DL_FUNC)&drdR_mco_Dispense, 3},
        {"drdR_mco_Diagnoses", (DL_FUNC)&drdR_mco_Diagnoses, 2},
        {"drdR_mco_Procedures", (DL_FUNC)&drdR_mco_Procedures, 2},
        {"drdR_mco_LoadStays", (DL_FUNC)&drdR_mco_LoadStays, 1},
        {"drdR_mco_SupplementTypes", (DL_FUNC)&drdR_mco_SupplementTypes, 0},
        {}
    };

    R_registerRoutines(dll, nullptr, call_entries, nullptr, nullptr);
    R_useDynamicSymbols(dll, FALSE);
}
