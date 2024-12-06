// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { Util, Log, Net, LruMap, LocalDate } from '../../web/core/common.js';

// ------------------------------------------------------------------------
// Data
// ------------------------------------------------------------------------

const DataCache = new function() {
    let json_cache = new LruMap(4);
    let dict_cache = new LruMap(4);

    this.fetchJson = async function(url) { return fetchAndCache(json_cache, url, url, json => json); };

    this.fetchDictionary = async function(name) {
        let url = `${ENV.base_url}dictionaries/${name}.json`;
        return fetchAndCache(dict_cache, name, url, parseDictionary);
    };

    this.getCacheDictionary = function(name) { return dict_cache.get(name); };

    async function fetchAndCache(cache, key, url, func) {
        let resource = cache.get(key);

        if (!resource) {
            let json = await Net.get(url);

            resource = func(json);
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
        let map = {};
        for (let defn of chapter.definitions) {
            map[defn.code] = defn;

            defn.children = [];

            if (chapter.parents) {
                for (let type of chapter.parents) {
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

        this.label = function(code) {
            let defn = map[code];
            return defn ? defn.label : '';
        };
        this.describe = function(code) {
            let defn = map[code];
            return defn ? defn.describe() : code;
        };
        this.describeParent = function(code, type) {
            let defn = map[code];
            return defn ? dict[type].describe(defn.parents[type]) : '';
        };

        function describeDefinition() { return `${this.code} – ${this.label}`; }
        function describeParent(type) { return dict[type].describe(this.parents[type]); }
    }

    this.clear = function() {
        json_cache.clear();
        dict_cache.clear();
    };
};

// ------------------------------------------------------------------------
// Format
// ------------------------------------------------------------------------

function formatPrice(price_cents, format_cents = true, show_plus = false) {
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
}

function formatDuration(duration) {
    if (duration != null && !isNaN(duration)) {
        return duration.toString() + (duration >= 2 ? ' nuits' : ' nuit');
    } else {
        return '';
    }
}

function parseDate(value) {
    try {
        return LocalDate.parse(value || null);
    } catch (err) {
        Log.error(err);
        return null;
    }
}

export {
    DataCache,

    formatPrice,
    formatDuration,

    parseDate
}
