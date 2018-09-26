// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_common = {};
(function() {
    'use strict';

    const ConceptSets = {
        'ccam': {path: 'concepts/ccam.json', key: 'procedure'},
        'cim10': {path: 'concepts/cim10.json', key: 'diagnosis'},
        'mco_ghm_roots': {path: 'concepts/mco_ghm_roots.json', key: 'ghm_root'}
    };

    // Cache
    let indexes = [];
    let concept_sets = {};

    function updateIndexes()
    {
        if (!indexes.length) {
            let url = drdw.baseUrl('api/mco_indexes.json');
            data.get(url, function(json) {
                indexes = json;
            });
        }

        return indexes;
    }
    this.updateIndexes = updateIndexes;

    function updateConceptSet(name)
    {
        let info = ConceptSets[name];
        let set = concept_sets[name];

        if (info && !set) {
            set = {
                concepts: [],
                map: {}
            };
            concept_sets[name] = set;

            let url = drdw.baseUrl(info.path);
            data.get(url, function(json) {
                set.concepts = json;
                for (let i = 0; i < json.length; i++) {
                    let concept = json[i];
                    set.map[concept[info.key]] = concept;
                }
            });
        }

        return set;
    }
    this.updateConceptSet = updateConceptSet;

    function refreshIndexes(indexes, main_index)
    {
        let builder = new VersionLine;

        for (let i = 0; i < indexes.length; i++) {
            let index = indexes[i];
            builder.addVersion(index.begin_date, index.begin_date, index.changed_prices);
        }
        if (main_index >= 0)
            builder.setValue(indexes[main_index].begin_date);

        builder.changeHandler = function() {
            drdw.go({date: this.object.getValue()});
        };

        let svg = builder.getWidget();
        let old_svg = query('#opt_indexes');
        svg.copyAttributesFrom(old_svg);
        old_svg.replaceWith(svg);
    }
    this.refreshIndexes = refreshIndexes;
}).call(mco_common);
