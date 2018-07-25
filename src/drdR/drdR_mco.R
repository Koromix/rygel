# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

mco_init <- function(data_dirs = character(0), table_dirs = character(0),
                     table_filenames = character(0), authorization_filename = NULL) {
    .Call(`drdR_mco_Init`, data_dirs, table_dirs, table_filenames, authorization_filename)
}

mco_classify <- function(classifier, stays, diagnoses = NULL, procedures = NULL,
                         sorted = TRUE, options = character(0), details = TRUE,
                         dispense = NULL, apply_coefficient = FALSE) {
    if (!is.data.frame(stays) && is.list(stays) && is.null(diagnoses) && is.null(procedures)) {
        diagnoses <- stays$diagnoses
        procedures <- stays$procedures
        stays <- stays$stays
    }

    result_set <- .Call(`drdR_mco_Classify`, classifier, stays, diagnoses, procedures,
                        options, details, dispense, apply_coefficient)

    class(result_set$summary) <- c('mco_summary', class(result_set$summary))
    if ('results' %in% names(result_set)) {
        class(result_set$results) <- c('mco_results', class(result_set$results))
    }
    if ('mono_results' %in% names(result_set)) {
        class(result_set$mono_results) <- c('mco_results', class(result_set$results))
    }
    class(result_set) <- c('mco_result_set', class(result_set))

    return(result_set)
}

drd_options <- function(debug = NULL) {
    .Call(`drdR_Options`, debug)
}

mco_indexes <- function(classifier) {
    .Call(`drdR_mco_Indexes`, classifier)
}

mco_diagnoses <- function(classifier, date) {
    .Call(`drdR_mco_Diagnoses`, classifier, date)
}

mco_exclusions <- function(classifier, date) {
    .Call(`drdR_mco_Exclusions`, classifier, date)
}

mco_procedures <- function(classifier, date) {
    .Call(`drdR_mco_Procedures`, classifier, date)
}

mco_load_stays <- function(filenames) {
    .Call(`drdR_mco_LoadStays`, filenames)
}

mco_compare <- function(summary1, summary2, ...) {
    if (!('mco_summary' %in% class(summary1))) {
        summary1 <- summary(summary1, ...)
    }
    if (!('mco_summary' %in% class(summary2))) {
        summary2 <- summary(summary2, ...)
    }

    groups <- setdiff(colnames(summary1), mco_summary_columns())

    m <- merge(summary1, summary2, by = groups, all = TRUE)
    for (col in setdiff(colnames(m), groups)) {
        m[[col]][is.na(m[[col]])] <- 0
    }

    diff <- cbind(
        m[, groups, drop = FALSE],
        as.data.frame(sapply(mco_summary_columns(), function(col) {
            m[[paste0(col, '.x')]] - m[[paste0(col, '.y')]]
        }, simplify = FALSE))
    )

    return(diff)
}

summary.mco_results <- function(results, by = NULL) {
    f <- attr(summary.mco_results, 'f')
    if (is.null(f)) {
        code <- paste0('
            function(results, by = NULL) {
                agg <- setDF(setDT(results)[, c(
                    list(
                        results = .N,
                        stays = sum(stays_count),
                        failures = sum(startsWith(\'90Z\', ghm)),
                        total_cents = sum(total_cents),
                        price_cents = sum(price_cents),
                        ghs_cents = sum(ghs_cents),',
                        paste(sapply(tolower(.Call(`drdR_mco_SupplementTypes`)),
                                     function(type) { paste0(type, '_cents = sum(', type, '_cents)') }), collapse = ', '), ', ',
                        paste(sapply(tolower(.Call(`drdR_mco_SupplementTypes`)),
                                     function(type) { paste0(type, '_count = sum(', type, '_count)') }), collapse = ', '),
                   ')
                ), keyby = by])
                setDF(results)

                class(agg) <- c(\'mco_summary\', class(agg))
                return(agg)
            }
        ')
        print(code)
        f <- eval(parse(text = code))
        attr(summary.mco_results, 'f') <- f
    }

    return(f(results, by = by))
}
summary.mco_result_set <- function(result_set, by = NULL) {
    if (is.null(by)) {
        return(result_set$summary)
    } else {
        return(summary.mco_results(result_set$results, by = by))
    }
}

mco_summary_columns <- function() {
    c('results', 'stays', 'failures', 'total_cents', 'price_cents', 'ghs_cents',
      paste0(tolower(.Call(`drdR_mco_SupplementTypes`)), '_cents'),
      paste0(tolower(.Call(`drdR_mco_SupplementTypes`)), '_count'))
}
