#!/usr/bin/env Rscript
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

# Make sure we run in UTF-8 encoding (mostly for Windows)
if (!exists('.script_utf8')) {
    .script_utf8 <- TRUE
    .script_path <- with(list(args = commandArgs()),
                        trimws(substring(args[startsWith(args, '--file=')], 8)))
    source(.script_path, encoding = 'UTF-8')
    quit(save = 'no')
}

library(drdR)
library(data.table)
library(optparse)
library(stringr)

opt_parser <- OptionParser(option_list = list(
    make_option(c('-s', '--stays'), type = 'character', help = 'filter valid units from stay file')
))
args <- parse_args(opt_parser, positional_arguments = 1)

eprd <- fread(args$args[1], encoding = 'Latin-1')
eprd$CODE_UF <- as.numeric(eprd$CODE_UF)
eprd <- eprd[!is.na(eprd$CODE_UF),]
eprd$PATH <- paste0('|', eprd$LIB_POLE, '|',
                    str_pad(eprd$CODE_UF, 4, 'left', '0'), '-', eprd$LIB_UF)
setorder(eprd, PATH)

if (!is.null(args$options$stays)) {
    stays <- mco_load_stays(args$options$stays)$stays
    eprd <- eprd[eprd$CODE_UF %in% unique(stays$unit),]
}

cat('[EPRD]\n')
write.table(eprd[, list(CODE_UF, PATH)],
            sep = ' = ', col.names = FALSE, row.names = FALSE, quote = FALSE)
