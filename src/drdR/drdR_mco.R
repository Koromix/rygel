# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

mco_summary_columns <- c('results', 'stays', 'failures',
                         'total_cents', 'price_cents', 'rea_cents', 'reasi_cents', 'si_cents',
                         'src_cents', 'nn1_cents', 'nn2_cents', 'nn3_cents', 'rep_cents',
                         'rea_days', 'reasi_days', 'si_days', 'src_days', 'nn1_days', 'nn2_days',
                         'nn3_days', 'rep_days')

mco_classify <- function(classifier, stays, diagnoses = NULL, procedures = NULL,
                         sorted = TRUE, options = character(0), details = TRUE, mono = FALSE) {
    if (!is.data.frame(stays) && is.list(stays) && is.null(diagnoses) && is.null(procedures)) {
        diagnoses <- stays$diagnoses
        procedures <- stays$procedures
        stays <- stays$stays
    }
    if (mono) {
        options = c(options, 'mono')
    }

    result_set <- .mco_classify(classifier, stays, diagnoses, procedures,
                                options = options, details = details)

    class(result_set$summary) <- c('mco_summary', class(result_set$summary))
    if (details) {
        setDT(result_set$results)
        class(result_set$results) <- c('mco_results', class(result_set$results))
    }
    if ('mono' %in% options) {
        setDT(result_set$mono_results)
    }
    class(result_set) <- c('mco_result_set', class(result_set))

    return (result_set)
}

mco_compare <- function(summary1, summary2, ...) {
    if (!('mco_summary' %in% class(summary1))) {
        summary1 <- summary(summary1, ...)
    }
    if (!('mco_summary' %in% class(summary2))) {
        summary2 <- summary(summary2, ...)
    }

    groups <- setdiff(colnames(summary1), mco_summary_columns)

    m <- merge(summary1, summary2, by = groups, all = TRUE)
    for (col in setdiff(colnames(m), groups)) {
        m[[col]][is.na(m[[col]])] <- 0
    }

    diff <- cbind(
        m[, groups, drop = FALSE],
        as.data.frame(sapply(mco_summary_columns, function(col) {
            m[[paste0(col, '.x')]] - m[[paste0(col, '.y')]]
        }, simplify = FALSE))
    )

    return(diff)
}

summary.mco_results <- function(results, by = NULL) {
    agg <- setDF(results[, c(
        list(
            results = .N,
            stays = sum(stays_count),
            failures = sum(startsWith('90Z', ghm)),
            total_cents = sum(total_cents),
            price_cents = sum(price_cents),
            rea_cents = sum(rea_cents),
            reasi_cents = sum(reasi_cents),
            si_cents = sum(si_cents),
            src_cents = sum(src_cents),
            nn1_cents = sum(nn1_cents),
            nn2_cents = sum(nn2_cents),
            nn3_cents = sum(nn3_cents),
            rep_cents = sum(rep_cents),
            rea_days = sum(rea_days),
            reasi_days = sum(reasi_days),
            si_days = sum(si_days),
            src_days = sum(src_days),
            nn1_days = sum(nn1_days),
            nn2_days = sum(nn2_days),
            nn3_days = sum(nn3_days),
            rep_days = sum(rep_days)
        )
    ), keyby = by])

    class(agg) <- c('mco_summary', class(agg))
    return (agg)
}
summary.mco_result_set <- function(result_set, by = NULL) {
    if (is.null(by)) {
        return (result_set$summary)
    } else {
        return (summary.mco_results(result_set$results, by = by))
    }
}
