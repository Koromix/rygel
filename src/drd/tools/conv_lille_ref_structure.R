#!/usr/bin/env Rscript

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

library(drdR)
library(data.table)
library(optparse)
library(stringr)

opt_parser <- OptionParser(option_list = list(
    make_option(c('-s', '--stays'), type = 'character', help = 'filter valid units from stay file')
))
args <- parse_args(opt_parser, positional_arguments = 1)

ref <- fread(args$args[1], encoding = 'Latin-1')
setnames(ref, c('REF - Code Pole', 'REF - Libelle long Pole', 'REF - Code Clinique',
                'REF - Libelle long Clinique', 'REF - Code Service', 'REF - Libelle long service',
                'REF - Code UF', 'REF - Libelle long UF'),
              c('CODE_POLE', 'LIB_POLE', 'CODE_CLINIQUE', 'LIB_CLINIQUE', 'CODE_SERVICE',
                'LIB_SERVICE', 'CODE_UF', 'LIB_UF'))
ref$CODE_UF <- as.numeric(ref$CODE_UF)
ref <- ref[!is.na(ref$CODE_UF),]
ref <- unique(ref, by = 'CODE_UF')
ref$PATH <- paste0('|', ref$LIB_POLE, '|', ref$LIB_CLINIQUE, '|', ref$LIB_SERVICE, '|',
                   str_pad(ref$CODE_UF, 4, 'left', '0'), '-', ref$LIB_UF)
setorder(ref, PATH)

if (!is.null(args$options$stays)) {
    stays <- mco_load_stays(args$options$stays)$stays
    ref <- ref[ref$CODE_UF %in% unique(stays$unit),]
}

cat('[Officielle]\n')
write.table(ref[, list(CODE_UF, PATH)],
            sep = ' = ', col.names = FALSE, row.names = FALSE, quote = FALSE)
