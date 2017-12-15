# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

library(data.table)
library(readxl)
library(stringr)
library(jsonlite)

ghm_roots <- local({
    files <- list.files('../data/desc/xlsx', pattern = '*.xlsx', full.names = TRUE)

    ghm_roots <- rbindlist(lapply(files, function(filename) {
        dt <- as.data.table(read_xlsx(filename, sheet = 1))
        dt$version <- str_match(tolower(basename(filename)), 'v([0-9]+[a-z]?)')[,2]
        dt
    }), fill = TRUE)
    setorder(ghm_roots, racine, -version)

    setnames(ghm_roots,
             c('racine', 'libelle', 'DA', 'libellé domaine d\'activité', 'GA', 'libellé Groupes d\'Activité'),
             c('root', 'root_desc', 'da', 'da_desc', 'ga', 'ga_desc'))

    keep_columns <- c('root', 'root_desc', 'da', 'da_desc', 'ga', 'ga_desc', 'version')
    ghm_roots <- ghm_roots[, intersect(colnames(ghm_roots), keep_columns), with = FALSE]
})

unique_ghm_roots <- unique(ghm_roots, by = 'root')
unique_ghm_roots$version <- NULL

writeLines(toJSON(unique_ghm_roots, pretty = 4), '../data/ghm_roots.json')
