# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

summary_columns <- c('ghs_cents', 'rea_cents', 'reasi_cents', 'si_cents', 'src_cents',
                     'nn1_cents', 'nn2_cents', 'nn3_cents', 'rep_cents', 'price_cents',
                     'rea_days', 'reasi_days', 'si_days', 'src_days', 'nn1_days', 'nn2_days',
                     'nn3_days', 'rep_days')

classify <- function(classifier, stays, diagnoses, procedures,
                     sorted = FALSE, details = TRUE) {
    if (!sorted) {
        sort_by_id <- function(df) {
            if (is.data.table(df)) {
                df <- setorder(data.table(df), id)
            } else {
                df <- df[order(df$id),]
            }
            return (df)
        }

        stays <- sort_by_id(stays)
        diagnoses <- sort_by_id(diagnoses)
        procedures <- sort_by_id(procedures)
    }

    result_set <- .classify(classifier, stays, diagnoses, procedures, details)

    class(result_set$summary) <- c('drd.summary', class(result_set$summary))
    if (details) {
        class(result_set$results) <- c('drd.results', class(result_set$results))
    }
    class(result_set) <- c('drd.result_set', class(result_set))

    return (result_set)
}

compare <- function(summary1, summary2, ...) {
    if (!('drd.summary' %in% class(summary1))) {
        summary1 <- summary(summary1, ...)
    }
    if (!('drd.summary' %in% class(summary2))) {
        summary2 <- summary(summary2, ...)
    }

    groups <- setdiff(colnames(summary1), summary_columns)

    m <- merge(summary1, summary2, by = groups, all = TRUE)
    for (col in setdiff(colnames(m), groups)) {
        m[[col]][is.na(m[[col]])] <- 0
    }

    diff <- cbind(
        m[, groups, drop = FALSE],
        as.data.frame(sapply(summary_columns, function(col) {
            m[[paste0(col, '.x')]] - m[[paste0(col, '.y')]]
        }, simplify = FALSE))
    )

    return(diff)
}

summary.drd.results <- function(results, by = NULL) {
    agg <- setDT(result_set)[, as.list(colSums(.SD[, ..summary_columns])),
                             keyby = eval(substitute(by))]
    agg <- setDF(agg)

    class(agg) <- c('drd.summary', class(agg))
    return (agg)
}
summary.drd.result_set <- function(result_set, by = NULL) {
    if (is.null(by)) {
        return (result_set$summary)
    } else {
        return (summary(result_set$results, by = by))
    }
}
