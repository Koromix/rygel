// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var mco_common = {};
(function() {
    const ConceptSets = {
        'ccam': {path: 'concepts/ccam.json', key: 'procedure'},
        'cim10': {path: 'concepts/cim10.json', key: 'diagnosis'},
        'mco_ghm_roots': {path: 'concepts/mco_ghm_roots.json', key: 'ghm_root'}
    };

    // Cache
    var indexes = [];
    var concept_sets = {};

    function updateIndexes()
    {
        if (!indexes.length) {
            let url = BaseUrl + 'api/mco_indexes.json';
            downloadJson('GET', url, function(json) {
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

            let url = BaseUrl + info.path;
            downloadJson('GET', url, function(json) {
                set.concepts = json;
                for (var i = 0; i < json.length; i++) {
                    var concept = json[i];
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
            module.object.route({date: this.object.getValue()});
        };

        let svg = builder.getWidget();
        let old_svg = _('#opt_indexes');
        cloneAttributes(old_svg, svg);
        old_svg.parentNode.replaceChild(svg, old_svg);
    }
    this.refreshIndexes = refreshIndexes;
}).call(mco_common);
