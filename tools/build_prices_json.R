# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

library(data.table)
library(stringr)
library(jsonlite)

ghs_prices <- local({
    temp_dir <- tempdir()

    zip_files <- list.files('../data/prices/', pattern = '*.zip', full.names = TRUE)

    rbindlist(lapply(zip_files, function(zip_filename) {
        unzip(zip_filename, exdir = temp_dir, setTimes = TRUE)
        csv_files <- list.files(temp_dir, pattern = '*.csv', full.names = TRUE)

        ghs_prices <- rbindlist(lapply(csv_files[startsWith(basename(csv_files), 'ghs_')],
                                       function(csv_filename) {
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

            sector <- switch(str_split(basename(csv_filename), '[_.]')[[1]][2],
                             pub = 'public',
                             pri = 'private')
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

        unlink(csv_files)

        return (ghs_prices)
    }), fill = TRUE)
})

separate_price_lists <- function(ghs_prices) {
    return (lapply(sort(unique(ghs_prices$date)),
                   function(l_date) {
        date_prices <- ghs_prices[date == l_date]

        make_price_list <- function(g) {
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

        prices_dt <- (date_prices[, .(
            public = .(make_price_list(.SD[sector == 'public'])),
            private = .(make_price_list(.SD[sector == 'private']))
        ), by = 'ghs'])

        prices <- list()
        for (i in 1:nrow(prices_dt)) {
            prices[[i]] <- list(ghs = prices_dt$ghs[i])
            if (!is.null(prices_dt$public[[i]])) {
                prices[[i]]$public <- prices_dt$public[[i]]
            }
            if (!is.null(prices_dt$private[[i]])) {
                prices[[i]]$private <- prices_dt$private[[i]]
            }
        }

        return (list(
            build_date = first(date_prices$build_date),
            date = l_date,
            ghs = prices
        ))
    }))
}

writeLines(toJSON(separate_price_lists(ghs_prices), auto_unbox = TRUE, pretty = 4))
