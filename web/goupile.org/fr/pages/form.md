# Principaux généraux

## Syntaxe d'un widget

Les widgets sont créés en appelant des fonctions prédéfinies avec la syntaxe suivante :

<p class="call"><span style="color: #24579d;">function</span> ( <span style="color: #c09500;">"nom_variable"</span>, <span style="color: #842d3d;">"Libellé présenté à l'utilisateur"</span> )</p></p>

Les noms de variables doivent commencer par une lettre ou \_, suivie de zéro ou plusieurs caractères alphanumériques ou \_. Ils ne doivent pas contenir d'espaces, de caractères accentués ou tout autre caractère spécial.

La plupart des widgets acceptent des paramètres optionnels de cette manière :

<p class="call"><span style="color: #24579d;">function</span> ( <span style="color: #c09500;">"nom_variable"</span>, <span style="color: #842d3d;">"Libellé présenté à l'utilisateur"</span>, {
    <span style="color: #054e2d;">mandatory</span> : <span style="color: #431180;">true</span>,
    <span style="color: #054e2d;">help</span> : <span style="color: #431180;">"Petit texte d'aide affiché sous le widget"</span>
} )</p>

Certains widgets prennent une liste de choix, qui doit être spécifié après le libellé. C'est le cas des widgets à choix unique (`form.enumButtons`, `form.enumRadio`, `form.enumDrop`) et pour les widgets à choix multiples (`form.multiCheck`, `form.multiButtons`) d'énumération.

Chaque choix doit préciser la valeur codée (qui sera la valeur présente dans l'export) et le libellé présenté à l'utilisateur :

<p class="call"><span style="color: #24579d;">function</span> ( <span style="color: #c09500;">"nom_variable"</span>, <span style="color: #842d3d;">"Libellé présenté à l'utilisateur"</span>, [
    [<span style="color: #054e2d;">"male"</span>, <span style="color: #431180;">"Homme"</span>],
    [<span style="color: #054e2d;">"female"</span>, <span style="color: #431180;">"Femme"</span>],
    [<span style="color: #054e2d;">"other"</span>, <span style="color: #431180;">"Autre"</span>]
] )</p>

> [!WARNING]
> Attention à la syntaxe du code. Lorsque les parenthèses ou les guillemets ne correspondent pas, une erreur se produit et la page affichée <b>ne peut pas être mise à jour tant que l'erreur persiste</b>.

# Champs de saisie

## Texte libre

Utilisez le widget `form.text` pour créer un champ de saisie libre, avec une seule ligne de saisie (par exemple pour récuéprer une adresse e-mail, ou un nom).

```js
form.text("pseudo", "Pseudonyme")
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/text.webp }}" height="80" alt=""/></div>

Le widget `form.textArea` permet la saisie d'un texte de plusieurs lignes. Vous pouvez modifier la taille par défauts à l'aide des options suivantes :

- `rows` : nombre de lignes
- `cols` : nombre de colonnes
- `wide` : true pour prendre la largeur de la page

```js
form.textArea("desc", "Description", { wide: false })
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/textArea.webp }}" height="120" alt=""/></div>

## Valeur numérique

Le widget `form.number` permet la saisie d'un nombre, sans minimum ou maximum par défaut. Sans option, le widget n'accepte que des nombres entiers.

```js
form.number("age", "Âge", { suffix: value > 1 ? "ans" : "an"})
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/number.webp }}" height="80" alt=""/></div>

> [!TIP]
> Le code ci-dessus illustre l'utilisation de l'option suffixe sous une forme dynamique, afin de définir un préfixe qui s'adapte en fonction de la valeur saisie.

En dehors des [options communes](#options-communes) à tous les widgets, ce widget accepte les options suivantes :

- `min` : valeur minimale autorisée
- `max` : valeur maximale autorisée
- `decimals` : nombre de décimales acceptées

L'exemple suivant illustre plusieurs options possibles, pour un widget correspondant à la taille d'un individu, en définissant le minimum, le maximum, un suffixe et l'utilisation de nombres décimaux.

```js
form.number("taille", "Taille", {
    decimals: 2,
    min: 0,
    max: 3,
    suffix: "m"
})
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/decimals.webp }}" height="80" alt=""/></div>

Pour saisir une valeur numérique bornée, vous pouvez également utilisez le widget `form.slider` qui correspond à une échelle visuelle. Le minimum et le maximum par défaut sont définis respectivement à 1 et 10.

```js
form.slider("sommeil", "Qualité du sommeil", {
    min: 1,
    max: 10
})
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/slider.webp }}" height="80" alt=""/></div>

Il est possible d'ajouter une graduation au slider, qui accepte une option `ticks` pouvant prendre les valeurs suivantes :

- `true` : ajoute une graduation à chaque valeur
- `{ 1: "début", 5.5: "milieu", 10: "fin" }` : ajoute une graduation avec un libellé sur les valeurs indiquées

```js
form.slider("eva", "EVA", {
    min: 1,
    max: 10,
    ticks: { 1: "début", 5.5: "milieu", 9: "fin" }
})
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/ticks.webp }}" height="100" alt=""/></div>

## Date et heure

Utilisez le widget `form.date` pour permettre à l'utilisateur de saisir une date (jour/mois/année). Comme les autres widgets, le champ date accepte une valeur par défaut à l'aide de l'option `value`, qui peut par exemple faire référence à la date du jour comme dans l'exemple ci-dessous.

```js
form.date("date_inclusion", "Date d'inclusion", { value: LocalDate.today() })
```

> [!NOTE]
> La valeur de ce widget (lorsqu'il est rempli) est un objet de type `LocalDate`, qui représente une date locale (sans notion de fuseau horaire), et dispose de plusieurs méthodes dont les suivantes :
>
> - `date.diff(other)` : calcul du nombre de jours entre deux dates
> - `date.plus(days)` : création d'une nouvelle date qui se situe plusieurs jours plus tard
> - `date.minus(days)` : création d'une nouvelle date qui se situe plusieurs jours auparavant
> - `date.plusMonths(months)` : création d'une nouvelle date qui se situe plusieurs mois plus tard
> - `date.minusMonths(months)` : création d'une nouvelle date qui se situe plusieurs mois auparavant

<div class="screenshot"><img src="{{ ASSET static/help/widgets/date.webp }}" height="80" alt=""/></div>

Utilisez le widget `form.time()` pour représenter une heure locale dans la journée, sans notion de fuseau horaire. Par défaut ce widget ne permet de saisir que l'heure et la minute au format *HH:MM*.

```js
form.time("heure_depart", "Heure de départ")
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/time.webp }}" height="80" alt=""/></div>

La valeur est représente sous la forme d'un objet `LocalTime`. Ce champ peut également accepter des secondes à condition de définir l'option `seconds: true`.

## Question à choix unique

Goupile propose 3 widgets pour créer une question contenant plusieurs propositions, et pour laquelle seule un choix peut être sélectionné. Ces widgets diffèrent par leur aspect visuel, et le choix du widget approprié dépend de vos préférences, de la question posée, et du nombre de propositions.

En dehors de l'aspect visuel, ces 3 widgets fonctionnent de manière similaire :

- `form.enumButtons` (anciennement `form.enum`) affiche les propositions sous forme de boutons disposés horizontalement.
- `form.enumRadio` affiche les propositions verticalement, et permet de sélectionner la réponse voulue par une case *radio* 🔘
- `form.enumDrop` affiche un menu déroulant qui contient les différentes propositions

```js
form.enum("tabagisme", "Tabagisme",  [
    ["actif", "Tabagisme actif"],
    ["sevre", "Tabagisme sevré"],
    ["non", "Non fumeur"]
])

form.enumRadio("csp", "Catégorie socio-professionnelle", [
    [1, "Agriculteur exploitant"],
    [2, "Artisan, commerçant ou chef d'entreprise"],
    [3, "Cadre ou profession intellectuelle supérieure"],
    [4, "Profession intermédiaire"],
    [5, "Employé"],
    [6, "Ouvrier"],
    [7, "Retraité"],
    [8, "Autre ou sans activité professionnelle"]
])

form.enumDrop("origine", "Pays d'origine", [
    ["fr", "France"],
    // ...
])
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/enumButtons.webp }}" height="90" alt=""/></div>
<div class="screenshot"><img src="{{ ASSET static/help/widgets/enumRadio.webp }}" height="320" alt=""/></div>
<div class="screenshot"><img src="{{ ASSET static/help/widgets/enumDrop.webp }}" height="90" alt=""/></div>

La liste des choix possibles est spécifiée avec le troisième paramètre de la fonction, sous la forme d'un double tableau JavaScript. Pour chaque proposition, il faut préciser le code (qui sera stocké en base de données, et disponible dans l'export), et le libellé affiché à l'utilisateur.

<p class="call"><span style="color: #24579d;">[</span>
    [<span style="color: #054e2d;">"male"</span>, <span style="color: #431180;">"Homme"</span>],
    [<span style="color: #054e2d;">"female"</span>, <span style="color: #431180;">"Femme"</span>],
    [<span style="color: #054e2d;">"other"</span>, <span style="color: #431180;">"Autre"</span>]
<span style="color: #24579d;">]</span></p>

Le code de chaque proposition peut être une chaîne ou une valeur numérique.

> [!TIP]
> Préférez `enumButtons` quand il y a peu de choix possibles, et que le texte des choix est court (par exemple le genre). Utilisez plutôt `enumRadio` quand les propositions sont plus nombreuses, avec des libellés plus long (par exemple, la catégorie socio-professionnelle). Réservez `enumDrop` et les menus déroulants aux longues listes (par exemple, une liste de pays).

Par défaut, les widgets de choix peuvent être déselectionnés par l'utilisateur. Utilisez l'option `untoggle: false` pour empêcher l'utilisateur de retirer un choix. Combinez `untoggle` et une valeur par défaut avec `value` pour imposer un choix à l'utilisateur, comme dans l'exemple ci-dessous.

```js
form.enumButtons("force", "Réponse non décochable", [
    [1, "Option 1"],
    [2, "Option 2"],
    [3, "Option 3"],
], {
    untoggle: false,
    value: 1
})
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/untoggle.webp }}" height="90" alt=""/></div>

## Question à choix multiples

Goupile propose 2 widgets pour créer une question contenant plusieurs propositions, et pour laquelle plusieurs choix peuvent être sélectionnés. Ces widgets diffèrent par leur aspect visuel, et le choix du widget approprié dépend de vos préférences, de la question posée, et du nombre de propositions :

- `form.multiCheck` affiche les propositions verticalement, avec des cases à cocher
- `form.multiButtons` affiche les propositions horizontalement, sous forme de boutons, visuellement similaires au widget `form.enumButtons` (mais avec la possibilité de sélectionner plusieurs choix)

```js
form.multiCheck("sommeil", "Trouble(s) du sommeil", [
    [1, "Troubles d’endormissement"],
    [2, "Troubles de maintien du sommeil"],
    [3, "Réveil précoce"],
    [4, "Sommeil non récupérateur"],
    [null, "Aucune de ces réponses"]
])
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/multiCheck.webp }}" height="240" alt=""/></div>

```js
form.multiButtons("sommeil", "Trouble(s) du sommeil", [
    [1, "Troubles d’endormissement"],
    [2, "Troubles de maintien du sommeil"],
    [3, "Réveil précoce"],
    [4, "Sommeil non récupérateur"],
    [null, "Aucune de ces réponses"]
])
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/multiButtons.webp }}" height="130" alt=""/></div>

> [!NOTE]
> Dans l'export de données, les valeurs à choix multiples sont exportées avec plusieurs colonnes, soit une colonne par proposition. La cellule correspond à l'enregistrement (ligne) et à la variable et sa proposition (colonne) contient la valeur 1 si l'utilisateur a sélectionné la proposition, et 0 le cas échéant.

Il est possible de créer un choix exclusif des autres en créant une proposition qui utilise le code `null`. Choisir cette proposition provoquera le décochage des autres choix faits par l'utilisateur, et vice-versa. Utilisez cela pour créer une proposition de type "Aucun choix de ne me correspond", comme dans l'exemple ci-dessous :

```js
form.multiCheck("sommeil", "Trouble(s) du sommeil", [
    [1, "Troubles d’endormissement"],
    [2, "Troubles de maintien du sommeil"],
    [3, "Réveil précoce"],
    [4, "Sommeil non récupérateur"],
    [null, "Aucune de ces réponses"] // Cette proposition est exclusive et désactive les autres
])
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/null.webp }}" height="240" alt=""/></div>

> [!NOTE]
> Une variable à choix multiples qui comprend une proposition `null` sera considéré comme manquante (NA) si aucune proposition n'est cochée. En revanche, s'il n'y a pas de proposition `null`, la variable ne sera jamais considérée comme manquante.

## Variable calculée

Vous pouvez faire tous les calculs que vous voulez en JavaScript !

Cependant, vous pouvez utiliser le widget `form.calc()` pour stocker une valeur calculée en base de données, l'afficher à l'utilisateur sous la forme d'un widget et ajouter la valeur calculée aux exports de données.

Pour ce faire, précisez la valeur calculée par votre code JavaScript comme troisième paramètre de `form.calc()` (après le libellé). L'exemple ci-dessous calcule deux variables à partir du poids et de la taille : l'IMC, et la classe d'IMC.

```js
form.number("poids", "Poids", {
    min: 20,
    max: 400,
    suffix: "kg"
})

form.number("taille", "Taille", {
    min: 1,
    max: 3,
    decimals: 2,
    suffix: "m",
    help: "Entrez un poids et une taille et les variables IMC et classe d'IMC seront calculées automatiquement."
})

let imc = values.poids / (values.taille ** 2)

form.calc("imc", "IMC", imc, { suffix: "kg/m²" })
form.sameLine(); form.calc("classe_imc", "Classe d'IMC", classeIMC(imc))

function classeIMC(imc) {
    if (imc >= 30) {
        return "Obésité"
    } else if (imc >= 25) {
        return "Surpoids"
    } else if (imc >= 18.5) {
        return "Normal"
    } else if (imc > 0) {
        return "Poids insuffisant"
    }
}
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/calc.webp }}" height="160" alt=""/></div>

## Pièce jointe

Utilisez le widget `form.file()` pour permettre à l'utilisateur de joindre un fichier au formulaire.

```js
form.file("attach", "Pièce jointe")
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/file.webp }}" height="120" alt=""/></div>

> [!NOTE]
> L'export de données, sous forme de fichier XLSX ou CSV, contiendra le hash SHA-256 du fichier et pas le fichier directement.

# Mise en page

## Sections

Utilisez le widget `form.section` pour identifier clairement les différentes parties de votre questionnaire, en plaçant des widgets à l'intérieur des sections.

Ce widget prend deux paramètres : le libellé ou titre de la section, et une fonction dans laquelle le contenu de la section sera créé.

<p class="call"><span style="color: #24579d;">form.section</span> ( <span style="color: #054e2d;">"Titre"</span>, () => {
    <span style="color: #888;">// Contenu de la section</span>
} )</p>

L'exemple qui suit regroupe plusieurs widgets destinés à calculer l'IMC, ainsi que le calcul de l'IMC affiché via un widget `form.calc`.

```js
form.section("Poids et taille", () => {
    form.number("poids", "Poids", {
        min: 20,
        max: 400,
        suffix: "kg"
    })

    form.number("taille", "Taille", {
        min: 1,
        max: 3,
        decimals: 2,
        suffix: "m",
    })

    let imc = values.poids / (values.taille ** 2)
    form.calc("imc", "IMC", imc, { suffix: "kg/m²" })
})
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/section.webp }}" height="340" alt=""/></div>

Les sections sont uniquement destinées à modifier l'aspect du questionnaire. Elles n'influent par sur le schéma de données et n'existent pas dans l'export de données.

> [!TIP]
> Vous pouvez imbriquer une section à l'intérieur d'une autre section en cas de besoin.

## Colonnes et blocs

Par défaut, les widgets sont disposés de haut en bas. Il est possible d'aligner horizontalement des widgets en plusieurs colonnes, à l'aide de la fonction `form.columns` qui s'utilise de manière similaire à `form.section`, mais sans donner de titre.

<p class="call"><span style="color: #24579d;">form.columns</span> ( () => {
    <span style="color: #888;">widget1("var", "Libellé")</span>
    <span style="color: #888;">widget2("var", "Libellé")</span>
} )</p>

En complément, Goupile fournit la fonction `form.block()` qui permet de regrouper plusieurs widgets en un bloc unique, qui sera aligné de manière cohérente verticalement.

L'exemple suivant illustre comment combiner un affichage en colonnes et un bloc pour aligner les variables *poids* et *taille* horizontalement, et afficher l'IMC calculé sous la taille.

```js
form.columns(() => {
    form.number("poids", "Poids", {
        min: 20,
        max: 400,
        suffix: "kg"
    })

    form.block(() => {
        form.number("taille", "Taille", {
            min: 1,
            max: 3,
            decimals: 2,
            suffix: "m",
        })

        let imc = values.poids / (values.taille ** 2)
        form.calc("imc", "IMC", imc, { suffix: "kg/m²" })
    })
})
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/columns.webp }}" height="240" alt=""/></div>

> [!NOTE]
> Sur les petits écrans (tablette, téléphone), les colonnes sont ignorées et les widgets sont disposés verticalement.

Pour les cas simples, Goupile propose un raccourci avec la fonction `form.sameLine()`. Celle-ci permet d'afficher un widget à droite du widget défini précédemment, en évitant d'avoir à englober ces deux widget par un appel à `form.columns()`.

<p class="call"><span style="color: #888;">widget1("var", "Libellé")</span>
<span style="color: #24579d;">form.sameLine();</span> <span style="color: #888;">widget2("var", "Libellé")</span></p>

> [!NOTE]
> L'option `wide: true` permet de créer des colonnes qui s'étirent horizontalement pour remplir la page. Cette option peut être utilisée avec `form.columns` ou bien avec la forme raccourcie `form.sameLine` comme illustré ci-dessous :
>
> ```js
> form.columns(() => {
>     form.number("poids", "Poids")
>     form.number("taille", "Taille")
> }, { wide: true })
>
> // Code équivalent avec form.sameLine
> form.number("poids", "Poids")
> form.sameLine(true); form.number("taille", "Taille")
> ```

# Options communes

## Saisie obligatoire

Utilisez l'option `mandatory: true` pour rendre la saisie d'un champ obligatoire. Pour des raisons pratiques, vous pouvez également activer cette option en préfixant le nom de la variable par un astérisque, comme dans l'exemple ci-dessous :

```js
form.number("*age", "Âge")

// Code équivalent avec une option classique :
// form.number("age", "Âge", { mandatory: true })
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/mandatory.webp }}" height="120" alt=""/></div>

> [!NOTE]
> L'erreur de saisie *Réponse obligatoire* ne s'affiche qu'après une tentative d'enregistement du formulaire.

## Message d'aide

L'option `help` permet d'ajouter un texte d'aide qui s'affiche en petit caractères sous le widget concerné. Utilisez cette option pour apporter une précision, guider la saisie de l'utilisateur, ou proposer des exemples.

```js
form.number("pseudonyme", "Pseudonyme", {
    help: "Nous préférons les pseudonymes au format nom.prenom mais ceci n'est pas obligatoire"
})
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/help.webp }}" height="120" alt=""/></div>

## Préfixe et suffixe

Utilisez les options `prefix` et `suffix` pour afficher un texte à gauche et à droite du widget (respectivement). Ce texte peut être statique ou dynamique, et calculé à partir de la valeur elle-même à l'aide d'une fonction. C'est ce qu'illustre l'exemple ci-dessous, dans lequel le suffixe affiché pour la saisie de l'âge dépend de la valeur.

```js
form.number("age", "Âge", { suffix: value > 1 ? "ans" : "an" })
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/suffix.webp }}" height="90" alt=""/></div>

## Valeur pré-remplie

Utilisez l'option `value` pour spécifier une valeur pré-remplie. Le widget continuera à suivre cette valeur par défaut même si celle-ci change (par exemple, si elle est calculée à partir d'un autre champ), jusqu'à ce que l'utilisateur saisisse une valeur lui-même. Par la suite, le widget gardera la valeur saisie.

Vous pouvez par exemple pré-renseigner une date d'inclusion, pour un jour située dans une semaine à compter du jour actuel, à l'aide du code suivant :

```js
form.date("date_inclusion", "Date d'inclusion", { value: LocalDate.today().plus(7) })
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/value.webp }}" height="90" alt=""/></div>

## Désactiver un widget

Désactivez un widget avec l'option `disabled: true`. Comme toutes les autres options, cette valeur peut être calculée dynamiquement.

Servez-vous de cette option pour désactiver un champ de saisie en fonction d'une réponse précédente. Dans l'exemple qui suit, le champ numérique est activé uniquement si l'utilisateur sélectionne *Tabagisme actif* dans le premier champ à choix unique.

```js
form.enumButtons("tabagisme", "Tabagisme",  [
    ["actif", "Tabagisme actif"],
    ["sevre", "Tabagisme sevré"],
    ["non", "Non fumeur"]
])

form.number("cigarettes", "Nombre de cigarettes par jour", { disabled: values.tabagisme != "actif" })
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/disabled.webp }}" height="220" alt=""/></div>

## Cacher un widget

Utilisez l'option `hidden: true` pour cacher un widget.

Il peut être utile de définir un widget mais ne pas l'afficher pour que la variable correspondante (et ses métadonnées, comme le libellé ou le type de variable) existe dans l'export de données. Combinez un widget calculé comme `form.calc()` et l'option `hidden: true` pour cela.

```js
let score = 42
form.calc("score", "Score calculé à partir du formulaire", score, { hidden: true })
```

## Valeur substituée

Utilisez l'option `placeholder` pour afficher une valeur à l'intérieur du champ (pour les champs à saisie libre). Cette valeur est affichée en transparence et disparait lorsque l'utilisateur saisit une valeur.

```js
// L'utilisation de null pour le libellé permet de ne pas afficher de libellé
form.text("email", null, { placeholder: "adresse e-mail" })
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/placeholder.webp }}" height="60" alt=""/></div>

## Aspect du widget

Utilisez l'option `wide: true` pour maximiser la largeur d'un widget, et faire en sorte qu'il occupe tout l'espace horizontal disponible. La capture ci-dessous illustre la différence entre un slider standard et un slider avec `wide: true`.

```js
form.slider("sommeil1", "Qualité du sommeil", {
    help: "Evaluez la qualité du sommeil avec un score entre 0 (médiocre) et 10 (très bon sommeil)"
})

form.slider("sommeil2", "Qualité du sommeil", {
    help: "Evaluez la qualité du sommeil avec un score entre 0 (médiocre) et 10 (très bon sommeil)",
    wide: true
})
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/wide.webp }}" height="280" alt=""/></div>

Utilisez l'option `compact: true` pour utiliser un affichage plus compact dans lequel le libellé de la question et le champ de saisie sont affichés sur la même ligne.

```js
form.text("nom", "Nom", { compact: true })
form.text("prenom", "Prénom", { compact: true })
form.number("age", "Âge", { compact: true })
```

<div class="screenshot"><img src="{{ ASSET static/help/widgets/compact.webp }}" height="180" alt=""/></div>

# Publication du projet

Une fois votre formulaire prêt, vous devez le publier pour le rendre accessible à tous les utilisateurs. Le code non publié n'est visible que par les utilisateurs qui utilisent le mode Conception.

Après publication, les utilisateurs pourront saisir des données sur ces formulaires.

Pour ce faire, cliquez sur le bouton Publier en haut à droite du panneau d'édition de code. Ceci affichera le panneau de publication (visible dans la capture à gauche).

<div class="screenshot"><img src="{{ ASSET static/help/dev/publish.webp }}" alt=""/></div>

Ce panneau récapitule les modifications apportées et les actions qu'engendrera la publication. Dans la capture à droite, on voit qu'une page a été modifiée localement (nommée « accueil ») et sera rendue publique après acceptation des modifications.

<style>
    .call {
        font-family: monospace;
        white-space: pre-wrap;
        font-size: 1.1em;
        margin-left: 2em;
        color: #444;
    }
</style>
