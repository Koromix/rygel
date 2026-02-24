# Historique des versions

## Goupile 3.12 (bêta)

*Actuellement en bêta*

**Identifiants :**

- Prise en charge des identifiants HID personnalisés, affichés à la place des numéros de séquence. Les HID peuvent être personnalisés en parallèle du *summary* de chaque page.
- Recalcul des données de formulaire lors de l'enregistrement avec le numéro de séquence et les compteurs calculés par le serveur. Cela permet de créer des identifiants personnalisés qui utilisent des valeurs calculées côté serveur, comme le numéro de séquence, un compteur personnalisé ou un identifiant aléatoire.

**Données :**

- Implémentation du statut brouillon (*draft*) pour chaque page enregistrée :

  * Le statut brouillon est automatiquement ajouté lors d'un enregistrement automatique (*autosave*)
  * Le statut brouillon est automatiquement ajouté sur les enregistrements créés ou modifiés en mode conception.
  * Aucun numéro de séquence ou compteur n'est utilisé par ces enregistrements.
  * Un bouton de Confirmation permet de retirer le statut brouillon, notamment pour les enregistrements créés en mode conception.

- Ajout de la prise en charge des erreurs non bloquantes. Cette fonctionnalité permet d'ajouter le statut d'erreur aux pages sans empêcher l'utilisateur d'enregistrer les données, et sans nécessiter l'annotation de la variable concernée.

- Amélioration du comportement de l'enregistrement automatique :

  * L'interface et les statuts affichés ne changent pas lors d'un enregistrement automatique.
  * Les erreurs retardées (*delayed*) ne sont pas déclenchées par l'enregistrement automatique.
  * Le statut brouillon est affecté à la page, puis supprimé lors de l'enregistrement explicite.

- Ajustement du comportement des étiquettes de statut des variables :

  * L'étiquette d'erreur reste active même si un statut utilisateur est défini (par exemple "En attente").
  * Les statuts *Erreur* et *Incomplet* ne sont plus affichés tous les deux sur les variable en cas de valeur manquante (redondant).

**Suivi :**

- Ajustement du tableau de suivi de remplissage :

  * Ajout de la pagination au tableau.
  * Modification des colonnes, affichage des colonnes de niveau 1 et niveau 2 dans le tableau.
  * Affichage de la date et de l'heure dans le tableau de supervision.

- Amélioration des contrôles de filtre :

  * Ajout d'une barre de recherche pour filtrer les enregistrements par numéro de séquence, HID ou la valeur du *summary*.
  * Amélioration de l'ordre et de la cohérence des filtres de statut.
  * Correction de comportements anormaux des boutons de filtre de statut.

- Affichage du statut (pastilles colorées) de chaque entrée et page dans le tableau de supervision.

- Utilisation d'un tri naturel pour les valeurs de séquence/HID au lieu d'un tri alphabétique simple.

- Correction du mauvais fonctionnement du filtre HID avec certaines valeurs dans les anciens projets v2, notamment lorsque des valeurs numériques et des chaînes sont mélangées.

- Correction de petits problèmes d'interface dans les projets v2 (icône manquante, titres de menu non stylés).

**Exports :**

- Utilisation d'une boîte de dialogue unique pour créer et télécharger les exports.

- Le droit *BulkExport* peut désormais être utilisé sans le droit *BulkDownload* pour créer un export et le télécharger immédiatement. Cette combinaison permet de réaliser de nouveaux exports mais pas de télécharger les exports existants.

- Ajout de la prise en charge de modèles d'export XLSX, permettant d'intégrer les données exportées dans un fichier XLSX préexistant, par exemple dans des feuilles spécifiques.

- Amélioration de la boîte de dialogue pour les clés d'API et d'export. Un nouveau code n'est généré que lorsque l'action est explicitement déclenchée, et un bouton Copier (dans le presse-papier) a été ajouté.

**Saisie des données :**

- Amélioration de la conscience situationnelle de l'enregistrement actif :

  * Affichage de la valeur séquence/HID dans le menu principal, et à côté des actions (en haut sur les grands écrans, en bas à gauche sur les petits écrans).
  * Déplacement du bouton "Créer un enregistrement" dans le panneau de données au lieu du menu supérieur.
  * Différenciation de l'icône utilisée pour les nouveaux enregistrements et les brouillons sans valeur de séquence/HID.

- Amélioration de l'interface pour les recueils simples et le remplissage à distance/en ligne :

  * Maintien des boutons d'action en bas pour les recueils invités ou à distance, au lieu d'afficher le bouton *« Enregistrer »* ou *« Continuer »* à droite sur les grands écrans.
  * Indication qu'un enregistrement a été sauvegardé correctement en remplaçant le libellé du bouton *« Enregistrer »* par *« Enregistré »*.
  * La possibilité d'annoter une variable n'est plus mentionnée lorsqu'aucune variable ne peut être annotée.
  * Retrait du préfixe '#' des titres de section.

- Réduction du contenu du menu principal sur les petits écrans pour le rendre lisible dans davantage de projets.

- Les erreurs immédiates et différées sont désormais toutes affichées dès l'ouverture d'un enregistrement existant.

- Correction d'éléments et liens vides apparaissant parfois dans la table des sections d'une page.

- Correction de bugs visuels dans la barre des tâches inférieure, affichées en mode conception et sur les petits écrans (ligne de séparation étrange, problèmes de contraste de texte et de couleur).

**Administration :**

- Renommage des paramètres *Clé* et *Nom* des projets en *Nom* et *Titre*.

- Ajout de la possibilité de modifier la complexité des mots de passe depuis le panneau d'administration.

- Suppression d'autorisations utilisateur inutilisées, comme *BuildBatch*. Une fois la fonctionnalité opérationnelle, elle sera liée à *DataAudit* à la place.

- Correction de l'impossibilité de changer de panneau actif sur les petits écrans.

- Empêchement de la division des anciens projets v2 en sous-projets, car cela ne fonctionne pas correctement et le support de ces projets sera supprimé à terme.

**Scripts de projet :**

- Prise en charge du verrouillage automatique des pages, déclenché lors de l'enregistrement ou après un délai configurable. Seuls les utilisateurs avec la permission *DataAudit* peuvent déverrouiller les enregistrements.

- Remplacement de l'option de page `claim: false` par `forget: true` pour plus de clarté. L'ancienne option est dépréciée mais reste prise en charge pour le moment.

- Amélioration de l'option de page `sequence` :

  * Prise en charge d'une option `sequence` dynamique calculée par une fonction, analogue à l'option `enabled`.
  * Prise en charge de l'utilisation de tableaux comme valeur de `sequence`.
  * Le retour à la page de statut doit désormais être déclaré explicitement.

- Les pages sans stockage de données (sans *store* défini) fonctionnement mieux. Il est possible d'y placer des widgets, mais il n'y a aucune action par défaut, et aucun avertissement n'apparait lorsque la page est fermée malgré la présence de données.

- Suppression du système de raccourcis Goupile inutilisé (`app.shortcut()`).

- Correction de la restauration de fichiers depuis l'historique, qui utilisait parfois le code *bundle* au lieu du script créé par l'utilisateur.

> [!NOTE]
> Les balises `<style>` personnalisées ne fonctionnent plus par défaut à cause du header `Content-Security-Policy`. Vous devez activer l'option « Autoriser les éléments CSS » dans le panneau d'administration pour corriger cela.

**Scripts de formulaire :**

- Améliorations du support des données et objets imbriqués :

  * Prise en charge de spécificateurs de clés en forme de chemin.
  * Correction de la fonction `form.pushPath()` qui était peu utilisable.

- Correction de quelques comportements anorùaux :

  * Correction de la valeur par défaut des widgets qui ne se réinitialise pas lorsque la valeur par défaut revient à *null*.
  * Correction des actions par défaut (comme Enregistrer) qui étaient désactivées par une option `disabled: true` résiduelle après la fin du script.
  * Correction de l'effet rétroactif de `form.pushOptions()` avec `form.sameLine()`.

- Exécution du code de page dans la boucle centrale au lieu de la fonction de rendu. Cela rend le code plus propre, et les erreurs apparaissent même si le panneau d'aperçu est fermé.

- Correction de la restauration de fichiers depuis l'historique, qui utilisait parfois le code *bundle* au lieu du script créé par l'utilisateur.

- Correction de divers bugs de surlignage de widgets en mode conception.

> [!NOTE]
> Les balises `<style>` personnalisées ne fonctionnent plus par défaut à cause du header `Content-Security-Policy`. Vous devez activer l'option « Autoriser les éléments CSS » dans le panneau d'administration pour corriger cela.

**Sécurité :**

Dans le cadre de la subvention NLnet, un audit de sécurité a été réalisé par [Radically Open Security](https://www.radicallyopensecurity.com/). Les failles de sécurité relevées ont en grande partie été corrigées, et la plupart des changements ont été implémentés dans Goupile 3.11 et dans cette version.

Le rapport est disponible ici : [https://goupile.org/static/nlnet/ros_goupile_2025.pdf](https://goupile.org/static/nlnet/ros_goupile_2025.pdf)

- Augmentation de la longueur minimale des mots de passe à 10 caractères au lieu de 8.

- Correction d'une faille de sécurité pouvant permettre à des utilisateurs sans permission *DataRead* de verrouiller des enregistrements qu'ils n'ont pas revendiqués.

- Correction de l'impossibilité de récupérer certaines pages dans des projets divisés même si les utilisateurs sont autorisés à accéder aux sous-projets. Cette régression a été introduite dans Goupile 3.11.

- Obligation de changer le mot de passe pour les utilisateurs root/admin après connexion si leur mot de passe est trop simple par rapport à la complexité requise.

- Définition d'une `Content-Security-Policy` pour empêcher l'injection de scripts et de feuilles de style. Toutefois, les attributs de style en ligne (*style-src-attr*) restent autorisés.

- Définition de l'en-tête `X-Content-Type-Options` sur la valeur "nosniff" pour prévenir les attaques par confusion de type MIME.

- Remplacement de l'utilisation d'en-têtes personnalisés par `Sec-Fetch-Site` et `Origin` (en secours) pour prévenir les attaques CSRF.

- Empêchement de l'injection de caractères de contrôle provenant de chaînes contrôlées par l'utilisateur dans le fichier de log.

**Bugs divers :**

- Mise en liste blanche de plusieurs appels système Linux rarement utilisés dans le bac à sable seccomp pouvant provoquer un crash dans de rares cas :

  * Nouveaux appels autorisés (*restart_syscalls*, *gettimeofday*).
  * Assouplissement du filtre seccomp *ioctl/tty* pour autoriser plus de commandes.

- Correction d'une isolation réseau excessive dans les bacs à sable landlock qui empêchait SMTP de fonctionner sur certains noyaux.

- Correction de la duplication d'instances dans les projets divisés en sous-projets après certaines actions administratives.

- Correction de problèmes de cache PWA causés par des URL d'assets dupliquées et de gros assets (comme `esbuild.wasm`, nécessaire uniquement pour la conception) provoquant une exception dans le service worker.

- Envoi des réponses JSON avec un encodage Zstd rapide si le navigateur le supporte.

- Correction du listener HTTP abandonnant lorsque trop de clients tentaient de se connecter (ce qui provoquait un épuisement des descripteurs de fichiers, et l'échec des appels à `accept`).

- Correction d'une possible régression provoquant une incrémentation excessive du numéro de séquence après modification d'enregistrements existants.

- Utilisation d'une requête POST pour la génération du secret TOTP.

**Traductions :**

- Traduction des pages par défaut des projets Goupile lorsqu'un projet est créé en anglais.

- Traduction des réponses des widgets binaires, qui étaient encore en français même dans les projets en anglais.

- Traduction de la documentation Goupile en anglais : [https://goupile.org/en](https://goupile.org/en).

- Utilisation systématique du libellé *« Fermer »* dans les dialogues au lieu de *« Annuler »*.

- Diverses corrections et améliorations de traduction.

**Distribution :**

- Distribution d'archives source (tarballs) en plus des paquets Debian et RPM existants et des images Docker.

- Nettoyage du fichier de configuration par défaut utilisé par `goupile init`.

## Goupile 3.11

> [!IMPORTANT]
> Le **déploiement et la configuration de Goupile** ont évolué avec cette version. Certains réglages ne sont pas repris automatiquement, et doivent être configurés graphiquement après la mise à jour.
> 
> Après la mise à jour, vous **devez ouvrir le panneau d'administration** de Goupile pour terminer la configuration !

### Goupile 3.11.1

*Sortie le 09/10/2025*

- Évite le rechargement de la page lors de l'enregistrement en mode invité
- Respect de la navigation séquentielle après un enregistrement en mode invité
- Correction du style de titre dans le menu latéral des instances v2

### Goupile 3.11

*Sortie le 06/10/2025*

**Corrections majeures :**

- *[CRITIQUE]* Correction d'un crash (dépassement de mémoire tampon) avec les valeurs numériques JSON excessives
- Protection du code et de l'API des courriels/messages contre les valeurs malformées
- Protection de l'API des SMS/messages contre les numéros de téléphone malformés
- Blocage de l'accès aux fichiers sauf si le mode invité ou hors ligne est activé
- Correction des plantages causés par des appels système manquants dans la liste de confinement (sandbox) seccomp
- Correction du problème des administrateurs non-root pouvant voir la liste complète des projets

**Configuration et installation initiale :**

- Mise à disposition d'images Docker de développement (tag *dev*) et de production (tag *latest*)
- Remplacement de `goupile init` par une étape d'installation graphique
- Remise à zéro et reconfiguration de la clé publique d'archivage lors de la mise à jour
- Remplacement des paramètres INI de bas niveau par une installation et une configuration graphique
- Abandon de divers paramètres INI de bas niveau obscurs et/ou inutilisés
- Supplémentation des paramètres SMTP INI par un paramétrage graphique
- Ajout et adoption par défaut de Linux Landlock pour le confinement du système de fichiers
- Réécriture du code d'initialisation de domaine et d'instance

**Fonctionnalités et améliorations relatives aux formulaires/pages :**

- Ajout de la prise en charge du stockage de fichiers et de blobs
- Ajout d'un widget simplifié pour les fichiers déposés par les répondants
- Ajout d'un système d'inscription personnalisé
- Ajout d'un système de variables publiques pour les études de type DELPHI
- Utilisation de compteurs randomisés non-cachés par défaut
- Ajout de la prise en charge de la sauvegarde automatique pour les instances récentes
- Prise en charge des sessions à jeton à thread unique (single-thread)
- Prise en charge des appels multiples à `form.restart()`
- Amélioration de l'alignement vertical des widgets côte à côte avec des étiquettes vides
- Ajout d'un objet `cache` stable à utiliser dans les scripts de formulaire
- Correction d'erreurs `querySelector()` causée par certains titres de section (en mode conception)

**Stockage des données :**

- Préservation et restauration des objets LocalDate/LocalTime
- Réinitialisation du numéro de séquence lorsque tous les enregistrements sont supprimés
- Réduction de l'apparition non souhaitée de numéros de séquence non consécutifs
- Suppression de l'étiquette de brouillon implicite pour les enregistrements non verrouillés
- Réécriture du format et du code d'annotation des données
- Distribution du code empaqueté pour le script principal et les scripts de pages aux répondants
- Correction d'un bug empêchant la conception de formulaire sur Safari
- Amélioration de la cohérence entre le code et la surbrillance des widgets

**Nouvelles fonctionnalités d'export :**

- Conservation de la liste des exports de données effectués par le passé
- Séparation des permissions d'export en deux (*ExportCreate* et *ExportDownload*)
- Prise en charge de multiples modes d'export (tous les enregistrements, nouveaux enregistrements, enregistrements modifiés)
- Ajout d'exports de données automatiques/planifiés

**Prise en charge multilingue :**

- Traduction de l'interface front-end de Goupile en anglais et en français
- Traduction de l'interface back-end de Goupile en anglais et en français
- Ajout d'un paramètre de langue par instance / par projet

**Modifications et améliorations de l'interface utilisateur :**

- Augmentation de la taille/du contraste des widgets radio et case à cocher
- Amélioration de la navigation entre les panneaux de données/formulaires en mode conception
- Correction de divers problèmes de mise en page des menus sur les petits écrans
- Correction des changements de panneaux lors de l'actualisation de la page
- Grisage des widgets désactivés
- Amélioration de la mise en page de la page racine "/" de Goupile et mention de moi-même ;)

**Autres modifications :**

- Écriture des sorties JSON en flux continu sans mise en mémoire préalable
- Correction de diverses erreurs de migration de schéma d'instance
- Application du mode `synchronous = FULL` sur les bases de données SQLite
- Passage à c-ares (au lieu de libc/libresolv) pour toutes les résolutions de domaine
- Nettoyage de l'initialisation des bibliothèques externes (curl, libsodium, SQLite)
- Utilisation de notre propre routine de génération aléatoire dans la plupart des endroits
- Prise en charge de la liaison du serveur HTTP à une IP spécifique
- Amélioration des performances du serveur HTTP
- Retrait du code Vosklet inutilisé dans goupile

## Goupile 3.10

### Goupile 3.10.2

*Sortie le 05/04/2025*

- Réduction de la longueur des noms des projets démo

### Goupile 3.10.1

*Sortie le 04/04/2025*

- Augmentation de la durée de persistance des projets démo à 7 jours
- Correction de l'erreur 502 précoce lors de la création d'archives volumineuses depuis le panneau d'administration

### Goupile 3.10

*Sortie le 03/04/2025*

- Ajout de `meta.count()` pour des compteurs séquentiels personnalisés
- Ajout de `meta.random()` pour la randomisation
- Restauration des résumés d'enregistrements vides (régression introduite dans Goupile 3.9)
- Amélioration de certaines étapes de migration du schéma de base de données
- Correction d'un espace vide dans la barre d'outils principale
- Suppression des numéros de séquence par entrée (non utilisés)
- Suppression du système d'inscription personnalisé (non utilisé)

## Goupile 3.9

### Goupile 3.9.3

*Sortie le 30/03/2025*

- Ajout de plusieurs appels système à la sandbox seccomp pour la compatibilité ARM64
- Correction du crash au démarrage dans le paquet Debian ARM64
- Fermeture de l'avertissement démo après un délai

### Goupile 3.9.2

*Sortie le 28/03/2025*

- Conservation de l'enregistrement actif après une sauvegarde en mode invité
- Correction de la surbrillance incohérente entre le code et l'aperçu des widgets
- Correction de l'erreur interne avec les enregistrements réclamés
- Améliorations du mode de démonstration :
  * Possibilité de réutiliser l'URL aléatoire du projet
  * Utilisation de JavaScript pour créer l'instance de démo (ce qui évite la création de projets à chaque requête HTTP à la racine)
  * Les projets démo sont purgés après 2 jours
- Nettoyage du code d'authentification
- Correction de la compilation cassée sous Windows
- Réduction du zoom automatique sur iPhone lors de la saisie

### Goupile 3.9.1

*Sortie le 21/02/2025*

- Correction de la valeur de séquence dans les exports

### Goupile 3.9

*Sortie le 21/02/2025*

- Correction des incohérences de statut avec les pages désactivées
- Mise en cohérence de l'identifiant séquentiel d'enregistrement
- Affichage systématique du bouton Continuer sur les pages séquencées
- Ajout d'un paramètre global pour la fonctionnalité d'annotation
- Correction de diverses incohérences dans les tags de variables
- Conservation des valeurs de séquence et de HID après migration des anciennes instances
- Simplication des numéros de version

## Goupile 3.8

### Goupile 3.8.3

*Sortie le 17/02/2025*

- Correction de l'erreur « page is undefined » lors de la navigation sur certaines pages
- Suppression de l'heuristique de masquage des colonnes dans l'ancien exportateur
- Inclusion des colonnes null de valeurs multiples dans les exportations existantes
- Conserver le suffixe ULID dans les tables d'exportation virtuelles
- Correction des erreurs FKEY de migration avec les sous-projets et les enregistrements supprimés
- Correction d'une erreur SQL lors de la restauration d'une archive
- Ajout d'une API pour super-utilisateurs pour quitter le processus goupile
- Envoi des détails des erreur serveur internes aux utilisateurs root

### Goupile 3.8.2

*Sortie le 29/12/2024*

- Correction d'une possible boucle infinie après l'échec d'une requête HTTP
- Retrait de l'action d'oubli lorsque la sauvegarde automatique est utilisée
- Refus des corps de requête HTTP compressés

### Goupile 3.8.1

*Sortie le 20/12/2024*

- Retrait du bouton « Enregistrer » si le bouton « Suivant » est affiché
- Correction d'un bug de concurrence entre la sauvegarde automatique et la confirmation dans les anciennes instances
- Utilisation de la nouvelle icône de conception (plutôt que code) dans les anciennes instances
- Mise à jour des exigences relatives à la version du navigateur

### Goupile 3.8.0

*Sortie le 20/12/2024*

- Suppression du droit utilisateur *New* en faveur de l'option de page `claim`
- Amélioration de la prise en charge de l'activation dynamique des pages avec `enabled`
- Ajout d'une confirmation optionnelle lors de l'enregistrement (option `confirm`)
- Affichage du suffixe d'état dans le menu de gauche même en verrouillage
- Utilisation d'icônes différentes pour le menu de conception et le panneau de code
- Correction d'une erreur empêchant la suppression de l'email ou du téléphone d'un utilisateur
- Correction du blocage du chargement lorsque l'enregistrement n'existe pas
- Correction de la boîte de dialogue erronée « confirmer les modifications » après un changement de code
- Suppression du filtre et des options d'export mal conçus
- Réduction des erreurs inattendues causées par des modifications de fichiers en double
- Réduction des erreurs (sans danger) « single SQLite value » lors de la modification d'anciens enregistrements

## Goupile 3.7

### Goupile 3.7.7

*Sortie le 23/11/2024*

- Correction des instances dupliquées (régression introduite dans Goupile 3.7.3)

### Goupile 3.7.6

*Sortie le 16/11/2024*

- Activation du mode conception par défaut pour les instances de démonstration
- Bascule automatique de l'éditeur vers l'onglet formulaire sauf si modifié explicitement
- Correction d'un crash possible lors d'un changement de mode

### Goupile 3.7.5

*Sortie le 12/11/2024*

- Correction d'un crash récurrent avec le mode de démonstration

### Goupile 3.7.4

*Sortie le 12/11/2024*

- Correction d'un timeout HTTP prématuré lors de l'export de données en v2

### Goupile 3.7.3

*Sortie le 12/11/2024*

- Correction d'un crash après changement de mot de passe et activation TOTP
- Correction des sous-projets mal rangés dans le panneau d'administration
- Correction d'erreurs avec des attributs utilisateur email et téléphone vides

### Goupile 3.7.2

*Sortie le 12/11/2024*

- Ajout du lien manquant vers le panneau d'administration
- Utilisation de boutons d'export sur les instances v2

### Goupile 3.7.1

*Sortie le 07/11/2024*

- Ajustement des contraintes par défaut des mots de passe :
  * Modérée pour les utilisateurs normaux et administrateurs
  * Difficile pour les super-administrateurs

### Goupile 3.7.0

*Sortie le 31/10/2024*

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
- Ciblage de JS ES 2022 pour le support `top-level await`
- Introduction d'un système d'inscription avec courrier électronique

## Goupile 3.6

### Goupile 3.6.4

*Sortie le 05/09/2024*

- Correction des panneaux affichés par défaut lors de l'assemblage et de l'ouverture d'URL
- Correction d'une boucle infinie pendant l'export des données en v3
- Correction d'une erreur serveur (FOREIGN KEY error) lors de l'enregistrement de données dans des projets multicentriques
- Support amélioré de l'option `homepage` de l'application
- Correction du bouton « Publier » désactivé après l'ajout d'un fichier
- Passage au nouveau code de serveur HTTP dans goupile
- Utilisation de mimalloc sur les systèmes Linux et BSD

### Goupile 3.6.3

*Sortie le 26/07/2024*

- Correction d'un crash possible lors de l'envoi de mails
- Mise à disposition de fonctions de fonctions de hachage et CRC32 aux scripts
- Mise à disposition de fonctions utilitaires aux scripts

### Goupile 3.6.2

*Sortie le 11/07/2024*

- Correction de bugs de navigation dans goupile (notamment le bouton "Ajouter" qui fonctionne mal)

### Goupile 3.6.1

*Sortie le 10/07/2024*

- Exposition de l'objet thread dans les scripts pour accéder aux données d'autres pages
- Retrait de l'affichage des marqueurs obligatoires sur les widgets de calcul

### Goupile 3.6.0

*Sortie le 28/06/2024*

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

### Goupile 3.5.3

*Sortie le 20/06/2024*

- Réintroduction de l'attribut `lock` pour les tokens de connexion
- Correction de divers bugs de verrouillage côté client
- Permet aux scripts d'utiliser la directive `until` de lit-html

### Goupile 3.5.2

*Sortie le 20/06/2024*

- Correction des libellés de widgets non stylisés dans les anciennes instances
- Correction du téléchargement de esbuild.wasm lorsque le client ne supporte pas Brotli
- Utilisation du transport `identity` lorsque Content-Encoding est manquant ou vide
- Publication de paquets Debian pour ARM64

### Goupile 3.5.1

*Sortie le 18/06/2024*

- Ajout du système pushPath/popPath pour remplacer l'option `path` des widgets
- Utilisation du code formulaire v2 pour les formulaires en mode de compatibilité
- Retrait du changement de mot de passe forcé lors de la connexion en cas de complexité insuffisante
- Correction de l'impossibilité d'activer le mode développement dans les projets multicentriques
- Mise à disposition du widget PeriodPicker pour les instances v2
- Nettoyage de déclarations de débogage oubliées

### Goupile 3.5.0

*Sortie le 17/06/2024*

- Prise en charge de l'exécution des projets Goupile créés dans la version 2
- Assemblage des scripts de formulaire avec esbuild, pour supporter les imports et mieux détecter les erreurs de syntaxe
- Correction des identifiants HTML non uniques dans les formulaires générés
- Correction de divers bugs dans le mode hors ligne

## Goupile 3.4

### Goupile 3.4.0

*Sortie le 22/03/2024*

- Simplification du menu principal (barre du haut) de Goupile
- Amélioration des pages récpitulatives avec séparation des niveaux et des tuiles
- Amélioration du menu d'actions sur les petits écrans
- Refus des mots de passe trop longs
- Correction de l'erreur de clé d'exportation lorsque les sessions invités sont activées
- Création automatique d'une session lorsque un invité enregistre des données
- Amélioration de la succession des pages en mode séquence automatique

## Goupile 3.3

### Goupile 3.3.6

*Sortie le 08/02/2024*

- Amélioration de la migration des données depuis les domaines goupile2

### Goupile 3.3.5

*Sortie le 26/01/2024*

- Réduction des permissions pour les utilisateurs de la démo en ligne
- Attribution des clés d'export à l'instance maître

### Goupile 3.3.4

*Sortie le 24/01/2024*

- Ajout d'une option pour désactiver les snapshots SQLite
- Désactivation les snapshots SQLite dans les domaines de démonstration

### Goupile 3.3.3

*Sortie le 22/01/2024*

- Ajout d'un avertissement pour les utilisateurs de la démo en ligne
- Correction pour la propriété HTTP définie à partir de la section Defaults par erreur

### Goupile 3.3.2

*Sortie le 22/01/2024*

- Ajout du mode démo à Goupile
- Affichage du nom d'utilisateur dans le dialogue de changement de mot de passe

### Goupile 3.3.1

*Sortie le 22/01/2024*

- Ajout d'un effet de survol aux tuiles
- Désactivation du bouton Publier si le script principal ne fonctionne pas

### Goupile 3.3.0

*Sortie le 20/01/2024*

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

### Goupile 3.2.3

*Sortie le 17/01/2024*

- Correction du blocage de l'interface utilisateur en mode développement

### Goupile 3.2.2

*Sortie le 16/01/2024*

- Correction du support des sections et notes répétées
- Correction d'une erreur de variable non définie lors de l'export
- Correction de problèmes mineurs de blocage de l'interface

### Goupile 3.2.1

*Sortie le 16/01/2024*

- Ajout du code manquant pour la suppression des enregistrements
- Amélioration de la sélection des colonnes de données visibles
- Gestion du temps de modification d'enregistrement côté serveur
- Embarquement de JavaScriptCore dans le serveur

### Goupile 3.2.0

*Sortie le 14/01/2024*

- Ajustement et ajout d'options pour paramétrer la complexité des mots de passe
- Correction de l'erreur générique 'NetworkError is not defined'
- Correction d'une erreur lors de la création de projets multicentriques
- Correction d'une possible déréférencement de NULL dans le code de connexion de goupile
- Correction de la réinitialisation de l'email et du téléphone dans le panneau d'administration de goupile

# Feuille de route

## Goupile 3.13

- Amélioration de l'API d'export avec les métadonnées de colonnes.
- Amélioration de l'export des objets imbriqués et des tableaux.
- Prendre en charge une API et une fonctionnalité de recalcul en masse.
- Amélioration et documentation de l'API d'import.
- Export / import de la structure du projet dans un fichier ZIP.

## Goupile 3.14

- Support de groupes de pages et de permissions afin d'envoyer des sous-ensembles de métadonnées et de données des enregistrements. Cela permettra par exemple l'implémentation d'un module de contact.
- Prise en charge de l'authentification SSO avec OIDC.
- Prise en charge de l'approvisionnement (*provisiong*) des utilisateurs via SCIM.

## Goupile 3.15

- Prendre en charge de codes d'erreur pour un système de *queries*.
- Présentation d'un tableau clair des erreurs bloquantes et non bloquantes (*query monitor*).
- Amélioration des outils d’audit des données : meilleur journal de modifications, fusion et séparation de données.

## Goupile 3.16

- Réécriture de la prise en charge du mode hors ligne avec une nouvelle interface.
- Restriction du hors ligne à la saisie de nouvelles données, suppression du système de miroir.

## Goupile 3.17

- Mise en œuvre d'une fusion correcte des données lorsqu’elles sont modifiées par plusieurs utilisateurs en parallèle.
- Prise en charge de l'édition collaborative des scripts de formulaire.
- Utilisation des WebSockets pour la mise à jour en temps réel des données et des scripts, et voir qui fait quoi.

## Goupile 4 (2027)

- Abandon de la prise en charge des projets v2.
