# Dates et heures

## Dates locales

### Widget form.date

```js
form.date("biology_date", "Date du prélèvement le plus proche de l'épreuve", {
    value: dates.today()
})
```

```js
form.date("biology_date", "Date du prélèvement le plus proche de l'épreuve", {
    value: "20/01/2022"
})
```

### Méthodes

```js
clone()
isValid()
isComplete()
equals()
toJulianDays()
toJSDate()
getWeekDay()
diff()
plus()
minus()
plusMonths()
minusMonths()
```

## Heures locales

### Widget form.time

```js
form.sameLine(); form.time("biology_time", "",{
    value: times.now() 
})
```

### Méthodes

*XXX*
