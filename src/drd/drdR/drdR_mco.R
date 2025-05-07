# Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

#' Create new MCO classifier object
#'
#' @description
#' The resulting object contains a set of ATIH tables, pricing and institutional
#' unit authorizations that will be used to classify stays.
#'
#' @details
#' A typical call should look like this:
#' `m <- mco_init('path/to/tables', 'path/to/FICUM.txt', default_sector = 'Public')`
#'
#' The `default_sector` parameter can be overriden in functions that need sector
#' information such as [mco_classify()].
#'
#' @param table_dirs Paths to directories containing classification tables.
#' @param authorization_filename Path to FICUM file, or NULL.
#' @param table_filenames Paths to additional classification table files.
#' @param default_sector Default sector 'Public' or 'Private'.
#' @return A classifier object that can be used by [mco_classify()] and a
#'         varierty of other functions.
#'
#' @md
mco_init <- function(table_dirs, authorization_filename = NULL,
                     table_filenames = character(0), default_sector = NULL) {
    .Call(`drdR_mco_Init`, table_dirs, table_filenames, authorization_filename, default_sector)
}

#' Load MCO stays from RSS, GRP, RSA (and FICHCOMP) files
#'
#' @details
#' You can pass multiple filenames. To load FICHCOMP files, pass them as
#' additional filenames, they must use a .txt extension.
#'
#' Supported file formats: RSS (.rss), GRP (.grp) and RSA (.rsa).
#'
#' @param filenames Paths of filenames to load stays (and FICHCOMP data) from.
#' @return A list containing three data.frames: l$stays, l$diagnoses and
#'         l$procedures. A sorted id column links all three together.
#'
#' @examples
#' \donttest{
#' l <- mco_load_stays('/path/to/stays.rss')
#'
#' head(l$stays)
#' head(l$diagnoses)
#' head(l$procedures)
#' }
#'
#' @md
mco_load_stays <- function(filenames) {
    .Call(`drdR_mco_LoadStays`, filenames)
}

#' Classify MCO stays
#'
#' @description
#' This function takes a classifier object (with its table and authorization set) and
#' loaded stays as input and classifies them according to the revelant ATIH algorithm.
#' Final GHM, GHS, supplements, pricings are calculated (among others). Stays with the
#' same bill_id (RSS id) are fused together and produce a single result.
#'
#' @details
#' The stays must be presented in three data.frames: stays, diagnoses, procedures.
#' These are linked by an id column which must be sorted before calling [mco_classify()]
#' otherwise the call will fail. While each data.frame must be sorted by this
#' column, they don't have to match perfectly. For example, you can filter out
#' stays from the first data.frame without having to do the same in the two
#' other data.frames.
#'
#' Several options can be used to control the algorithm used to classify stays:
#'
#' * MonoOrigStay: Use original stays in mono algorithm
#' * IgnoreConfirm: Ignore RSS confirmation flag
#' * IgnoreCompProc: Ignore complementary procedure check
#' * IgnoreProcExt: Ignore ATIH procedure extension check
#'
#' You can use the `dispense_mode` parameter to redistribute pricings from multi-stay
#' results to individual stays, the following algorithms can be used:
#'
#' * J: Proportional to each stay duration
#' * E: Proportional to pseudo-GHS pricing of each stay
#' * Ex: Proportional to pseudo-GHS-EXB+EXH pricing of each stay
#' * Ex': Proportional to pseudo-GHS-EXB+EXH pricing of each stay if duration < EXB or
#'        pseudo-GHS pricing otherwise
#' * ExJ: Combine J and Ex algorithms
#' * Ex'J: Combine J and Ex' algorithms
#'
#' After loading stays with [mco_load_stays()], you can use a shortcut call `mco_classify(m, l)`
#' instead of the longer `mco_classify(m, l$stays, l$diagnoses, l$procedures)`.
#'
#' @param classifier Classifier object created by [mco_init()].
#' @param stays Data.frame of stays, or a list with three objects (stays, diagnoses, procedures).
#' @param diagnoses Data.frame of diagnoses, or NULL is you pass a list of data.frames to stays.
#' @param procedures Data.frame of procedures, or NULL is you pass a list of data.frames to stays.
#' @param sector Sector 'Public' or 'Private', by default the `default_sector` value
#'               specified in [mco_init()] will be used.
#' @param options List of options to alter classifier algorithm.
#' @param results Return results for each RSS as a data.frame.
#' @param dispense_mode Retribute paiements to individual stays (RUM), several algorithms
#'                      are available: "J", "Ex", "Ex'", "ExJ", "Ex'J". Individual results
#'                      will be available in the `$mono_results` component of the returned list.
#' @param apply_coefficient Apply GHS coefficients to results.
#' @param supplement_columns Change returned supplement columns, the following values can
#'                           be used: "none", "count", "cents" or "both".
#' @return A list containing one to three data.frames: `$summary`, `$results`
#'         (if `results = TRUE`) and `$mono_results` (if `dispense_mode != NULL`).
#'
#' @examples
#' \donttest{
#' library(drdR)
#'
#' # Load tables and authorizations, the resulting object can be reused as many
#' # times as necessary.
#' m <- mco_init('path/to/tables/', 'path/to/FICUM.txt', default_sector = 'Public')
#'
#' # Load stays, diagnoses and procedures from a file (RSS, GRP or RSA), the
#' # returned object is a list containing three data.frames: l$stays, l$diagnoses
#' # and l$procedures. A sorted id column links all three together.
#' l <- mco_load_stays('fichier.rss')
#'
#' c <- mco_classify(m, l)
#'
#' summary(c)
#' summary(c, by = 'ghm')
#' summary(c$results, by = 'ghm') # This does the same
#' head(c$results)
#' }
#'
#' @md
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

    class(result_set$summary) <- c('mco.result.summary', class(result_set$summary))
    if ('results' %in% names(result_set)) {
        class(result_set$results) <- c('mco.result.df', class(result_set$results))
    }
    if ('mono_results' %in% names(result_set)) {
        class(result_set$mono_results) <- c('mco.result.df', class(result_set$results))
    }
    class(result_set) <- c('mco.result', class(result_set))

    return(result_set)
}

#' Get information about available MCO table sets
#'
#' @param classifier Classifier object created by [mco_init()].
#'
#' @md
mco_indexes <- function(classifier) {
    .Call(`drdR_mco_Indexes`, classifier)
}

#' Extract GHM/GHS information from MCO tables
#'
#' @details
#' You can use any `date` supported by the set of tables loaded in [mco_init()].
#'
#' @param classifier Classifier object created by [mco_init()].
#' @param date Determines which tables to use.
#' @param sector Sector 'Public' or 'Private' (defaults to `default_sector` specified in [mco_init()]).
#' @param map Calculate information about possible GHM durations.
#'
#' @md
mco_ghm_ghs <- function(classifier, date, sector = NULL, map = FALSE) {
    .Call(`drdR_mco_GhmGhs`, classifier, date, sector, map)
}

#' Extract diagnosis information from MCO tables
#'
#' @details
#' You can use any `date` supported by the set of tables loaded in [mco_init()].
#'
#' @param classifier Classifier object created by [mco_init()].
#' @param date Determines which tables to use, format: 'yyyy-mm-dd' (e.g. '2018-03-01').
#'
#' @md
mco_diagnoses <- function(classifier, date) {
    .Call(`drdR_mco_Diagnoses`, classifier, date)
}

#' Extract DAS exclusion information from MCO tables
#'
#' @details
#' You can use any `date` supported by the set of tables loaded in [mco_init()].
#'
#' @param classifier Classifier object created by [mco_init()].
#' @param date Determines which tables to use, format: 'yyyy-mm-dd' (e.g. '2018-03-01').
#'
#' @md
mco_exclusions <- function(classifier, date) {
    .Call(`drdR_mco_Exclusions`, classifier, date)
}

#' Extract CCAM procedure information from MCO tables
#'
#' @details
#' You can use any `date` supported by the set of tables loaded in [mco_init()].
#'
#' @param classifier Classifier object created by [mco_init()].
#' @param date Determines which tables to use, format: 'yyyy-mm-dd' (e.g. '2018-03-01').
#'
#' @md
mco_procedures <- function(classifier, date) {
    .Call(`drdR_mco_Procedures`, classifier, date)
}

#' Create delta between two summary data.frames
#'
#' @md
mco_compare <- function(summary1, summary2, ...) {
    if (!('mco.result.summary' %in% class(summary1))) {
        summary1 <- summary(summary1, ...)
    }
    if (!('mco.result.summary' %in% class(summary2))) {
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

#' Summarize MCO results
#'
#' @description
#' Results obtained from [mco_classify()] can be by simply calling [summary()]
#' on the returned object, or on the `c$results` data.frame.
#'
#' @details
#' You can use the `by` parameter to group summary rows on a specific key (e.g. 'ghm').
#' By default the summary will only have a single row (total), which you can also
#' access directly with `c$summary`.
#'
#' @param result_set List returned by [mco_classify()].
#' @param by Column used to group summary (e.g. ghm), or NULL.
#'
#' @examples
#' \donttest{
#' c <- mco_classify(m, l)
#'
#' summary(c)
#' summary(c, by = 'ghm')
#' summary(c$results, by = 'ghm') # Does the same thing
#' }
#'
#' @md
summary.mco.result <- function(result_set, by = NULL) {
    if (is.null(by)) {
        return(result_set$summary)
    } else {
        return(summary.mco.result.df(result_set$results, by = by))
    }
}

summary.mco.result.df <- function(results, by = NULL) {
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

            class(agg) <- c(\'mco.result.summary\', class(agg))
            return(agg)
        }
    ')
    f <- eval(parse(text = code))

    return(f(results, by = by))
}

#' Dispense MCO pricings to medical units
mco_dispense <- function(results, group = NULL, group_var = 'group',
                         reassign_list = NULL, reassign_counts = FALSE) {
    if ('mco.result' %in% class(results)) {
        results <- results$mono_results
    }

    agg <- setDT(summary.mco.result.df(results, by = 'unit'))

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

        # Read comment in summary.mco.result.df for information about use of eval
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

#' Validate CIM-10 codes and strip extensions
#'
#' @description
#' Valid CIM-10 codes will be returned stripped of any extension (space extensions,
#' CIM +, etc.), NA will be returned for invalid CIM-10 codes.
#'
#' @param diagnoses Character vector of CIM-10 codes.
#' @return Cleaned up character vector of CIM-10 codes.
#'
#' @md
mco_clean_diagnoses <- function(diagnoses) {
    .Call(`drdR_mco_CleanDiagnoses`, diagnoses)
}

#' Validate CCAM codes
#'
#' @description
#' Valid CCAM codes will be returned as is, NA will be returned for invalid
#' CCAM codes.
#'
#' @param procedures Character vector of CCAM codes.
#' @return Cleaned up character vector of CCAM codes.
#'
#' @md
mco_clean_procedures <- function(procedures) {
    .Call(`drdR_mco_CleanProcedures`, procedures)
}
