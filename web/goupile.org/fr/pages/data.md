# Tableau de suivi

## Suivi de remplissage

Chaque enregistrement alimente le tableau de suivi, accessible via le panneau de données (encadré en rouge ci-dessous). Ce tableau de suivi est affiché par défaut lorsque vous vous connectez à un projet Goupile.

Le **tableau de suivi** comprend une ligne par enregistrement, ainsi que les colonnes suivantes :

- Un [identifiant séquentiel](identifiers#tid-et-sequence) spécifique de chaque enregistrement
- La date de création de l'enregistrement
- Une colonne par page, avec son statut de remplissage et éventuellement le [summary](identifiers#summary) (si il existe)

<div class="screenshot"><img src="{{ ASSET static/help/data/data.webp }}" height="260" alt=""/></div>

Au-dessus du tableau se trouvent différents filtres relatifs aux [annotations](#filtres-d-affichage) détaillés plus loin.

Les boutons d'export sont situés sous le tableau. Ils ne sont visibles que par les utilisateurs avec les droits correspondants.

## Filtres d'affichage

Lorsqu'une variable est annotée avec un statut, l'enregistrement hérite des statuts de toutes les variables qui le composent. Dans le tableau de suivi, les statuts sont visibles sous la forme de **petits ronds colorés** à côté de l'enregistrement et de la page correspondante.

La capture ci-dessous montre la présence de deux enregistrements (ID 2 et 3) avec un statut « À vérifier » :

<div class="screenshot"><img src="{{ ASSET static/help/data/filter1.webp }}" height="200" alt=""/></div>

Utilisez le **menu « Filtrer »** au-dessus du tableau pour afficher uniquement les enregistrements ayant des pages correspondant à un ou plusieurs statuts spécifiques.

Dans la capture ci-dessus, seuls les enregistrements avec le statut « À vérifier » sont affichés :

<div class="screenshot"><img src="{{ ASSET static/help/data/filter2.webp }}" height="200" alt=""/></div>

# Exports de données

Les données receuillies peuvent être exportées dans un fichier XLSX que vous pouvez ouvrir dans votre tableur ou chargé dans un logiciel d'analyse (comme R).

> [!NOTE]
> Vous pouvez créer des exports manuellement (voir ci-dessous) ou bien paramétrer des [exports automatisés](instances#exports-automatises) dans le module d'administration de Goupile.

## Réaliser un export

Utilisez le bouton « Créer un export » sous le tableau de suivi pour exporter les données au format XLSX.

<div class="screenshot"><img src="{{ ASSET static/help/data/export1.webp }}" height="200" alt=""/></div>

Il est possible d'exporter soit :

- Tous les enregistrements (choix par défaut)
- Les enregistrements créés depuis un export précédent
- Les enregistrements créés ou modifiés depuis un export précédent

<div class="screenshot"><img src="{{ ASSET static/help/data/options.webp }}" height="240" alt=""/></div>

> [!NOTE]
> Il existe deux droits relatifs aux exports : le droit de création d'export (ExportCreate), et le droit de téléchargement d'export (ExportDownload).
>
> Ces droits rendent possible la création d'un utilisateur qui capable de télécharger les exports existants, sans pouvoir en créer de nouveau.

## Format des données

Chaque page est exportée dans un onglet séparé du fichier XLSX, dont le nom correspond à la clé de la page.

> [!NOTE]
> Les pages pour lesquels aucun enregistrement n'est rempli ne sont pas exportées.

Les colonnes de chaque onglet sont organisées comme suit :

- Colonne `__tid` : [identifiant TID](identifiers#tid-et-sequence) de l'enregistrement
- Colonne `__sequence` : [identifiant séquence](identifiers#tid-et-sequence) de l'enregistrement
- Une colonne par variable, nommée à partir de la clé de la variable, en dehors des questions à choix multiples qui sont exportées dans plusieurs colonnes (une modalité par colonne nommée `variable.modalite`)

<div class="screenshot"><img src="{{ ASSET static/help/data/export3.webp }}" height="200" alt=""/></div>

Lorsqu'une page n'est pas remplie pour un enregistrement, aucun ligne n'est présente dans l'onglet correspondant du fichier exporté. Cela signifie que les différentes pages correspondant à un enregistrement donné seront à des lignes différentes dans chaque onglet.

> [!IMPORTANT]
> Vous devez donc utiliser une jointure basée sur la colonne `__tid` pour joindre les données des différents onglets !

## Dictionnaire de variables

Le fichier exporté comporte également deux onglets relatifs au dictionnaire de variables :

- *@definitions* : liste des variables avec le libellé et le type correspondant
- *@propositions* : liste des modalités de réponse des variables à choix unique et à choix multiple

### Liste des variables

L'onglet **@definitions** comporte quatre colonnes, dont la première indique la table (ou page) concernée, la deuxième le nom de la variable, puis le libellé affiché à l'utilisateur et enfin le type de données.

Dans le tableau donné en exemple ci-dessous, deux variables sont définies : une variable d'âge de type numérique (*number*), et une variable à choix unique (*enum*) relative au tabagisme.

| table | variable | label | type |
| --- | --- | --- | --- |
| introduction | age | Âge | number |
| introduction | tabagisme | Tabagisme | enum |

### Modalités de réponse

L'onglet **@propositions** donne la liste des choix possibles pour toutes les variables à choix unique (*enum*) ou à choix multiples (*multi*) recensées dans les données enregistrées.

Dans le tableau donné en exemple ci-dessous, la variable `tabagisme` a 3 modalités de réponse : *actif*, *sevre* et *non*.

| table | variable | prop | label |
| --- | --- | --- | --- |
| introduction | tabagisme | actif | Tabagisme actif |
| introduction | tabagisme | sevre | Tabagisme sevré |
| introduction | tabagisme | non | Non fumeur |
