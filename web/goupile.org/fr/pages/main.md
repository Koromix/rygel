# Interface de recueil

## Disposition de l'interface

Lorsque vous ouvrez un nouveau projet Goupile, vous arriverez sur l'interface suivante :

<div class="screenshot"><img src="{{ ASSET static/help/instance/main.webp }}" height="240" alt=""/></div>

### Menu principal

Le menu principal correspond à la *barre noire en haut de la page*, celle-ci contient principalement :

1. Le **menu de conception** qui permet d'activer ou de désactiver le [mode de conception](#environnement-de-conception), dans lequel vous pouvez modifier les formulaires.
2. Les **boutons de panneaux** qui permettent de choisir entre le [tableau de suivi](data#tableau-de-suivi), la page de formulaire ou bien le mode en double panneaux.
3. Le **titre du projet**, dans les [études multi-centriques](instances#projets-multi-centriques) celui-ci devient un menu déroulant qui permet d'accéder aux différents centres.
4. Le **menu utilisateur** qui permet de modifier vos paramètres de profil (comme le changement de mot de passe), d'accéder au panneau d'administration ou bien de vous déconnecter.

### Tableau de suivi

Le panneau de suivi comporte deux éléments principaux :

5. La **liste des enregistrements** qui comprend plusieurs colonnes : un [identifiant séquentiel] d'enregistrement, la date de création et une colonne de statut par page.
6. Les **boutons d'export** permettent de créer un nouvel export ou de récupérer les exports déjà réalisés

> [!NOTE]
> Consultez la documentation sur le [suivi et les exports](data) pour en savoir plus à ce sujet, notamment sur les filtres d'affichage qui sont situés au-dessus de la liste.

## Statut et pages

En cliquant sur le bouton « Créer un enregistrement » ou bien sur l'icône du panneau de formulaire (capture ci-dessous) vous accéderez au formulaire en cours :

<div class="screenshot"><img src="{{ ASSET static/help/instance/view.webp }}" height="50" alt=""/></div>

### Statut de remplissage

Sur un projet par défaut, ceci ouvrira la page de statut de l'enregistrement, qui affiche l'état de remplissage des différentes pages qui composent le projet. Les **tuiles** correspondent aux différentes pages :

<div class="screenshot"><img src="{{ ASSET static/help/instance/form.webp }}" height="240" alt=""/></div>

Les différents éléments de la page de statut s'ajustent en fonction de l'état de remplissage :

<div class="screenshot"><img src="{{ ASSET static/help/instance/status.webp }}" height="180" alt=""/></div>

### Page de formulaire

Les pages de formulaire correspondent aux pages que vous développez, elles ont cet aspect avec le projet par défaut :

<div class="screenshot"><img src="{{ ASSET static/help/instance/page1.webp }}" height="440" alt=""/></div>

Le **menu s'ajuste automatiquement** en fonction de la complexité du projet. Lorsqu'un formulaire contient peu de pages, celles-ci sont accessibles via le menu principal, comme sur la capture ci-dessous (encadré rouge).

Pour les projets plus complexes (avec de nombreuses pages ou des groupes de pages), et si votre écran est assez large, un menu vertical apparait à gauche du formulaire :

<div class="screenshot"><img src="{{ ASSET static/help/instance/page2.webp }}" height="380" alt=""/></div>

# Mode conception

## Interface de conception

Utilisez le menu de conception pour activer le mode de conception, qui vous permettra de paramétrer votre projet et de créer les différentes pages.

<div class="screenshot"><img src="{{ ASSET static/help/instance/conception.webp }}" height="80" alt=""/></div>

Une fois le mode activé, les panneaux s'ajustent et vous aurez accès à la vue en double panneaux avec le **code à gauche et le formulaire à droite** :

<div class="screenshot"><img src="{{ ASSET static/help/instance/develop.webp }}" height="340" alt=""/></div>

Lorsque vous êtes sur une page de formulaire, deux onglets de code sont disponibles :

- L'onglet **Projet** permet de modifier le [script principal](app), ce script définit l'organisation globale du projet et des différentes pages
- L'onglet **Formulaire** correspond au [contenu de la page](widgets) en cours

<div class="screenshot"><img src="{{ ASSET static/help/instance/tabs.webp }}" height="160" alt=""/></div>

> [!NOTE]
> Si vous ne voyez pas l'onglet « Formulaire », pas de panique ! Vous êtes probablement sur la page de statut, qui n'a pas de code (par défaut). Ouvrez une page de formulaire et l'onglet apparaitra.

### Onglet Projet

Les modifications du code de projet ne s'appliquent pas immédiatement; vous devez **cliquez sur le bouton « Appliquer » pour tester** et visualiser les changements.

La page se rafraichit lorsque vous appliquez vos modifications.

### Onglet Formulaire

Les modifications faites dans un script de page **sont appliquées en temps réel**.

Le ou les widgets en cours de modifications dans l'éditeur de code sont *encadrés en bleu* dans le panneau d'aperçu, pour vous aider à faire le lien entre le code et le contenu généré.

<div class="screenshot"><img src="{{ ASSET static/help/instance/highlight.webp }}" height="180" alt=""/></div>

## Environnement de conception

En mode Conception, les modifications apportées aux différentes pages **ne sont visibles qu'en mode Conception**. Les autres utilisateurs ne verront les modifications apportées qu'après [publication du projet](publish).

Vous pouvez utiliser le formulaire normalement pendant le développement, et même enregistrer des données.

Faites attention à ne pas mélanger des données de test et des données réelles si vous modifiez le formulaire après sa mise en production. Les données enregistrées en mode de conception utilisent un numéro de séquence, qui **ne sera pas réutilisé** même si l'enregistrement est supprimé !

> [!TIP]
> Une exception existe : si vous **supprimez tous les enregistrements** (par exemple après avoir terminé les tests de votre projet, avant la mise en production), le numéro de séquence est remis à sa valeur initiale et le premier enregistrement aura le numéro de séquence 1.
