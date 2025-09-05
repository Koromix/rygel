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

library(optparse)

PLATFORMS <- c('linux', 'macos', 'windows', 'freebsd', 'openbsd')

extract_description <- function(filename) {
    name <- basename(filename)

    prefix <- sub('^([a-zA-Z0-9\\-]+).*', '\\1', name)
    member <- paste0(prefix, '/DESCRIPTION')

    if (endsWith(name, '.tgz') || grepl('\\.tar(\\.[a-z]+)?$', name)) {
        cmd <- paste0('tar -x --to-stdout -f "', filename, '" ', member)
        description <- system(cmd, intern = TRUE)
    } else if (endsWith(name, '.zip')) {
        cmd <- paste0('unzip -p "', filename, '" ', member)
        description <- readLines(unz(filename, member))
    } else {
         stop('Unknown package format')
    }

    return (description)
}

analyze_description <- function(description) {
    info <- list(
        name = NULL,
        binary = FALSE,
        platform = NULL,
        version = NULL
    )

    info$name <- sub('^Package: ', '', description[grepl('^Package: ', description)])
    built <- description[grepl('^Built: ', description)]

    if (length(built) > 0) {
        info$binary <- TRUE

        for (platform in PLATFORMS) {
            if (grepl(platform, built))
                info$platform <- platform
        }
        if (is.null(info$platform))
            stop('Unknown binary platform')

        info$version <- sub('^.*R ([0-9]+\\.[0-9]+).*$', '\\1', built)
    }

    return (info)
}

import_package <- function(filename, repo) {
    description <- extract_description(filename)
    info <- analyze_description(description)

    if (info$binary) {
        dest_dir <- paste0(repo, '/bin/', info$platform, '/contrib/', info$version)
        dest_filename <- paste0(dest_dir, '/', basename(filename))

        dir.create(dest_dir, recursive = TRUE, showWarnings = FALSE)
        file.copy(filename, dest_filename, overwrite = TRUE)
    } else {
        dest_dir <- paste0(repo, '/src/contrib')
        dest_filename <- paste0(dest_dir, '/', basename(filename))

        dir.create(dest_dir, recursive = TRUE, showWarnings = FALSE)
        file.copy(filename, dest_filename, overwrite = TRUE)
    }
}

local({
    parser <- OptionParser(option_list = list(
        make_option(c('--import'), type = 'character', help = 'import packages from directory')
    ))
    opt <- parse_args(parser, positional_arguments = 1)

    repo_dir <- opt$args[1]

    if (!is.null(opt$options$import)) {
        pkgs <- c(
            list.files(opt$options$import, pattern = '*.tgz', full.names = TRUE),
            list.files(opt$options$import, pattern = '*.tar.gz', full.names = TRUE),
            list.files(opt$options$import, pattern = '*.zip', full.names = TRUE)
        )

        for (pkg in pkgs) {
            import_package(pkg, repo_dir)
        }
    }

    directories <- c(
        paste0(repo_dir, '/src/contrib'),

        unlist(sapply(PLATFORMS, function(platform) {
            path <- paste0(repo_dir, '/bin/', platform, '/contrib')

            if (file.exists(path)) {
                dirs <- list.files(path, full.names = TRUE)
            } else {
                dirs <- c()
            }

            return (dirs)
        }, USE.NAMES = FALSE))
    )

    for (directory in directories) {
        if (file.exists(directory))
            tools::write_PACKAGES(directory, verbose = TRUE)
    }
})
