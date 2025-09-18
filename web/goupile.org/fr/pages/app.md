# Interface de projet

Un nouvel onglet s'ouvre dans votre navigateur. Le bandeau noir en haut de page permet de configurer l'affichage de la page. Vous pouvez afficher (ou non) un ou deux panneaux en sélectionnant (ou désélectionnant) le(s) panneau(x) d'intérêt. Par défaut, le panneau « Liste » [2] est sélectionné. Les différents panneaux de configuration sont : « Code » [1], « Liste » [2] et « Saisie » [3]. Le menu déroulant central [4] vous permet de naviguer entre les différentes pages de votre eCRF (ici la première page se nomme « Introduction »). Le menu déroulant intermédiaire [5] vous permet de naviguer entre les différents sous-projets si votre projet a été subdivisé. Enfin le menu déroulant tout à droite [6] vous permet de changer votre mot de passe ou déconnecter votre session.

<div class="screenshot"><img src="{{ ASSET static/help/dev/panels.webp }}" alt=""/></div>

## Panneau de code

Le panneau de configuration « Code » vous permet d'éditer votre eCRF. Il contient deux onglets : « Application » et « Formulaire ». Par défaut, l'onglet « Formulaire » est affiché.

L'onglet « Formulaire » vous permet l'édition du contenu de votre eCRF pour une page donnée (ici « Introduction ». Pour rappel la navigation entre les différentes pages de votre formulaire s'effectue via le menu déroulant [1]).
Le contenu est édité en ligne de codes via un éditeur de script. Le langage de programmation est le JavaScript. La connaissance du langage n'est pas nécessaire pour la création et l'édition de scripts simples. L'édition de l'eCRF et les différents modules de code seront abordés plus en détails ultérieurement.

<div class="screenshot"><img src="{{ ASSET static/help/dev/code1.webp }}" alt=""/></div>

L'onglet « Application » vous permet d'éditer la structure générale de votre eCRF. Elle permet ainsi de définir les différentes pages et ensemble de pages. La structure est également éditée en ligne de code via un éditeur de script (également Javascript). L'édition de la structure de l'eCRF et les différents modules de code seront abordés plus en détail ultérieurement.

<div class="screenshot"><img src="{{ ASSET static/help/dev/code2.webp }}" alt=""/></div>

## Panneau de données

Le panneau « Liste » vous permet d'ajouter des nouvelles observations (« Ajouter un suivi ») et de monitorer le recueil de données. La variable « ID » correspond à l'identifiant d'un formulaire de recueil. Il s'agit d'un numéro séquentiel par défaut mais cela peut être paramétré. Les variables « Introduction », « Avancé » et « Mise en page » correspondent aux trois pages de l'eCRF d'exemple.

<div class="screenshot"><img src="{{ ASSET static/help/dev/list.webp }}" alt=""/></div>

## Affiche du formulaire

Le panneau de configuration « Saisie » vous permet de réaliser le recueil d'une nouvelle observation (nouveau patient) ou de modifier une observation donnée (après sélection de l'observation dans le panneau de configuration « Liste »). La navigation entre les différentes pages de l'eCRF peut s'effectuer avec le menu déroulant [1] ou le menu de navigation [2]. Après avoir réalisé la saisie d'une observation, cliquer sur « Enregistrer » [3].

<div class="screenshot"><img src="{{ ASSET static/help/dev/entry.webp }}" alt=""/></div>

# Code du projet

## Définition des pages

## Affichage conditionnel

Variable `enabled`

```js
// ...
app.form("tab_general", "Généralités", () => {
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
app.form("tab_general", "Généralités", () => {
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

## Séquençage automatique

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
