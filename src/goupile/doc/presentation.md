Conception de formulaires : Goupile
======

# Synopsis

Goupile est un éditeur de formulaires pour le recueil de données pour la recherche. Il s'agit d'une application web utilisable sur ordinateur, sur mobile et hors ligne.

L'association InterHop est l'éditeur du logiciel libre Goupile.

# Présentation

Goupile est un outil de conception de *Clinical Report Forms* (CRF), libre et gratuit, qui s'efforce de rendre la création de formulaires et la saisie de données à la fois puissantes et faciles.

Le *Clinical Report Form* (CRF) est un document (traditionnellement papier) destiné à recueillir les données des personnes incluses dans un projet de recherche, que ce soit au moment de l'inclusion dans un projet de recherche ou, par la suite, dans le suivi de la personne.
Ces documents sont de plus en plus remplacés (ou complétés) par des portails numériques, on parle alors d'eCRF (''e'' pour *electronical*).

![](https://goupile.fr/static/screenshots/overview.webp)

L'instance de démo à l'adresse https://goupile.fr/demo est administrée par l'association InterHop. Attention l'instance de démo n'est pas certifiée pour la santé ; uniquement des données fictives peuvent être récoltées.

# Histoire de Goupile

## Pourquoi ?

Souvent, les données sont recueillies sur dans des fichiers Excel, sont stockés sur les machines personnelles des chercheurs. D'autres utilisent des outils en ligne, comme "Google Sheets" ou "Office 365". Les données sont donc stockées chez les GAFAMs, sur des serveurs non adaptés aux données de santé, avec tous les risques que cela comporte.

Il existe pourtant des logiciels spécifiquement développés pour la santé. Ces outils proposent une approche glisser/déposer et/ou un métalangage, à partir d'une palette de champs de saisie (ex : champs date, nombre, texte). Cette approche est facile et intuitive, mais elle limite les possibilités de développement. De plus, ces outils sont souvent lourds à mettre en place, et généralement utilisés dans le cadre d'études financées.

## Pour qui ?

Nous !
Les membres d'InterHop sont aussi des utilisateurs quotidiens de Goupile. Nous développons aussi nos propres formulaires avec cet outil, et ils sont testés et utilisés par nos proches collaborateurs (et amis).

Les chercheurs ! 
Pour des études prospectives, rétrospectives, mono-centriques ou multicentriques.

Les étudiants en santé !
Les projets de thèses et mémoires nécessitent généralement de recueillir des données. Ce sont des projets relativement simples techniquement et sont notre cible immédiate pour plusieurs raisons : ils n’ont pas besoin de fonctionnalités très complexes, le nombre de thèses (par an) est de plusieurs centaines et les alternatives proposées ne sont ni libres ni sécurisées. 

## Le nom

Le nom Goupile n'a aucune signification particulière. Lorsque Niels Martignène a créé le dossier du projet, il lui fallait un nom. Il a demandé à Apolline (sa petite amie qui était à côté), grande amoureuse des renards, et le nom "goupil" est sorti tout seul.

Après quelques mois, le "e" a été rajouté pour faciliter la recherche sur Google et avoir un nom de domaine disponible.

# Fonctionnalités

## Principes de base

Goupile permet de concevoir un eCRF avec une approche un peu différente des outils habituels, puisqu'il s'agit de programmer le contenu des formulaires, tout en automatisant les autres aspects communs à tous les eCRF : 
- Types de champs préprogrammés et flexibles, 
- Publication des formulaires,
- Enregistrement et synchronisation des données,
- Recueil en ligne et hors ligne, sur ordinateur, tablette ou mobile,
- Gestion des utilisateurs et droits

En plus des fonctionnalités habituelles, nous nous sommes efforcés de réduire au maximum le délai entre le développement d'un formulaire et la saisie des données.

## Je code à gauche, je vois le résultat à droite

Les changements réalisés dans l'éditeur sont immédiatement exécutés et le panneau d'aperçu à droite affiche le formulaire qui en résulte. Il s'agit de la même page qui sera présentée à l'utilisateur final. À ce stade, il est déjà possible de saisir des données pour tester le formulaire, mais aussi de les visualiser et de les exporter.

![](https://goupile.fr/static/screenshots/editor.webp)

Le fait de programmer nous donne beaucoup de possibilités, notamment la réalisation de formulaires complexes, sans sacrifier la simplicité pour les formulaires usuels.

Goupile n'est pas basé sur un métalangage (contrairement aux outils classiques dans ce domaine) mais utilise le langage Javascript. Ainsi, il y a très peu de limites de développement de nouveaux champs de saisie ou de fonctionnalités.

Nous tenons à vous rassurer sur la complexité. Vous pouvez commencer à créer votre formulaire sans connaissances en programmation. En effet, Goupile fournit un ensemble de fonctions prédéfinies qui permettent de créer les champs de saisie (champ texte, numérique, liste déroulante, échelle visuelle, date, etc.) très simplement.

![](https://goupile.fr/static/screenshots/instant.png)

## Suivi du recueil

Une fois le formulaire prêt, les panneaux de suivi vous permettent de suivre l'avancée de la saisie, de visualiser les enregistrements et d'exporter les données.

![](https://goupile.fr/static/screenshots/data.webp)

## Autres

Toutes les pages de Goupile sont *responsive*, ce qui signifie qu'elles s'adaptent à la navigation sur mobile, tablette et ordinateur de bureau.

Goupile a un mode hors ligne en cas de coupure du réseau. Les données sont synchronisées lorsque la connexion est à nouveau disponible.

# Développement

Goupile est un projet porté et développé par l'association InterHop.org. Niels Martignène est le développeur principal de Goupile.

L'équipe d'InterHop est en support pour héberger Goupile sur des serveurs certifiés HDS (Hébergeurs de données de santé), pour accompagner les utilisateurs dans le développement de leurs formulaires et pour développer de nouvelles fonctionnalités. Enfin, InterHop propose également une aide pour les questions relatives à la protection et au Réglement général sur la protection des données (RGPD).

# Hébergement

Dans la mesure où Goupile est un logiciel libre, deux possibilités s'offrent aux utilisateurs.trices : 
- Utiliser et installer Goupile localement dans leur centre. Dans ce cas il.elle.s doivent gérer tous les aspects d'hégergement.
- Utiliser le service clé en main fourni par l'association InterHop via les serveurs loués chez notre hébergeur de données de santé, GPLExpert. InterHop installe Goupile sur ces serveurs et maintient le logiciel à jour.

> Voir fiche – [Notre hébergeur de données de santé](https://pad.interhop.org/KgkE7laFSDqvAKhiAsSqvg?both#)

Dans tous les cas, les utilisateurs.rices peuvent nous solliciter pour le développement de nouvelles fonctionnalités.

# Licence

Goupile est un logiciel libre et opensource délivré sous licence libre : licence AGPL 3.0.

Le code source est disponible gratuitement en ligne : https://framagit.org/interhop/goupile
