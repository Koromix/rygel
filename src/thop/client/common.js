// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ------------------------------------------------------------------------
// Data
// ------------------------------------------------------------------------

let data = (function() {
    let self = this;

    this.busyHandler = null;
    this.errorHandler = null;

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

            if (self.errorHandler)
                self.errorHandler(response);
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
                {error: 151, desc: "Code de conversion du RSS incorrect"},
                {error: 152, desc: "Variable conversion renseignée à OUI et UM de type HP"},
                {error: 153, desc: "Condition de conversion présente et variable mise à NON"},
                {error: 154, desc: "Condition de conversion possible et variable vide"},
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
                {error: 188, desc: "Code RAAC incorrect"},
                {error: 189, desc: "Code RAAC incompatible avec mode de sortie"},
                {error: 190, desc: "Âge incompatible avec le mode d'entrée N"},
                {error: 191, desc: "Mode d'entrée N incompatible avec le DP Z76.2"},
                {error: 192, desc: "Acte CCAM : extension ATIH existante mais non codée"},
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

            if (info.concepts) {
                set.concepts = info.concepts;
                for (const concept of info.concepts)
                    set.map[concept[info.key]] = concept;
            } else {
                let url = thop.baseUrl(info.path);

                thop.setIgnoreBusy(true);
                data.get(url, 'json', function(json) {
                    set.concepts = json;
                    for (const concept of json)
                        set.map[concept[info.key]] = concept;

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
