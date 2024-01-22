# Partage des variables

## Import des variables

Variable `load`

```js
// ...
app.form("tab_general", "Génèralités", () => {
    // ...
    app.form("app_optimap_gravity", "Optimap - IGS2", null, {
        load: ['app_inclusion'] 
    })
    // ...
})
//...
```

Permet de charger les variables de la page `app_inclusion` dans `app_optimap_gravity`

## Utilisation dans la page

La variable ce trouve dans `forms.app_demography`

Exemple:

```js
form.number("gravity_igsii_age_value", "Âge", {
    min: 0,
    max: 120,
    mandatory: true,
    suffix: value => value > 1 ? "ans" : "an",
    value: forms.app_demography?.values?.demography_age,
    readonly: true
})
```
