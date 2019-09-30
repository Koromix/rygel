// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ------------------------------------------------------------------------
// Data
// ------------------------------------------------------------------------

let data = (function() {
    let self = this;

    let cache = new LruMap(32);

    this.fetchJSON = async function(url) { return fetchAndCache(url, url, json => json); };
    this.fetchDictionary = async function(name) {
        let url = `${env.base_url}dictionaries/${name}.json`;
        return fetchAndCache(name, url, parseDictionary);
    };
    this.fetchCachedDictionary = function(name) { return cache.get(name); }

    async function fetchAndCache(key, url, func) {
        let resource = cache.get(key);

        if (!resource) {
            let response = await fetch(url);
            if (!response.ok)
                throw new Error(await response.text());

            resource = func(await response.json());
            cache.set(key, resource);
        }

        return resource;
    }

    function parseDictionary(json) {
        let dict = {};

        for (let chapter in json)
            dict[chapter] = new DictionaryChapter(json[chapter], dict);
        for (let chapter in dict)
            Object.freeze(dict[chapter]);

        return dict;
    }

    function DictionaryChapter(chapter, dict) {
        let self = this;

        let map = {};
        for (let defn of chapter.definitions) {
            map[defn.code] = defn;

            defn.children = [];

            if (chapter.parents) {
                for (type of chapter.parents) {
                    let parent = dict[type].find(defn.parents[type]);
                    parent.children.push(defn);
                }
            }

            defn.describe = describeDefinition;
            defn.describeParent = describeParent;
        }

        this.title = chapter.title;
        this.definitions = chapter.definitions;
        if (chapter.parents)
            this.parents = chapter.parents;

        this.find = function(code) { return map[code]; };

        this.describe = function(code) {
            let defn = map[code];
            return defn ? defn.describe() : code;
        };
        this.describeParent = function(code, type) {
            let defn = map[code];
            return defn ? dict[type].describeParent(type) : '????';
        };

        function describeDefinition() { return `${this.code} – ${this.desc}`; }
        function describeParent(type) { return dict[type].describe(this.parents[type]); }
    }

    this.clearCache = function() {
        cache.clear();
    };

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
