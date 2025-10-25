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

library(rprojroot)
library(stringr)
library(devtools)
library(drat)
library(roxygen2)
library(parallel)
library(optparse)

bundle_drdR <- function(project_dir, version, build_dir) {
    unlink(build_dir, recursive = TRUE)

    list_files <- function(dir) {
        dir <- str_interp('${project_dir}/${dir}')
        files <- substring(list.files(dir, full.names = TRUE), nchar(project_dir) + 2)

        return (files)
    }

    copy_files <- function(src_files, dest_dir, recursive = FALSE) {
        src_files <- sapply(src_files, function(path) str_interp('${project_dir}/${path}'))
        dest_dir <- str_interp('${build_dir}/${dest_dir}/')

        dir.create(dest_dir, recursive = TRUE, showWarnings = FALSE)
        file.copy(src_files, dest_dir, recursive = recursive, overwrite = TRUE)
    }

    local({
        for (name in c('Makevars', 'Makevars.win', 'Makevars.inc')) {
            copy_files(str_interp('src/drd/drdR/src/${name}'), 'src')

            filename <- str_interp('${build_dir}/src/${name}')
            lines <- readLines(filename)

            lines <- sub('.o: ../../../../vendor/', '.o: vendor/', lines, fixed = TRUE)
            lines <- sub('.o: ../../../core/', '.o: src/core/', lines, fixed = TRUE)
            lines <- sub('.o: ../../libdrd/', '.o: src/drd/libdrd/', lines, fixed = TRUE)
            lines <- sub('.o: ../', '.o: src/drd/drdR/', lines, fixed = TRUE)
            lines <- lines[!grepl('../R', lines, fixed = TRUE)]

            writeLines(lines, filename)
        }
    })

    copy_files(c('src/drd/drdR/DESCRIPTION',
                 'src/drd/drdR/NAMESPACE',
                 'src/drd/drdR/drdR.Rproj'), '.')

    local({
        filename <- str_interp('${build_dir}/DESCRIPTION')
        lines <- readLines(filename)

        lines <- ifelse(
            grepl('Version:', lines, fixed = TRUE),
            str_interp('Version: ${version}'),
            lines
        )

        writeLines(lines, filename)
    })

    copy_files('src/drd/drdR/drdR_mco.cc', 'src/src/drd/drdR')
    copy_files(c('src/drd/drdR/drdR.R',
                 'src/drd/drdR/drdR_mco.R'), 'R')
    copy_files(list_files('src/drd/libdrd'), 'src/src/drd/libdrd')
    copy_files(list_files('src/core/base'), 'src/src/core/base')
    copy_files(list_files('src/core/compress'), 'src/src/core/compress')
    copy_files(c('src/core/wrap/Rcc.cc',
                 'src/core/wrap/Rcc.hh'), 'src/src/core/wrap')
    copy_files(list_files('vendor/miniz'), 'src/vendor/miniz')
    copy_files('vendor/dragonbox/include/dragonbox/dragonbox.h', 'src/vendor/dragonbox/include/dragonbox')
}

run_roxygen2 <- function(pkg_dir) {
    env_legacy <- function(path) {
        env <- new.env()
        files <- dir(file.path(path, "R"), full.names = TRUE)
        for (file in files) source(file, local = env)
        env
    }

    roxygen2::roxygenize(pkg_dir, roclets = 'rd', load_code = env_legacy)
}

build_package <- function(pkg_dir) {
    devtools::build(pkg_dir)
    devtools::build(pkg_dir, binary = TRUE)
}

# Parse arguments
local({
    opt_parser <- OptionParser(option_list = list(
        make_option(c('-D', '--destination'), type = 'character', help = 'destination directory')
    ))
    args <- parse_args(opt_parser, positional_arguments = FALSE)

    src_dir <<- rprojroot::find_root('FelixBuild.ini')

    if (!is.null(args$options$destination)) {
        dest_dir <<- args$options$destination
    } else {
        dest_dir <<- str_interp('${src_dir}/bin/R')
    }
})

# Use all cores to build package
local({
    cores <- detectCores()
    flags <- Sys.getenv('MAKEFLAGS', unset = '')

    Sys.setenv(MAKEFLAGS = str_interp('-j${cores + 1} ${flags}'))
})

# Package version (commit date)
version <- trimws(system(str_interp('git -C "${src_dir}" log -n1 --pretty=format:%cd --date=format:%Y.%m%d.%H%M'),
                         show.output.on.console = FALSE, intern = TRUE))

# Bundle, build and register packages
local({
    build_dir <- str_interp('${dest_dir}/drdR')

    bundle_drdR(src_dir, version, build_dir)
    run_roxygen2(build_dir)
    build_package(build_dir)
})
