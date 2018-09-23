#!/usr/bin/env Rscript

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

library(tibble)
library(data.table)
library(readxl)
library(stringr)
library(jsonlite)
library(optparse)
library(enc) # UTF-8 support in R on Windows is completely fucked up

load_mco_ghm_roots <- function(root) {
    cmds <- tribble(
        ~cmd, ~cmd_desc,
        1,  'Affections du système nerveux',
        2,  'Affections de l\'œil',
        3,  'Affections des oreilles, du nez, de la gorge, de la bouche et des dents',
        4,  'Affections de l\'appareil respiratoire',
        5,  'Affections de l\'appareil circulatoire',
        6,  'Affections du tube digestif',
        7,  'Affections du système hépatobiliaire et du pancréas',
        8,  'Affections et traumatismes de l\'appareil musculosquelettique et du tissu conjonctif',
        9,  'Affections de la peau, des tissus sous-cutanés et des seins',
        10, 'Affections endocriniennes, métaboliques et nutritionnelles',
        11, 'Affections du rein et des voies urinaires',
        12, 'Affections de l\'appareil génital masculin',
        13, 'Affections de l\'appareil génital féminin',
        14, 'Grossesses pathologiques, accouchements et affections du postpartum',
        15, 'Nouveau-nés, prématurés et affections de la période périnatale',
        16, 'Affections du sang et des organes hématopoïétiques',
        17, 'Affections myéloprolifératives et tumeurs de siège imprécis ou diffus',
        18, 'Maladies infectieuses et parasitaires',
        19, 'Maladies et troubles mentaux',
        20, 'Troubles mentaux organiques liés à l\'absorption de drogues ou induits par celles-ci',
        21, 'Traumatismes, allergies et empoisonnements',
        22, 'Brulures',
        23, 'Facteurs influant sur l\'état de santé et autres motifs de recours aux services de santé',
        25, 'Maladies dues à une infection par le VIH',
        26, 'Traumatismes multiples graves',
        27, 'Transplantations d\'organes',
        28, 'Séances',
        90, 'Erreurs et autres séjours inclassables'
    )

    files <- list.files(root, pattern = 'regroup.*\\.xlsx?', full.names = TRUE)
    if (!length(files))
        stop('Cannot find any GHM catalog file')

    ghm_roots <- rbindlist(lapply(files, function(filename) {
        dt <- as.data.table(read_excel(filename, sheet = 1))
        dt$version <- str_match(tolower(basename(filename)), 'v([0-9]+[a-z]?)')[,2]
        dt
    }), fill = TRUE)
    setorder(ghm_roots, racine, -version)

    setnames(ghm_roots,
             c('racine', 'libelle', 'DA', 'libellé domaine d\'activité', 'GA', 'libellé Groupes d\'Activité'),
             c('ghm_root', 'desc', 'da', 'da_desc', 'ga', 'ga_desc'))

    ghm_roots$cmd <- as.integer(substr(ghm_roots$ghm_root, 1, 2))
    ghm_roots[cmds, cmd_desc := i.cmd_desc, on = 'cmd']

    keep_columns <- c('ghm_root', 'desc', 'da', 'da_desc', 'ga', 'ga_desc',
                      'cmd', 'cmd_desc', 'version')
    ghm_roots <- ghm_roots[, intersect(colnames(ghm_roots), keep_columns), with = FALSE]

    ghm_roots <- unique(ghm_roots, by = 'ghm_root')
    ghm_roots$version <- NULL

    return (ghm_roots)
}

load_ccam <- function(filename) {
    ccam <- fread(filename, encoding = 'Latin-1')

    # hiera <- fread(str_interp('${root}/ccam_hiera.csv'), encoding = 'Latin-1')

    # ccam$hiera <- str_pad(as.character(ccam$hiera), 8, 'left', 0)
    # hiera$hiera <- str_pad(as.character(hiera$hiera), 8, 'left', 0)

    # setorder(hiera, hiera)
    # for (i in 4:1) {
    #     hiera$.hiera <- str_pad(substr(hiera$hiera, 1, i * 2), 8, 'right', '0')
    #     hiera[hiera, lib := ifelse(hiera != i.hiera, paste0(i.lib, '::', lib), lib), on = c('.hiera'='hiera')]
    # }
    # hiera$.hiera <- NULL

    # ccam[hiera, `:=`(
    #     lib = paste0('::', i.lib, '::', lib),
    #     liblong = paste0('::', i.lib, '::', liblong)
    # ), on = 'hiera']

    setorder(ccam, -last_version)
    ccam <- unique(ccam[, list(procedure = code, desc = liblong)])

    return (ccam)
}

load_cim10 <- function(filename) {
    cim10 <- fread(filename, encoding = 'Latin-1')

    cim10 <- unique(cim10[, list(diagnosis = code, desc = liblong)])

    return (cim10)
}

opt_parser <- OptionParser(option_list = list(
    make_option(c('-D', '--destination'), type = 'character', help = 'destination directory')
))
args <- parse_args(opt_parser, positional_arguments = 1)

if (is.null(args$options$destination))
    stop('Missing destination directory');

mco_ghm_roots <- load_mco_ghm_roots(args$args[1])
ccam <- load_ccam(str_interp('${args$args[1]}/ccam.csv'))
cim10 <- load_cim10(str_interp('${args$args[1]}/cim10.csv'))

write_lines_enc(toJSON(mco_ghm_roots, pretty = 4),
                path = str_interp('${args$options$destination}/mco_ghm_roots.json'))
write_lines_enc(toJSON(ccam, pretty = 4),
                path = str_interp('${args$options$destination}/ccam.json'))
write_lines_enc(toJSON(cim10, pretty = 4),
                path = str_interp('${args$options$destination}/cim10.json'))
