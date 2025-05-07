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

library(rprojroot)
library(stringr)
library(devtools)
library(drat)
library(roxygen2)
library(parallel)
library(optparse)

bundle_drdR <- function(project_dir, version, build_dir) {
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

            lines <- sub('.o: ../', '.o: drd/drdR/', lines)
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

    copy_files('src/drd/drdR/src/dummy.cc', 'src')
    copy_files('src/drd/drdR/drdR_mco.cc', 'src/drd/drdR')
    copy_files(c('src/drd/drdR/drdR.R',
                 'src/drd/drdR/drdR_mco.R'), 'R')
    copy_files(list_files('src/drd/libdrd'), 'src/drd/libdrd')
    copy_files(list_files('src/core/base'), 'src/core/base')
    copy_files(c('src/core/wrap/Rcc.cc',
                 'src/core/wrap/Rcc.hh'), 'src/core/wrap')
    copy_files(list_files('vendor/miniz'), 'vendor/miniz')
    copy_files('vendor/dragonbox/include/dragonbox/dragonbox.h', 'vendor/dragonbox/include/dragonbox')

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

# Parse arguments
local({
    opt_parser <- OptionParser(option_list = list(
        make_option(c('-D', '--destination'), type = 'character', help = 'destination directory')
    ))
    args <- parse_args(opt_parser, positional_arguments = FALSE)

    src_dir <<- rprojroot::find_root('FelixBuild.ini')
    if (!is.null(args$options$destination)) {
        repo_dir <<- args$options$destination
    } else {
        repo_dir <<- str_interp('${src_dir}/bin/R')
    }
})

# Put Rtools40 in PATH (Windows)
local({
    if (.Platform$OS.type == 'windows') {
        path <- Sys.getenv('PATH')
        rtools_home <- Sys.getenv('RTOOLS40_HOME', unset = NA)

        if (is.na(rtools_home)) {
            stop('Environment variable RTOOLS40_HOME is not set')
        }

        Sys.setenv(PATH = str_interp('${rtools_home}\\usr\\bin;${path}'))
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
    pkg_dir <- bundle_drdR(src_dir, version, str_interp('${repo_dir}/tmp/drdR'))
    run_roxygen2(pkg_dir)
    build_package(pkg_dir, repo_dir)
})
