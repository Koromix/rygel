# Répétitions

## Pages se répétant x fois

Fonction `app.many`

```js
// ...
app.many("tab_evenements", "Evenements", () => {
    app.form("app_vital_signal", "Signes vitaux")
    app.form("app_therapeutic_intensity", "Intensité thérapeutique")
    app.form("app_pharmacology", "Pharmacologie")
    app.form("app_biology", "Biologie")
    app.form("app_events", "Evènements")
})
// ...
```

L'ensemble des pages listés seront répétées autant de fois que nécessaires. Elles sont liées entre elles.

## Sections répétées dans une page

*XXX*
