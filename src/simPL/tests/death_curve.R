library(data.table)

data <- fread('results.csv')
deads <- data[data$dead == 1,]

hist(deads$age[deads$sex == 0 & deads$age < 200], main = 'Hommes')
hist(deads$age[deads$sex == 1 & deads$age < 200], main = 'Femmes')
