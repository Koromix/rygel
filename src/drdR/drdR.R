# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

summary_columns <- c('ghs_cents', 'rea_cents', 'reasi_cents', 'si_cents',
                     'src_cents', 'nn1_cents', 'nn2_cents', 'nn3_cents', 'rep_cents',
                     'price_cents')

classify <- function(classifier_set, stays, diagnoses, procedures, sorted = FALSE) {
    if (!sorted) {
        stays <- stays[order(stays$id),]
        diagnoses <- diagnoses[order(diagnoses$id),]
        procedures <- procedures[order(procedures$id),]
    }

    result_set <- .classify(classifier_set, stays, diagnoses, procedures)

    class(result_set) <- c('drd.result_set', class(result_set))
    return(result_set)
}

compare <- function(summary1, summary2, ...) {
    if (!('drd.result_summary' %in% class(summary1))) {
        summary1 <- summary(summary1, ...)
    }
    if (!('drd.result_summary' %in% class(summary2))) {
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

summary.drd.result_set <- function(result_set, by = NULL) {
    agg <- setDT(result_set)[, as.list(colSums(.SD[, ..summary_columns])),
                             keyby = eval(substitute(by))]
    agg <- setDF(agg)

    class(agg) <- c('drd.result_summary', class(agg))
    return (agg)
}
