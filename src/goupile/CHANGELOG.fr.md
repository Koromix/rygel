# Goupile changelog

## Goupile 3.7

### Goupile 3.7.1 (2024-11-07)

- Ajustement des contraintes par défaut des mots de passe :
  * Modérée pour les utilisateurs normaux et administrateurs
  * Difficile pour les super-administrateurs

### Goupile 3.7.0 (2024-10-31)

- Modification de l'objet `thread` dans les scripts : `thread['form']` devient `thread.entries['form']`
- Prise en charge des scripts personnalisés au niveau des catégories
- Prise en charge du contenu personnalisé dans les éléments du formulaire
- Prise en charge des icônes de tuiles de formulaire personnalisées
- Augmentation de la hauteur des barres de menu supérieure et inférieure
- Centrage des boutons d'action sur mobile
- Afficher l'icône d'accueil dans le menu principal
- Rendre les catégories de menu cliquables
- Passer à des boutons espacés pour les énumérations
- Faire défiler vers le haut lors d'un changement de page
- Simplification de l'enchaînement automatique des formulaires
- Prise en charge des classes personnalisées pour la plupart des widgets
- Correction de l'espace vide au bas des formulaires sur les mobiles
- Amélioration du style et du texte des boîtes d'erreur
- Correction de l'attribut « changed » cassé pour les widgets
- Correction du widget `form.time()` cassé dans l'ancien Goupile
- Correction des balises d'erreur prématurées avec les erreurs retardées
- Réduction des statuts annotés disponibles en mode verrouillé
- Ciblage de JS ES 2022 pour lae support `top-level await`
- Introduction d'un système d'inscription avec courrier électronique

## Goupile 3.6

### Goupile 3.6.4 (05/09/2024)

- Correction des panneaux affichés par défaut lors de l'assemblage et de l'ouverture d'URL
- Correction d'une boucle infinie pendant l'export des données en v3
- Correction d'une erreur serveur (FOREIGN KEY error) lors de l'enregistrement de données dans des projets multicentriques
- Support amélioré de l'option `homepage` de l'application
- Correction du bouton « Publier » désactivé après l'ajout d'un fichier
- Passage au nouveau code de serveur HTTP dans goupile
- Utilisation de mimalloc sur les systèmes Linux et BSD

### Goupile 3.6.3 (26/07/2024)

- Correction d'un crash possible lors de l'envoi de mails
- Mise à disposition de fonctions de fonctions de hachage et CRC32 aux scripts
- Mise à disposition de fonctions utilitaires aux scripts

### Goupile 3.6.2 (11/07/2024)

- Correction de bugs de navigation dans goupile (notamment le bouton "Ajouter" qui fonctionne mal)

### Goupile 3.6.1 (10/07/2024)

- Exposition de l'objet thread dans les scripts pour accéder aux données d'autres pages
- Retrait de l'affichage des marqueurs obligatoires sur les widgets de calcul

### Goupile 3.6.0 (28/06/2024)

- Ajout du système de `summary` remplacer les anciens identifiants HID
- Afficher la liste des données lorsque des utilisateurs normaux se connectent
- Correction de l'erreur 404 pour l'export des instances v2
- Correction de divers bugs concernant le fonctionnement hors ligne et la synchronisation des profils
- Correction du nom d'utilisateur non défini dans dans le dialogue de changement initial de mot de passe
- Réparation de la fonctionnalité de reconnexion automatique après expiration
- Correction d'une erreur de migration de base de données à partir de certaines versions de Goupile
- Correction de quelques erreurs de parsage dans l'API de sauvegarde
- Correction du chargement infini pour les erreurs de démarrage tardif
- Nettoyage du code lié au verrouillage côté client

## Goupile 3.5

### Goupile 3.5.3 (20/06/2024)

- Réintroduction de l'attribut `lock` pour les tokens de connexion
- Correction de divers bugs de verrouillage côté client
- Permet aux scripts d'utiliser la directive `until` de lit-html

### Goupile 3.5.2 (20/06/2024)

- Correction des libellés de widgets non stylisés dans les anciennes instances
- Correction du téléchargement de esbuild.wasm lorsque le client ne supporte pas Brotli
- Utilisation du transport `identity` lorsque Content-Encoding est manquant ou vide
- Publication de paquets Debian pour ARM64

### Goupile 3.5.1 (18/06/2024)

- Ajout du système pushPath/popPath pour remplacer l'option `path` des widgets
- Utilisation du code formulaire v2 pour les formulaires en mode de compatibilité
- Retrait du changement de mot de passe forcé lors de la connexion en cas de complexité insuffisante
- Correction de l'impossibilité d'activer le mode développement dans les projets multicentriques
- Mise à disposition du widget PeriodPicker pour les instances v2
- Nettoyage de déclarations de débogage oubliées

### Goupile 3.5.0 (17/06/2024)

- Prise en charge de l'exécution des projets Goupile créés dans la version 2
- Assemblage des scripts de formulaire avec esbuild, pour supporter les imports et mieux détecter les erreurs de syntaxe
- Correction des identifiants HTML non uniques dans les formulaires générés
- Correction de divers bugs dans le mode hors ligne

## Goupile 3.4

### Goupile 3.4.0 (22/03/2024)

- Simplification du menu principal (barre du haut) de Goupile
- Amélioration des pages récpitulatives avec séparation des niveaux et des tuiles
- Amélioration du menu d'actions sur les petits écrans
- Refus des mots de passe trop longs
- Correction de l'erreur de clé d'exportation lorsque les sessions invités sont activées
- Création automatique d'une session lorsque un invité enregistre des données
- Amélioration de la succession des pages en mode séquence automatique

## Goupile 3.3

### Goupile 3.3.6 (08/02/2024)

- Amélioration de la migration des données depuis les domaines goupile2

### Goupile 3.3.5 (26/01/2024)

- Réduction des permissions pour les utilisateurs de la démo en ligne
- Attribution des clés d'export à l'instance maître

### Goupile 3.3.4 (24/01/2024)

- Ajout d'une option pour désactiver les snapshots SQLite
- Désactivation les snapshots SQLite dans les domaines de démonstration

### Goupile 3.3.3 (22/01/2024)

- Ajout d'un avertissement pour les utilisateurs de la démo en ligne
- Correction pour la propriété HTTP définie à partir de la section Defaults par erreur

### Goupile 3.3.2 (22/01/2024)

- Ajout du mode démo à Goupile
- Affichage du nom d'utilisateur dans le dialogue de changement de mot de passe

### Goupile 3.3.1 (22/01/2024)

- Ajout d'un effet de survol aux tuiles
- Désactivation du bouton Publier si le script principal ne fonctionne pas

### Goupile 3.3.0 (20/01/2024)

- Affichage de tuiles de complétion pour les formulaires
- Affichage des colonnes de premier niveau et des statistiques dans le tableau de données
- Affichage du statut d'enregistrement et de complétion dans les menus
- Prise en charge de la configuration des séquences de pages
- Réintroduction du support de l'option de nom de fichier de page personnalité
- Conservation des relations de menu à enfant unique explicitement définies
- Correction des éléments de menu manquants dans certains cas
- Correction du clignotement du panneau lorsque l'on clique sur l'élément de menu de la page en cours
- Abandon du code de connexion hors ligne cassé pour l'instant
- Correction d'un crash lors de la requête pour `/manifest.json`
- Amélioration du code de secours de `main.js` lorsque les sessions invités sont activées

## Goupile 3.2

### Goupile 3.2.3 (17/01/2024)

- Correction du blocage de l'interface utilisateur en mode développement

### Goupile 3.2.2 (16/01/2024)

- Correction du support des sections et notes répétées
- Correction d'une erreur de variable non définie lors de l'export
- Correction de problèmes mineurs de blocage de l'interface

### Goupile 3.2.1 (16/01/2024)

- Ajout du code manquant pour la suppression des enregistrements
- Amélioration de la sélection des colonnes de données visibles
- Gestion du temps de modification d'enregistrement côté serveur
- Embarquement de JavaScriptCore dans le serveur

### Goupile 3.2.0 (14/01/2024)

- Ajustement et ajout d'options pour paramétrer la complexité des mots de passe
- Correction de l'erreur générique 'NetworkError is not defined'
- Correction d'une erreur lors de la création de projets multicentriques
- Correction d'une possible déréférencement de NULL dans le code de connexion de goupile
- Correction de la réinitialisation de l'email et du téléphone dans le panneau d'administration de goupile
