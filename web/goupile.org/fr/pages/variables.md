# Objets disponibles

Goupile met à disposition plusieurs objets que vous pouvez utiliser dans les scripts. Certains ont déjà été présentés dans cette documentation :

- L'objet `app` est utilisé pour définir la [structure du projet](app) dans le script de projet, ou bien pour lire la structure du projet dans le scripts de page.
- L'objet `form` permet d'ajouter les [différents widgets](widgets) dans les scripts de page.
- L'objet `meta` permet d'accéder aux fonctions relatives aux [identifiants](identifiers), au summary, à la randomisation.

Il en existe d'autres que nous allons aborder sur cette page, en particulier :

- L'objet `thread` permet d'accéder au statut de l'enregistrement et aux [données des différentes pages](#donnees-d-autres-pages).
- L'objet `profile` contient des informations relatives au [profil de l'utilisateur](#profil-de-l-utilisateur) connecté.

# Données d'autres pages

## Contextes d'utilisation

L'objet `thread` peut être utilisé par les scripts de page, afin de récupérer des valeurs d'autres pages. Vous pouvez par exemple l'âge d'un participant, rempli dans la page d'inclusion, dans une autre page afin d'ajuster le formulaire et les questions affichées.

Il n'est pas disponible lorsque le script de projet est exécuté. En revanche, l'objet thread est disponible lors de l'évaluation des propriétés dynamiques des pages. C'est le cas de l'option `enabled` qui est utilisée pour définir des [pages conditionnelles](app#pages-conditionnelles)).

## Propriétés de thread

L'objet thread contient les propriétés suivantes :

```js
{
    tid: "01K8RCXZ4BPS272HHHWFM2T0S8", // Identifiant aléatoire de type TID qui identifie chaque enregistrement
    saved: false,                      // Au moins une page de l'enregistrement est enregistrée (et donc matéralisée en base de données)
    locked: false,                     // Vaut true si l'enregistrement est verrouillé, soit de manière automatique ou par un utilisateur avec droit d'audit

    entries: {},                       // Contient les entrées correspondant aux différentes stores/pages (voir ci-dessous)
    data: {},                          // Contient les données correspondant aux différentes stores/pages (voir ci-dessous)

    counters: {}                       // Objet content les valeurs des différents compteurs (monotones ou randomisés) non secrets (voir ci-dessous)
}
```

### thread.entries

> [!NOTE]
> Les pages de formulaire créées par `app.form()` utilisent chacun un *store* (magasin) dédié, qui porte par défaut le même nom que la page elle-même.
>
> Pour simplifier la documentation relative à `thread`, nous partons du principe que chaque page stocke ses données dans un *store* du même nom.

Utilisez la propriété `thread.entries.name` pour accéder aux différents *stores*, c'est à dire aux différentes pages dans la majorité des cas d'utilisation de Goupile. Les données et métadonnées relatives à une page nommée « inclusion » seront donc disponibles dans `thread.entries.inclusion`.

Chaque entrée a la structure suivante :

```js
{
    summary: null,        // Valeur du summary de la page
    ctime: 1761750347739, // Date de création de l'entrée (timestamp Unix en millisecondes)
    mtime: 1761750347739, // Date de modification de l'entrée (timestamp Unix en millisecondes)
    data: {},             // Objet d'accès aux données de la page (ou store) correspondante
    anchor: null,         // Contient null pour les entrées non enregistrées, ou un nombre le cas contraire
    store: "nom",         // Nom du store, identique à la clé de la page (dans la majorité des cas)
    tags: [],             // Liste des tags agrégés à partir des tags des différentes variables
    siblings: [],         // Liste des autres entrées du même store pour les relations 1-à-N
}
```

Pour accéder aux variables d'une autre page, vous pouvez utilisez la propriété `data` de l'entrée correspondante. Par exemple pour récupérer l'âge renseignée en page d'inclusion, la syntaxe serait `thread.entries.inclusion.data.age`.

> [!IMPORTANT]
> L'entrée correspondante à une store (ou page) est `null` pour les pages qui ne sont pas encore enregistrées. Faites attention en accédant à la propriété d'une page qui pourrait ne pas exister.
>
> Utilisez les opérateurs de chaînage optionnel pour ces situations. Pour l'exemple lié à l'âge ci-dessus, utilisez cette syntaxe : `thread.entries.inclusion?.data?.age`. Cela évitera d'avoir une erreur quand la page d'inclusion n'existe pas, la valeur de l'âge sera `null`.

### thread.data

Par rapport à `thread.entries`, la propriété `thread.data` simplifie l'accès aux données des différentes pages :

- Les variables des pages sont directement exposées dans l'objet du *store* correspondant.
- Les pages non enregistrées sont matérialisées malgré tout par un objet vide plutôt que par la valeur `null`.

Pour accéder à la variable âge d'une page d'inclusion, ces deux syntaxes sont équivalentes :

```js
let age1 = thread.entries.inclusion?.data?.age
let age2 = thread.data.inclusion.age

console.log(age1 == age2) // Affiche true
```

### thread.counters

Goupile propose des [compteurs personnalisés](identifiers#compteurs-personnalises) qui peuvent être visibles ou secrets.

La valeur des compteurs visibles est disponible via la propriété `thread.counters`. Vous pouvez par exemple récupérer la valeur du compteur nommé « groupe » (par exemple) avec la syntaxe `thread.counters.groupe`.

# Profil de l'utilisateur

## Contextes d'utilisation

L'objet `profile` peut être utilisé dans le script de projet et dans les scripts de page. Il contient des informations sur l'utilisateur : son userid, son nom d'utilisateur, et les droits dont il dispose sur le projet.

Vous pouvez utiliser cet objet pour différentes fonctionnalités, par exemple :

- Ajouter une page uniquement disponible pour les utilisateurs avec droit d'audit (`data_audit: true`)
- Renseigner le champ d'un formulaire avec le nom d'utilisateur

## Propriétés disponibles

L'objet de profil a la structure suivante :

```js
{
    userid: 42,        // Identifiant interne de l'utilisateur
    username: "niels", // Nom de l'utilisateur
    develop: false,    // Vaut true en mode conception, false en mode normal
    online: true,      // Vaut true en mode en ligne, false quand l'application fonctionne hors ligne
    permissions: {}    // Objet où chaque clé correspond à un droit (par exemple data_read) et la valeur true ou false
    root: false        // Vaut true si l'utilisateur est un super-administrateur
}
```
