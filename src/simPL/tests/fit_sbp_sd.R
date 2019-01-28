library(data.table)

sd <- 15
data <- rbind(
    data.table(age_cl = '18-34', sbp = rnorm(500, 123.4, sd)),
    data.table(age_cl = '35-44', sbp = rnorm(500, 123.4, sd)),
    data.table(age_cl = '45-54', sbp = rnorm(500, 132.5, sd)),
    data.table(age_cl = '55-64', sbp = rnorm(500, 137.9, sd)),
    data.table(age_cl = '65-74', sbp = rnorm(500, 143.9, sd))
)

data$sbp_cl <- cut(data$sbp, breaks = c(0, 120, 130, 140, 160, 180, +Inf),
                   include.lowest = TRUE, right = FALSE)
round(prop.table(table(data$age_cl, data$sbp_cl), margin = 1) * 100, 1)
