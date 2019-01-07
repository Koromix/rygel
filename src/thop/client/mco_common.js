// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_common = {};
(function() {
    const Catalogs = {
        'ccam': {path: 'catalogs/ccam.json', key: 'procedure'},
        'cim10': {path: 'catalogs/cim10.json', key: 'diagnosis'},
        'mco_ghm_roots': {path: 'catalogs/mco_ghm_roots.json', key: 'ghm_root'},
        'mco_errors': {
            concepts: [
                {error: 2, desc: "Incompatibilite sexe-diagnostic principal"},
                {error: 3, desc: "Diagnostic principal incohérent"},
                {error: 4, desc: "Tables endommagées ou erreur dans le parcours de l'arbre"},
                {error: 5, desc: "Diagnostic principal : code insuffisamment précis pour la classification des GHM"},
                {error: 6, desc: "Nombre de séances > 0 et DP n'est pas un motif de séances"},
                {error: 7, desc: "Poids incompatible pour un nouveau-né"},
                {error: 8, desc: "Corruption d'une table"},
                {error: 9, desc: "Table introuvable"},
                {error: 10, desc: "RSS multiunité avec numéro de RSS inconstant"},
                {error: 11, desc: "Numéro de RSS absent"},
                {error: 13, desc: "Date de naissance absente"},
                {error: 14, desc: "Date de naissance non numérique"},
                {error: 15, desc: "Date de naissance improbable par rapport à la date d'entrée"},
                {error: 16, desc: "Code sexe absent"},
                {error: 17, desc: "Code sexe erroné"},
                {error: 19, desc: "Date d'entrée absente"},
                {error: 20, desc: "Date d'entrée non numérique"},
                {error: 21, desc: "Date d'entrée incohérente"},
                {error: 23, desc: "RSS multiunité : chaînage date d'entrée date de sortie incohérent"},
                {error: 24, desc: "Mode d'entrée absent"},
                {error: 25, desc: "Mode d'entrée erroné ou provenance erronée"},
                {error: 26, desc: "Mode d'entrée incorrect ou provenance incorrecte pour commencer un RSS"},
                {error: 27, desc: "RSS multiunité : mode d'entrée incorrect ou provenance incorrecte sur un RUM de suite"},
                {error: 28, desc: "Date de sortie absente"},
                {error: 29, desc: "Date de sortie non numérique"},
                {error: 30, desc: "Date de sortie incohérente"},
                {error: 32, desc: "RUM avec incohérence date de sortie date d'entrée"},
                {error: 33, desc: "Mode de sortie absent"},
                {error: 34, desc: "Mode de sortie erroné, ou destination erronée"},
                {error: 35, desc: "Mode de sortie incorrect ou destination incorrecte pour clore un RSS"},
                {error: 36, desc: "Nombre de séances non numérique"},
                {error: 37, desc: "RSS multiunité : présence de séances sur un des RUM"},
                {error: 39, desc: "Date de naissance incohérente"},
                {error: 40, desc: "Diagnostic principal absent"},
                {error: 41, desc: "Code de diagnostic principal ne respectant pas le format de la CIM10"},
                {error: 42, desc: "Code de diagnostic associé ne respectant pas le format de la CIM10"},
                {error: 43, desc: "Code d'acte ne respectant pas le format de la CCAM"},
                {error: 45, desc: "RSS multiunité : date de naissance inconstante"},
                {error: 46, desc: "RSS multiunité : code sexe inconstant"},
                {error: 49, desc: "RSS multiunité : mode de sortie incorrect ou destination incorrecte pour un RUM autre que le dernier"},
                {error: 50, desc: "Délai de séjour incompatible avec le principe administratif de prestation interétablissement"},
                {error: 51, desc: "Code de diagnostic relié ne respectant pas le format de la CIM10"},
                {error: 52, desc: "Nombre de réalisations d'actes non numérique, ou erroné"},
                {error: 53, desc: "Provenance absente"},
                {error: 54, desc: "Destination absente"},
                {error: 55, desc: "Nombre de DA ou de DAD absent"},
                {error: 56, desc: "Nombre de DA ou de DAD non numérique"},
                {error: 57, desc: "Nombre de zones d'actes absent"},
                {error: 58, desc: "Nombre de zones d'actes non numérique"},
                {error: 59, desc: "Format de RUM inconnu"},
                {error: 62, desc: "Unité médicale non renseignée"},
                {error: 64, desc: "Date système antérieure à la date d'entrée"},
                {error: 65, desc: "Date système antérieure à la date de sortie"},
                {error: 66, desc: "Nombre de séances : valeur invraisemblable"},
                {error: 67, desc: "Diagnostic principal : n'a jamais existé dans la CIM10"},
                {error: 68, desc: "Diagnostic principal : n'existe plus dans la CIM10"},
                {error: 70, desc: "Diagnostic associé : n'a jamais existé dans la CIM10"},
                {error: 71, desc: "Diagnostic associé : n'existe plus dans la CIM10"},
                {error: 73, desc: "Acte n'ayant jamais existé dans la CCAM"},
                {error: 76, desc: "Numéro FINESS de format incorrect"},
                {error: 77, desc: "Date d'entrée improbable car trop ancienne"},
                {error: 78, desc: "Date d'entrée du RUM incompatible avec l'utilisation d'un acte CCAM"},
                {error: 79, desc: "Date de sortie du RUM incompatible avec l'utilisation d'un acte CCAM"},
                {error: 80, desc: "Séjour avec acte opératoire mineur reclassant dans un GHM médical"},
                {error: 81, desc: "Code postal non numérique"},
                {error: 82, desc: "Poids d'entrée non numérique"},
                {error: 83, desc: "Zone réservée non vide"},
                {error: 84, desc: "Diagnostic principal invraisemblable car rare"},
                {error: 85, desc: "Diagnostic principal invraisemblable en raison de l'âge"},
                {error: 86, desc: "Diagnostic principal incompatible avec le sexe indiqué"},
                {error: 87, desc: "Diagnostic principal imprécis"},
                {error: 88, desc: "Codes Z a éviter en DP car n'est habituellement pas un motif d'hospitalisation"},
                {error: 90, desc: "Diagnostic associé invraisemblable car rare"},
                {error: 91, desc: "Diagnostic associé invraisemblable en raison de l'âge"},
                {error: 92, desc: "Diagnostic associé incompatible avec le sexe indiqué"},
                {error: 93, desc: "Diagnostic associé imprécis"},
                {error: 94, desc: "Diagnostic relié : n'a jamais existé dans la CIM10"},
                {error: 95, desc: "Diagnostic relié : n'existe plus dans la CIM10"},
                {error: 96, desc: "Diagnostic relié invraisemblable car rare"},
                {error: 97, desc: "Diagnostic relié invraisemblable en raison de l'âge"},
                {error: 98, desc: "Diagnostic relié incompatible avec le sexe indiqué"},
                {error: 99, desc: "Diagnostic relié imprécis"},
                {error: 100, desc: "Type d'autorisation d'unité medicale non accepté"},
                {error: 101, desc: "Type d'autorisation de lit dedié non accepté"},
                {error: 102, desc: "Date de réalisation de l'acte CCAM incohérente"},
                {error: 103, desc: "Code d'activité d'un acte CCAM non renseignee ou valeur erronee"},
                {error: 110, desc: "Activité 4 inexistante pour un acte CCAM"},
                {error: 111, desc: "Code d'activité autre que 4 inexistant pour un acte CCAM"},
                {error: 112, desc: "Geste complémentaire sans acte principal"},
                {error: 113, desc: "Code interdit en DP car très imprécis"},
                {error: 114, desc: "Code interdit en DP parce qu'il s'agit d'une cause externe de morbidité (V, W, X, Y)"},
                {error: 115, desc: "Code interdit en DP parce qu'il s'agit d'une catégorie non vide ou d'un code père interdit"},
                {error: 116, desc: "Code interdit en DR car très imprécis"},
                {error: 117, desc: "Code interdit en DR parce qu'il s'agit d'une cause externe de morbidité (V, W, X, Y)"},
                {error: 118, desc: "Code interdit en DR parce qu'il s'agit d'une catégorie non vide ou d'un code père interdit"},
                {error: 119, desc: "Code interdit en DA parce qu'il s'agit d'une catégorie non vide ou d'un code père interdit"},
                {error: 120, desc: "Confirmation du RSS nécessaire mais absente"},
                {error: 121, desc: "Code de confirmation du RSS incorrect"},
                {error: 122, desc: "Code de type de machine de radiothérapie incorrect"},
                {error: 123, desc: "Code de dosimétrie incorrect"},
                {error: 124, desc: "Confirmation du RSS non nécessaire et présente"},
                {error: 125, desc: "Age gestationnel non numérique"},
                {error: 126, desc: "Age gestationnel requis absent"},
                {error: 127, desc: "Age gestationnel incohérent"},
                {error: 128, desc: "Poids d'entrée sur un ou deux caractères non autorisé"},
                {error: 129, desc: "Âge gestationnel incohérent par rapport au poids d'entrée"},
                {error: 130, desc: "DP en O ou Z37 non autorise par rapport à l'âge"},
                {error: 131, desc: "DR en O ou Z37 non autorise par rapport à l'âge"},
                {error: 132, desc: "DA en O ou Z37 non autorise par rapport à l'âge"},
                {error: 133, desc: "DP en P ou Z38 non autorise par rapport à l'âge"},
                {error: 134, desc: "DR en P ou Z38 non autorise par rapport à l'âge"},
                {error: 135, desc: "DA en P ou Z38 non autorise par rapport à l'âge"},
                {error: 142, desc: "Date de réalisation de l'acte d'accouchement non renseignée ou incohérente"},
                {error: 143, desc: "Mode de sortie incoherent par rapport au DP"},
                {error: 145, desc: "Nombre de séances à zero avec DP motif de séances"},
                {error: 146, desc: "Nombre de séances incohérent par rapport a la duree de séjour"},
                {error: 147, desc: "Données incompatibles avec le DP P95"},
                {error: 148, desc: "Acte incompatible avec le sexe indiqué"},
                {error: 149, desc: "Acte incompatible avec l'age indiqué"},
                {error: 150, desc: "DP de séances non autorisé dans un RSS multiunité"},
                {error: 160, desc: "Date des dernières règles non numérique"},
                {error: 161, desc: "Date des dernières règles incoherente"},
                {error: 162, desc: "Date des dernières règles requise absente"},
                {error: 163, desc: "Date des dernières règles inconstante"},
                {error: 164, desc: "Date des dernières incompatible avec le sexe indiqué"},
                {error: 165, desc: "Date des dernières règles posterieure a la date d'entrée du séjour"},
                {error: 166, desc: "Date des dernières règles trop ancienne par rapport a la date d'entrée du sejour"},
                {error: 167, desc: "Activité 1 obligatoire pour un acte CCAM mais absente"},
                {error: 168, desc: "Poids d'entrée requis par rapport à l'âge"},
                {error: 169, desc: "IGS non numérique"},
                {error: 170, desc: "Extension documentaire obligatoire pour un acte CCAM mais absente"},
                {error: 173, desc: "Extension documentaire d'un acte CCAM erronée"},
                {error: 174, desc: "Âge gestationnel incohérent par rapport à la présence d'un acte d'accouchement"},
                {error: 179, desc: "GHM 14Z08Z : absence d'acte CCAM indiquant si l'IVG est médicamenteuse ou instrumentale"},
                {error: 180, desc: "Diagnostic principal : code OMS réservé pour usage urgent"},
                {error: 181, desc: "Diagnostic relié : code OMS réservé pour usage urgent"},
                {error: 182, desc: "Diagnostic associé : code OMS réservé pour usage urgent"},
                {error: 184, desc: "Code postal non renseigné"},
                {error: 185, desc: "Acte CCAM : format incorrect de l'extension ATIH"},
                {error: 186, desc: "Acte CCAM : extension ATIH non acceptée"},
                {error: 187, desc: "Diagnostic relié requis absent"},
                {error: 200, desc: "Erreur tables : acte non trouvé"},
                {error: 201, desc: "Erreur tables : diagnostic non affecté à une racine médicale"},
                {error: 202, desc: "Erreur tables : autre"},
                {error: 203, desc: "Dialyse péritonéale avec une durée de séjour de 0 jour"},
                {error: 204, desc: "DP d'accouchement hors d'un établissement incompatible avec mention d'un acte"},
                {error: 205, desc: "Diagnostic d'accouchement sans acte d'accouchement"},
                {error: 206, desc: "Acte d'accouchement sans diagnostic d'accouchement"},
                {error: 207, desc: "Acte d'avortement sans diagnostic en rapport"},
                {error: 208, desc: "Diagnostic d'entrée d'ante partum avec diagnostic indiquant le post partum"},
                {error: 209, desc: "Diagnostic d'entrée du post partum avec diagnostic indiquant l'ante partum"},
                {error: 210, desc: "Aucun des diagnostics ne permet de grouper"},
                {error: 211, desc: "DP de la période périnatale, incompatible avec l'âge ou le mode d'entrée"},
                {error: 212, desc: "DP de préparation à l'irradiation avec un nombre de séance à 0"},
                {error: 213, desc: "Incompatibilité poids à l'entrée dans l'unité médicale et âge gestationnel"},
                {error: 214, desc: "Manque acte de dialyse ou extensions des codes diagnostiques"},
                {error: 215, desc: "Manque acte de préparation à l'irradiation"},
                {error: 216, desc: "Manque acte d'irradiation"},
                {error: 222, desc: "Séjour avec acte opératoire non mineur reclassant dans un GHM médical"},
                {error: 223, desc: "Confirmation du RSS nécessaire et présente"},
                {error: 500, desc: "Fichier d'autorisations des unités médicales non trouvé"},
                {error: 501, desc: "Fichier d'autorisations des unités médicales corrompu"},
                {error: 502, desc: "Date de sortie du RSS non gérée par les tables binaires"}
            ],
            key: 'error'
        }
    };

    // Cache
    let settings = {};
    let catalogs = {};

    function updateSettings()
    {
        if (user.getUrlKey() !== settings.url_key) {
            settings = {
                indexes: settings.indexes || [],
                url_key: null
            };

            let url = buildUrl(thop.baseUrl('api/mco_settings.json'), {key: user.getUrlKey()});
            data.get(url, 'json', function(json) {
                settings = json;

                if (settings.start_date) {
                    settings.permissions = new Set(settings.permissions);
                    for (let structure of settings.structures) {
                        structure.units = {};
                        for (let ent of structure.entities) {
                            ent.path = ent.path.substr(1).split('|');
                            structure.units[ent.unit] = ent;
                        }
                    }
                }

                settings.url_key = user.getUrlKey();
            });
        }

        return settings;
    }
    this.updateSettings = updateSettings;

    function updateCatalog(name)
    {
        let info = Catalogs[name];
        let set = catalogs[name];

        if (info && !set) {
            set = {
                concepts: [],
                map: {}
            };
            catalogs[name] = set;

            if (info.concepts) {
                set.concepts = info.concepts;
                for (const concept of info.concepts)
                    set.map[concept[info.key]] = concept;
            } else {
                let url = thop.baseUrl(info.path);
                data.get(url, 'json', function(json) {
                    set.concepts = json;
                    for (const concept of json)
                        set.map[concept[info.key]] = concept;
                });
            }
        }

        return set;
    }
    this.updateCatalog = updateCatalog;

    function refreshIndexesLine(indexes, main_index)
    {
        if (!thop.needsRefresh(refreshIndexesLine, arguments))
            return;

        let svg = query('#opt_index');

        let builder = new VersionLine(svg);
        builder.anchorBuilder = function(version) {
            return thop.routeToUrl({date: version.date}).url;
        };
        builder.changeHandler = function() {
            thop.go({date: this.object.getValue()});
        };

        for (const index of indexes) {
            let label = index.begin_date;
            if (label.endsWith('-01'))
                label = label.substr(0, label.length - 3);

            builder.addVersion(index.begin_date, label, index.begin_date, index.changed_prices);
        }
        if (main_index >= 0)
            builder.setValue(indexes[main_index].begin_date);

        builder.render();
    }
    this.refreshIndexesLine = refreshIndexesLine;
}).call(mco_common);
