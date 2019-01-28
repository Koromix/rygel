library(data.table)

sd <- 0.4
data <- rbind(
    data.table(age_cl = '18-34', sbp = rnorm(500, 1.16, sd)),
    data.table(age_cl = '35-44', sbp = rnorm(500, 1.34, sd)),
    data.table(age_cl = '45-54', sbp = rnorm(500, 1.46, sd)),
    data.table(age_cl = '55-64', sbp = rnorm(500, 1.34, sd)),
    data.table(age_cl = '65-74', sbp = rnorm(500, 1.28, sd))
)

data$sbp_cl <- cut(data$sbp, breaks = c(0, 1, 1.6, 1.9, +Inf),
                   include.lowest = TRUE, right = FALSE)
round(prop.table(table(data$age_cl, data$sbp_cl), margin = 1) * 100, 1)
