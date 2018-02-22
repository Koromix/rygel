# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

library(data.table)
library(stringr)
library(jsonlite)
library(reshape2)

local({
    convert_price <- function(price) {
        return (sapply(str_split(price, ','), function(parts) {
            if (length(parts) == 2) {
                dec <- paste0(parts[1], str_pad(parts[2], 2, 'right', '0'))
            } else {
                dec <- paste0(parts[1], '00')
            }
            return (as.integer(dec))
        }))
    }
    filename_to_sector <- function(filename) {
        sector <- switch(str_split(basename(filename), '[_.]')[[1]][2],
                         pub = 'public',
                         pri = 'private')
        return (sector)
    }

    temp_dir <- tempdir()
    zip_files <- list.files('../data/prices/', pattern = '*.zip', full.names = TRUE)

    lists <- lapply(zip_files, function(zip_filename) {
        unzip(zip_filename, exdir = temp_dir, setTimes = TRUE)
        csv_files <- list.files(temp_dir, pattern = '*.csv', full.names = TRUE)

        ghs_prices <- rbindlist(lapply(csv_files[startsWith(basename(csv_files), 'ghs_')],
                                       function(csv_filename) {
            sector <- filename_to_sector(csv_filename)
            if (is.null(sector))
                return (NULL)

            prices <- fread(csv_filename)
            colnames(prices) <- sub('[^a-zA-Z0-9]', '_', tolower(colnames(prices)))
            if ('dat_effet' %in% colnames(prices)) {
                setnames(prices, 'dat_effet', 'date_effet')
            }

            prices <- data.table(
                build_date = as.Date(file.mtime(csv_filename)),
                date = max(as.Date(prices$date_effet, format = '%d/%m/%Y')),
                ghs = prices$ghs_nro,
                sector = sector,

                price_cents = convert_price(prices$ghs_pri),
                exh_treshold = ifelse(prices$seu_hau > 0, prices$seu_hau + 1, 0),
                exh_cents = convert_price(prices$exh_pri),
                exb_treshold = prices$seu_bas,
                exb_cents = ifelse(convert_price(prices$exb_forfait) > 0, convert_price(prices$exb_forfait),
                                                                          convert_price(prices$exb_journalier)),
                exb_once = (convert_price(prices$exb_forfait) > 0)
            )

            return (prices)
        }), fill = TRUE)

        supplement_prices <- rbindlist(lapply(csv_files[startsWith(basename(csv_files), 'sup_')],
                                              function(csv_filename) {
            sector <- filename_to_sector(csv_filename)
            if (is.null(sector))
                return (NULL)

            prices <- fread(csv_filename)
            colnames(prices) <- sub('[^a-zA-Z0-9]', '_', tolower(colnames(prices)))

            prices <- data.table(
                build_date = as.Date(file.mtime(csv_filename)),
                date = max(ghs_prices$date),
                sector = sector,

                type = paste0(prices$code, '_cents'),
                price_cents = convert_price(prices$tarif)
            )

            return (prices)
        }), fill = TRUE)
        supplement_prices <- dcast(supplement_prices, build_date + date + sector ~ type,
                                   value.var = 'price_cents')
        colnames(supplement_prices) <- tolower(colnames(supplement_prices))

        unlink(csv_files)

        return (list(ghs_prices = ghs_prices,
                     supplement_prices = supplement_prices))
    })

    ghs_prices <<- rbindlist(lapply(lists, function(l) l$ghs_prices), fill = TRUE)
    supplement_prices <<- rbindlist(lapply(lists, function(l) l$supplement_prices), fill = TRUE)
})

separate_price_lists <- function(ghs_prices, supplement_prices) {
    return (lapply(sort(unique(ghs_prices$date)),
                   function(l_date) {
        make_ghs_price_list <- function(g) {
            if (!nrow(g))
                return (NULL)
            l <- list(
                price_cents = g$price_cents,
                exh_treshold = g$exh_treshold,
                exh_cents = g$exh_cents,
                exb_treshold = g$exb_treshold,
                exb_cents = g$exb_cents,
                exb_once = g$exb_once
            )
            if (!l$exh_treshold) {
                l$exh_treshold <- NULL
                l$exh_cents <- NULL
            }
            if (!l$exb_treshold) {
                l$exb_treshold <- NULL
                l$exb_cents <- NULL
            }
            if (!l$exb_once) {
                l$exb_once <- NULL
            }
            return (l)
        }
        make_supplement_price_list <- function(s) {
            s <- s[, endsWith(colnames(s), '_cents'), with = FALSE]
            s <- as.list(s)
            return (s[!sapply(s, is.na)])
        }

        ghs_prices_l <- list()
        ghs_prices_dt <- (ghs_prices[date == l_date][, .(
            public = .(make_ghs_price_list(.SD[sector == 'public'])),
            private = .(make_ghs_price_list(.SD[sector == 'private']))
        ), by = 'ghs'])
        for (i in 1:nrow(ghs_prices_dt)) {
            ghs_prices_l[[i]] <- list(ghs = ghs_prices_dt$ghs[i])
            if (!is.null(ghs_prices_dt$public[[i]])) {
                ghs_prices_l[[i]]$public <- ghs_prices_dt$public[[i]]
            }
            if (!is.null(ghs_prices_dt$private[[i]])) {
                ghs_prices_l[[i]]$private <- ghs_prices_dt$private[[i]]
            }
        }

        supplement_prices_l <- list(
            public = make_supplement_price_list(supplement_prices[date == l_date][sector == 'public']),
            private = make_supplement_price_list(supplement_prices[date == l_date][sector == 'private'])
        )

        return (list(
            build_date = first(ghs_prices[date == l_date]$build_date),
            date = l_date,
            ghs = ghs_prices_l,
            supplements = supplement_prices_l
        ))
    }))
}

writeLines(toJSON(separate_price_lists(ghs_prices, supplement_prices),
                  auto_unbox = TRUE, pretty = 4))
