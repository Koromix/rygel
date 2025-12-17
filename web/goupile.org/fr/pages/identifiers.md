# TID et séquence

Chaque enregistrement dans Goupile dispose de deux identifiants uniques :

- Un **identifiant TID**, qui est une chaîne de 26 caractères encodant le moment de l'enregistrement et une partie aléatoire (ex : *01K5EY3SCEM1D1NBABXXDZW7XP*).
- Un **identifiant de séquence**, qui est un nombre incrémenté à chaque enregistrement, en commençant par 1.

Utilisez le TID pour faire la jointure entre les différentes tables dans les données exportées. La valeur du TID est disponible dans la colonne `__tid` de chaque table.

# Compteurs personnalisés

## Compteurs incrémentaux

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

<div class="screenshot"><img src="{{ ASSET static/help/dev/count.webp }}" alt=""/></div>

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

<div class="screenshot"><img src="{{ ASSET static/help/dev/randomize.webp }}" alt=""/></div>

Par défaut, les compteurs ne sont pas secrets, c'est à dire qu'ils peuvent être lus depuis une autre page du formuaire, et affichés ou utilisés pour adapter les pages.

Pour définir un compteur secret, qui ne sera disponible que dans le fichier d'export final, mettez le troisième paramètre `secret` de la fonction de randomisation à *true* :

```js
// Randomisation secrète
meta.randomize("groupe", 4, true)
```

# Summary

Pour chaque page du formulaire, il est possible de définir un identifiant supplémentaire dit *summary*, qui sera affiché à la place de la date dans le tableau récapitulatif des enregistrements (voir capture ci-dessous).

Pour cela, assignez une valeur à `meta.summary` dans le script de formulaire. Dans l'exemple ci-dessous, la valeur affichée dans la colonne « Introduction » est construite en fonction de l'âge indiqué dans la variable correspondante :

```js
form.number("*age", "Âge", {
    min: 0,
    max: 120
})

if (values.age != null)
    meta.summary = values.age + (values.age > 1 ? ' ans' : ' an')
```

<div class="screenshot"><img src="{{ ASSET static/help/dev/summary.webp }}" alt=""/></div>

Vous pouvez utiliser cette fonctionnalité pour afficher un numéro d'inclusion créé manuellement (par exemple si le numéro d'inclusion est créé en dehors de Goupile), comme le montre l'exemple ci-dessous :

```js
form.number("num_inclusion", "Numero d'inclusion")
meta.summary = values.num_inclusion
```
