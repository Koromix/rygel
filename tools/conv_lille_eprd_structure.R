#!/usr/bin/env Rscript

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

library(drdR)
library(data.table)
library(optparse)

opt_parser <- OptionParser(option_list = list(
    make_option(c('-s', '--stays'), type = 'character', help = 'filter valid units from stay file')
))
args <- parse_args(opt_parser, positional_arguments = 1)

eprd <- fread(args$args[1], encoding = 'Latin-1')
eprd$CODE_UF <- as.numeric(eprd$CODE_UF)
eprd <- eprd[!is.na(eprd$CODE_UF),]
eprd$PATH <- paste0('::', eprd$LIB_POLE, '::', eprd$CODE_UF, '-', eprd$LIB_UF)
setorder(eprd, PATH)

if (!is.null(args$options$stays)) {
    stays <- mco_load_stays('/media/veracrypt1/dim/mco/rss/rss.dspak.gz')$stays
    eprd <- eprd[eprd$CODE_UF %in% unique(stays$unit),]
}

cat('[EPRD]\n')
write.table(eprd[, list(CODE_UF, PATH)],
            sep = ' = ', col.names = FALSE, row.names = FALSE, quote = FALSE)
