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
        'mco_errors': {path: 'catalogs/mco.json', sub: 'errors'}
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
