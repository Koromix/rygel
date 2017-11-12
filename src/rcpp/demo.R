if (!exists('result_set')) {
    source('moya_r.R')

    library(data.table)
    load('V:/chu/test/R_2016.RData')

    result_set <- moya.classify(stays, diagnoses, procedures)
}

result_set_mod <- local({
    stays2 <- data.table(stays)
    diagnoses2 <- data.table(diagnoses)
    procedures2 <- data.table(procedures)

    # Z43 en Z93
    # id_z43 <- stays2[startsWith(main_diagnosis, "Z431")]$id
    # stays2[
    #     id %in% id_z43,
    #     main_diagnosis := "Z931"
    # ]

    # stays2[, igs2 := igs2 - 2]

    # stays2[unit == 10013, unit := 10000]

    # procedures2 <- procedures2[code != "HPSA001"]

    diagnoses2[startsWith(diag, "Z431"), diag := "Z931"]

    moya.classify(stays2, diagnoses2, procedures2)
})

diff <- moya.compare(result_set_mod, result_set, by = list(racine_ghm = substr(ghm, 1, 5)))
print(diff)
