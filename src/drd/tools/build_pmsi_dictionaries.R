#!/usr/bin/env Rscript

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

# Make sure we run in UTF-8 encoding (mostly for Windows)
if (!exists('.script_utf8')) {
    .script_utf8 <- TRUE
    .script_path <- with(list(args = commandArgs()),
                        trimws(substring(args[startsWith(args, '--file=')], 8)))
    source(.script_path, encoding = 'UTF-8')
    quit(save = 'no')
}

library(tibble)
library(data.table)
library(readxl)
library(stringr)
library(jsonlite)
library(optparse)
library(enc)

load_mco <- function(root, err_filename) {
    cmds <- setDT(tribble(
        ~cmd, ~cmd_label,
        'C01', 'Affections du système nerveux',
        'C02', 'Affections de l\'œil',
        'C03', 'Affections des oreilles, du nez, de la gorge, de la bouche et des dents',
        'C04', 'Affections de l\'appareil respiratoire',
        'C05', 'Affections de l\'appareil circulatoire',
        'C06', 'Affections du tube digestif',
        'C07', 'Affections du système hépatobiliaire et du pancréas',
        'C08', 'Affections et traumatismes de l\'appareil musculosquelettique et du tissu conjonctif',
        'C09', 'Affections de la peau, des tissus sous-cutanés et des seins',
        'C10', 'Affections endocriniennes, métaboliques et nutritionnelles',
        'C11', 'Affections du rein et des voies urinaires',
        'C12', 'Affections de l\'appareil génital masculin',
        'C13', 'Affections de l\'appareil génital féminin',
        'C14', 'Grossesses pathologiques, accouchements et affections du postpartum',
        'C15', 'Nouveau-nés, prématurés et affections de la période périnatale',
        'C16', 'Affections du sang et des organes hématopoïétiques',
        'C17', 'Affections myéloprolifératives et tumeurs de siège imprécis ou diffus',
        'C18', 'Maladies infectieuses et parasitaires',
        'C19', 'Maladies et troubles mentaux',
        'C20', 'Troubles mentaux organiques liés à l\'absorption de drogues ou induits par celles-ci',
        'C21', 'Traumatismes, allergies et empoisonnements',
        'C22', 'Brulures',
        'C23', 'Facteurs influant sur l\'état de santé et autres motifs de recours aux services de santé',
        'C25', 'Maladies dues à une infection par le VIH',
        'C26', 'Traumatismes multiples graves',
        'C27', 'Transplantations d\'organes',
        'C28', 'Séances',
        'C90', 'Erreurs et autres séjours inclassables'
    ))

    files <- list.files(root, pattern = 'regroup.*\\.xls[xm]?', full.names = TRUE)
    if (!length(files))
        stop('Cannot find any GHM catalog file')

    # GHM roots

    ghm_roots <- rbindlist(lapply(files[grepl('racine', files)], function(filename) {
        dt <- as.data.table(read_excel(filename, sheet = 5, skip = 2))
        if (!('DA' %in% colnames(dt))) {
            dt <- as.data.table(read_excel(filename, sheet = 5, skip = 3))
        }
        dt <- dt[, 1:10]
        dt$version <- str_match(tolower(basename(filename)), 'v([0-9]+[a-z]?)')[,2]
        dt
    }), use.names = FALSE)
    setnames(ghm_roots,
             c('racine', 'libelle', 'DA', 'libellé domaine d\'activité', 'GA', 'libellé Groupes d\'Activité'),
             c('ghm_root', 'label', 'da', 'da_label', 'ga', 'ga_label'))
    setorder(ghm_roots, ghm_root, -version)
    ghm_roots$cmd <- paste0('C', substr(ghm_roots$ghm_root, 1, 2))
    ghm_roots[cmds, cmd_label := i.cmd_label, on = 'cmd']
    ghm_roots <- ghm_roots[, intersect(colnames(ghm_roots),
                                       c('ghm_root', 'label', 'da', 'da_label', 'ga', 'ga_label',
                                         'cmd', 'cmd_label', 'version')), with = FALSE]
    ghm_roots <- unique(ghm_roots, by = 'ghm_root')
    ghm_roots$version <- NULL

    # GHMs

    ghms <- rbindlist(lapply(files[!grepl('racine', files)], function(filename) {
        dt <- as.data.table(read_excel(filename, sheet = 1, skip = 2))
        if (!('DA' %in% colnames(dt))) {
            dt <- as.data.table(read_excel(filename, sheet = 1, skip = 3))
        }
        dt <- dt[, 1:2]
        dt$version <- str_match(tolower(basename(filename)), 'v([0-9]+[a-z]?)')[,2]
        dt
    }), use.names = FALSE)
    setnames(ghms, c('ghm', 'label', 'version'))
    setorder(ghms, ghm, -version)
    ghms <- unique(ghms, by = 'ghm')

    # Errors

    errors <- setDT(read_excel(err_filename, sheet = 'Erreurs'))
    setnames(errors,
             c('Code erreur', 'Intitulé'),
             c('erreur', 'label'))
    errors$erreur <- as.integer(errors$erreur)

    # Output

    parents <- lapply(1:nrow(ghm_roots),
                      function(i) list(cmd = unbox(ghm_roots[i,]$cmd),
                                       da = unbox(ghm_roots[i,]$da),
                                       ga = unbox(ghm_roots[i,]$ga)))

    return (list(
        cmd = list(
            title = unbox('Catégories Majeures de Diagnostics'),
            definitions = unique(ghm_roots[, list(code = cmd, label = cmd_label)], by = 'code')
        ),
        da = list(
            title = unbox('Domaines d\'activité'),
            definitions = unique(ghm_roots[, list(code = da, label = da_label)], by = 'code')
        ),
        ga = list(
            title = unbox('Groupes d\'activité'),
            definitions = unique(ghm_roots[, list(code = ga, label = ga_label)], by = 'code')
        ),
        ghm_roots = list(
            title = unbox('Racines de GHM'),
            parents = c('cmd', 'da', 'ga'),
            definitions = data.table(code = ghm_roots$ghm_root, label = ghm_roots$label,
                                     parents = parents)
        ),
        ghm = list(
            title = unbox('GHM'),
            definitions = data.table(code = ghms$ghm, label = ghms$label)
        ),
        errors = list(
            title = unbox('Erreurs MCO'),
            definitions = errors[, list(code = erreur, label)]
        )
    ))
}

load_ccam <- function(filename) {
    ccam <- fread(filename, encoding = 'UTF-8')

    setorder(ccam, -last_version)
    ccam <- unique(ccam[, list(code = code, label = label)])

    return (list(
        procedures = list(
            title = unbox('CCAM'),
            definitions = ccam
        )
    ))
}

load_cim10 <- function(filename) {
    cim10 <- fread(filename, encoding = 'Latin-1')

    cim10 <- unique(cim10[, list(code = code, label = liblong)])

    return (list(
        diagnoses = list(
            title = unbox('CIM-10'),
            definitions = cim10
        )
    ))
}

opt_parser <- OptionParser(option_list = list(
    make_option(c('-D', '--destination'), type = 'character', help = 'destination directory')
))
args <- parse_args(opt_parser, positional_arguments = 1)

if (is.null(args$options$destination))
    stop('Missing destination directory');

mco <- load_mco(args$args[1], str_interp('${args$args[1]}/erreurs.xlsx'))
ccam <- load_ccam(str_interp('${args$args[1]}/ccam.csv'))
cim10 <- load_cim10(str_interp('${args$args[1]}/cim10.csv'))

write_lines_enc(toJSON(mco, pretty = 4),
                path = str_interp('${args$options$destination}/mco.json'))
write_lines_enc(toJSON(ccam, pretty = 4),
                path = str_interp('${args$options$destination}/ccam.json'))
write_lines_enc(toJSON(cim10, pretty = 4),
                path = str_interp('${args$options$destination}/cim10.json'))
