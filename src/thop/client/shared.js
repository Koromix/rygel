// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let concepts = (function() {
    let self = this;

    let sets = {};
    let maps = {};

    this.load = async function(name) {
        if (sets[name])
            return;

        let set = await fetch(`${env.base_url}catalogs/${name}.json`).then(response => response.json());
        sets[name] = set;

        for (let type in set) {
            let arr = set[type];

            let map = {};
            for (let info of arr)
                map[info.code] = info;

            maps[`${name}_${type}`] = map;
        }

        return;
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
