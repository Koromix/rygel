#ifdef _WIN32
    #define WINVER 0x0602
    #define _WIN32_WINNT 0x0602
#endif

#define MOYA_IMPLEMENTATION
#define KUTIL_NO_MINIZ
#include "../moya/libmoya.hh"

#include <Rcpp.h>

// [[Rcpp::export(name = '.moya.classify')]]
Rcpp::DataFrame moyaClassify(Rcpp::DataFrame stays_df, Rcpp::DataFrame diagnoses_df,
                             Rcpp::DataFrame procedures_df)
{
    struct {
        Rcpp::IntegerVector id;

        Rcpp::IntegerVector bill_id;
        Rcpp::IntegerVector stay_id;
        Rcpp::CharacterVector birthdate;
        Rcpp::IntegerVector sex;
        Rcpp::CharacterVector entry_date;
        Rcpp::CharacterVector entry_mode;
        Rcpp::CharacterVector entry_origin;
        Rcpp::CharacterVector exit_date;
        Rcpp::CharacterVector exit_mode;
        Rcpp::CharacterVector exit_destination;
        Rcpp::IntegerVector unit;
        Rcpp::IntegerVector bed_authorization;
        Rcpp::IntegerVector session_count;
        Rcpp::IntegerVector igs2;
        Rcpp::IntegerVector gestational_age;
        Rcpp::IntegerVector newborn_weight;
        Rcpp::CharacterVector last_menstrual_period;

        Rcpp::CharacterVector main_diagnosis;
        Rcpp::CharacterVector linked_diagnosis;
    } stays;

    struct {
        Rcpp::IntegerVector id;

        Rcpp::CharacterVector diag;
    } diagnoses;

    struct {
        Rcpp::IntegerVector id;

        Rcpp::CharacterVector proc;
        Rcpp::IntegerVector phase;
        Rcpp::IntegerVector activities;
        Rcpp::IntegerVector count;
        Rcpp::CharacterVector date;
    } procedures;

    // FIXME: There's nearly no error checking, can crash easily

    stays.id = stays_df["id"];
    stays.bill_id = stays_df["bill_id"];
    stays.stay_id = stays_df["stay_id"];
    stays.birthdate = stays_df["birthdate"];
    stays.sex = stays_df["sex"];
    stays.entry_date = stays_df["entry_date"];
    stays.entry_mode = stays_df["entry_mode"];
    stays.entry_origin = stays_df["entry_origin"];
    stays.exit_date = stays_df["exit_date"];
    stays.exit_mode = stays_df["exit_mode"];
    stays.exit_destination = stays_df["exit_destination"];
    stays.unit = stays_df["unit"];
    stays.bed_authorization = stays_df["bed_authorization"];
    stays.session_count = stays_df["session_count"];
    stays.igs2 = stays_df["igs2"];
    stays.gestational_age = stays_df["gestational_age"];
    stays.newborn_weight = stays_df["newborn_weight"];
    stays.last_menstrual_period = stays_df["last_menstrual_period"];
    stays.main_diagnosis = stays_df["main_diagnosis"];
    stays.linked_diagnosis = stays_df["linked_diagnosis"];

    diagnoses.id = diagnoses_df["id"];
    diagnoses.diag = diagnoses_df["diag"];

    procedures.id = procedures_df["id"];
    procedures.proc = procedures_df["code"];
    procedures.phase = procedures_df["phase"];
    procedures.activities = procedures_df["activities"];
    procedures.count = procedures_df["count"];
    procedures.date = procedures_df["date"];

    // TODO: Don't require sorted id column (id)
    StaySet stay_set;
    {
        stay_set.stays.Reserve(stays_df.nrow());
        stay_set.store.diagnoses.Reserve(diagnoses_df.nrow() + 2 * stays_df.nrow());
        stay_set.store.procedures.Reserve(procedures_df.nrow());

        int j = 0, k = 0;
        for (int i = 0; i < stays_df.nrow(); i++) {
            Stay stay = {};

            stay.bill_id = stays.bill_id[i];
            stay.stay_id = stays.stay_id[i];
            stay.birthdate = Date::FromString(stays.birthdate[i]);
            stay.sex = (Sex)stays.sex[i];
            stay.dates[0] = Date::FromString(stays.entry_date[i]);
            stay.dates[1] = Date::FromString(stays.exit_date[i]);
            stay.entry.mode = strtol(stays.entry_mode[i], nullptr, 10);
            if (stays.entry_origin[i] == "R" || stays.entry_origin[i] == "r") {
                stay.entry.origin = 34;
            } else {
                stay.entry.origin = strtol(stays.entry_origin[i], nullptr, 10);
            }
            stay.exit.mode = strtol(stays.exit_mode[i], nullptr, 10);
            stay.exit.destination = strtol(stays.exit_destination[i], nullptr, 10);
            stay.unit.number = stays.unit[i];
            stay.bed_authorization = stays.bed_authorization[i];
            stay.session_count = stays.session_count[i];
            stay.igs2 = stays.igs2[i];
            stay.gestational_age = stays.gestational_age[i];
            stay.newborn_weight = stays.newborn_weight[i];
            if (stays.last_menstrual_period[i] != NA_STRING) {
                stay.last_menstrual_period = Date::FromString(stays.last_menstrual_period[i]);
            }
            if (LIKELY(stays.main_diagnosis[i] != NA_STRING)) {
               stay.main_diagnosis = DiagnosisCode::FromString(stays.main_diagnosis[i]);
            }
            if (stays.linked_diagnosis[i] != NA_STRING) {
                stay.linked_diagnosis = DiagnosisCode::FromString(stays.linked_diagnosis[i]);
            }

            /*PrintLn("%1 -- %2 -- %3 -- %4 -- %5 -- %6 -- %7 -- %8 -- %9 -- %10 -- %11 -- %12 -- %13 -- %14 -- %15 -- %16 -- %17 -- %18",
                    stay.bill_id, stay.stay_id, (int)stay.sex, stay.dates[0], stay.dates[1],
                    stay.entry.mode, stay.entry.origin, stay.exit.mode, stay.exit.destination,
                    stay.unit, stay.bed_authorization, stay.session_count, stay.igs2,
                    stay.gestational_age, stay.newborn_weight, stay.last_menstrual_period,
                    stay.main_diagnosis, stay.linked_diagnosis);*/

            stay.diagnoses.ptr = stay_set.store.diagnoses.end();
            while (j < diagnoses_df.nrow() && diagnoses.id[j] == stays.id[i]) {
                DiagnosisCode diag = DiagnosisCode::FromString(diagnoses.diag[j]);

                // PrintLn("%1", diag);

                stay_set.store.diagnoses.Append(diag);
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
                ProcedureRealisation proc;
                proc.proc = ProcedureCode::FromString(procedures.proc[k]);
                proc.phase = procedures.phase[k];
                proc.activities = procedures.activities[k];
                proc.count = procedures.count[k];
                proc.date = Date::FromString(procedures.date[k]);

                // PrintLn("%1 -- %2 -- %3 -- %4 -- %5", proc.proc, proc.phase, proc.activities, proc.count, proc.date);

                stay_set.store.procedures.Append(proc);
                k++;
            }
            stay.procedures.len = stay_set.store.procedures.end() - stay.procedures.ptr;

            stay_set.stays.Append(stay);
        }
    }

    if (!main_data_directories.len) {
        main_data_directories.Append("C:/projects/moya/data");
    }

    const TableSet *table_set = GetMainTableSet();
    if (!table_set)
        Rcpp::stop("error");
    const AuthorizationSet *authorization_set = GetMainAuthorizationSet();
    if (!authorization_set)
        Rcpp::stop("error");
    const PricingSet *pricing_set = GetMainPricingSet();
    if (!pricing_set) {
        LogError("No pricing information will be available");
    }

    ClassifyResultSet result_set = {};
    Classify(*table_set, *authorization_set, pricing_set, stay_set.stays, ClusterMode::BillId,
             &result_set);

    Rcpp::DataFrame retval;
    {
        Allocator temp_alloc;

        Rcpp::IntegerVector bill_id(result_set.results.len);
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
            bill_id[i] = result_set.results[i].stays[0].bill_id;
            ghm[i] = Fmt(&temp_alloc, "%1", result_set.results[i].ghm).ptr;
            ghs[i] = result_set.results[i].ghs.number;
            ghs_price[i] = (double)result_set.results[i].ghs_price_cents / 100.0;
            rea[i] = result_set.results[i].supplements.rea;
            reasi[i] = result_set.results[i].supplements.reasi;
            si[i] = result_set.results[i].supplements.si;
            src[i] = result_set.results[i].supplements.src;
            nn1[i] = result_set.results[i].supplements.nn1;
            nn2[i] = result_set.results[i].supplements.nn2;
            nn3[i] = result_set.results[i].supplements.nn3;
            rep[i] = result_set.results[i].supplements.rep;
        }

        retval = Rcpp::DataFrame::create(
            Rcpp::Named("bill_id") = bill_id, Rcpp::Named("ghm") = ghm,
            Rcpp::Named("ghs") = ghs, Rcpp::Named("ghs_price") = ghs_price,
            Rcpp::Named("rea") = rea, Rcpp::Named("reasi") = reasi, Rcpp::Named("si") = si,
            Rcpp::Named("src") = src, Rcpp::Named("nn1") = nn1, Rcpp::Named("nn2") = nn2,
            Rcpp::Named("nn3") = nn3, Rcpp::Named("rep") = rep,
            Rcpp::Named("stringsAsFactors") = false
        );
    }

    return retval;
}
