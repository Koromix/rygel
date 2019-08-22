#!/usr/bin/env Rscript

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

library(stringr)
library(devtools)
library(drat)
library(roxygen2)
library(parallel)
library(optparse)

bundle_heimdallR <- function(project_dir, build_dir) {
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

        version <- gsub('-', '.', Sys.Date(), fixed = TRUE)
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
    opt_parser <- OptionParser(option_list = list(
        make_option(c('-R', '--repository'), type = 'character', help = 'repository directory')
    ))
    args <- parse_args(opt_parser, positional_arguments = 1)

    if (is.null(args$options$repository))
        stop('Missing repository directory');

    src_dir <<- args$args[1]
    repo_dir <<- args$options$repository
})

# Use all cores to build package
local({
    cores <- detectCores()
    flags <- Sys.getenv('MAKEFLAGS', unset = '')

    Sys.setenv(MAKEFLAGS = str_interp('-j${cores + 1} ${flags}'))
})

# Bundle, build and register the package
pkg_dir <- bundle_heimdallR(src_dir, str_interp('${repo_dir}/tmp/heimdallR'))
run_roxygen2(pkg_dir)
build_package(pkg_dir, repo_dir)
