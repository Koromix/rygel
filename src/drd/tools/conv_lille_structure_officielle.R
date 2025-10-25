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
library(zoo)

opt_parser <- OptionParser(option_list = list(
    make_option(c('-S', '--stays'), type = 'character', help = 'filter valid units from stay file')
))
args <- parse_args(opt_parser, positional_arguments = 1)

headers <- local({
    rows <- fread(args$args[1], header = FALSE, nrows = 2, na.strings = '')

    header1 <- na.locf(as.character(rows[1,]))
    header2 <- as.character(rows[2,])

    tolower(paste0(header1, '_', header2))
})

structure <- fread(args$args[1], encoding = 'Latin-1', skip = 2, col.names = headers)
structure$uf_code <- as.numeric(structure$uf_code)

if (!is.null(args$options$stays)) {
    stays <- mco_load_stays(args$options$stays)$stays
    structure <- structure[structure$uf_code %in% unique(stays$unit),]
}

structure$path_p <- paste0('|', structure$pole_libelle, '|',
                           str_pad(structure$uf_code, 4, 'left', '0'), '-', structure$uf_libelle)
structure$path_ps <- paste0('|', structure$pole_libelle, '|', structure$service_libelle, '|',
                           str_pad(structure$uf_code, 4, 'left', '0'), '-', structure$uf_libelle)
structure$path_s <- paste0('|', structure$service_libelle, '|',
                           str_pad(structure$uf_code, 4, 'left', '0'), '-', structure$uf_libelle)
structure$path <- paste0('|', str_pad(structure$uf_code, 4, 'left', '0'), '-', structure$uf_libelle)

cat('[Poles]\n')
write.table(structure[order(structure$path_p), list(uf_code, path_p)],
            sep = ' = ', col.names = FALSE, row.names = FALSE, quote = FALSE)
cat('\n')
cat('[Poles et services]\n')
write.table(structure[order(structure$path_ps), list(uf_code, path_ps)],
            sep = ' = ', col.names = FALSE, row.names = FALSE, quote = FALSE)
cat('\n')
cat('[Services]\n')
write.table(structure[order(structure$path_s), list(uf_code, path_s)],
            sep = ' = ', col.names = FALSE, row.names = FALSE, quote = FALSE)
cat('\n')
cat('[Plate]\n')
write.table(structure[order(structure$path), list(uf_code, path)],
            sep = ' = ', col.names = FALSE, row.names = FALSE, quote = FALSE)
cat('\n')
