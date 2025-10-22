# Identifiants des enregistrements

## TID et séquence

Chaque enregistrement dans Goupile dispose de deux identifiants uniques :

- Un **identifiant TID**, qui est une chaîne de 26 caractères encodant le moment de l'enregistrement et une partie aléatoire (ex : *01K5EY3SCEM1D1NBABXXDZW7XP*).
- Un **identifiant de séquence**, qui est un nombre incrémenté à chaque enregistrement, en commençant par 1.

Utilisez le TID pour faire la jointure entre les différentes tables dans les données exportées. La valeur du TID est disponible dans la colonne `__tid` de chaque table.

## Summary

Pour chaque page du formulaire, il est possible de définir un identifiant supplémentaire dit *summary*, qui sera affiché à la place de la date dans le tableau récapitulatif des enregistements (voir capture ci-dessous).

Pour cela, assignez une valeur à `meta.summary` dans le script de formulaire. Dans l'exemple ci-dessous, la valeur affichée dans la colonne « Introduction » est construite en fonction de l'âge indiqué dans la variable correspondante :

```js
form.number("*age", "Âge", {
    min: 0,
    max: 120
})

if (values.age != null)
    meta.summary = values.age + (values.age > 1 ? ' ans' : ' an')
```

<div class="screenshot"><img src="{{ ASSET static/help/dev/summary.webp }}" height="200" alt=""/></div>

Vous pouvez utiliser cette fonctionnalité pour afficher un numéro d'inclusion créé manuellement (par exemple si le numéro d'inclusion est créé en dehors de Goupile), comme le montre l'exemple ci-dessous :

```js
form.number("num_inclusion", "Numero d'inclusion")
meta.summary = values.num_inclusion
```

## Compteurs personnalisés

Il est possible de créer des compteurs personnalisés, qui sont **incrémentés pour chaque enregistrement** (en commençant à 1) en fonction de conditions définies par le code.

La fonction `meta.count(key, secret = false)` est utilisée pour créer un compteur.

L'exemple ci-dessous illustre l'utilisation de cette fonctionnalité pour diviser les patients en 3 groupes. Après chaque enregistrement, l'un des trois compteurs suivant est incrémenté et affecté au participant :

- *tabagisme_actif* : compte le nombre de patients activement fumeur
- *tabagisme_sevre* : compte le nombre de patients ayant arrêté de fumer
- *tabagisme_non* : compte le nombre de patients n'ayant jamais fumé

```js
form.enum("tabagisme", "Tabagisme", [
    ["actif", "Tabagisme actif"],
    ["sevre", "Tabagisme sevré"],
    ["non", "Non fumeur"]
])

if (values.tabagisme)
    meta.count("tabac_" + values.tabagisme)
```

Après inclusion de plusieurs patients, la valeur du compteur de chaque participant est disponible dans l'export, dans l'onglet `@counters` du fichier XLSX. La capture ci-dessous contient 3 participants : le premier est un ancien fumeur, les deux suivants sont des fumeurs actifs :

<div class="screenshot"><img src="{{ ASSET static/help/dev/count.webp }}" height="200" alt=""/></div>

## Randomisation

Le système de randomisation de Goupile est une **extension des compteurs** introduits précédemment.

Utilisez la fonction `meta.randomize(key, max, secret = false)` pour assigner un compteur. Le compteur randomisé n'est pas séquentiel, chaque participant reçoit une valeur aléatoire entre 1 et *max*, par bloc de taille *max*.
Une fois que *max* participants ont été inclus, le compteur est remis à zéro pour les inclusions suivantes.

Par exemple, une valeur maximale de 4 garantit que les 4 premiers participants auront les valeurs 1, 2, 3 et 4 dans un ordre randomisé. Il en sera de même pour les 4 suivants :

```js
meta.randomize("groupe", 4)
```

Comme dans l'exemple sur les compteurs séquentiels, vous pouvez *stratifier la randomisation* en utilisant une clé différente, qui peut dépendre d'une variable présente dans le formulaire.

L'exemple ci-dessous effectue une randomisation stratfiée par le genre, les patients auront une numéro de randomisation qui dépend de leur genre :

- *groupe_H* : randomisation des hommes
- *groupe_F* : randomisation des femmes
- *groupe_NB* : randomisation des individus non-bnaires

```js
form.enum("genre", "Genre", [
    ["H", "Homme"],
    ["F", "Femme"],
    ["NB", "Non-binaire"]
])

if (values.genre)
    meta.count("groupe_" + values.genre)
```

La valeur de chaque compteur randomisé est disponible dans l'onglet `@counters` du fichier d'export. La capture ci-dessous illustre le groupe affecté à 8 inclusions, avec un appel à `meta.randomize("groupe", 4)`.

<div class="screenshot"><img src="{{ ASSET static/help/dev/randomize.webp }}" height="200" alt=""/></div>

Par défaut, les compteurs ne sont pas secrets, c'est à dire qu'ils peuvent être lus depuis une autre page du formuaire, et affichés ou utilisés pour adapter les pages.

Pour définir un compteur secret, qui ne sera disponible que dans le fichier d'export final, mettez le troisième paramètre `secret` de la fonction de randomisation à *true* :

```js
// Randomisation secrète
meta.randomize("groupe", 4, true)
```

# Erreurs et contrôles

Les détection d'erreurs est utile à deux fins :

- Empêcher ou **limiter le risque d'erreur de remplissage**, en repérant les valeurs anormales et en les signalant à l'utilisateur qui peut ainsi les corriger avant d'enregistrer ses données
- Effectuer des **contrôles de saisie et de cohérence** a posteriori en repérant les valeurs anormales ou suspectes en fonction de règles prédéfinies

## Erreurs bloquantes et non bloquantes

Goupile propose un système d'erreurs pour répondre à ces besoins, on distingue deux types d'erreurs :

- Les **erreurs bloquantes** empêchent l'utilisateur d'enregistrer son formulaire, qui doit soit les corriger soit [annoter la variable](#annotations-de-saisie) pour neutraliser le blocage
- Les **erreurs non bloquantes** ne gênent pas l'enregistrement, elles sont enregistrées en base et les enregistrements en erreur sont signalés dans le [tableau de suivi](export#tableau-de-suivi)

> [!NOTE]
> Une erreur bloquante peut être contournée en [annotant la variable](export#annotation-des-variables).
>
> Vous pouvez désactiver l'annotation d'une variable avec l'option `annotate: false`, et obliger l'utilisateur à saisir une valeur correcte pour valider l'enregistrement.

## Générer une erreur

### Saisie obligatoire

La [saisie obligatoire](form#saisie-obligatoire) (`mandatory: true`) est disponible avec tous les widgets. Une variable manquante génère une erreur avec le tag "Incomplet", qui est bloquante par défaut.

Pour générer cette erreur sans bloquer l'enregistrement vous pouvez définir l'option `block: false` sur le widget :

```js
form.number("age", "Âge", {
    mandatory: true, // Saisie obligatoire, génère un tag "Incomplet" si manquant
    block: false     // Mais ne bloque pas l'enregistrement sans nécessiter une annotation
})
```

### Erreurs intégrées

Plusieurs widgets intégrés à Goupile sont capables de générer une erreur à partir d'options simples. C'est par exemple le cas du widget numérique (`form.number`) qui peut signaler une erreur lorsque la valeur est trop petite (supérieure option `min`) ou trop grande (supérieure à l'option `max`).

```js
form.number("age", "Âge", {
    min: 18,
    max: 120
})
```

<div class="screenshot"><img src="{{ ASSET static/help/dev/number.webp }}" height="100" alt=""/></div>

Ces erreurs sont bloquantes par défaut, mais vous pouvez les rendre non bloquantes en définissant l'option `block: false`.

```js
form.number("age", "Âge", {
    min: 18,
    max: 120,
    block: false
})
```

### Erreurs personnalisées

N'oubliez pas que le formulaire est scripté en JavaScript. En combinant cela avec la fonction `form.error(key, message, options)`, vous pouvez générer des erreurs de toute nature :

- Le *paramètre key* désigne la variable concernée par l'erreur
- Le *paramètre message* correspond au message d'erreur à afficher
- Les *options* indiquent si l'erreur est bloquante ou non bloquante, et immédiate ou bien différée

Les différentes combinaisons possibles sont récapitulées dans le tableau ci-dessous :

| Bloquante | Différée | Syntaxe | Description |
| ----- | ----- | ------- | ----------- |
| Oui  | Non | `{ block: true, delay: false }`    | Erreur bloquant l'enregistement et affichée immédiatement (par défaut) |
| Non | Non | `{ block: false, delay: false }`    | Erreur non bloquante, affichée immédiatement |
| Oui  | Oui  | `{ block: true, delay: true }`     | Erreur bloquante, affichée lors de la tentative d'enregistrement |
| Non  | Oui  | `{ block: false, delay: true }`     | Erreur non bloquante, affichée lors de la tentative d'enregistrement |

> [!TIP]
> Il faut juste toujours penser à vérifier que la valeur n'est pas vide (valeur `undefined` ou `null` en JavaScript) avant d'appeler une méthode sur celle-ci.

Voici un premier exempler qui valide la valeur d'un numéro d'inclusion à l'aide d'une [expression régulière](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/String/match), qui doit contenir 2 lettres puis 5 chiffres :

```js
form.text("num_inclusion", "Numéro d'inclusion")

// Les valeurs sont mises à undefined quand elles sont vides.
//
// Il faut donc vérifier qu'il y a une valeur avant d'appeler une méthode sur celle-ci pour
// éviter une erreur JavaScript, c'est ce que nous faisons dans la condition ci-dessous.

if (values.num_inclusion && !values.num_inclusion.match(/^[a-zA-Z]{2}[0-9]{5}$/))
    form.error("num_inclusion", "Format incorrect, saississez 2 lettres puis 5 chiffres")
```

> [!NOTE]
> Pour des raisons visuelles et historiques, chaque erreur **doit être assignée à une variable**, même si l'erreur en concerne plusieurs.
>
> Nous verrons ci-dessous un exemple dans lequel l'erreur dépend de deux variables. Par convention, on assigne souvent l'erreur à la dernière variable concernée.

Dans l'exemple qui suit, la cohérence entre une date d'inclusion et une date de fin d'étude est vérifiée : la date de fin doit être au moins 28 jours après la date d'inclusion.

```js
form.date("date_inclusion", "Date d'inclusion")
form.date("date_fin", "Date de fin")

// D'abord, vérifions que les dates ne sont pas vides !

if (values.date_inclusion && values.date_fin) {
    let inclusion_plus_28j = values.date_inclusion.plus(28)

    if (values.date_fin < inclusion_plus_28j)
        form.error("date_fin", "La date de fin doit se situer au moins 28 jours après la date d'inclusion")
}
```

## Erreurs immédiates et différées

Par défaut, l'erreur est affichée immédiatement pendant la saisie.

Il est possible de faire en sorte que l'erreur ne s'affiche qu'après une tentative d'enregistrement en spécifiant l'option `delay: true` dans les options :

```js
form.text("num_inclusion", "Numéro d'inclusion")

// L'option { delay: true } à la fin de form.error() indique que l'erreur ne s'affiche
// qu'au moment de la validation.

if (values.num_inclusion && !values.num_inclusion.match(/^[0-9]{5}$/))
    form.error("num_inclusion", "Format incorrect (5 chiffres attendus)", { delay: true })
```

# Manipulation des valeurs

## Données d'autres pages

*Rédaction en cours*

## Profil de l'utilisateur

*Rédaction en cours*
