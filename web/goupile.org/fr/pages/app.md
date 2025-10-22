# Définition du projet

## Projet à une page

La structure globale d'un recueil est définie par le code du projet, qui définit les différentes tables et pages du recueil, ainsi que les liens entre celles-ci.

Pour créer à la fois une table et la page correspondante, utilisez la fonction `app.form()` de la manière suivante :

<p class="call"><span style="color: #24579d;">app.form</span> ( <span style="color: #054e2d;">"cle_page"</span>, <span style="color: #431180;">"Titre de la page"</span> )</p></p>

Cet appel suffit à définir un projet simple avec une page de recueil, et vous pouvez vous contenter de l'exemple suivant si c'est votre cas :

```js
app.form("projet", "Projet de recueil")
```

## Plusieurs pages

Pour définir un projet avec plusieurs pages dans Goupile, il est nécessaire d'imbriquer les pages à l'intérieur d'une catégorie principale :

<p class="call"><span style="color: #24579d;">app.form</span> ( <span style="color: #054e2d;">"cle_categorie"</span>, <span style="color: #431180;">"Nom de la catégorie"</span>, () => {
    <span style="color: #24579d;">app.form</span> ( <span style="color: #054e2d;">"page1"</span>, <span style="color: #431180;">"Titre de la première page"</span> )
    <span style="color: #24579d;">app.form</span> ( <span style="color: #054e2d;">"page2"</span>, <span style="color: #431180;">"Titre de la deuxième page"</span> )<br>
    <span style="color: #888;">// Autres pages...</span>
} )</p>

Avec cette structure simple, le recueil sera structuré selon la capture ci-dessous :

<div class="screenshot"><img src="{{ ASSET static/help/app/status.webp }}" height="240" alt=""/></div>

> [!NOTE]
> Chaque page d'un formulaire dispose de sa propre table de données, ces tables sont reliées entre elles par un [identifiant TID](code#tid-et-sequence), qui est un numéro d'enregistrement unique et aléatoire.
>
> Lors de l'export, les données des deux pages seront exportées dans deux tables séparées (deux onglets dans un export XLSX), qui pourront être reliées par la colonne `__TID`.

## Menu hiérarchisé

L'imbrication des pages définit également la hiérachie de menu du projet. C'est ce que montre l'exemple suivant, plus proche d'un projet réel (avec une inclusion et 2 temps de recueil), avec un menu hiérarchique qui sépare bien les temps de recueil :

<div class="screenshot"><img src="{{ ASSET static/help/app/menu.webp }}" height="280" alt=""/></div>

Le code de projet qui correspond à la structure du formulaire présenté en capture est disponible ci-dessous :

```js
app.form("projet", "Validation d'échelle", () => {
    app.form("inclusion", "Inclusion")

    app.form("bilan", "Bilan initial", () => {
        app.form("atcd", "Antécédents")
        app.form("echelle0", "Échelle initiale")
    })

    app.form("m1", "Suivi M1", () => {
        app.form("suivi1", "Suivi à 1 mois")
        app.form("echelle1", "Échelle intermédiaire")
    })
})
```

# Options de passation

## Système d'annotation

Goupile propose un système d'annotations de variables, chaque variable peut être accompagnée d'un statut de remplissage et d'un commentaire libre. Consultez la documentation sur les [variable annotées](export#annotation-des-variables) pour en savoir plus.

Cette fonctionnalité n'est pas activée par défaut pour des raisons historiques. Vous devez l'activer avec l'option `app.annotate` comme ceci :

```js
app.annotate = true

// app.form("projet", "Titre")
// ...
```

Toutes les variables peuvent être annotées par défaut, mais vous pouvez modifier cette option [pour chaque variable](form#annotation-de-variable) en cas de besoin.

## Pages conditionnelles

Il est souvent nécessaire d'activer ou de désactiver certaines pages en fonctions de conditions de remplissage.

Un exemple classique est l'utilisation d'une page d'inclusion : la page d'inclusion doit être la seule page disponible jusqu'à ce que les conditions d'inclusion d'un participant soit remplie. Si les conditions ne sont pas remplies, les pages restent désactivées.

Utilisez l'option `enabled` pour désactiver des pages. Il s'agit d'une **option dynamique**, c'est à dire qu'elle peut être calculée en fonction des données de l'enregistrement.

```js
app.form("projet", "Validation", {
    app.form("inclusion", "Inclusion du patient")

    // Le bilan ne sera disponible que si la variable "ok" est égale à 1
    app.form("bilan", "Bilan initial", { enabled: thread => thread.entries.inclusion?.data?.ok == 1 })
})
```

> [!TIP]
> Consultez la documentation concernant l'utilisation de [données d'autres page](code#donnees-d-autres-pages) pour manier l'objet `thread` utilisé ci-dessus.
>
> Celui-ci permet entre autres d'accéder aux données des pages remplies, aux [identifiants](code#identifiants-des-enregistrements) (TID, séquence, compteurs, randomisation non-aveugle), et à d'autres métadonnées.

Voici un exemple plus complet, avec une page d'inclusion et deux pages de suivi, qui ne sont activées qu'après vérification de critères d'inclusion (âge et partage des données) :

```js
// Script de projet

app.form("projet", "Validation", () => {
    app.form("inclusion", "Inclusion du patient")

    app.pushOptions({ enabled: thread => thread.entries.inclusion?.data?.ok == 1 })

    app.form("bilan", "Bilan initial")
    app.form("suivi1", "Suivi à 1 mois")
})
```

```js
// Contenu de la page "inclusion"

form.section("Critères d'inclusion", () => {
    form.binary("majeur", "Avez-vous 18 ans ?")
    form.binary("opposition", "Acceptez-vous de participer à cette étude ?")

    values.ok = (values.majeur == 1 && values.opposition == 1) ? 1 : 0
})
```

> [!NOTE]
> Cette option est la seule option dynamique à l'heure actuelle, mais d'autres options seront peut-être concernées dans des versions ultérieures.

## Pages successives

Par défaut, chaque page fonctionne de manière indépendante, et doit être ouverte par le menu ou bien depuis la page de statut d'enregistrement. Lorsque l'utilisateur enregistre une page, celle-ci reste ouverte.

Vous pouvez définir une séquence (ou suite) de pages à l'aide de l'option `sequence`, qui peut être utilisée de trois manières :

- `sequence: true` : l'enregistrement d'une page ouvre la prochaine page avec une option sequence
- `sequence: "prochaine_page"` : l'enregistrement d'une page redirige vers la page nommée explicitement
- `sequence: ["page1", "page2", "page3"]` : l'enregistrement d'une page envoie vers la page suivante du tableau indiqué

Une fois la fin de la séquence atteinte, l'utilisateur est redirigé vers la page de statut.

> [!NOTE]
> La séquence n'est suivie que lors du premier enregistrement d'une page.
>
> Lorsque l'utilisateur ouvre une page déjà enregistrée pour la modifier, le bouton Continuer le redirige directement vers la page de statut.
>
> Par ailleurs, les séquences sont totalement ignorées en mode à *double panneaux* (avec la liste des enregistrements à gauche et le formulaire à droite). Dans cas cas, la page actuelle reste ouverte après enregistrement.

### Séquence simple

L'exemple suivant démontre l'utilisation d'une séquence simple, avec deux pages, à la fin desquelles l'utilisateur est redirigé vers la page de statut.

```js
app.form("projet", "2 pages", {
    app.pushOptions({ sequence: true })

    app.form("page1", "Première page")
    app.form("page2", "Deuxième page")
})
```

> [!TIP]
> Cet exemple utilise la fonction `app.pushOptions()` pour définir l'option de séquence pour les différentes pages sans répéter l'option à chaque fois.

### Séquences multiples

L'exemple suivant démontre l'utilisation de deux séquences. Lorsque l'utilisateur ouvre la page "A", il devra remplir les pages "A", "C" et "E" à la suite. Si il ouvre la page "B", il devra remplir les pages "B", "D" et "F" à la suite.

```js
let ace = ['a', 'c', 'e']
let bdf = ['b', 'd', 'f']

app.form("projet", "6 pages", {
    form.text('a', 'A', { sequence: ace }) // Prochaine page: C
    form.text('b', 'B', { sequence: bdf }) // Prochaine page: D

    form.text('c', 'C', { sequence: ace }) // Prochaine page: E
    form.text('d', 'D', { sequence: bdf }) // Prochaine page: F

    form.text('e', 'E', { sequence: ace }) // Retour à la page de statut
    form.text('f', 'F', { sequence: bdf }) // Retour à la page de statut
})
```

## Sauvegarde automatique

Par défaut, les données ne sont enregistrées que lors de la validation par le bouton Enregistrer. L'enregistrement implique un formulaire entièrement valide, pour lequel les oublis et les erreurs sont corrigées ou annotées.

<div class="screenshot"><img src="{{ ASSET static/help/app/save.webp }}" height="240" alt=""/></div>

Vous pouvez opter pour un mode de sauvegarde automatique. Dans ce cas, les changements effectués sont enregistrés à intervalle régulier, sans erreur bloquante. Utilisez le paramètre `autosave` en précisant le délai d'enregistrement en millisecondes.

```js
// Les changements sont enregistrés après 5 secondes d'inactivité (5000 ms)
app.form("auto", "Sauvegarde automatique", { autosave: 5000 })
```

Les pages enregistrées de manière automatique sont marqués par un **statut de brouillon**, qui est visible et peut être filtré dans le tableau de suivi. Le bouton d'enregistrement reste disponible même en mode de sauvegarde automatique; il permet de valider les données et de faire disparaitre le statut de brouillon.

<div class="screenshot"><img src="{{ ASSET static/help/app/draft.webp }}" height="200" alt=""/></div>

## Verrouillage et oubli

Par défaut, les enregistrements Goupile sont modifiables à tout moment. Les droits utilisateurs relatifs aux données distinguent la lecture des données, la sauvegarde et la suppression, sans distinction entre la création et la modification d'un enregistrement.

Goupile propose deux fonctionnalités pour empêcher la modification d'un enregistrement existant, qui peuvent être utilisées séparémment ou bien combinées :

- Le **verrouillage** (option *lock*) pour que les données soit mises en lecture seule
- L'**oubli** (option *forget*) pour les utilisateurs en mode invité (ou pour les utilisateurs avec droit de sauvegarde sans droit de lecture)

### Verrouillage

L'option `lock` permet de verrouiller les données saisies par l'utilisateur, il y a 3 possibilités :

- `lock: true` : entraine un verrouillage immédiat au moment de la sauvegarde
- `lock: delay` : entraine un verrouillage différé, après le délai spécifié en millisecondes (par exemple `lock: 300 * 1000` pour verrouiller après 300 secondes soit 5 minutes)
- `lock: false` : verrouillage manuel avec un bouton dédié

> [!NOTE]
> Notez bien la distinction entre la valeur par défaut `lock: null` (absence de fonction verrouillage) et la valeur `lock: false` (verrouillage manuel).
>
> Dans le deuxième cas, l'enregistrement n'entraine pas de verrouillage, mais un bouton "Verrouiller" apparait dans l'interface et peut être utilisé manuellement.

Comme les autres options décrites précédemment, cette option peut être différente pour chaque page du projet. Dans un formulaire avec plusieurs pages, vous pouvez définir l'option `lock` (immédiate ou après délai) sur la dernière page, pour verrouiller l'enregistrement une fois qu'il est rempli entièrement.

```js
app.form("projet", "2 pages", () => {
    app.form("vous", "Informations sur le participant")
    app.form("avis", "Avis sur la journée scientifique", { lock: 300000 })
})
```

> [!TIP]
> Mais vous pouvez également la définir sur la première page, avec un délai, si vous souhaitez faire un questionnaire avec un délai de remplissage limité.

Les **utilisateurs avec droit d'audit** (DataAudit) peuvent déverrouiller un enregistrement à l'aide du bouton dédié.

### Oubli

Cette fonctionnalité concerne deux types d'utilisateurs :

- Les invités (questionnaires accessibles aux invités)
- Les utilisateurs avec droit de sauvegarde (DataSave) sans droit de lecture

Lorsque ces utilisateurs créent un enregistrement dans Goupile, ils obtiennent un droit de lecture et de modification limité aux enregistrements qu'ils ont créé. Dans Goupile, on dit qu'ils ont un *claim* sur l'enregistrement concerné.

Ce **claim peut être supprimé** en définissant l'option `forget: true` dans les options de page. Tout comme pour le verrouillage décrit précédemment, cette option peut être utilisée sur la dernière page d'un enregistrement à plusieurs pages: ainsi, l'utilisateur pourra remplir les différentes pages, jusqu'à finaliser le remplissage sur la dernière page.

```js
app.form("projet", "2 pages", () => {
    app.form("vous", "Informations sur le participant")
    app.form("avis", "Avis sur la journée scientifique", { forget: true })
})
```

> [!NOTE]
> Les utilisateurs avec droit de lecture (DataRead) peuvent **voir tous les enregistrements du projet**. La suppression du claim (ou oubli) est sans effet pour ces utilisateurs !
>
> Utilisez plutôt le [verrouillage](#verrouillage) dans ce cas de figure.

<style>
    .call {
        font-family: monospace;
        white-space: pre-wrap;
        font-size: 1.1em;
        margin-left: 2em;
        color: #444;
    }
</style>
