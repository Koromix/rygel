# Erreurs et contrôles

Les détection d'erreurs est utile à deux fins :

- Empêcher ou **limiter le risque d'erreur de remplissage**, en repérant les valeurs anormales et en les signalant à l'utilisateur qui peut ainsi les corriger avant d'enregistrer ses données
- Effectuer des **contrôles de saisie et de cohérence** a posteriori en repérant les valeurs anormales ou suspectes en fonction de règles prédéfinies

## Erreurs bloquantes et non bloquantes

Goupile propose un système d'erreurs pour répondre à ces besoins, on distingue deux types d'erreurs :

- Les **erreurs bloquantes** empêchent l'utilisateur d'enregistrer son formulaire, qui doit soit les corriger soit [annoter la variable](#annotations-de-saisie) pour neutraliser le blocage
- Les **erreurs non bloquantes** ne gênent pas l'enregistrement, elles sont enregistrées en base et les enregistrements en erreur sont signalés dans le [tableau de suivi](data#tableau-de-suivi)

> [!NOTE]
> Une erreur bloquante peut être contournée en [annotant la variable](#annotation-des-variables).
>
> Vous pouvez désactiver l'annotation d'une variable avec l'option `annotate: false`, et obliger l'utilisateur à saisir une valeur correcte pour valider l'enregistrement.

## Générer une erreur

### Saisie obligatoire

La [saisie obligatoire](form#saisie-obligatoire) (`mandatory: true`) est disponible avec tous les widgets. Une variable manquante génère une erreur avec le tag « Incomplet », qui est bloquante par défaut.

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

# Annotation des variables

> [!NOTE]
> Le système d'annotation n'apparait pas par défaut dans les projets créés avec une version plus ancienne de Goupile.
>
> Modifiez le script de projet pour activer cette fonctionnalité :
>
> ```js
> app.annotate = true
>
> app.form("projet", "Titre", () => {
>     // ...
> })
> ```

Chaque variable peut être annotée avec un statut, un commentaire libre, et verrouillée si besoin (uniquement par les utilisateurs avec le droit d'audit). Cliquez sur le petit stylet 🖊 à côté de la variable pour l'annoter.

<div class="screenshot"><img src="{{ ASSET static/help/data/annotate1.webp }}" height="280" alt=""/></div>

Préciser le statut de la variable **permet de ne pas répondre** même lorsque la question est obligatoire. Les statuts disponibles sont les suivants :

- *En attente* : ce statut est utilisé lorsque l'information n'est pas encore disponible (par exemple, un résultat de biologie médicale)
- *À vérifier* : ce statut indique que la valeur renseignée n'est pas certaine, et devrait être vérifiée
- *Ne souhaite par répondre (NSP)* : ce statut indique un refus de répondre, même si la question est obligatoire
- *Non applicable (NA)* : la question n'est pas applicable ou pas pertinente
- *Information non disponible (ND)* : l'information nécessaire n'est pas connue (par exemple, erreur ou oubli de mesure)

Les *statuts NSP, NA et ND ne sont pas disponibles* dès l'instant où une valeur est renseignée.

<div class="screenshot">
    <img src="{{ ASSET static/help/data/annotate2.webp }}" height="200" alt=""/>
    <img src="{{ ASSET static/help/data/annotate3.webp }}" height="200" alt=""/>
</div>

Vous pouvez également ajouter un commentaire libre en annotation, qui peut servir au suivi du remplissage.

Les utilisateurs avec le droit d'audit (DataAudit) peuvent verrouiller la valeur, qui ne pourra alors plus être modifiée à moins d'être déverrouillée.

> [!TIP]
> Consultez la documentation sur le [tableau de suivi](data#filtres-d-affichage) pour repérer et filtrer les enregistrements annotés.
