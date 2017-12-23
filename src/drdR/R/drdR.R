# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

summary_columns <- c('ghs_price', 'rea', 'reasi', 'si',
                     'src', 'nn1', 'nn2', 'nn3', 'rep')

classify <- function(classifier_set, stays, diagnoses, procedures) {
    stays <- stays[order(stays$id),]
    diagnoses <- diagnoses[order(diagnoses$id),]
    procedures <- procedures[order(procedures$id),]

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
    diff <- diff[rowSums(diff[, summary_columns]) != 0,]
    diff <- diff[do.call('order', diff[, groups, drop = FALSE]),]

    class(diff) <- c('drd.result_set', class(diff))
    return(diff)
}

summary.drd.result_set <- function(result_set, by = list(group = 1)) {
    by_val <- eval(substitute(by), result_set, parent.frame())
    if (!is.list(by_val)) {
        by_val <- list(by_val)
    }
    by_val <- lapply(by_val, function(x) rep_len(x, nrow(result_set)))

    if (first(as.character(substitute(by))) == 'list') {
        names1 <- tail(as.character(substitute(by)), length(by_val))
        names2 <- names(by_val)
        if (!is.null(names2)) {
            by_names <- ifelse(names2 == '', names1, names2)
        } else {
            by_names <- names1
        }
    } else if (length(by_val) == 1) {
        by_names <- deparse(substitute(by))
    } else {
        by_names <- paste0('Group.', seq_along(by))
    }

    if (nrow(result_set) > 0) {
        agg <- aggregate(result_set[, summary_columns], by = by_val, FUN = sum)
        colnames(agg)[seq_along(by_val)] <- by_names
        agg <- agg[do.call('order', agg[, by_names, drop = FALSE]),]
    } else {
        agg <- data.frame()
    }

    class(agg) <- c('drd.result_summary', class(agg))
    return(agg)
}
