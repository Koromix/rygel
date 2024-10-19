>>> [!IMPORTANT]
>>> La documentation est en cours de rédaction et d'amélioration, certaines sections sont incomplètes.

# Ordre des pages

## Affichage conditionnel des pages

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

# Apparence et mise en page

## Disposition des widgets

*Rédaction en cours*

## Style visuel (CSS)

### Portée globale ou page actuelle

Si le CSS est mis dans l'onglet application, alors il s'applique à toutes les pages.

```js
app.head(html`
    <style>
        .fm_legend { background: red; }
    </style>
`)
```

En revanche, il ne s'applique qu'à la page en cours de cette manière :

```js
form.output(html`
    <style>
        .fm_legend { background: green; }
    </style>
`)
```

### Couleur des sections de la page

```js
form.output(html`
    <style>
        .fm_legend { background: green; }
    </style>
`)
```

### Couleur d'une section

```js
form.section("Amines", () => {
    form.binary("key", "traitement")
}, { color: "red" })
```

### Couleur des boutons

```js
form.output(html`
    <style>
        body {
            --anchor_color: #00ff00;
            --anchor_color_n1: #00ee00;
            --anchor_color_n2: #00dd00;
            --anchor_color_n3: #00cc00;

            --page_color: #ff0000;
            --page_color_n1: #0f7ddfb8;
            --page_color_n2: #dd0000;
            --page_color_n3: #cc0000;
        }
        .fm_legend { background: green; }
    </style>
`)
```

# Partage des variables

## Import des variables

Variable `load`

```js
// ...
app.form("tab_general", "Généralités", () => {
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

*Rédaction en cours*

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

*Rédaction en cours*

# Trucs et astuces

## Variables disponibles

### meta

### profile

```js
profile.username
profile.userid
```

### forms

### app

Contient la structure de l'appli : formulaires, pages, etc

## Widgets

### Assignation / lecture

Sur une pages données les variables sont accesibles via `values.ma_variable`. Il est aussi possible de les assigner et de le créer avec `values.new_variable`.

> [!NOTE]
> La syntaxe `form.value("")` est supportée mais n'est plus recommandée.

## Valeurs par défaut

*XXX*

## Liste non exhaustives des options

- **compact** (bool, show label and widget on same line)
- **mandatory** (bool)
- **help** (text)
- **wide** (bool)
- **prefix** (text or function taking value)
- **suffix** (text or function taking value)
- **columns** (number, for radio or checkboxes only)
- **placeholder** (text)
- **disabled** (bool)
- **readonly** (bool)
- **hidden** (bool)
- **decimals** (number)
- **value** (default value)
- **min** (number)
- **max** (number)
- **floating** (bool, for sliders only)
- **ticks** (bool or list, for sliders only)
- **text** (for calc only)
- **anchor** (give specific id, for sections only)
- **deploy** (bool, for sections only)
- **color** (hexa, for sections only)
- **tab** (default tab, for tabs only)
