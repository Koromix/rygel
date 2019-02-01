library(data.table)
library(stringr)

opt_humans <- 20000
opt_seed <- 2708

data1 <- fread(cmd = str_interp('simpl -H ${opt_humans} -S ${opt_seed} -f EnablePL'))
data2 <- fread(cmd = str_interp('simpl -H ${opt_humans} -S ${opt_seed}'))

deads1 <- data1[data1$dead == 1,]

hist(deads1$age[deads1$sex == 0 & deads1$age < 200], main = 'Décès Hommes')
hist(deads1$age[deads1$sex == 1 & deads1$age < 200], main = 'Décès Femmes')

print(list(
    utility_diff = sum(data1$utility) - sum(data2$utility),
    cost_diff = sum(data1$cost) - sum(data2$cost),
    cost_utility_ratio = (sum(data1$cost) - sum(data2$cost)) /
                         (sum(data1$utility) - sum(data2$utility))
))
