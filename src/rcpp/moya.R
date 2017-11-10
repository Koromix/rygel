library(data.table)
library(Rcpp)

Sys.setenv(PKG_CXXFLAGS="-std=c++1y")
sourceCpp('moya_rcpp.cc')

moya.summary_columns <- c('ghs_price', 'rea', 'reasi', 'si',
                          'src', 'nn1', 'nn2', 'nn3', 'rep')

moya.classify <- function(stays, diagnoses, procedures,
                          copy_columns = FALSE) {
    setorder(stays, id)
    setorder(diagnoses, id)
    setorder(procedures, id)

    result_set <- .moya.classify(stays, diagnoses, procedures)

    if (is.logical(copy_columns)) {
        if (copy_columns) {
            result_set <- cbind(stays, result_set)
        }
    } else {
        result_set <- cbind(stays[, copy_columns], result_set)
    }

    class(result_set) <- c('moya.result_set', class(result_set))
    return(result_set)
}

moya.compare <- function(summary1, summary2, ...) {
    if (!('moya.result_summary' %in% class(summary1))) {
        summary1 <- summary(summary1, ...)
    }
    if (!('moya.result_summary' %in% class(summary2))) {
        summary2 <- summary(summary2, ...)
    }

    groups <- setdiff(colnames(summary1), moya.summary_columns)

    m <- merge(summary1, summary2, by = groups, all = TRUE)
    for (col in setdiff(colnames(m), groups)) {
        m[[col]][is.na(m[[col]])] <- 0
    }

    diff <- cbind(
        m[, groups, drop = FALSE],
        as.data.frame(sapply(moya.summary_columns, function(col) {
            m[[paste0(col, '.x')]] - m[[paste0(col, '.y')]]
        }, simplify = FALSE))
    )
    diff <- diff[rowSums(diff[, moya.summary_columns]) != 0,]
    diff <- diff[do.call('order', diff[, groups, drop = FALSE]),]

    class(diff) <- c('moya.result_diff', class(diff))
    return(diff)
}

summary.moya.result_set <- function(result_set, by = 1) {
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
        agg <- aggregate(result_set[, moya.summary_columns], by = by_val, FUN = sum)
        colnames(agg)[seq_along(by_val)] <- by_names

        agg <- agg[do.call('order', agg[, by_names, drop = FALSE]),]
    } else {
        agg <- data.frame()
    }

    class(agg) <- c('moya.result_summary', class(agg))
    return(agg)
}
