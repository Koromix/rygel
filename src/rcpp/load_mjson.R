library(jsonlite)

stays <- fromJSON("V:/chu/rsa/lille_2016.mjson", flatten = TRUE)
stays <- data.table(stays)

stays[, `:=`(
    test.cluster_len = NULL,
    test.error = NULL,
    test.ghm = NULL,
    test.ghs = NULL,
    test.nn1 = NULL,
    test.nn2 = NULL,
    test.nn3 = NULL,
    test.rea = NULL,
    test.reasi = NULL,
    test.rep = NULL,
    test.si = NULL,
    test.src = NULL
)]
setnames(stays, c('dp', 'dr'), c('main_diagnosis', 'linked_diagnosis'))

diagnoses <- rbindlist(lapply(1:nrow(stays), function(i) {
    das <- unlist(stays$das[i])
    if (length(das) > 0) {
        data.table(
            id = i,
            diag = das
        )
    } else {
        NULL
    }
}))
procedures <- rbindlist(lapply(1:nrow(stays), function(i) {
    procedures <- stays$procedures[i][[1]]
    if (nrow(procedures) > 0) {
        procedures$id <- i
        as.data.table(procedures)
    } else {
        NULL
    }
}))

stays[, `:=`(
    id = 1:nrow(stays),
    das = NULL,
    procedures = NULL
)]

save(stays, diagnoses, procedures, file = 'V:/test4.RData')
