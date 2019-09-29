// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ------------------------------------------------------------------------
// Concepts
// ------------------------------------------------------------------------

let concepts = (function() {
    let self = this;

    let sets = {};
    let maps = {};

    this.load = async function(name) {
        let set = sets[name];

        if (!set) {
            set = await thop.fetchJSON(`${env.base_url}catalogs/${name}.json`);
            sets[name] = set;

            for (let type in set) {
                let arr = set[type];

                let map = {};
                for (let info of arr)
                    map[info.code] = info;

                maps[`${name}_${type}`] = map;
            }
        }

        return set;
    };

    this.findGhmRoot = function(code) {
        return maps.mco_ghm_roots[code];
    };
    this.completeGhmRoot = function(code) {
        let info = maps.mco_ghm_roots[code];
        return info ? `${code} - ${info.desc}` : null;
    };
    this.descGhmRoot = function(code) {
        let info = maps.mco_ghm_roots[code];
        return info ? info.desc : null;
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
