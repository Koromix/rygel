#!/usr/bin/env Rscript

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

bundle_heimdallR <- function(project_dir, version, build_dir) {
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
            copy_files(str_interp('src/heimdall/heimdallR/src/${name}'), 'src')

            filename <- str_interp('${build_dir}/src/${name}')
            lines <- readLines(filename)

            lines <- sub('.o: ../', '.o: heimdall/heimdallR/', lines)
            lines <- lines[!grepl('../R', lines, fixed = TRUE)]

            writeLines(lines, filename)
        }
    })

    copy_files(c('src/heimdall/heimdallR/DESCRIPTION',
                 'src/heimdall/heimdallR/NAMESPACE',
                 'src/heimdall/heimdallR/heimdallR.Rproj'), '.')
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

    copy_files('src/heimdall/heimdallR/src/dummy.cc', 'src')
    copy_files('src/heimdall/heimdallR/heimdallR.cc', 'src/heimdall/heimdallR')
    copy_files('src/heimdall/heimdallR/heimdallR.R', 'R')
    copy_files(list_files('src/heimdall/libheimdall'), 'src/heimdall/libheimdall')
    copy_files(list_files('src/libcc'), 'src/libcc')
    copy_files(list_files('src/libgui'), 'src/libgui')
    copy_files(c('src/wrappers/Rcc.cc',
                 'src/wrappers/Rcc.hh',
                 'src/wrappers/opengl.cc',
                 'src/wrappers/opengl.hh',
                 'src/wrappers/opengl_func.inc'), 'src/wrappers')
    copy_files(list_files('vendor/miniz'), 'vendor/miniz')
    copy_files(list_files('vendor/imgui'), 'vendor/imgui')
    copy_files(list_files('vendor/glfw'), 'vendor/glfw')
    copy_files(list_files('vendor/glfw/src'), 'vendor/glfw/src')
    copy_files(list_files('vendor/glfw/include/GLFW'), 'vendor/glfw/include/GLFW')
    copy_files(list_files('vendor/tkspline'), 'vendor/tkspline')
    copy_files(list_files('vendor/tkspline/src'), 'vendor/tkspline/src')

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
    opt_parser <- OptionParser(option_list = list())
    args <- parse_args(opt_parser, positional_arguments = 1)

    src_dir <<- rprojroot::find_root('FelixBuild.ini')
    repo_dir <<- args$args[1]
})

# Use all cores to build package
local({
    cores <- detectCores()
    flags <- Sys.getenv('MAKEFLAGS', unset = '')

    Sys.setenv(MAKEFLAGS = str_interp('-j${cores + 1} ${flags}'))
})

# Package version (commit date)
version <- trimws(system(str_interp('git -C "${src_dir}" log -n1 --pretty=format:%ad --date=format:%Y.%m.%d'),
                         show.output.on.console = FALSE, intern = TRUE))

# Bundle, build and register the package
pkg_dir <- bundle_heimdallR(src_dir, version, str_interp('${repo_dir}/tmp/heimdallR'))
run_roxygen2(pkg_dir)
build_package(pkg_dir, repo_dir)
