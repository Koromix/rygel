library(data.table)

data1 <- fread(cmd = 'simpl -H 20000 -S 2708 -f EnablePL')
data2 <- fread(cmd = 'simpl -H 20000 -S 2708')

deads1 <- data1[data1$dead == 1,]

hist(deads1$age[deads1$sex == 0 & deads1$age < 200], main = 'Hommes')
hist(deads1$age[deads1$sex == 1 & deads1$age < 200], main = 'Femmes')

print(sum(data1$utility) - sum(data2$utility))
