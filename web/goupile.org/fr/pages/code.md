# Identifiants des enregistrements

## TID et séquence

Chaque enregistrement dans Goupile dispose de deux identifiants uniques :

- L'identifiant TID, qui est une chaîne de 26 caractères encodant le moment de l'enregistrement et une partie aléatoire (ex : *01K5EY3SCEM1D1NBABXXDZW7XP*).
- L'identifiant de séquence, qui est un nombre incrémenté à chaque enregistrement, en commençant par 1.

Utilisez le TID pour faire la jointure entre les différentes tables dans les données exportées. La valeur du TID est disponible dans la colonne `__tid` de chaque table.

## Summary

Pour chaque page du formulaire, il est possible de définir un identifiant supplémentaire dit `summary`, qui sera affiché à la place de la date dans le tableau récapitulatif des enregistements (voir capture ci-dessous).

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

Il est possible de créer des compteurs personnalisés, qui sont incrémentés (en commençant à 1) en fonction de conditions définies par le code.

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

Le système de randomisation de Goupile est une extension des compteurs introduits précédemment.

Utilisez la fonction `meta.randomize(key, max, secret = false)` pour assigner un compteur. Le compteur randomisé n'est pas séquentiel, chaque participant reçoit une valeur aléatoire entre 1 et *max*, par bloc de taille *max*. 
Une fois que *max* participants ont été inclus, le compteur est remis à zéro pour les inclusions suivantes.

Par exemple, une valeur maximale de 4 garantit que les 4 premiers participants auront les valeurs 1, 2, 3 et 4 dans un ordre randomisé. Il en sera de même pour les 4 suivants :

```js
meta.randomize("groupe", 4)
```

Comme dans l'exemple sur les compteurs séquentiels, vous pouvez stratifier la randomisation en utilisant une clé différente, qui peut dépendre d'une variable présente dans le formulaire.

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

# Erreurs et annotations

## Contrôles de saisie

Les options `mandatory`, ainsi que `min` et `max` présents sur le widget `form.number` sont simplement des raccourcis pour gérer les erreurs les plus courantes. Cependant, comme le formulaire est programmé en JavaScript, vous pouvez utiliser programmer détecter toutes sortes de conditions d'erreur en les programmant !

Prenons l'exemple du widget de texte suivant : ```form.text("*num_inclusion", "N° d'inclusion")```. Celui-ci enregistre une chaine de caractères JavaScript (ou *string*). Vous pouvez par exemple utiliser l'ensemble des méthodes présentes ici :  https://developer.mozilla.org/fr/docs/Web/JavaScript/Reference/Global_Objects/String pour déclencher une erreur.

L'example ci-dessous valide la valeur d'un numéro d'inclusion à l'aide d'une expression régulière :

```js
// Les valeurs sont mises à undefined quand elles sont vides. Il faut donc vérifier qu'il y a une valeur avant d'appeler une méthode sur celle-ci pour éviter une erreur JavaScript, c'est ce que nous faisons dans la condition ci-dessous.

if (values.num_inclusion && !values.num_inclusion.match(/^[0-9]{5}$/))
    form.error("num_inclusion", "Format incorrect, 5 chiffres sont attendus")
```

Voici un autre exemple dans lequel le numéro d'inclusion suit une logique différente :

```js
if (values.inclusion_num && !values.inclusion_num.match(/^(REN|GRE|CFD|LRB|STE)[A-Z][a-z][A-Z]$/))
    form.error("inclusion_num", html`Format incorrect.<br/> Renseigner le code centre (REN, GRE, CFD, LRB, STE)  puis les 2 premières lettres du Nom puis la première lettre du Prénom.<br/>Exemple : Mr Philippe DURAND à Rennes : 'RENDuP'`)
```

> [!NOTE]
> Il faut juste toujours penser à vérifier que la valeur n'est pas vide (valeur `undefined` ou `null` en JavaScript) avant d'appeler une méthode sur celle-ci.

Par défaut, l'erreur est affichée immédiatement pendant la saisie. Il est possible de faire en sorte que l'erreur ne s'affiche qu'après une tentative d'enregistrement en ajoutant un troisième paramètre lors de l'appel à `form.error()` :

```js
// L'option { delayed: true } à la fin de form.error() indique que l'erreur est retardée à la validation

if (values.num_inclusion && !values.num_inclusion.match(/^[0-9]{5}$/))
    form.error("num_inclusion", "Format incorrect (5 chiffres attendus)", { delayed: true })
```

> [!TIP]
> Vous pouvez tester des expressions régulières en live en ligne, en utilisant un site comme celui-ci : https://regex101.com/

## Annotations de saisie

*Rédaction en cours*

# Manipulation des valeurs

## Données d'autres pages

thread

## Profil de l'utilisateur

profile

# Ajout de fichiers et d'images

Il est possible d'intégrer des images, des vidéos, des PDF et tous types de fichiers, qui seront directement hébergés par Goupile. Pour cela, ouvrez le panneau de publication (accessible au-dessus de l'éditeur), puis cliquez sur le lien « Ajouter un fichier » [1].

<div class="screenshot">
    <img src="{{ ASSET static/help/dev/file1.webp }}" style="height: 240px;" alt=""/>
    <img src="{{ ASSET static/help/dev/file2.webp }}" style="height: 240px;" alt=""/>
</div>

Vous pouvez ensuite sélectionner un fichier à ajouter depuis votre ordinateur, et le renommer si vous le souhaitez. Vous pouvez également le mettre en arborescence en lui donnant un nom tel que « images/alpes/montblanc.png ».

Une fois le fichier ajouté, vous pouvez directement y faire référence dans vos pages à l'aide du widget `form.output`. Le code HTML suivant vous explique comment afficher le logo Apple, à partir d'un fichier `images/apple.png` ajouté au projet :

```js
form.output(html`
    <img src=${ENV.urls.files + 'images/apple.png'} alt="Logo Apple" />
`)
```

L'utilisation de `ENV.urls.files` pour construire l'URL vous garantit une URL qui changera à chaque publication du projet, pour éviter des problèmes liés au cache des navigateurs. Cependant, chaque fichier est aussi accessible via `/projet/files/images/alpes/montagne.png`, et cette URL est stable depuis l'extérieur de Goupile.

> [!NOTE]
> Le nom de fichier `favicon.png` est particulier. Si vous mettez une image nommée favicon.png elle remplacera la favicon affichée dans le navigateur, et l'icône affichée sur l'écran de connexion.
