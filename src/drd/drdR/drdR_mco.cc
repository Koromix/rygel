// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "../../core/libcc/libcc.hh"
#include "../libdrd/libdrd.hh"
#include "../../core/libwrap/Rcc.hh"

namespace RG {

struct ClassifierInstance {
    mco_TableSet table_set;
    mco_AuthorizationSet authorization_set;

    drd_Sector default_sector;
};

static drd_Sector GetSectorFromString(SEXP sector_xp, drd_Sector default_sector)
{
    const char *sector_str = !Rf_isNull(sector_xp) ? Rcpp::as<const char *>(sector_xp) : nullptr;

    if (sector_str) {
        const char *const *ptr =
            FindIfPtr(drd_SectorNames,
                      [&](const char *name) { return TestStr(name, sector_str); });
        if (!ptr) {
            LogError("Sector '%1' does not exist", sector_str);
            rcc_StopWithLastError();
        }

        return (drd_Sector)(ptr - drd_SectorNames);
    } else {
        return default_sector;
    }
}

RcppExport SEXP drdR_mco_Init(SEXP table_dirs_xp, SEXP table_filenames_xp,
                              SEXP authorization_filename_xp, SEXP default_sector_xp)
{
    BEGIN_RCPP
    RG_DEFER { rcc_DumpWarnings(); };

    rcc_Vector<const char *> table_dirs(table_dirs_xp);
    rcc_Vector<const char *> table_filenames(table_filenames_xp);
    rcc_Vector<const char *> authorization_filename(authorization_filename_xp);
    if (authorization_filename.Len() > 1)
        Rcpp::stop("Cannot load more than one authorization file");

    ClassifierInstance *classifier = new ClassifierInstance;
    RG_DEFER_N(classifier_guard) { delete classifier; };

    HeapArray<const char *> table_dirs2;
    HeapArray<const char *> table_filenames2;
    const char *authorization_filename2 = nullptr;
    for (const char *str: table_dirs) {
        table_dirs2.Append(str);
    }
    for (const char *str: table_filenames) {
        table_filenames2.Append(str);
    }
    if (authorization_filename.Len()) {
        authorization_filename2 = authorization_filename[0].ptr;
    }

    classifier->default_sector = GetSectorFromString(default_sector_xp, drd_Sector::Public);

    LogInfo("Load tables");
    if (!mco_LoadTableSet(table_dirs2, table_filenames2, &classifier->table_set) ||
            !classifier->table_set.indexes.len)
        rcc_StopWithLastError();

    LogInfo("Load authorizations");
    if (!mco_LoadAuthorizationSet(nullptr, authorization_filename2, &classifier->authorization_set))
        rcc_StopWithLastError();

    SEXP classifier_xp = R_MakeExternalPtr(classifier, R_NilValue, R_NilValue);
    R_RegisterCFinalizerEx(classifier_xp, [](SEXP classifier_xp) {
        ClassifierInstance *classifier = (ClassifierInstance *)R_ExternalPtrAddr(classifier_xp);
        delete classifier;
    }, TRUE);
    classifier_guard.Disable();

    return classifier_xp;

    END_RCPP
}

struct StaysProxy {
    Size nrow;

    rcc_NumericVector<int> id;

    rcc_NumericVector<int> admin_id;
    rcc_NumericVector<int> bill_id;
    rcc_Vector<Date> birthdate;
    rcc_NumericVector<int> sex;
    rcc_Vector<Date> entry_date;
    rcc_NumericVector<int> entry_mode;
    rcc_Vector<const char *> entry_origin;
    rcc_Vector<Date> exit_date;
    rcc_NumericVector<int> exit_mode;
    rcc_NumericVector<int> exit_destination;
    rcc_NumericVector<int> unit;
    rcc_NumericVector<int> bed_authorization;
    rcc_NumericVector<int> session_count;
    rcc_NumericVector<int> igs2;
    rcc_NumericVector<int> gestational_age;
    rcc_NumericVector<int> newborn_weight;
    rcc_Vector<Date> last_menstrual_period;

    rcc_Vector<const char *> main_diagnosis;
    rcc_Vector<const char *> linked_diagnosis;

    rcc_NumericVector<int> confirm;
    rcc_NumericVector<int> ucd;
    rcc_NumericVector<int> raac;
    rcc_NumericVector<int> dip_count;
};

struct DiagnosesProxy {
    Size nrow;

    rcc_NumericVector<int> id;

    rcc_Vector<const char *> diag;
    rcc_Vector<const char *> type;
};

struct ProceduresProxy {
    Size nrow;

    rcc_NumericVector<int> id;

    rcc_Vector<const char *> proc;
    rcc_NumericVector<int> extension;
    rcc_NumericVector<int> phase;
    rcc_NumericVector<int> activity;
    rcc_NumericVector<int> count;
    rcc_Vector<Date> date;
    rcc_Vector<const char *> doc;
};

static bool RunClassifier(const ClassifierInstance &classifier,
                          const StaysProxy &stays, Size stays_offset, Size stays_end,
                          const DiagnosesProxy &diagnoses, Size diagnoses_offset, Size diagnoses_end,
                          const ProceduresProxy &procedures, Size procedures_offset, Size procedures_end,
                          drd_Sector sector, unsigned int flags, mco_StaySet *out_stay_set,
                          HeapArray<mco_Result> *out_results, HeapArray<mco_Result> *out_mono_results)
{
    out_stay_set->stays.Reserve(stays_end - stays_offset);

    HeapArray<drd_DiagnosisCode> other_diagnoses2(&out_stay_set->array_alloc);
    HeapArray<mco_ProcedureRealisation> procedures2(&out_stay_set->array_alloc);
    other_diagnoses2.Reserve((stays_end - stays_offset) * 2 + diagnoses_end - diagnoses_offset);
    procedures2.Reserve(procedures_end - procedures_offset);

    Size j = diagnoses_offset;
    Size k = procedures_offset;
    for (Size i = stays_offset; i < stays_end; i++) {
        mco_Stay stay = {};

        if (RG_UNLIKELY(i && (stays.id[i] < stays.id[i - 1] ||
                              (j < diagnoses_end && diagnoses.id[j] < stays.id[i - 1]) ||
                              (k < procedures_end && procedures.id[k] < stays.id[i - 1]))))
            return false;

        stay.admin_id = rcc_GetOptional(stays.admin_id, i, 0);
        stay.bill_id = stays.bill_id[i];
        stay.birthdate = stays.birthdate[i];
        if (RG_UNLIKELY(stay.birthdate.value && !stay.birthdate.IsValid())) {
            stay.errors |= (int)mco_Stay::Error::MalformedBirthdate;
        }
        if (stays.sex[i] != NA_INTEGER) {
            stay.sex = (int8_t)stays.sex[i];
            if (RG_UNLIKELY(stay.sex != stays.sex[i])) {
                stay.errors |= (int)mco_Stay::Error::MalformedSex;
            }
        }
        stay.entry.date = stays.entry_date[i];
        if (RG_UNLIKELY(stay.entry.date.value && !stay.entry.date.IsValid())) {
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
        if (RG_UNLIKELY(stay.exit.date.value && !stay.exit.date.IsValid())) {
            stay.errors |= (int)mco_Stay::Error::MalformedExitDate;
        }
        stay.exit.mode = (char)('0' + stays.exit_mode[i]);
        stay.exit.destination = (char)('0' + rcc_GetOptional(stays.exit_destination, i, -'0'));

        stay.unit.number = (int16_t)rcc_GetOptional(stays.unit, i, 0);
        stay.bed_authorization = (int8_t)rcc_GetOptional(stays.bed_authorization, i, 0);
        stay.session_count = (int16_t)rcc_GetOptional(stays.session_count, i, 0);
        stay.igs2 = (int16_t)rcc_GetOptional(stays.igs2, i, 0);
        stay.gestational_age = (int16_t)stays.gestational_age[i];
        stay.newborn_weight = (int16_t)stays.newborn_weight[i];
        stay.last_menstrual_period = stays.last_menstrual_period[i];
        if (stays.confirm.Len() && stays.confirm[i] && stays.confirm[i] != NA_INTEGER) {
            stay.flags |= (int)mco_Stay::Flag::Confirmed;
        }
        if (stays.ucd.Len() && stays.ucd[i] && stays.ucd[i] != NA_INTEGER) {
            stay.flags |= (int)mco_Stay::Flag::UCD;
        }
        if (stays.raac.Len() && stays.raac[i] && stays.raac[i] != NA_INTEGER) {
            stay.flags |= (int)mco_Stay::Flag::RAAC;
        }
        stay.dip_count = stays.dip_count[i];

        stay.other_diagnoses.ptr = other_diagnoses2.end();
        if (diagnoses.type.Len()) {
            while (RG_UNLIKELY(j < diagnoses_end && diagnoses.id[j] < stays.id[i])) {
                j++;
            }
            for (; j < diagnoses_end && diagnoses.id[j] == stays.id[i]; j++) {
                if (RG_UNLIKELY(diagnoses.diag[j] == CHAR(NA_STRING)))
                    continue;

                drd_DiagnosisCode diag =
                    drd_DiagnosisCode::FromString(diagnoses.diag[j], (int)ParseFlag::End);
                const char *type_str = diagnoses.type[j].ptr;

                if (RG_LIKELY(type_str[0] && !type_str[1])) {
                    switch (type_str[0]) {
                        case 'p':
                        case 'P': {
                            stay.main_diagnosis = diag;
                            if (RG_UNLIKELY(!stay.main_diagnosis.IsValid())) {
                                stay.errors |= (int)mco_Stay::Error::MalformedMainDiagnosis;
                            }
                        } break;
                        case 'r':
                        case 'R': {
                            stay.linked_diagnosis = diag;
                            if (RG_UNLIKELY(!stay.linked_diagnosis.IsValid())) {
                                stay.errors |= (int)mco_Stay::Error::MalformedLinkedDiagnosis;
                            }
                        } break;
                        case 's':
                        case 'S': {
                            if (RG_LIKELY(diag.IsValid())) {
                                other_diagnoses2.Append(diag);
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
            if (RG_LIKELY(stays.main_diagnosis[i] != CHAR(NA_STRING))) {
                stay.main_diagnosis =
                    drd_DiagnosisCode::FromString(stays.main_diagnosis[i], (int)ParseFlag::End);
                if (RG_UNLIKELY(!stay.main_diagnosis.IsValid())) {
                    stay.errors |= (int)mco_Stay::Error::MalformedMainDiagnosis;
                }
            }
            if (stays.linked_diagnosis[i] != CHAR(NA_STRING)) {
                stay.linked_diagnosis =
                    drd_DiagnosisCode::FromString(stays.linked_diagnosis[i], (int)ParseFlag::End);
                if (RG_UNLIKELY(!stay.linked_diagnosis.IsValid())) {
                    stay.errors |= (int)mco_Stay::Error::MalformedLinkedDiagnosis;
                }
            }

            while (RG_UNLIKELY(j < diagnoses_end && diagnoses.id[j] < stays.id[i])) {
                j++;
            }
            for (; j < diagnoses_end && diagnoses.id[j] == stays.id[i]; j++) {
                if (RG_UNLIKELY(diagnoses.diag[j] == CHAR(NA_STRING)))
                    continue;

                drd_DiagnosisCode diag =
                    drd_DiagnosisCode::FromString(diagnoses.diag[j], (int)ParseFlag::End);
                if (RG_UNLIKELY(!diag.IsValid())) {
                    stay.errors |= (int)mco_Stay::Error::MalformedOtherDiagnosis;
                }

                other_diagnoses2.Append(diag);
            }
        }
        stay.other_diagnoses.len = other_diagnoses2.end() - stay.other_diagnoses.ptr;

        stay.procedures.ptr = procedures2.end();
        while (RG_UNLIKELY(k < procedures_end && procedures.id[k] < stays.id[i])) {
            k++;
        }
        for (; k < procedures_end && procedures.id[k] == stays.id[i]; k++) {
            if (RG_UNLIKELY(procedures.proc[k] == CHAR(NA_STRING)))
                continue;

            mco_ProcedureRealisation proc = {};

            proc.proc = drd_ProcedureCode::FromString(procedures.proc[k], (int)ParseFlag::End);
            if (procedures.extension.Len() && procedures.extension[k] != NA_INTEGER) {
                int extension = procedures.extension[k];
                if (RG_LIKELY(extension >= 0 && extension < 100)) {
                    proc.extension = (int8_t)extension;
                } else {
                    stay.errors |= (int)mco_Stay::Error::MalformedProcedureExtension;
                }
            }
            proc.phase = (int8_t)rcc_GetOptional(procedures.phase, k, 0);
            proc.activity = (int8_t)procedures.activity[k];
            proc.count = (int16_t)rcc_GetOptional(procedures.count, k, 0);
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

            if (RG_LIKELY(proc.proc.IsValid())) {
                procedures2.Append(proc);
            } else {
                stay.errors |= (int)mco_Stay::Error::MalformedProcedureCode;
            }
        }
        stay.procedures.len = procedures2.end() - stay.procedures.ptr;

        out_stay_set->stays.Append(stay);
    }
    if (j < diagnoses_end || k < procedures_end)
        return false;

    mco_Classify(classifier.table_set, classifier.authorization_set, sector, out_stay_set->stays,
                 flags, out_results, out_mono_results);

    other_diagnoses2.Leak();
    procedures2.Leak();

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
                                   bool export_units, bool export_supplement_cents,
                                   bool export_supplement_counts)
{
    Size results_count = 0;
    for (const HeapArray<mco_Result> &results: result_sets) {
        results_count += results.len;
    }

    rcc_DataFrameBuilder df_builder(results_count);
    rcc_Vector<int> admin_id = df_builder.Add<int>("admin_id");
    rcc_Vector<int> bill_id = df_builder.Add<int>("bill_id");
    rcc_Vector<int> unit;
    if (export_units) {
        unit = df_builder.Add<int>("unit");
    }
    rcc_Vector<Date> exit_date = df_builder.Add<Date>("exit_date");
    rcc_Vector<int> stays = df_builder.Add<int>("stays");
    rcc_Vector<int> duration = df_builder.Add<int>("duration");
    rcc_Vector<int> main_stay = df_builder.Add<int>("main_stay");
    rcc_Vector<const char *> ghm = df_builder.Add<const char *>("ghm");
    rcc_Vector<int> main_error = df_builder.Add<int>("main_error");
    rcc_Vector<int> ghs = df_builder.Add<int>("ghs");
    rcc_Vector<double> total_cents = df_builder.Add<double>("total_cents");
    rcc_Vector<double> price_cents = df_builder.Add<double>("price_cents");
    rcc_Vector<double> ghs_cents = df_builder.Add<double>("ghs_cents");
    rcc_Vector<double> ghs_coefficient = df_builder.Add<double>("ghs_coefficient");
    rcc_Vector<int> ghs_duration = df_builder.Add<int>("ghs_duration");
    rcc_Vector<int> exb_exh = df_builder.Add<int>("exb_exh");
    rcc_Vector<double> supplement_cents[RG_LEN(mco_SupplementTypeNames)];
    rcc_Vector<int> supplement_count[RG_LEN(mco_SupplementTypeNames)];
    if (export_supplement_cents) {
        for (Size i = 0; i < RG_LEN(mco_SupplementTypeNames); i++) {
            char name_buf[32];
            MakeSupplementColumnName(mco_SupplementTypeNames[i], "_cents", name_buf);
            supplement_cents[i] = df_builder.Add<double>(name_buf);
        }
    }
    if (export_supplement_counts) {
        for (Size i = 0; i < RG_LEN(mco_SupplementTypeNames); i++) {
            char name_buf[32];
            MakeSupplementColumnName(mco_SupplementTypeNames[i], "_count", name_buf);
            supplement_count[i] = df_builder.Add<int>(name_buf);
        }
    }

    Size k = 0;
    for (Size i = 0; i < result_sets.len; i++) {
        Span<const mco_Result> results = result_sets[i];
        Span<const mco_Pricing> pricings = pricing_sets[i];

        for (Size j = 0; j < results.len; j++) {
            const mco_Result &result = results[j];
            const mco_Pricing &pricing = pricings[j];

            char buf[32];

            admin_id[k] = result.stays[0].admin_id;
            bill_id[k] = result.stays[0].bill_id;
            if (export_units) {
                RG_ASSERT(result.stays.len == 1);
                unit[k] = result.stays[0].unit.number;
            }
            exit_date.Set(k, result.stays[result.stays.len - 1].exit.date);
            stays[k] = (int)result.stays.len;
            duration[k] = (result.duration >= 0) ? result.duration : NA_INTEGER;
            main_stay[k] = result.main_stay_idx + 1;
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
            ghs_duration[k] = (result.ghs_duration >= 0) ? result.ghs_duration : NA_INTEGER;
            exb_exh[k] = pricing.exb_exh;
            for (Size l = 0; l < RG_LEN(mco_SupplementTypeNames); l++) {
                if (export_supplement_cents) {
                    supplement_cents[l][k] = pricing.supplement_cents.values[l];
                }
                if (export_supplement_counts) {
                    supplement_count[l][k] = result.supplement_days.values[l];
                }
            }

            k++;
        }
    }

    return df_builder.Build();
}

RcppExport SEXP drdR_mco_Classify(SEXP classifier_xp, SEXP stays_xp, SEXP diagnoses_xp,
                                  SEXP procedures_xp, SEXP sector_xp, SEXP options_xp,
                                  SEXP results_xp, SEXP dispense_mode_xp,
                                  SEXP apply_coefficient_xp, SEXP supplement_columns_xp)
{
    BEGIN_RCPP
    RG_DEFER { rcc_DumpWarnings(); };

    static const int task_size = 2048;

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)rcc_GetPointerSafe(classifier_xp);
    Rcpp::DataFrame stays_df(stays_xp);
    Rcpp::DataFrame diagnoses_df(diagnoses_xp);
    Rcpp::DataFrame procedures_df(procedures_xp);
    drd_Sector sector = GetSectorFromString(sector_xp, classifier->default_sector);
    Rcpp::CharacterVector options_vec(options_xp);
    bool results = Rcpp::as<bool>(results_xp);
    const char *dispense_mode_str = !Rf_isNull(dispense_mode_xp) ?
                                    Rcpp::as<const char *>(dispense_mode_xp) : nullptr;
    bool apply_coefficient = Rcpp::as<bool>(apply_coefficient_xp);
    const char *supplement_columns_str = Rcpp::as<const char *>(supplement_columns_xp);

    unsigned int flags = 0;
    for (const char *opt: options_vec) {
        const OptionDesc *desc =
            FindIfPtr(mco_ClassifyFlagOptions,
                      [&](const OptionDesc &desc) { return TestStr(desc.name, opt); });
        if (!desc)
            Rcpp::stop("Unknown classifier option '%1'", opt);
        flags |= 1u << (desc - mco_ClassifyFlagOptions);
    }

    int dispense_mode = -1;
    if (dispense_mode_str) {
        const OptionDesc *desc =
            FindIfPtr(mco_DispenseModeOptions,
                      [&](const OptionDesc &desc) { return TestStr(desc.name, dispense_mode_str); });
        if (!desc) {
            LogError("Unknown dispensation mode '%1'", dispense_mode_str);
            rcc_StopWithLastError();
        }
        dispense_mode = (int)(desc - mco_DispenseModeOptions);
    }

    bool export_supplement_cents;
    bool export_supplement_counts;
    if (TestStr(supplement_columns_str, "both")) {
        export_supplement_cents = true;
        export_supplement_counts = true;
    } else if (TestStr(supplement_columns_str, "cents")) {
        export_supplement_cents = true;
        export_supplement_counts = false;
    } else if (TestStr(supplement_columns_str, "count")) {
        export_supplement_cents = false;
        export_supplement_counts = true;
    } else if (TestStr(supplement_columns_str, "none")) {
        export_supplement_cents = false;
        export_supplement_counts = false;
    } else {
        Rcpp::stop("Invalid value for supplement_columns parameter");
    }

#define LOAD_OPTIONAL_COLUMN(Var, Name) \
        do { \
            if ((Var ## _df).containsElementNamed(RG_STRINGIFY(Name))) { \
                (Var).Name = (Var ##_df)[RG_STRINGIFY(Name)]; \
            } \
        } while (false)

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
    LOAD_OPTIONAL_COLUMN(stays, ucd);
    LOAD_OPTIONAL_COLUMN(stays, raac);
    LOAD_OPTIONAL_COLUMN(stays, dip_count);

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
    procedures.proc = procedures_df["proc"];
    if (!(flags & (int)mco_ClassifyFlag::IgnoreProcedureExtension)) {
        LOAD_OPTIONAL_COLUMN(procedures, extension);
    }
    LOAD_OPTIONAL_COLUMN(procedures, phase);
    procedures.activity = procedures_df["activity"];
    LOAD_OPTIONAL_COLUMN(procedures, count);
    procedures.date = procedures_df["date"];
    LOAD_OPTIONAL_COLUMN(procedures, doc);

#undef LOAD_OPTIONAL_COLUMN

    Size sets_count = (stays.nrow - 1) / task_size + 1;
    HeapArray<mco_StaySet> stay_sets(sets_count);
    HeapArray<HeapArray<mco_Result>> result_sets(sets_count);
    HeapArray<HeapArray<mco_Pricing>> pricing_sets(sets_count);
    HeapArray<HeapArray<mco_Result>> mono_result_sets;
    HeapArray<HeapArray<mco_Pricing>> mono_pricing_sets(sets_count);
    if (dispense_mode >= 0) {
        mono_result_sets.Reserve(sets_count);
        mono_pricing_sets.Reserve(sets_count);
    }
    HeapArray<mco_Pricing> summaries(sets_count);

    // Parallel transform and classify
    {
        Async async;

        Size stays_offset = 0;
        Size diagnoses_offset = 0;
        Size procedures_offset = 0;
        while (stays_offset < stays.nrow) {
            Size stays_end = std::min(stays.nrow, stays_offset + task_size);
            while (stays_end < stays.nrow &&
                   !mco_SplitTest(stays.bill_id[stays_end - 1], stays.bill_id[stays_end])) {
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
            if (dispense_mode >= 0) {
                task_mono_results = mono_result_sets.AppendDefault();
                task_mono_pricings = mono_pricing_sets.AppendDefault();
            }
            mco_Pricing *task_summary = summaries.AppendDefault();

            async.Run([=, &stays, &diagnoses, &procedures]() mutable {
                if (!RunClassifier(*classifier, stays, stays_offset, stays_end,
                                   diagnoses, diagnoses_offset, diagnoses_end,
                                   procedures, procedures_offset, procedures_end,
                                   sector, flags, task_stay_set, task_results, task_mono_results))
                    return false;

                if (results || dispense_mode >= 0) {
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

        if (!async.Sync())
            Rcpp::stop("The 'id' column must be ordered in all data.frames");
    }

    mco_Pricing summary = {};
    mco_Summarize(summaries, &summary);

    rcc_AutoSexp summary_df;
    {
        rcc_ListBuilder df_builder;
        df_builder.Set("results", (int)summary.results_count);
        df_builder.Set("stays", (int)summary.stays_count);
        df_builder.Set("failures", (int)summary.failures_count);
        df_builder.Set("total_cents", (double)summary.total_cents);
        df_builder.Set("price_cents", (double)summary.price_cents);
        df_builder.Set("ghs_cents", (double)summary.ghs_cents);
        if (export_supplement_cents) {
            for (Size i = 0; i < RG_LEN(mco_SupplementTypeNames); i++) {
                char name_buf[32];
                MakeSupplementColumnName(mco_SupplementTypeNames[i], "_cents", name_buf);
                df_builder.Set(name_buf, (double)summary.supplement_cents.values[i]);
            }
        }
        if (export_supplement_counts) {
            for (Size i = 0; i < RG_LEN(mco_SupplementTypeNames); i++) {
                char name_buf[32];
                MakeSupplementColumnName(mco_SupplementTypeNames[i], "_count", name_buf);
                df_builder.Set(name_buf, (int)summary.supplement_days.values[i]);
            }
        }
        summary_df = df_builder.BuildDataFrame();
    }

    rcc_AutoSexp results_df;
    if (results) {
        results_df = ExportResultsDataFrame(result_sets, pricing_sets, false,
                                            export_supplement_cents, export_supplement_counts);
    }

    rcc_AutoSexp mono_results_df;
    if (dispense_mode >= 0) {
        mono_results_df = ExportResultsDataFrame(mono_result_sets, mono_pricing_sets, true,
                                                 export_supplement_cents, export_supplement_counts);
    }

    rcc_AutoSexp ret_list;
    {
        rcc_ListBuilder ret_builder;
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

RcppExport SEXP drdR_mco_Indexes(SEXP classifier_xp)
{
    BEGIN_RCPP
    RG_DEFER { rcc_DumpWarnings(); };

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)rcc_GetPointerSafe(classifier_xp);

    rcc_AutoSexp indexes_df;
    {
        Size valid_indexes_count = 0;
        for (const mco_TableIndex &index: classifier->table_set.indexes) {
            valid_indexes_count += index.valid;
        }

        rcc_DataFrameBuilder df_builder(valid_indexes_count);
        rcc_Vector<Date> start_date = df_builder.Add<Date>("start_date");
        rcc_Vector<Date> end_date = df_builder.Add<Date>("end_date");
        rcc_Vector<bool> changed_tables = df_builder.Add<bool>("changed_tables");
        rcc_Vector<bool> changed_prices = df_builder.Add<bool>("changed_prices");

        Size i = 0;
        for (const mco_TableIndex &index: classifier->table_set.indexes) {
            if (!index.valid)
                continue;

            start_date.Set(i, index.limit_dates[0]);
            end_date.Set(i, index.limit_dates[1]);
            changed_tables.Set(i, index.changed_tables & ~MaskEnum(mco_TableType::PriceTablePublic));
            changed_prices.Set(i, index.changed_tables & MaskEnum(mco_TableType::PriceTablePublic));
            i++;
        }

        indexes_df = df_builder.Build();
    }

    return indexes_df;

    END_RCPP
}

RcppExport SEXP drdR_mco_GhmGhs(SEXP classifier_xp, SEXP date_xp, SEXP sector_xp, SEXP map_xp)
{
    BEGIN_RCPP
    RG_DEFER { rcc_DumpWarnings(); };

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)rcc_GetPointerSafe(classifier_xp);

    Date date = rcc_Vector<Date>(date_xp).Value();
    if (!date.value)
        rcc_StopWithLastError();
    drd_Sector sector = GetSectorFromString(sector_xp, classifier->default_sector);
    bool map = Rcpp::as<bool>(map_xp);

    const mco_TableIndex *index = classifier->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        rcc_StopWithLastError();
    }

    HashTable<mco_GhmCode, mco_GhmConstraint> constraints;
    if (map && !mco_ComputeGhmConstraints(*index, &constraints))
        rcc_StopWithLastError();

    rcc_AutoSexp ghm_ghs_df;
    {
        Size row_count = 0;
        for (const mco_GhmRootInfo &ghm_root_info: index->ghm_roots) {
            Span<const mco_GhmToGhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.ghm_root);
            row_count += compatible_ghs.len;
        }

        rcc_DataFrameBuilder df_builder(row_count);
        rcc_Vector<const char *> ghm = df_builder.Add<const char *>("ghm");
        rcc_Vector<int> ghs = df_builder.Add<int>("ghs");
        rcc_Vector<int> allow_ambulatory = df_builder.Add<int>("allow_ambulatory");
        rcc_Vector<int> short_duration_treshold = df_builder.Add<int>("short_duration_treshold");
        rcc_Vector<int> allow_raac = df_builder.Add<int>("allow_raac");
        rcc_Vector<int> confirm_treshold = df_builder.Add<int>("confirm_treshold");
        rcc_Vector<int> young_age_treshold = df_builder.Add<int>("young_age_treshold");
        rcc_Vector<int> young_severity_limit = df_builder.Add<int>("young_severity_limit");
        rcc_Vector<int> old_age_treshold = df_builder.Add<int>("old_age_treshold");
        rcc_Vector<int> old_severity_limit = df_builder.Add<int>("old_severity_limit");
        rcc_Vector<int> unit_authorization = df_builder.Add<int>("unit_authorization");
        rcc_Vector<int> bed_authorization = df_builder.Add<int>("bed_authorization");
        rcc_Vector<int> minimum_duration = df_builder.Add<int>("minimum_duration");
        rcc_Vector<int> minimum_age = df_builder.Add<int>("minimum_age");
        rcc_Vector<const char *> main_diagnosis = df_builder.Add<const char *>("main_diagnosis");
        rcc_Vector<const char *> diagnoses = df_builder.Add<const char *>("diagnoses");
        rcc_Vector<const char *> procedures = df_builder.Add<const char *>("procedures");
        rcc_Vector<int> ghs_cents = df_builder.Add<int>("ghs_cents");
        rcc_Vector<double> ghs_coefficient = df_builder.Add<double>("ghs_coefficient");
        rcc_Vector<int> exh_treshold = df_builder.Add<int>("exb_treshold");
        rcc_Vector<int> exh_cents = df_builder.Add<int>("exh_cents");
        rcc_Vector<int> exb_treshold = df_builder.Add<int>("exb_treshold");
        rcc_Vector<int> exb_cents = df_builder.Add<int>("exb_cents");
        rcc_Vector<int> exb_once = df_builder.Add<int>("exb_once");
        rcc_Vector<int> durations;
        rcc_Vector<int> warn_cmd28;
        if (map) {
            durations = df_builder.Add<int>("durations");
            warn_cmd28 = df_builder.Add<int>("warn_cmd28");
        }

        Size i = 0;
        for (const mco_GhmRootInfo &ghm_root_info: index->ghm_roots) {
            Span<const mco_GhmToGhsInfo> compatible_ghs = index->FindCompatibleGhs(ghm_root_info.ghm_root);
            for (const mco_GhmToGhsInfo &ghm_to_ghs_info: compatible_ghs) {
                mco_GhsCode ghs_code = ghm_to_ghs_info.Ghs(sector);
                const mco_GhsPriceInfo *ghs_price_info = index->FindGhsPrice(ghs_code, sector);

                char buf[256];

                ghm.Set(i, ghm_to_ghs_info.ghm.ToString(buf));
                ghs[i] = ghs_code.number;
                allow_ambulatory[i] = ghm_root_info.allow_ambulatory;
                short_duration_treshold[i] = ghm_root_info.short_duration_treshold ? ghm_root_info.short_duration_treshold : NA_INTEGER;
                allow_raac[i] = ghm_root_info.allow_raac;
                confirm_treshold[i] = ghm_root_info.confirm_duration_treshold ? ghm_root_info.confirm_duration_treshold : NA_INTEGER;
                if (ghm_root_info.young_severity_limit) {
                    young_age_treshold[i] = ghm_root_info.young_age_treshold;
                    young_severity_limit[i] = ghm_root_info.young_severity_limit;
                } else {
                    young_age_treshold[i] = NA_INTEGER;
                    young_severity_limit[i] = NA_INTEGER;
                }
                if (ghm_root_info.old_severity_limit) {
                    old_age_treshold[i] = ghm_root_info.old_severity_limit;
                    old_severity_limit[i] = ghm_root_info.old_severity_limit;
                } else {
                    old_age_treshold[i] = NA_INTEGER;
                    old_severity_limit[i] = NA_INTEGER;
                }
                unit_authorization[i] = ghm_to_ghs_info.unit_authorization ? ghm_to_ghs_info.unit_authorization : NA_INTEGER;
                bed_authorization[i] = ghm_to_ghs_info.bed_authorization ? ghm_to_ghs_info.bed_authorization : NA_INTEGER;
                minimum_duration[i] = ghm_to_ghs_info.minimum_duration ? ghm_to_ghs_info.minimum_duration : NA_INTEGER;
                minimum_age[i] = ghm_to_ghs_info.minimum_age ? ghm_to_ghs_info.minimum_age : NA_INTEGER;
                if (ghm_to_ghs_info.main_diagnosis_mask.value) {
                    main_diagnosis.Set(i, Fmt(buf, "D$%1.%2",
                                              ghm_to_ghs_info.main_diagnosis_mask.offset,
                                              ghm_to_ghs_info.main_diagnosis_mask.value));
                } else {
                    main_diagnosis.Set(i, nullptr);
                }
                if (ghm_to_ghs_info.diagnosis_mask.value) {
                    diagnoses.Set(i, Fmt(buf, "D$%1.%2",
                                         ghm_to_ghs_info.diagnosis_mask.offset,
                                         ghm_to_ghs_info.diagnosis_mask.value));
                } else {
                    diagnoses.Set(i, nullptr);
                }
                if (ghm_to_ghs_info.procedure_masks.len) {
                    Size buf_len = 0;
                    for (const drd_ListMask &mask: ghm_to_ghs_info.procedure_masks) {
                        buf_len += Fmt(MakeSpan(buf + buf_len, RG_SIZE(buf) - buf_len),
                                       "|A$%1.%2", mask.offset, mask.value).len;
                    }
                    procedures.Set(i, buf + 1);
                } else {
                    procedures.Set(i, nullptr);
                }

                if (ghs_price_info) {
                    ghs_cents[i] = ghs_price_info->ghs_cents;
                    ghs_coefficient[i] = index->GhsCoefficient(sector);
                    if (ghs_price_info->exh_treshold) {
                        exh_treshold[i] = ghs_price_info->exb_treshold;
                        exh_cents[i] = ghs_price_info->exh_cents;
                    } else {
                        exh_treshold[i] = NA_INTEGER;
                        exh_cents[i] = NA_INTEGER;
                    }
                    if (ghs_price_info->exb_treshold) {
                        exb_treshold[i] = ghs_price_info->exb_treshold;
                        exb_cents[i] = ghs_price_info->exh_cents;
                        exb_once[i] = !!(ghs_price_info->flags & (int)mco_GhsPriceInfo::Flag::ExbOnce);
                    } else {
                        exb_treshold[i] = NA_INTEGER;
                        exb_cents[i] = NA_INTEGER;
                        exb_once[i] = NA_INTEGER;
                    }
                } else {
                    ghs_cents[i] = NA_INTEGER;
                    ghs_coefficient[i] = NA_REAL;
                    exh_treshold[i] = NA_INTEGER;
                    exh_cents[i] = NA_INTEGER;
                    exb_treshold[i] = NA_INTEGER;
                    exb_cents[i] = NA_INTEGER;
                    exb_once[i] = NA_INTEGER;
                }

                if (map) {
                    mco_GhmConstraint *constraint = constraints.Find(ghm_to_ghs_info.ghm);
                    if (constraint) {
                        uint32_t combined_durations = constraint->durations &
                                                      ~((1u << ghm_to_ghs_info.minimum_duration) - 1);

                        durations[i] = combined_durations;
                        warn_cmd28[i] = (combined_durations & 1) &&
                                        !!(constraint->warnings & (int)mco_GhmConstraint::Warning::PreferCmd28);
                    } else {
                        durations[i] = NA_INTEGER;
                        warn_cmd28[i] = NA_INTEGER;
                    }
                }

                i++;
            }
        }

        ghm_ghs_df = df_builder.Build();
    }

    return ghm_ghs_df;

    END_RCPP
}

static int GetDiagnosisSexSpec(const mco_DiagnosisInfo &diag_info)
{
    switch (diag_info.sexes) {
        case 0x1: return 1;
        case 0x2: return 2;
        case 0x3: return NA_INTEGER;
    }

    RG_ASSERT(false);
}

RcppExport SEXP drdR_mco_Diagnoses(SEXP classifier_xp, SEXP date_xp)
{
    BEGIN_RCPP
    RG_DEFER { rcc_DumpWarnings(); };

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)rcc_GetPointerSafe(classifier_xp);

    Date date = rcc_Vector<Date>(date_xp).Value();
    if (!date.value)
        rcc_StopWithLastError();

    const mco_TableIndex *index = classifier->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        rcc_StopWithLastError();
    }

    rcc_AutoSexp diagnoses_df;
    {
        rcc_DataFrameBuilder df_builder(index->diagnoses.len);
        rcc_Vector<const char *> diag = df_builder.Add<const char *>("diag");
        rcc_Vector<int> sex_spec = df_builder.Add<int>("sex");
        rcc_Vector<int> cmd = df_builder.Add<int>("cmd");
        rcc_Vector<int> jump = df_builder.Add<int>("jump");
        rcc_Vector<int> severity = df_builder.Add<int>("severity");

        for (Size i = 0; i < index->diagnoses.len; i++) {
            const mco_DiagnosisInfo &diag_info = index->diagnoses[i];

            diag.Set(i, diag_info.diag.str);
            sex_spec[i] = GetDiagnosisSexSpec(diag_info);
            cmd[i] = diag_info.cmd ? diag_info.cmd : NA_INTEGER;
            jump[i] = diag_info.jump ? diag_info.jump : NA_INTEGER;
            severity[i] = diag_info.severity ? diag_info.severity : NA_INTEGER;
        }

        diagnoses_df = df_builder.Build();
    }

    return diagnoses_df;

    END_RCPP
}

RcppExport SEXP drdR_mco_Exclusions(SEXP classifier_xp, SEXP date_xp)
{
    BEGIN_RCPP
    RG_DEFER { rcc_DumpWarnings(); };

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)rcc_GetPointerSafe(classifier_xp);

    Date date = rcc_Vector<Date>(date_xp).Value();
    if (!date.value)
        rcc_StopWithLastError();

    const mco_TableIndex *index = classifier->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        rcc_StopWithLastError();
    }

    rcc_AutoSexp ghm_roots_df;
    {
        struct ExclusionInfo {
            drd_DiagnosisCode diag;
            int sex_spec;
            mco_GhmRootCode ghm_root;
        };
        HeapArray<ExclusionInfo> ghm_exclusions;

        for (const mco_DiagnosisInfo &diag_info: index->diagnoses) {
            for (const mco_GhmRootInfo &ghm_root_info: index->ghm_roots) {
                if (mco_TestGhmRootExclusion(diag_info, ghm_root_info)) {
                    ExclusionInfo excl = {};
                    excl.diag = diag_info.diag;
                    excl.sex_spec = GetDiagnosisSexSpec(diag_info);
                    excl.ghm_root = ghm_root_info.ghm_root;

                    ghm_exclusions.Append(excl);
                }
            }
        }

        rcc_DataFrameBuilder df_builder(ghm_exclusions.len);
        rcc_Vector<const char *> diag = df_builder.Add<const char *>("diag");
        rcc_Vector<int> sex_spec = df_builder.Add<int>("sex");
        rcc_Vector<const char *> ghm_root = df_builder.Add<const char *>("ghm_root");

        for (Size i = 0; i < ghm_exclusions.len; i++) {
            char buf[64];

            const ExclusionInfo &excl = ghm_exclusions[i];

            diag.Set(i, excl.diag.str);
            sex_spec[i] = excl.sex_spec;
            ghm_root.Set(i, excl.ghm_root.ToString(buf));
        }

        ghm_roots_df = df_builder.Build();
    }

    rcc_AutoSexp diagnoses_df;
    {
        struct ExclusionInfo {
            drd_DiagnosisCode diag;
            int sex_spec;
            drd_DiagnosisCode main_diag;
            int main_sex_spec;
        };
        HeapArray<ExclusionInfo> exclusions;

        for (const mco_DiagnosisInfo &diag_info: index->diagnoses) {
            for (const mco_DiagnosisInfo &main_diag_info: index->diagnoses) {
                if (mco_TestDiagnosisExclusion(*index, diag_info, main_diag_info)) {
                    ExclusionInfo excl = {};
                    excl.diag = diag_info.diag;
                    excl.sex_spec = GetDiagnosisSexSpec(diag_info);
                    excl.main_diag = main_diag_info.diag;
                    excl.main_sex_spec = GetDiagnosisSexSpec(main_diag_info);

                    exclusions.Append(excl);
                }
            }
        }

        rcc_DataFrameBuilder df_builder(exclusions.len);
        rcc_Vector<const char *> diag = df_builder.Add<const char *>("diag");
        rcc_Vector<int> sex_spec = df_builder.Add<int>("sex");
        rcc_Vector<const char *> main_diag = df_builder.Add<const char *>("main_or_linked_diag");
        rcc_Vector<int> main_sex_spec = df_builder.Add<int>("main_or_linked_sex");

        for (Size i = 0; i < exclusions.len; i++) {
            const ExclusionInfo &excl = exclusions[i];

            diag.Set(i, excl.diag.str);
            sex_spec[i] = excl.sex_spec;
            main_diag.Set(i, excl.main_diag.str);
            main_sex_spec[i] = excl.main_sex_spec;
        }

        diagnoses_df = df_builder.Build();
    }

    rcc_AutoSexp conditions_df;
    {
        Size age_exclusions_count = 0;
        for (const mco_DiagnosisInfo &diag_info: index->diagnoses) {
            age_exclusions_count += (diag_info.cma_minimum_age ||
                                     diag_info.cma_maximum_age);
        }

        rcc_DataFrameBuilder df_builder(age_exclusions_count);
        rcc_Vector<const char *> diag = df_builder.Add<const char *>("diag");
        rcc_Vector<int> sex_spec = df_builder.Add<int>("sex");
        rcc_Vector<int> minimum_age = df_builder.Add<int>("minimum_age");
        rcc_Vector<int> maximum_age = df_builder.Add<int>("maximum_age");

        Size i = 0;
        for (const mco_DiagnosisInfo &diag_info: index->diagnoses) {
            bool test = diag_info.cma_minimum_age ||
                        diag_info.cma_maximum_age;

            if (test) {
                diag.Set(i, diag_info.diag.str);
                sex_spec[i] = GetDiagnosisSexSpec(diag_info);
                minimum_age[i] = diag_info.cma_minimum_age ? diag_info.cma_minimum_age : NA_INTEGER;
                maximum_age[i] = diag_info.cma_maximum_age ? diag_info.cma_maximum_age : NA_INTEGER;
                i++;
            }
        }

        conditions_df = df_builder.Build();
    }

    rcc_AutoSexp ret_list;
    {
        rcc_ListBuilder ret_builder;
        ret_builder.Add("ghm_roots", ghm_roots_df);
        ret_builder.Add("diagnoses", diagnoses_df);
        ret_builder.Add("conditions", conditions_df);
        ret_list = ret_builder.BuildList();
    }

    return ret_list;

    END_RCPP
}

RcppExport SEXP drdR_mco_Procedures(SEXP classifier_xp, SEXP date_xp)
{
    BEGIN_RCPP
    RG_DEFER { rcc_DumpWarnings(); };

    const ClassifierInstance *classifier =
        (const ClassifierInstance *)rcc_GetPointerSafe(classifier_xp);

    Date date = rcc_Vector<Date>(date_xp).Value();
    if (!date.value)
        rcc_StopWithLastError();

    const mco_TableIndex *index = classifier->table_set.FindIndex(date);
    if (!index) {
        LogError("No table index available on '%1'", date);
        rcc_StopWithLastError();
    }

    rcc_AutoSexp procedures_df;
    {
        rcc_DataFrameBuilder df_builder(index->procedures.len);
        rcc_Vector<const char *> proc = df_builder.Add<const char *>("proc");
        rcc_Vector<int> phase = df_builder.Add<int>("phase");
        rcc_Vector<Date> start_date = df_builder.Add<Date>("start_date");
        rcc_Vector<Date> end_date = df_builder.Add<Date>("end_date");
        rcc_Vector<const char *> activities = df_builder.Add<const char *>("activities");
        rcc_Vector<const char *> extensions = df_builder.Add<const char *>("extensions");

        for (Size i = 0; i < index->procedures.len; i++) {
            const mco_ProcedureInfo &proc_info = index->procedures[i];
            char buf[512];

            proc.Set(i, proc_info.proc.str);
            phase[i] = proc_info.phase;
            start_date.Set(i, proc_info.limit_dates[0]);
            if (proc_info.limit_dates[1] < mco_MaxDate1980) {
                end_date.Set(i, proc_info.limit_dates[1]);
            } else {
                end_date.Set(i, {});
            }
            activities.Set(i, proc_info.ActivitiesToStr(buf));
            extensions.Set(i, proc_info.ExtensionsToStr(buf));
        }

        procedures_df = df_builder.Build();
    }

    return procedures_df;

    END_RCPP
}

RcppExport SEXP drdR_mco_LoadStays(SEXP filenames_xp)
{
    BEGIN_RCPP
    RG_DEFER { rcc_DumpWarnings(); };

    rcc_Vector<const char *> filenames(filenames_xp);

    mco_StaySet stay_set;
    {
        mco_StaySetBuilder stay_set_builder;

        bool valid = true;
        for (const char *filename: filenames) {
            valid &= stay_set_builder.LoadFiles(filename);
        }
        if (!valid)
            rcc_StopWithLastError();

        if (!stay_set_builder.Finish(&stay_set))
            rcc_StopWithLastError();
    }

    if (stay_set.stays.len >= INT_MAX)
        Rcpp::stop("Cannot load more than %1 stays in data.frame", INT_MAX);

    Size diagnoses_count = 0;
    Size procedures_count = 0;
    for (const mco_Stay &stay: stay_set.stays) {
        diagnoses_count += stay.other_diagnoses.len;
        procedures_count += stay.procedures.len;
    }

    SEXP stays_df;
    SEXP diagnoses_df;
    SEXP procedures_df;
    {
        rcc_DataFrameBuilder stays_builder(stay_set.stays.len);
        rcc_Vector<int> stays_id = stays_builder.Add<int>("id");
        rcc_Vector<int> stays_admin_id = stays_builder.Add<int>("admin_id");
        rcc_Vector<int> stays_bill_id = stays_builder.Add<int>("bill_id");
        rcc_Vector<int> stays_sex = stays_builder.Add<int>("sex");
        rcc_Vector<Date> stays_birthdate = stays_builder.Add<Date>("birthdate");
        rcc_Vector<Date> stays_entry_date = stays_builder.Add<Date>("entry_date");
        rcc_Vector<int> stays_entry_mode = stays_builder.Add<int>("entry_mode");
        rcc_Vector<const char *> stays_entry_origin = stays_builder.Add<const char *>("entry_origin");
        rcc_Vector<Date> stays_exit_date = stays_builder.Add<Date>("exit_date");
        rcc_Vector<int> stays_exit_mode = stays_builder.Add<int>("exit_mode");
        rcc_Vector<int> stays_exit_destination = stays_builder.Add<int>("exit_destination");
        rcc_Vector<int> stays_unit = stays_builder.Add<int>("unit");
        rcc_Vector<int> stays_bed_authorization = stays_builder.Add<int>("bed_authorization");
        rcc_Vector<int> stays_session_count = stays_builder.Add<int>("session_count");
        rcc_Vector<int> stays_igs2 = stays_builder.Add<int>("igs2");
        rcc_Vector<Date> stays_last_menstrual_period = stays_builder.Add<Date>("last_menstrual_period");
        rcc_Vector<int> stays_gestational_age = stays_builder.Add<int>("gestational_age");
        rcc_Vector<int> stays_newborn_weight = stays_builder.Add<int>("newborn_weight");
        rcc_Vector<const char *> stays_main_diagnosis = stays_builder.Add<const char *>("main_diagnosis");
        rcc_Vector<const char *> stays_linked_diagnosis = stays_builder.Add<const char *>("linked_diagnosis");
        rcc_Vector<int> stays_confirm = stays_builder.Add<int>("confirm");
        rcc_Vector<int> stays_ucd = stays_builder.Add<int>("ucd");
        rcc_Vector<int> stays_raac = stays_builder.Add<int>("raac");
        rcc_Vector<int> stays_dip_count = stays_builder.Add<int>("dip_count");

        rcc_DataFrameBuilder diagnoses_builder(diagnoses_count);
        rcc_Vector<int> diagnoses_id = diagnoses_builder.Add<int>("id");
        rcc_Vector<const char *> diagnoses_diag = diagnoses_builder.Add<const char *>("diag");

        rcc_DataFrameBuilder procedures_builder(procedures_count);
        rcc_Vector<int> procedures_id = procedures_builder.Add<int>("id");
        rcc_Vector<const char *> procedures_proc = procedures_builder.Add<const char *>("proc");
        rcc_Vector<int> procedures_extension = procedures_builder.Add<int>("extension");
        rcc_Vector<int> procedures_phase = procedures_builder.Add<int>("phase");
        rcc_Vector<int> procedures_activity = procedures_builder.Add<int>("activity");
        rcc_Vector<int> procedures_count = procedures_builder.Add<int>("count");
        rcc_Vector<Date> procedures_date = procedures_builder.Add<Date>("date");
        rcc_Vector<const char *> procedures_doc = procedures_builder.Add<const char *>("doc");

        Size j = 0, k = 0;
        for (Size i = 0; i < stay_set.stays.len; i++) {
            const mco_Stay &stay = stay_set.stays[i];

            stays_id[i] = (int)(i + 1);
            stays_admin_id[i] = RG_LIKELY(stay.admin_id) ? stay.admin_id : NA_INTEGER;
            stays_bill_id[i] = RG_LIKELY(stay.bill_id) ? stay.bill_id : NA_INTEGER;
            stays_sex[i] = RG_LIKELY(stay.sex) ? stay.sex : NA_INTEGER;
            stays_birthdate.Set(i, stay.birthdate);
            stays_entry_date.Set(i, stay.entry.date);
            stays_entry_mode[i] = RG_LIKELY(stay.entry.mode) ? stay.entry.mode - '0' : NA_INTEGER;
            if (stay.entry.origin) {
                stays_entry_origin.Set(i, stay.entry.origin);
            } else {
                stays_entry_origin.Set(i, nullptr);
            }
            stays_exit_date.Set(i, stay.exit.date);
            stays_exit_mode[i] = RG_LIKELY(stay.exit.mode) ? stay.exit.mode - '0' : NA_INTEGER;
            stays_exit_destination[i] = stay.exit.destination ? stay.exit.destination - '0' : NA_INTEGER;
            stays_unit[i] = RG_LIKELY(stay.unit.number) ? stay.unit.number : NA_INTEGER;
            stays_bed_authorization[i] = stay.bed_authorization ? stay.bed_authorization : NA_INTEGER;
            stays_session_count[i] = stay.session_count;
            stays_igs2[i] = stay.igs2 ? stay.igs2 : NA_INTEGER;
            stays_last_menstrual_period.Set(i, stay.last_menstrual_period);
            stays_gestational_age[i] = stay.gestational_age ? stay.gestational_age : NA_INTEGER;
            stays_newborn_weight[i] = stay.newborn_weight ? stay.newborn_weight : NA_INTEGER;
            if (RG_LIKELY(stay.main_diagnosis.IsValid())) {
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
            stays_ucd[i] = !!(stay.flags & (int)mco_Stay::Flag::UCD);
            stays_raac[i] = !!(stay.flags & (int)mco_Stay::Flag::RAAC);
            stays_dip_count[i] = stay.dip_count;

            for (drd_DiagnosisCode diag: stay.other_diagnoses) {
                diagnoses_id[j] = (int)(i + 1);
                diagnoses_diag.Set(j, diag.str);
                j++;
            }

            for (const mco_ProcedureRealisation &proc: stay.procedures) {
                procedures_id[k] = (int)(i + 1);
                procedures_proc.Set(k, proc.proc.str);
                procedures_extension[k] = proc.extension ? proc.extension : NA_INTEGER;
                procedures_phase[k] = proc.phase;
                procedures_activity[k] = proc.activity;
                procedures_date.Set(k, proc.date);
                procedures_count[k] = RG_LIKELY(proc.count) ? proc.count : NA_INTEGER;
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
        rcc_ListBuilder list_builder;
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
    rcc_Vector<const char *> types(RG_LEN(mco_SupplementTypeNames));
    for (Size i = 0; i < RG_LEN(mco_SupplementTypeNames); i++) {
        types.Set(i, mco_SupplementTypeNames[i]);
    }

    return types;
}

RcppExport SEXP drdR_mco_CleanDiagnoses(SEXP diagnoses_xp)
{
    rcc_Vector<const char *> diagnoses(diagnoses_xp);

    rcc_Vector<const char *> diagnoses2(diagnoses.Len());
    for (Size i = 0; i < diagnoses.Len(); i++) {
        Span<const char> str = diagnoses[i];
        if (!diagnoses.IsNA(str)) {
            drd_DiagnosisCode diag = drd_DiagnosisCode::FromString(diagnoses[i]);
            if (RG_LIKELY(diag.IsValid())) {
                diagnoses2.Set(i, diag.str);
            } else {
                diagnoses2.Set(i, nullptr);
            }
        } else {
            diagnoses2.Set(i, nullptr);
        }
    }

    return diagnoses2;
}

RcppExport SEXP drdR_mco_CleanProcedures(SEXP procedures_xp)
{
    rcc_Vector<const char *> procedures(procedures_xp);

    rcc_Vector<const char *> procedures2(procedures.Len());
    for (Size i = 0; i < procedures.Len(); i++) {
        Span<const char> str = procedures[i];
        if (!procedures.IsNA(str)) {
            drd_ProcedureCode proc = drd_ProcedureCode::FromString(procedures[i]);
            if (RG_LIKELY(proc.IsValid())) {
                procedures2.Set(i, proc.str);
            } else {
                procedures2.Set(i, nullptr);
            }
        } else {
            procedures2.Set(i, nullptr);
        }
    }

    return procedures2;
}

}

RcppExport void R_init_drdR(DllInfo *dll) {
    static const R_CallMethodDef call_entries[] = {
        {"drdR_mco_Init", (DL_FUNC)&RG::drdR_mco_Init, 4},
        {"drdR_mco_Classify", (DL_FUNC)&RG::drdR_mco_Classify, 10},
        // {"drdR_mco_Dispense", (DL_FUNC)&drdR_mco_Dispense, 3},
        {"drdR_mco_Indexes", (DL_FUNC)&RG::drdR_mco_Indexes, 1},
        {"drdR_mco_GhmGhs", (DL_FUNC)&RG::drdR_mco_GhmGhs, 4},
        {"drdR_mco_Diagnoses", (DL_FUNC)&RG::drdR_mco_Diagnoses, 2},
        {"drdR_mco_Exclusions", (DL_FUNC)&RG::drdR_mco_Exclusions, 2},
        {"drdR_mco_Procedures", (DL_FUNC)&RG::drdR_mco_Procedures, 2},
        {"drdR_mco_LoadStays", (DL_FUNC)&RG::drdR_mco_LoadStays, 1},
        {"drdR_mco_SupplementTypes", (DL_FUNC)&RG::drdR_mco_SupplementTypes, 0},
        {"drdR_mco_CleanDiagnoses", (DL_FUNC)&RG::drdR_mco_CleanDiagnoses, 1},
        {"drdR_mco_CleanProcedures", (DL_FUNC)&RG::drdR_mco_CleanProcedures, 1},
        {}
    };

    R_registerRoutines(dll, nullptr, call_entries, nullptr, nullptr);
    R_useDynamicSymbols(dll, FALSE);
}
