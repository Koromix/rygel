# Ordre des pages

## Affichage conditionnel de pages

Variable `enabled`

```js
// ...
app.form("tab_general", "Génèralités", () => {
    // ...
    app.form("app_optimap_gravity", "Optimap - IGS2", null, {
        enabled: meta => meta.map.app_demography.values.age >= 45,
        load: ['app_demography'] // il faut charger app_demography!
    })
    // ...
})
// ...
```

Affiche la page `app_optimap_gravity` si la variable `age` de la page `app_demography` est >= 45.

Ou

```js
app.form("tab_general", "Génèralités", () => {
    // ...
    app.form("app_optimap_gravity", "Optimap - IGS2", null, {
        enabled: meta => {
            return meta.map.pharmacology 
                && meta.map.pharmacology.values.dobutamin == 1
        }
    })
    // ...
})
```

## Utilisation séquentielle du formulaire : Suivant et Précédent

Il faut utiliser ```sequence``` dans les options.

### Option 1 : séquence unique

```js
app.pushOptions({ sequence: true })
```

Remarque : ```app.pushOptions``` permet d'envoyer des options à toutes les pages sans avoir à le répéter plusieurs fois.

L'ensemble du formulaire va pouvoir s'enchainer avec suivant et précédent.

### Option 2 : séquences multiples

2 syntaxes :

- `sequence : 'foo'`.
- `sequence : ['pagea', 'pageb']`.

```js
form.text('a', 'A', { sequence: 'foo' })
form.text('b', 'B', { sequence: 'bar' })
form.text('c', 'C', { sequence: 'foo' })
form.text('d', 'D', { sequence: 'bar' })
form.text('e', 'E', { sequence: 'foo' })
form.text('f', 'F', { sequence: 'bar' })
```

Est identique à :

```js
form.text('a', 'A', { sequence: ['a', 'c', 'e'] })
form.text('b', 'B', { sequence: ['b', 'd', 'f'] })
form.text('c', 'C', { sequence: ['a', 'c', 'e'] })
form.text('d', 'D', { sequence: ['b', 'd', 'f'] })
form.text('e', 'E', { sequence: ['a', 'c', 'e'] })
form.text('f', 'F', { sequence: ['b', 'd', 'f'] })

```

La page A puis C puis E sont jouées à la suite.
La page B, puis D puis F sont jouées à la suite.

POur le deuxième exemple ; les séquences doivent être identiques.
