// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ------------------------------------------------------------------------
// Data
// ------------------------------------------------------------------------

let data = (function() {
    let self = this;

    this.busyHandler = null;

    let queue = new Set();
    let busy = 0;

    this.get = function(url, type, proceed, fail){
        let xhr = openRequest('GET', url, type, proceed, fail);
        if (!xhr)
            return;

        xhr.send();
    };

    this.post = function(url, type, parameters, proceed, fail) {
        let xhr = openRequest('POST', url, type, proceed, fail);
        if (!xhr)
            return;

        let encoded_parameters = util.buildUrl('', parameters).substr(1);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send(encoded_parameters || null);
    };

    this.lazyLoad = function(module, func) {
        if (typeof LazyModules !== 'object' || !LazyModules[module]) {
            console.log('Cannot load module ' + module);
            return;
        }

        self.get(LazyModules[module], null, function(js) {
            eval.call(window, js);
            if (func)
                func();
        });
    };

    function openRequest(method, url, type, proceed, fail) {
        if (queue.has(url))
            return;
        queue.add(url);
        busy++;

        function handleFinishedRequest(status, response) {
            callRequestHandlers(url, proceed, fail, status, response);
            if (!--busy)
                callIdleHandler();
        }

        let xhr = new XMLHttpRequest();
        xhr.timeout = 20000;
        xhr.onload = function(e) {
            if (this.status === 200) {
                if (!type)
                    type = null;

                let data;
                try {
                    switch (type) {
                        case null:
                        case 'text': {  data = xhr.response; } break;
                        case 'json': {  data = JSON.parse(xhr.response); } break;
                    }
                } catch (err) {
                    data = null;
                }

                handleFinishedRequest(this.status, data);
            } else {
                handleFinishedRequest(this.status, xhr.response);
            }
        };
        xhr.onerror = function(e) { handleFinishedRequest(503); };
        xhr.ontimeout = function(e) { handleFinishedRequest(504); };
        xhr.open(method, url, true);

        if (self.busyHandler)
            self.busyHandler(true);

        return xhr;
    }

    function callRequestHandlers(url, proceed, fail, status, response) {
        if (status === 200) {
            proceed(response);
        } else {
            if (!response) {
                switch (status) {
                    case 200: { /* Success */ } break;
                    case 403: { response = 'Accès refusé'; } break;
                    case 404: { response = 'Adresse \'' + url + '\' introuvable'; } break;
                    case 422: { response = 'Paramètres incorrects'; } break;
                    case 502:
                    case 503: { response = 'Service non accessible'; } break;
                    case 504: { response = 'Délai d\'attente dépassé, réessayez'; } break;
                    default: { response = 'Erreur inconnue ' + status; } break;
                }
            }

            log.error(response);
            if (fail)
                fail(response);
        }
    }

    function callIdleHandler() {
        if (self.busyHandler) {
            setTimeout(function() {
                self.busyHandler(false);
                if (!busy)
                    queue.clear();
            }, 0);
        } else {
            queue.clear();
        }
    }

    return this;
}).call({});

// ------------------------------------------------------------------------
// Format
// ------------------------------------------------------------------------

let format = (function() {
    this.number = function(n, show_plus = false) {
        return (show_plus && n > 0 ? '+' : '') +
               n.toLocaleString('fr-FR');
    };

    this.percent = function(value, show_plus = false) {
        let parameters = {
            style: 'percent',
            minimumFractionDigits: 1,
            maximumFractionDigits: 1
        };

        if (value != null && !isNaN(value)) {
            return (show_plus && fraction > 0 ? '+' : '') +
                   value.toLocaleString('fr-FR', parameters);
        } else {
            return '-';
        }
    };

    this.price = function(price_cents, format_cents = true, show_plus = false) {
        if (price_cents != null && !isNaN(price_cents)) {
            let parameters = {
                minimumFractionDigits: format_cents ? 2 : 0,
                maximumFractionDigits: format_cents ? 2 : 0
            };

            return (show_plus && price_cents > 0 ? '+' : '') +
                   (price_cents / 100.0).toLocaleString('fr-FR', parameters);
        } else {
            return '';
        }
    };

    this.duration = function(duration) {
        if (duration != null && !isNaN(duration)) {
            return duration.toString() + (duration >= 2 ? ' nuits' : ' nuit');
        } else {
            return '';
        }
    };

    this.age = function(age) {
        if (age != null && !isNaN(age)) {
            return age.toString() + (age >= 2 ? ' ans' : ' an');
        } else {
            return '';
        }
    };

    return this;
}).call({});

// ------------------------------------------------------------------------
// Aggregation
// ------------------------------------------------------------------------

function Aggregator(template, func, by) {
    let self = this;

    let list = [];
    let map = {};

    this.findPartial = function(values) {
        if (!Array.isArray(values))
            values = Array.prototype.slice.call(arguments, 0);

        let ptr = map;
        for (const value of values) {
            ptr = ptr[value];
            if (ptr === undefined)
                return null;
        }

        return ptr;
    };

    this.find = function(values) {
        let ptr = self.findPartial.apply(null, arguments);
        if (ptr && ptr.count === undefined)
            return null;

        return ptr;
    };

    function aggregateRec(row, row_ptrs, ptr, col_idx, key_idx) {
        for (let i = key_idx; i < by.length; i++) {
            let key = by[i];
            let value;
            if (typeof key === 'function') {
                let ret = key(row);
                key = ret[0];
                value = ret[1];
            } else {
                value = row[key];
            }

            if (Array.isArray(value))
                value = value[col_idx];

            if (value === true) {
                continue;
            } else if (value === false) {
                return;
            }

            if (Array.isArray(value)) {
                const values = value;
                for (const value of values) {
                    if (ptr[value] === undefined)
                        ptr[value] = {};
                    template[key] = value;

                    aggregateRec(row, row_ptrs, ptr[value], col_idx, key_idx + 1);
                }

                return;
            }

            if (ptr[value] === undefined)
                ptr[value] = {};
            template[key] = value;

            ptr = ptr[value];
        }

        if (!ptr.valid) {
            Object.assign(ptr, template);
            list.push(ptr);
        }

        func(row, col_idx, !row_ptrs.has(ptr), ptr);
        row_ptrs.add(ptr);
    }

    this.aggregate = function(rows) {
        for (const row of rows) {
            const max_idx = row.mono_count.length;

            let row_ptrs = new Set;
            for (let i = 0; i < max_idx; i++)
                aggregateRec(row, row_ptrs, map, i, 0);
        }
    };

    this.getList = function() { return list; };

    if (!Array.isArray(by))
        by = Array.prototype.slice.call(arguments, 2);
    template = Object.assign({valid: true}, template);
}

// ------------------------------------------------------------------------
// Indexes
// ------------------------------------------------------------------------

function refreshIndexesLine(indexes, main_index) {
    if (!thop.needsRefresh(refreshIndexesLine, arguments))
        return;

    let builder = new VersionLine;
    builder.hrefBuilder = version => thop.routeToUrl({date: version.date.toString()}).url;
    builder.changeHandler = e => thop.go({date: builder.getDate().toString()});

    for (const index of indexes) {
        let date = dates.fromString(index.begin_date);
        let label = index.begin_date;

        if (label.endsWith('-01'))
            label = label.substr(0, label.length - 3);

        builder.addVersion(date, label, index.begin_date, index.changed_prices);
    }

    if (main_index >= 0) {
        let current_date = dates.fromString(indexes[main_index].begin_date);
        builder.setDate(current_date);
    }

    let svg = query('#opt_index');
    builder.render(svg);
}

// ------------------------------------------------------------------------
// Catalogs
// ------------------------------------------------------------------------

let catalog = (function() {
    let self = this;

    const Catalogs = {
        'cim10': {path: 'catalogs/cim10.json', sub: 'diagnoses'},
        'ccam': {path: 'catalogs/ccam.json', sub: 'procedures'},
        'mco_ghm_roots': {path: 'catalogs/mco.json', sub: 'ghm_roots'},
        'mco_errors': [
            {code: 2, desc: "Incompatibilite sexe-diagnostic principal"},
            {code: 3, desc: "Diagnostic principal incohérent"},
            {code: 4, desc: "Tables endommagées ou erreur dans le parcours de l'arbre"},
            {code: 5, desc: "Diagnostic principal : code insuffisamment précis pour la classification des GHM"},
            {code: 6, desc: "Nombre de séances > 0 et DP n'est pas un motif de séances"},
            {code: 7, desc: "Poids incompatible pour un nouveau-né"},
            {code: 8, desc: "Corruption d'une table"},
            {code: 9, desc: "Table introuvable"},
            {code: 10, desc: "RSS multiunité avec numéro de RSS inconstant"},
            {code: 11, desc: "Numéro de RSS absent"},
            {code: 13, desc: "Date de naissance absente"},
            {code: 14, desc: "Date de naissance non numérique"},
            {code: 15, desc: "Date de naissance improbable par rapport à la date d'entrée"},
            {code: 16, desc: "Code sexe absent"},
            {code: 17, desc: "Code sexe erroné"},
            {code: 19, desc: "Date d'entrée absente"},
            {code: 20, desc: "Date d'entrée non numérique"},
            {code: 21, desc: "Date d'entrée incohérente"},
            {code: 23, desc: "RSS multiunité : chaînage date d'entrée date de sortie incohérent"},
            {code: 24, desc: "Mode d'entrée absent"},
            {code: 25, desc: "Mode d'entrée erroné ou provenance erronée"},
            {code: 26, desc: "Mode d'entrée incorrect ou provenance incorrecte pour commencer un RSS"},
            {code: 27, desc: "RSS multiunité : mode d'entrée incorrect ou provenance incorrecte sur un RUM de suite"},
            {code: 28, desc: "Date de sortie absente"},
            {code: 29, desc: "Date de sortie non numérique"},
            {code: 30, desc: "Date de sortie incohérente"},
            {code: 32, desc: "RUM avec incohérence date de sortie date d'entrée"},
            {code: 33, desc: "Mode de sortie absent"},
            {code: 34, desc: "Mode de sortie erroné, ou destination erronée"},
            {code: 35, desc: "Mode de sortie incorrect ou destination incorrecte pour clore un RSS"},
            {code: 36, desc: "Nombre de séances non numérique"},
            {code: 37, desc: "RSS multiunité : présence de séances sur un des RUM"},
            {code: 39, desc: "Date de naissance incohérente"},
            {code: 40, desc: "Diagnostic principal absent"},
            {code: 41, desc: "Code de diagnostic principal ne respectant pas le format de la CIM10"},
            {code: 42, desc: "Code de diagnostic associé ne respectant pas le format de la CIM10"},
            {code: 43, desc: "Code d'acte ne respectant pas le format de la CCAM"},
            {code: 45, desc: "RSS multiunité : date de naissance inconstante"},
            {code: 46, desc: "RSS multiunité : code sexe inconstant"},
            {code: 49, desc: "RSS multiunité : mode de sortie incorrect ou destination incorrecte pour un RUM autre que le dernier"},
            {code: 50, desc: "Délai de séjour incompatible avec le principe administratif de prestation interétablissement"},
            {code: 51, desc: "Code de diagnostic relié ne respectant pas le format de la CIM10"},
            {code: 52, desc: "Nombre de réalisations d'actes non numérique, ou erroné"},
            {code: 53, desc: "Provenance absente"},
            {code: 54, desc: "Destination absente"},
            {code: 55, desc: "Nombre de DA ou de DAD absent"},
            {code: 56, desc: "Nombre de DA ou de DAD non numérique"},
            {code: 57, desc: "Nombre de zones d'actes absent"},
            {code: 58, desc: "Nombre de zones d'actes non numérique"},
            {code: 59, desc: "Format de RUM inconnu"},
            {code: 62, desc: "Unité médicale non renseignée"},
            {code: 64, desc: "Date système antérieure à la date d'entrée"},
            {code: 65, desc: "Date système antérieure à la date de sortie"},
            {code: 66, desc: "Nombre de séances : valeur invraisemblable"},
            {code: 67, desc: "Diagnostic principal : n'a jamais existé dans la CIM10"},
            {code: 68, desc: "Diagnostic principal : n'existe plus dans la CIM10"},
            {code: 70, desc: "Diagnostic associé : n'a jamais existé dans la CIM10"},
            {code: 71, desc: "Diagnostic associé : n'existe plus dans la CIM10"},
            {code: 73, desc: "Acte n'ayant jamais existé dans la CCAM"},
            {code: 76, desc: "Numéro FINESS de format incorrect"},
            {code: 77, desc: "Date d'entrée improbable car trop ancienne"},
            {code: 78, desc: "Date d'entrée du RUM incompatible avec l'utilisation d'un acte CCAM"},
            {code: 79, desc: "Date de sortie du RUM incompatible avec l'utilisation d'un acte CCAM"},
            {code: 80, desc: "Séjour avec acte opératoire mineur reclassant dans un GHM médical"},
            {code: 81, desc: "Code postal non numérique"},
            {code: 82, desc: "Poids d'entrée non numérique"},
            {code: 83, desc: "Zone réservée non vide"},
            {code: 84, desc: "Diagnostic principal invraisemblable car rare"},
            {code: 85, desc: "Diagnostic principal invraisemblable en raison de l'âge"},
            {code: 86, desc: "Diagnostic principal incompatible avec le sexe indiqué"},
            {code: 87, desc: "Diagnostic principal imprécis"},
            {code: 88, desc: "Codes Z a éviter en DP car n'est habituellement pas un motif d'hospitalisation"},
            {code: 90, desc: "Diagnostic associé invraisemblable car rare"},
            {code: 91, desc: "Diagnostic associé invraisemblable en raison de l'âge"},
            {code: 92, desc: "Diagnostic associé incompatible avec le sexe indiqué"},
            {code: 93, desc: "Diagnostic associé imprécis"},
            {code: 94, desc: "Diagnostic relié : n'a jamais existé dans la CIM10"},
            {code: 95, desc: "Diagnostic relié : n'existe plus dans la CIM10"},
            {code: 96, desc: "Diagnostic relié invraisemblable car rare"},
            {code: 97, desc: "Diagnostic relié invraisemblable en raison de l'âge"},
            {code: 98, desc: "Diagnostic relié incompatible avec le sexe indiqué"},
            {code: 99, desc: "Diagnostic relié imprécis"},
            {code: 100, desc: "Type d'autorisation d'unité medicale non accepté"},
            {code: 101, desc: "Type d'autorisation de lit dedié non accepté"},
            {code: 102, desc: "Date de réalisation de l'acte CCAM incohérente"},
            {code: 103, desc: "Code d'activité d'un acte CCAM non renseignee ou valeur erronee"},
            {code: 110, desc: "Activité 4 inexistante pour un acte CCAM"},
            {code: 111, desc: "Code d'activité autre que 4 inexistant pour un acte CCAM"},
            {code: 112, desc: "Geste complémentaire sans acte principal"},
            {code: 113, desc: "Code interdit en DP car très imprécis"},
            {code: 114, desc: "Code interdit en DP parce qu'il s'agit d'une cause externe de morbidité (V, W, X, Y)"},
            {code: 115, desc: "Code interdit en DP parce qu'il s'agit d'une catégorie non vide ou d'un code père interdit"},
            {code: 116, desc: "Code interdit en DR car très imprécis"},
            {code: 117, desc: "Code interdit en DR parce qu'il s'agit d'une cause externe de morbidité (V, W, X, Y)"},
            {code: 118, desc: "Code interdit en DR parce qu'il s'agit d'une catégorie non vide ou d'un code père interdit"},
            {code: 119, desc: "Code interdit en DA parce qu'il s'agit d'une catégorie non vide ou d'un code père interdit"},
            {code: 120, desc: "Confirmation du RSS nécessaire mais absente"},
            {code: 121, desc: "Code de confirmation du RSS incorrect"},
            {code: 122, desc: "Code de type de machine de radiothérapie incorrect"},
            {code: 123, desc: "Code de dosimétrie incorrect"},
            {code: 124, desc: "Confirmation du RSS non nécessaire et présente"},
            {code: 125, desc: "Age gestationnel non numérique"},
            {code: 126, desc: "Age gestationnel requis absent"},
            {code: 127, desc: "Age gestationnel incohérent"},
            {code: 128, desc: "Poids d'entrée sur un ou deux caractères non autorisé"},
            {code: 129, desc: "Âge gestationnel incohérent par rapport au poids d'entrée"},
            {code: 130, desc: "DP en O ou Z37 non autorise par rapport à l'âge"},
            {code: 131, desc: "DR en O ou Z37 non autorise par rapport à l'âge"},
            {code: 132, desc: "DA en O ou Z37 non autorise par rapport à l'âge"},
            {code: 133, desc: "DP en P ou Z38 non autorise par rapport à l'âge"},
            {code: 134, desc: "DR en P ou Z38 non autorise par rapport à l'âge"},
            {code: 135, desc: "DA en P ou Z38 non autorise par rapport à l'âge"},
            {code: 142, desc: "Date de réalisation de l'acte d'accouchement non renseignée ou incohérente"},
            {code: 143, desc: "Mode de sortie incoherent par rapport au DP"},
            {code: 145, desc: "Nombre de séances à zero avec DP motif de séances"},
            {code: 146, desc: "Nombre de séances incohérent par rapport a la duree de séjour"},
            {code: 147, desc: "Données incompatibles avec le DP P95"},
            {code: 148, desc: "Acte incompatible avec le sexe indiqué"},
            {code: 149, desc: "Acte incompatible avec l'age indiqué"},
            {code: 150, desc: "DP de séances non autorisé dans un RSS multiunité"},
            {code: 151, desc: "Code de conversion du RSS incorrect"},
            {code: 152, desc: "Variable conversion renseignée à OUI et UM de type HP"},
            {code: 153, desc: "Condition de conversion présente et variable mise à NON"},
            {code: 154, desc: "Condition de conversion possible et variable vide"},
            {code: 160, desc: "Date des dernières règles non numérique"},
            {code: 161, desc: "Date des dernières règles incoherente"},
            {code: 162, desc: "Date des dernières règles requise absente"},
            {code: 163, desc: "Date des dernières règles inconstante"},
            {code: 164, desc: "Date des dernières incompatible avec le sexe indiqué"},
            {code: 165, desc: "Date des dernières règles posterieure a la date d'entrée du séjour"},
            {code: 166, desc: "Date des dernières règles trop ancienne par rapport a la date d'entrée du sejour"},
            {code: 167, desc: "Activité 1 obligatoire pour un acte CCAM mais absente"},
            {code: 168, desc: "Poids d'entrée requis par rapport à l'âge"},
            {code: 169, desc: "IGS non numérique"},
            {code: 170, desc: "Extension documentaire obligatoire pour un acte CCAM mais absente"},
            {code: 173, desc: "Extension documentaire d'un acte CCAM erronée"},
            {code: 174, desc: "Âge gestationnel incohérent par rapport à la présence d'un acte d'accouchement"},
            {code: 179, desc: "GHM 14Z08Z : absence d'acte CCAM indiquant si l'IVG est médicamenteuse ou instrumentale"},
            {code: 180, desc: "Diagnostic principal : code OMS réservé pour usage urgent"},
            {code: 181, desc: "Diagnostic relié : code OMS réservé pour usage urgent"},
            {code: 182, desc: "Diagnostic associé : code OMS réservé pour usage urgent"},
            {code: 184, desc: "Code postal non renseigné"},
            {code: 185, desc: "Acte CCAM : format incorrect de l'extension ATIH"},
            {code: 186, desc: "Acte CCAM : extension ATIH non acceptée"},
            {code: 187, desc: "Diagnostic relié requis absent"},
            {code: 188, desc: "Code RAAC incorrect"},
            {code: 189, desc: "Code RAAC incompatible avec mode de sortie"},
            {code: 190, desc: "Âge incompatible avec le mode d'entrée N"},
            {code: 191, desc: "Mode d'entrée N incompatible avec le DP Z76.2"},
            {code: 192, desc: "Acte CCAM : extension ATIH existante mais non codée"},
            {code: 200, desc: "Erreur tables : acte non trouvé"},
            {code: 201, desc: "Erreur tables : diagnostic non affecté à une racine médicale"},
            {code: 202, desc: "Erreur tables : autre"},
            {code: 203, desc: "Dialyse péritonéale avec une durée de séjour de 0 jour"},
            {code: 204, desc: "DP d'accouchement hors d'un établissement incompatible avec mention d'un acte"},
            {code: 205, desc: "Diagnostic d'accouchement sans acte d'accouchement"},
            {code: 206, desc: "Acte d'accouchement sans diagnostic d'accouchement"},
            {code: 207, desc: "Acte d'avortement sans diagnostic en rapport"},
            {code: 208, desc: "Diagnostic d'entrée d'ante partum avec diagnostic indiquant le post partum"},
            {code: 209, desc: "Diagnostic d'entrée du post partum avec diagnostic indiquant l'ante partum"},
            {code: 210, desc: "Aucun des diagnostics ne permet de grouper"},
            {code: 211, desc: "DP de la période périnatale, incompatible avec l'âge ou le mode d'entrée"},
            {code: 212, desc: "DP de préparation à l'irradiation avec un nombre de séance à 0"},
            {code: 213, desc: "Incompatibilité poids à l'entrée dans l'unité médicale et âge gestationnel"},
            {code: 214, desc: "Manque acte de dialyse ou extensions des codes diagnostiques"},
            {code: 215, desc: "Manque acte de préparation à l'irradiation"},
            {code: 216, desc: "Manque acte d'irradiation"},
            {code: 222, desc: "Séjour avec acte opératoire non mineur reclassant dans un GHM médical"},
            {code: 223, desc: "Confirmation du RSS nécessaire et présente"},
            {code: 500, desc: "Fichier d'autorisations des unités médicales non trouvé"},
            {code: 501, desc: "Fichier d'autorisations des unités médicales corrompu"},
            {code: 502, desc: "Date de sortie du RSS non gérée par les tables binaires"}
        ]
    };

    // Cache
    let catalogs = {};

    this.update = function(type) {
        let info = Catalogs[type];
        let set = catalogs[type];

        if (info && (!set || !set.concepts.length)) {
            if (!set) {
                set = {
                    concepts: [],
                    map: {}
                };
                catalogs[type] = set;
            }

            if (Array.isArray(info)) {
                set.concepts = info;

                for (const concept of set.concepts)
                    set.map[concept.code] = concept;
            } else {
                let url = thop.baseUrl(info.path);

                thop.setIgnoreBusy(true);
                data.get(url, 'json', function(json) {
                    let maps = {};
                    for (const cat in json) {
                        let map = {};
                        maps[cat] = map;

                        for (const concept of json[cat])
                            map[concept.code] = concept;
                    }

                    set.concepts = json[info.sub];
                    set.map = maps[info.sub];

                    for (const concept of set.concepts) {
                        if (concept.parents) {
                            for (const parent_type in concept.parents) {
                                let parent_code = concept.parents[parent_type];

                                concept[parent_type] = maps[parent_type][parent_code].code;
                                concept[parent_type + '_desc'] = maps[parent_type][parent_code].desc;
                            }
                        }

                        delete concept.parents;
                    }

                    thop.forceRefresh();
                });
                thop.setIgnoreBusy(false);
            }
        }

        return set;
    };

    this.getInfo = function(type, key) {
        let map = self.update(type).map;
        return map[key];
    };

    this.getDesc = function(type, key) {
        let info = self.getInfo(type, key);
        return info ? info.desc : null;
    };
    this.appendDesc = function(type, key, text = null) {
        let desc = self.getDesc(type, key);
        if (!text)
            text = key;
        return desc ? `${text} - ${desc}` : text;
    };

    return this;
}).call({});
