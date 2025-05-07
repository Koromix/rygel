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

# Start the script from a command line this way:
# Rscript flatten_ccam.csv [in.xls] -D [out.csv]
# Example: Rscript flatten_ccam.csv CCAM_V57.xls -D ccam57_plate.csv

# Make sure we run in UTF-8 encoding (mostly for Windows)
if (!exists('.script_utf8')) {
    .script_utf8 <- TRUE
    .script_path <- with(list(args = commandArgs()),
                        trimws(substring(args[startsWith(args, '--file=')], 8)))
    source(.script_path, encoding = 'UTF-8')
    quit(save = 'no')
}

library(zoo, warn.conflicts = FALSE)
library(data.table)
library(readxl)
library(optparse)
library(stringr)
library(jsonlite)
library(enc)

# ----- Arguments -----

local({
    args <- parse_args(
        OptionParser(option_list = list(
            make_option(c('-D', '--destination'), type = 'character', help = 'destination file'),
            make_option(c('-f', '--format'), type = 'character', help = 'export format (CSV, THOP)', default = 'CSV')
        )),
        positional_arguments = 1
    )

    if (is.null(args$options$destination))
        stop('Missing destination filename');

    args_src <<- args$args[1]
    args_dest <<- args$options$destination
    args_format <<- args$options$format
})

# ----- Lecture du fichier brut -----

ccam_brute <- setDT(read_excel(args_src, skip = 1, na = '',
                               col_types = c('text', 'skip', 'text', 'numeric', 'numeric',
                                             'text', 'text', 'text', 'text', 'text', 'text')))
names(ccam_brute) <- c('code', 'texte', 'activite', 'phase', 'tarif1', 'tarif2', 'rc',
                       'accord_prealable', 'exo_tm', 'regroupement')

# ---- Table nettoyée ----

ccam_dup <- data.table(
    code = ccam_brute$code,
    desc = ccam_brute$texte,
    phase = ccam_brute$phase,
    chapitre1 = ccam_brute$code,
    chapitre1_desc = NA_character_,
    chapitre2 = ccam_brute$code,
    chapitre2_desc = NA_character_,
    chapitre3 = ccam_brute$code,
    chapitre3_desc = NA_character_,
    chapitre4 = ccam_brute$code,
    chapitre4_desc = NA_character_,
    activite1 = 0,
    activite2 = 0,
    activite3 = 0,
    activite4 = 0,
    activite5 = 0,
    regroupement = ccam_brute$regroupement,
    gestcomp = NA_character_,
    tarif1 = round(as.numeric(ccam_brute$tarif1), 2),
    tarif2 = round(as.numeric(ccam_brute$tarif2), 2),

    texte = ccam_brute$texte,
    activite = ccam_brute$activite
)

# ---- Informations manquantes (LOCF) ----

ccam_dup$code <- ifelse(grepl('^[A-Z]{4}[0-9]{3}$', ccam_dup$code), ccam_dup$code, NA)
ccam_dup$desc <- ifelse(!is.na(ccam_dup$code), ccam_dup$desc, NA)
ccam_dup$code <- na.locf(ccam_dup$code, na.rm = FALSE)
ccam_dup$desc <- na.locf(ccam_dup$desc, na.rm = FALSE)
ccam_dup$desc <- paste0(ccam_dup$code, ' - ', ifelse(grepl('^Phase [0-9] :', ccam_dup$texte),
                                                     paste0(ccam_dup$desc, ' (', ccam_dup$texte, ')'), ccam_dup$desc))

# ---- Découpage des chapitres ----

ccam_dup$chapitre1 <- ifelse(grepl('^[0-9]{1,2}$', ccam_dup$chapitre1), ccam_dup$chapitre1, NA)
ccam_dup$chapitre1 <- na.locf(ccam_dup$chapitre1, na.rm = FALSE)
ccam_dup$chapitre2 <- ifelse(grepl('^[0-9]{1,2}(\\.[0-9]{2})?$', ccam_dup$chapitre2), ccam_dup$chapitre2, NA)
ccam_dup$chapitre2 <- na.locf(ccam_dup$chapitre2, na.rm = FALSE)
ccam_dup$chapitre3 <- ifelse(grepl('^[0-9]{1,2}(\\.[0-9]{2}(\\.[0-9]{2})?)?$', ccam_dup$chapitre3), ccam_dup$chapitre3, NA)
ccam_dup$chapitre3 <- na.locf(ccam_dup$chapitre3, na.rm = FALSE)
ccam_dup$chapitre4 <- ifelse(grepl('^[0-9]{1,2}(\\.[0-9]{2}(\\.[0-9]{2}(\\.[0-9]{2})?)?)?$', ccam_dup$chapitre4), ccam_dup$chapitre4, NA)
ccam_dup$chapitre4 <- na.locf(ccam_dup$chapitre4, na.rm = FALSE)

# ---- Informations chapitres et gestes complémentaires ----

chapitres <- data.table(
    chapitre = ccam_brute$code,
    desc = paste0(str_pad(ccam_brute$code, 2, pad = '0'), ' - ', ccam_brute$texte)
)
chapitres <- chapitres[grepl('^[0-9]{1,2}(\\.[0-9]{2}(\\.[0-9]{2}(\\.[0-9]{2})?)?)?$', chapitre),]

gestcomp <- data.table(
    code = ccam_dup$code,
    actes = ccam_dup$texte
)
gestcomp <- gestcomp[grepl('\\(([A-Z]{4}[0-9]{3})(\\, ([A-Z]{4}[0-9]{3}))*\\)$', actes),]
gestcomp$actes <- sapply(regmatches(gestcomp$actes, gregexpr('[A-Z]{4}[0-9]{3}', gestcomp$actes)),
                         function(x) paste(x, collapse = '|'))

# ----- Table finale -----

ccam <- unique(ccam_dup[!is.na(code) & !is.na(phase),], by = c('code', 'phase'))
ccam$texte <- NULL
ccam$activite <- NULL
ccam[chapitres, chapitre1 := i.desc, on = c('chapitre1' = 'chapitre')]
ccam[chapitres, chapitre2 := i.desc, on = c('chapitre2' = 'chapitre')]
ccam[chapitres, chapitre3 := i.desc, on = c('chapitre3' = 'chapitre')]
ccam[chapitres, chapitre4 := i.desc, on = c('chapitre4' = 'chapitre')]
ccam[ccam_dup[activite == 1,], activite1 := 1, on = 'code']
ccam[ccam_dup[activite == 2,], activite2 := 1, on = 'code']
ccam[ccam_dup[activite == 3,], activite3 := 1, on = 'code']
ccam[ccam_dup[activite == 4,], activite4 := 1, on = 'code']
ccam[ccam_dup[activite == 5,], activite5 := 1, on = 'code']
ccam[gestcomp, gestcomp := i.actes, on = 'code']

# ----- Export -----

if (args_format == 'CSV') {
    write.csv2(ccam, file = args_dest, row.names = FALSE, na = '')
} else if (args_format == 'JSON') {
    ccam$activite1 <- (ccam$activite1 == 1)
    ccam$activite2 <- (ccam$activite2 == 1)
    ccam$activite3 <- (ccam$activite3 == 1)
    ccam$activite4 <- (ccam$activite4 == 1)
    ccam$activite5 <- (ccam$activite5 == 1)

    write_lines_enc(toJSON(ccam, pretty = 4), path = args_dest)
} else if (args_format == 'THOP') {
    json <- list(
        procedures = list(
            title = unbox('CCAM'),
            definitions = data.frame(code = ccam$code, label = substring(ccam$desc, 11))
        )
    )
    write_lines_enc(toJSON(json, pretty = 4), path = args_dest)
} else {
    stop(str_interp('Unexpected format ${args_format}'))
}
