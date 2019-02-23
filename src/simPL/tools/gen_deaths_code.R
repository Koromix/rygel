#!/usr/bin/env Rscript

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

local({
    prev_warn <- getOption("warn")
    options(warn = -1)

    library(data.table, warn.conflicts = FALSE)
    library(stringr, warn.conflicts = FALSE)
    library(tibble, warn.conflicts = FALSE)
    library(optparse, warn.conflicts = FALSE)

    options(warn = prev_warn)
})

GROUPS <- setDT(tribble(
    ~name, ~codes,
    'CardiacIschemia', 'I20 -I25',
    'LungCancer', 'C32 -C34',
    'OtherCauses', NA
))

load_death_rates <- function(filename, encoding = 'UTF-8') {
    rows <- fread(filename, encoding = encoding)

    rbindlist(lapply(c('M', 'F'), function(sex) {
        sex_rows <- rows[rows$Sexe == sex,]

        rbindlist(lapply(1:nrow(GROUPS), function(group_idx) {
            group <- GROUPS[group_idx,]

            if (!is.na(group$codes)) {
                group_rows <- sex_rows[sex_rows$`Code CIM` == group$codes,]
            } else {
                group_rows <- sex_rows[!(sex_rows$`Code CIM` %in% GROUPS$codes),]
            }

            data.table(
                sex = sex,
                group = group$name,
                rate_45_54 = sum(group_rows$`45-54`),
                rate_55_64 = sum(group_rows$`55-64`),
                rate_65_74 = sum(group_rows$`65-74`),
                rate_75_84 = sum(group_rows$`75-84`),
                rate_85_94 = sum(group_rows$`85-94`),
                rate_95_ = sum(group_rows$`95+`)
            )
        }))
    }))
}

output_code <- function(rates) {
    for (group_it in unique(rates$group)) {
        cat(str_interp('
    if (flags & (1 << (int)DeathType::${group_it})) {
        switch (sex) {'))

        for (sex_it in unique(rates$sex)) {
            sex_str <- ifelse(sex_it == 'M', 'Male', 'Female')
            row <- rates[rates$sex == sex_it & rates$group == group_it,]

            cat(str_interp('
            case Sex::${sex_str}: {
                if (age < 45) probability += NAN;
                else if (age < 55) probability += ${format(row$rate_45_54, nsmall = 1)} / 100000.0;
                else if (age < 65) probability += ${format(row$rate_55_64, nsmall = 1)} / 100000.0;
                else if (age < 75) probability += ${format(row$rate_65_74, nsmall = 1)} / 100000.0;
                else if (age < 85) probability += ${format(row$rate_75_84, nsmall = 1)} / 100000.0;
                else if (age < 95) probability += ${format(row$rate_85_94, nsmall = 1)} / 100000.0;
                else probability += ${format(row$rate_95_, nsmall = 1)} / 100000.0;
            } break;'
            ))
        }

        cat('
        }
    }
')
    }
}

opt <- parse_args(
    OptionParser(
        usage = '%prog file',
        option_list = list()
    ),
    positional_arguments = 1
)
filename <- opt$args[1]

death_rates <- load_death_rates(filename)
output_code(death_rates)
