#!/usr/bin/env Rscript

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

library(stringr)
library(devtools)
library(drat)
library(roxygen2)
library(optparse)

bundle_drdR <- function(project_dir, build_dir) {
    list_files <- function(dir) {
        dir <- str_interp('${project_dir}/${dir}')
        list.files(dir, full.names = TRUE)
    }
    copy_files <- function(src_files, dest_dir, recursive = FALSE) {
        src_files <- sapply(src_files, function(path) str_interp('${project_dir}/${path}'))
        dest_dir <- str_interp('${build_dir}/${dest_dir}/')

        dir.create(dest_dir, recursive = TRUE, showWarnings = FALSE)
        file.copy(src_files, dest_dir, recursive = recursive, overwrite = TRUE)
    }

    copy_files(c('src/drd/drdR/src/Makevars',
                 'src/drd/drdR/src/Makevars.win'), 'src')
    local({
        filename <- str_interp('${build_dir}/src/Makevars')
        lines <- readLines(filename)

        lines <- sub('.o: ../', '.o: drd/drdR/', lines)
        lines <- lines[!grepl('../R', lines, fixed = TRUE)]

        writeLines(lines, filename)
    })

    copy_files(c('src/drd/drdR/DESCRIPTION',
                 'src/drd/drdR/NAMESPACE',
                 'src/drd/drdR/drdR.Rproj'), '.')
    local({
        filename <- str_interp('${build_dir}/DESCRIPTION')
        lines <- readLines(filename)

        lines <- ifelse(
            grepl('Version:', lines, fixed = TRUE),
            paste0('Version: ', gsub('-', '.', Sys.Date(), fixed = TRUE)),
            lines
        )

        writeLines(lines, filename)
    })

    copy_files('src/drd/drdR/src/dummy.cc', 'src')
    copy_files('src/drd/drdR/drdR_mco.cc', 'src/drd/drdR')
    copy_files('src/drd/drdR/drdR_mco.R', 'R')
    copy_files(list_files('src/drd/libdrd'), 'src/drd/libdrd')
    copy_files(list_files('src/libcc'), 'src/libcc')
    copy_files(c('src/wrappers/Rcc.cc',
                 'src/wrappers/Rcc.hh'), 'src/wrappers')
    copy_files(list_files('vendor/miniz'), 'vendor/miniz')

    return (build_dir)
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

build_package <- function(pkg_dir, repo_dir) {
    pkg_src_filename <- devtools::build(pkg_dir)
    if (Sys.info()[['sysname']] == 'Windows') {
        pkg_win32_filename <- devtools::build(pkg_dir, binary = TRUE)
    } else {
        warning('Cannot build Win32 binary package on non-Windows platform')
    }

    drat::insertPackage(pkg_src_filename, repodir = repo_dir)
    if (Sys.info()[['sysname']] == 'Windows') {
        drat::insertPackage(pkg_win32_filename, repodir = repo_dir)
    }
}

local({
    opt_parser <- OptionParser(option_list = list(
        make_option(c('-R', '--repository'), type = 'character', help = 'repository directory')
    ))
    args <- parse_args(opt_parser, positional_arguments = 1)

    if (is.null(args$options$repository))
        stop('Missing repository directory');

    src_dir <<- args$args[1]
    repo_dir <<- args$options$repository
})

pkg_dir <- bundle_drdR(src_dir, str_interp('${repo_dir}/tmp/drdR'))
run_roxygen2(pkg_dir)
build_package(pkg_dir, repo_dir)
