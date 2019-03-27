# Installation et mise à jour

Il y a deux possibilités :

* Simple : Installer une version précompilée
* Compliquée : Compiler manuellement drdc et drdR

## Tables et versions précompilées (Windows + Linux)

Pour obtenir les *tables de groupage* et le client en ligne de commande, il suffit
de télécharger la dernière archive de drdc disponible sur mon serveur :
[https://koromix.dev/files/drd](https://koromix.dev/files/drd).

La version Linux de drdc est compilée statiquement et avec musl-libc, donc elle devrait
fonctionner sur toutes les distributions (x86_64).

J'ai créé un *dépôt R* contenant le paquet source (toutes plateformes) et le paquet
précompilé (Windows), il suffit d'utiliser les commandes suivantes pour installer le paquet
ou le mettre à jour :

```r
install.packages('drdR', repos = 'https://koromix.dev/files/R')
update.packages(repos = 'https://koromix.dev/files/R', ask = FALSE)
```

## Compilation manuelle

Voir le fichier [build.md](build.md).

# Configuration

Les tables ne sont pas dans ce dépôt, je n'ai pas envie de provoquer l'ATIH. Elles doivent
être extraites patiemment de GenRSA ou, beaucoup plus simplement, il suffit de télécharger la
dernière version de drdc sur [https://koromix.dev/files/drd](https://koromix.dev/files/drd).

Une fois l'archive extraite, il faut configurer les autorisations d'UF. Pour cela il faut
remplacer *path/to/profile/mco_authorizations.txt* par le fichier FICUM utilisé par
l'établissement.

En fait c'est la seule chose à faire. D'ailleurs même sans ça, ça devrait fonctionner
(avec un avertissement) mais les suppléments liés à autorisation ne seront pas déclenchés,
ainsi que quelques GHS spécifiques qui exigent des autorisations d'UF.

# Utilisation de drdc

```sh
# Deux secteurs disponibles avec l'option -s: Public, Private.

# Cette première commande va grouper les fichiers passés en ligne de commande et donner un résumé.
# des valorisations totales. Deux secteurs sont disponibles avec l'option -s: Public, Private.
drdc mco_classify -sPrivate -C path/to/profile/drdc.ini fichier.rss ...

# Pour avoir les résultats détaillés il faut passer -v voire -vv.
drdc mco_classify -v -sPrivate -C path/to/profile/drdc.ini fichier.grp ...
drdc mco_classify -vv -sPrivate -C path/to/profile/drdc.ini fichier.grp ...

# Tu peux tester les performances avec l'argument --torture <nombre_dessais>, par exemple '--torture 20'
# et tu auras des résultats de benchmark.
drdc mco_classify --torture 20 -sPrivate -C path/to/profile/drdc.ini fichier.grp ...

# Il est également possible d'avoir les infos de ventilation des recettes dans les différentes
# unités selon différents algorithmes :
# - J (proportionnel à la durée des RUM)
# - E (proportionnel au tarif GHS du groupage mono-RUM)
# - Ex (proportionnel au tarif GHS-EXB+EXH du groupage mono-RUM)
# - Ex' (proportionnel au tarif GHS-EXB+EXH du groupage mono-RUM si EXB, au tarif GHS si pas d'EXB)
# - ExJ (combine Ex et J)
# - Ex'J (combine Ex' et J = utilisé au CHU)
drdc mco_classify -v -dExJ -sPrivate -C path/to/profile/drdc.ini fichier.rss ...

# Il y a d'autres commandes, tu peux utiliser --help pour connaitre la syntaxe
# générale ou bien <commande> --help pour une commande particulière.
drdc --help
drdc mco_classify --help
```

# Utilisation de drdR

```r
library(drdR)

# Chargement des tables et autorisations, l'objet obtenu peut être réutilisé
# autant de fois que nécessaire.
m <- mco_init('path/to/tables/', 'path/to/profile/mco_authorizations.txt',
              default_sector = 'Private')

# Chargement des RSS, l'objet retourné est une liste avec trois data.frames:
# l$stays, l$diagnoses, l$procedures. La colonne id relie les trois tables.
l <- mco_load_stays('fichier.rss')

# Ces deux syntaxes sont équivalentes. On peut aussi forcer le secteur à l'aide
# de l'argument sector='Private' ou sector='Public'. Il y a d'autres paramètres
# que je dois encore documenter.
c <- mco_classify(m, l)
c <- mco_classify(m, l$stays, l$diagnoses, l$procedures)

# Attention, la fonction mco_classify() déclenchera une erreur si la colonne id
# n'est pas triée par ordre croissant dans une des trois tables d'entrée.
# Il peut y avoir des ellipses, des id en plus dans l'une ou l'autre (par exemple
# un RUM sans diagnostic), la seule contrainte c'est qu'avant d'appeler
# mco_classify() les trois data.frames soit triés par la colonne id. Par défaut
# après appel à mco_load_stays() c'est le cas puisque la colonne est créée
# par simple incrémentation.

# Tu peux faire un summary pour avoir le résultat global, ou explorer
# c$results (data.frame) pour avoir les résultats détaillés.
summary(c)
head(c$results)

# Pour grouper plus vite (si tu veux uniquement le summary), tu peux
# passer l'argument 'results = FALSE' à la fonction mco_classify(). Dans
# ce cas c$results ne sera pas créé, mais le summary() fonctionnera.
c <- mco_classify(m, l, results = FALSE)

# Il y aussi des fonctions permettant d'extraire les données des tables
# vers des data.frames.
head(mco_ghm_ghs(m, '2017-03-01'))
head(mco_diagnoses(m, '2017-03-01'))
head(mco_procedures(m, '2017-03-01'))
head(mco_exclusions(m, '2017-03-01'))
```

# Notes sur le groupage

En réalité, pour reproduire le groupeur ATIH (MCO) il faut implémenter quatre choses :

1. Lecture des fichiers RSS/GRP et FICHCOMP (et RSA éventuellement)
2. L'algorithme de sélection du RUM principal dans les multi-RUM
3. Les contrôles d'erreurs bloquantes et non bloquantes sont implémentées dans le code
4. Lecture et interprétation des listes/caractéristiques d'actes, de diagnostics et
   de l'arbre stockés dans des fichiers binaires ".tab" distribués par l'ATIH

Le groupeur libdrd reproduit ce fonctionnement. Les fichiers binaires _.tab_ ont été
récupérés dans GenRSA.

Comme je n'ai pas les sources du groupeur, tout le code de lecture de ces fichiers non
documentés à été déchiffré et réimplémenté par rétro-engineering, de même qu'une grande
partie du code de vérification des erreurs de groupage. Une partie provient évidemment
de la lecture des guides MCO, mais il y beaucoup de subtilités qui n'y figurent pas.
Actuellement seules les tables à partir du 01/03/2012 peuvent être utilisées.

J'ai passé beaucoup de temps à comparer les résultats obtenus avec ceux de GenRSA, mais
j'ai tout fait sur les données d'un seul établissement. En particulier, je n'ai jamais
testé sur un établissement privé.

Une partie provient évidemment de la lecture des guides MCO, mais il y beaucoup de
subtilités qui n'y figurent pas.

# Limitations

Les numéros de RSS "non-numériques" ne sont pas supportés. Techniquement, les fichiers
RSS, GRP et RSA peuvent utiliser des identifiants alpha-numériques de 20 caractères
pour le n° RSS et le N° admininstratif (IEP), je ne sais pas si c'est utilisé en pratique,
si besoin ça peut être ajouté !

En ce qui concerne le groupage GHM, il faut noter que les contrôles bloquants sont
bons (en tout cas sur les tests que j’ai fait sur 6 années de CHU dont des tas de données
non consolidées) mais pas toujours faits dans le même ordre. Donc ça finit bien en
90Z mais parfois le code retour n’est pas le même. En terme de valorisation ou de GHM,
ça ne change rien.

Autre chose, les suppléments de radiothérapie ne sont pas implémentés. Il sont simples à
ajouter, je n'ai pas pris le temps de le faire car il n'y a pas de radiothérapie au CHU,
si besoin je peux les ajouter assez rapidement.

Les numéros FINESS du FICUM et des RSS sont ignorés, et donc les autorisations globales
(comme l'autorisation 30 pour les IOA complexes) peuvent être appliquées à l'ensemble des
RUM et pas juste à deux dont le FINESS match. Le CHU n'avait pas besoin de cela et je
n'ai pas pris le temps de supporter comme il faut les FINESS, si besoin ça peut être
ajouté rapidement.
