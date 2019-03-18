# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

mco_init <- function(table_dirs, authorization_filename,
                     table_filenames = character(0), default_sector = NULL) {
    .Call(`drdR_mco_Init`, table_dirs, table_filenames, authorization_filename, default_sector)
}

mco_classify <- function(classifier, stays, diagnoses = NULL, procedures = NULL, sector = NULL,
                         options = character(0), results = TRUE, dispense_mode = NULL,
                         apply_coefficient = FALSE, supplement_columns = 'both') {
    if (!is.data.frame(stays) && is.list(stays)) {
        if (is.null(diagnoses)) {
            diagnoses <- stays$diagnoses
        }
        if (is.null(procedures)) {
            procedures <- stays$procedures
        }
        stays <- stays$stays
    }

    result_set <- .Call(`drdR_mco_Classify`, classifier, stays, diagnoses, procedures, sector,
                        options, results, dispense_mode, apply_coefficient, supplement_columns)

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

mco_indexes <- function(classifier) {
    .Call(`drdR_mco_Indexes`, classifier)
}

mco_ghm_ghs <- function(classifier, date, sector = NULL, map = TRUE) {
    .Call(`drdR_mco_GhmGhs`, classifier, date, sector, map)
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
    sum_columns <- colnames(results)
    sum_columns <- sum_columns[grepl('(_cents|_count)$', sum_columns)]

    # Much faster than alternatives because the list of columns to summarize is flexible,
    # and doing it this way we give complete control to data.table. If you find a cleaner
    # way, it has to be as fast or faster than this version.
    code <- paste0('
        function(results, by = NULL) {
            agg <- setDF(setDT(results)[, c(
                list(
                    results = .N,
                    stays = sum(stays),
                    failures = sum(startsWith(\'90Z\', ghm)),',
                    paste(sapply(sum_columns,
                                 function(col) { paste0(col, ' = sum(', col, ')') }), collapse = ', '),
               ')
            ), keyby = by])
            setDF(results)

            class(agg) <- c(\'mco_summary\', class(agg))
            return(agg)
        }
    ')
    f <- eval(parse(text = code))

    return(f(results, by = by))
}
summary.mco_result_set <- function(result_set, by = NULL) {
    if (is.null(by)) {
        return(result_set$summary)
    } else {
        return(summary.mco_results(result_set$results, by = by))
    }
}

mco_dispense <- function(results, group = NULL, group_var = 'group',
                         reassign_list = NULL, reassign_counts = FALSE) {
    if ('mco_result_set' %in% class(results)) {
        results <- results$mono_results
    }

    agg <- setDT(summary.mco_results(results, by = 'unit'))

    if (!is.null(reassign_list)) {
        if (is.data.frame(reassign_list)) {
            reassign_names <- reassign_list$supplement
            reassign_list <- reassign_list$unit
            names(reassign_list) <- reassign_names
        }

        for (supp in names(reassign_list)) {
            unit <- reassign_list[[supp]]
            if (reassign_counts) {
                agg[[paste0(supp, '_count')]] <- ifelse(agg$unit == unit, sum(agg[[paste0(supp, '_count')]]), 0)
            }
            agg[[paste0(supp, '_cents')]] <- ifelse(agg$unit == unit, sum(agg[[paste0(supp, '_cents')]]), 0)
        }
    }

    if (!is.null(group)) {
        if (!is.data.frame(group)) {
            group <- data.frame(unit = as.integer(names(group)), group = group)
            names(group) <- c('unit', group_var)
        }

        group <- merge(agg[, list(unit)], group, by = 'unit', all.x = TRUE)

        # Read comment in summary.mco_results for information about use of eval
        sum_columns <- intersect(mco_summary_columns(), colnames(agg))
        code <- paste0('
            function(agg, group, group_var) {
                agg2 <- agg[, c(
                    list(',
                        paste(sapply(sum_columns,
                                     function(col) { paste0(col, ' = sum(', col, ')') }), collapse = ', '),
                   ')
                ), keyby = group]
                setnames(agg2, "group", group_var)

                class(agg2) <- class(agg)
                return(agg2)
            }
        ')
        f <- eval(parse(text = code))

        agg <- f(agg, group[[group_var]], group_var)
    }

    agg <- setDF(agg)
    return(agg)
}

mco_summary_columns <- function() {
    c('results', 'stays', 'failures', 'total_cents', 'price_cents', 'ghs_cents',
      paste0(tolower(.Call(`drdR_mco_SupplementTypes`)), '_cents'),
      paste0(tolower(.Call(`drdR_mco_SupplementTypes`)), '_count'))
}

mco_clean_diagnoses <- function(diagnoses) {
    .Call(`drdR_mco_CleanDiagnoses`, diagnoses)
}

mco_clean_procedures <- function(procedures) {
    .Call(`drdR_mco_CleanProcedures`, procedures)
}
