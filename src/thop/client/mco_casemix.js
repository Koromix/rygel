// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_casemix = {};
(function() {
    let page_len = 100;

    // Route
    let unspecified = false;
    let pages = {};
    let sorts = {};

    // Settings
    let settings = {};

    // Casemix
    let mix_ready = false;
    let mix_params = {};
    let mix_url = null;
    let mix_rows = [];
    let mix_ghm_roots = new Set;
    let mix_durations = {};
    let mix_mismatched_roots = new Set;

    // Results
    let rt_url = null;
    let rt_results = [];

    // Cache
    let summaries = {};
    let deploy_results = new Set;

    function initModule()
    {
        // Clear casemix data when user changes or disconnects
        user.addChangeHandler(function() {
            clearCasemix();

            deploy_results.clear();
            summaries = {};
        });
    }
    this.initModule = initModule;

    function runModule(route)
    {
        // Memorize route info
        pages[route.view] = route.page;
        sorts[route.view] = route.sort;

        // Resources
        settings = thop.updateMcoSettings();
        let indexes = settings.indexes;
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        let ghm_roots = catalog.update('mco_ghm_roots').concepts;
        if (['durations', 'results'].includes(route.view) && !route.ghm_root && ghm_roots.length)
            route.ghm_root = ghm_roots[0].ghm_root;
        if (unspecified && settings.structures && settings.structures.length && ghm_roots.length) {
            if (!route.units.length && settings.structures[route.structure])
                route.units = settings.structures[route.structure].entities.map(function(ent) { return ent.unit; }).sort();
            if (!route.ghm_roots.length)
                route.ghm_roots = ghm_roots.map(function(ghm_root) { return ghm_root.ghm_root; }).sort();

            unspecified = false;
        }

        // Permissions
        if (!user.isConnected() || !settings.permissions) {
            if (!user.isConnected())
                thop.error('Vous n\'êtes pas connecté(e)');
            return;
        }
        if (route.view === 'results' && !settings.permissions.has('FullResults')) {
            thop.error('Vous n\'avez pas les droits pour utiliser cette page');
            return;
        }

        // Casemix
        if (!route.period[0]) {
            let year = parseInt(settings.end_date.split('-')[0], 10);
            if (settings.end_date.endsWith('-01-01')) {
                route.period = [`${year - 1}-01-01`, settings.end_date];
            } else {
                route.period = [`${year}-01-01`, settings.end_date];
            }
        }
        if (route.mode !== 'none' && !route.prev_period[0]) {
            let parts = route.period[0].split('-');
            let year = parseInt(parts[0], 10);
            route.prev_period = [`${year - 1}-${parts[1]}-${parts[2]}`, route.period[0]];
            if (route.prev_period[0] < settings.start_date)
                route.prev_period = route.period;
        }
        if (!route.algorithm)
            route.algorithm = settings.default_algorithm;
        let prev_period = (route.mode !== 'none') ? route.prev_period : [null, null];
        updateCasemixParams(route.period[0], route.period[1],
                            settings.permissions.has('UseFilter') ? route.filter : null,
                            route.algorithm, prev_period[0], prev_period[1],
                            route.apply_coefficient, route.refresh);
        switch (route.view) {
            case 'ghm_roots':
            case 'units': {
                updateCasemix();
            } break;
            case 'durations': {
                updateCasemixGhmRoot(route.ghm_root);
            } break;
            case 'results': {
                updateResults(route.ghm_root);
            } break;
        }
        delete route.refresh;

        // Errors
        if (!(['ghm_roots', 'units', 'durations', 'results'].includes(route.view)))
            thop.error('Mode d\'affichage incorrect');
        if (!route.units.length && mix_ready)
            thop.error('Aucune unité sélectionnée');
        if (['ghm_roots', 'units'].includes(route.view) && !route.ghm_roots.length && mix_ready)
            thop.error('Aucune racine sélectionnée');
        if (route.structure > settings.structures.length && settings.structures.length)
            thop.error('Structure inexistante');
        if (!['none', 'cmd', 'da', 'ga'].includes(route.regroup))
            thop.error('Regroupement incorrect');
        if (['durations', 'results'].includes(route.view)) {
            if (!checkCasemixGhmRoot(route.ghm_root))
                thop.error('Cette racine n\'existe pas dans cette période');
            if (route.view === 'durations' && mix_mismatched_roots.has(route.ghm_root))
                thop.error('Regroupement des GHS suite à changement')
        }
        if (!(['none', 'absolute'].includes(route.mode)))
            thop.error('Mode de comparaison inconnu');
        if (settings.algorithms.length &&
                !settings.algorithms.find(function(algorithm) { return algorithm.name === route.algorithm; }))
            thop.error('Algorithme inconnu');

        // Refresh settings
        queryAll('#opt_units, #opt_periods, #opt_algorithm, #opt_update, #opt_apply_coefficient')
            .removeClass('hide');
        query('#opt_filter').toggleClass('hide', !settings.permissions.has('UseFilter'));
        query('#opt_mode').toggleClass('hide', !['units', 'ghm_roots', 'durations'].includes(route.view));
        query('#opt_ghm_roots').toggleClass('hide', !['units', 'ghm_roots'].includes(route.view));
        query('#opt_ghm_root').toggleClass('hide', !['durations', 'results'].includes(route.view));
        refreshPeriodsPickers(route.period, route.prev_period,
                              route.view !== 'results' ? route.mode : 'none');
        query('#opt_filter > textarea').value = route.filter;
        query('#opt_mode > select').value = route.mode;
        refreshAlgorithmsMenu(route.algorithm);
        query('#opt_apply_coefficient > input').checked = route.apply_coefficient;
        query('#opt_update').disabled = mix_ready;
        refreshStructuresTree(route.units, route.structure);
        switch (route.view) {
            case 'ghm_roots':
            case 'units': {
                refreshGhmRootsTree(ghm_roots, route.ghm_roots, route.regroup);
            } break;
            case 'durations':
            case 'results': {
                refreshGhmRootsMenu(ghm_roots, route.ghm_root);
            } break;
        }

        // Refresh view
        if (!thop.isBusy()) {
            let view_el = query('#view');

            switch (route.view) {
                case 'ghm_roots': {
                    render(html`<div id="cm_ghm_roots" class="cm_summary"></div>`, view_el);
                    refreshGhmRootsTable(route.units, route.ghm_roots, route.regroup,
                                         route.page, route.sort);
                } break;
                case 'units': {
                    render(html`<div id="cm_units" class="cm_summary"></div>`, view_el);
                    refreshUnitsTable(route.units, route.ghm_roots, route.structure,
                                      route.page, route.sort);
                } break;
                case 'durations': {
                    render(html`<table id="cm_table" class="pr_grid"></table>`, view_el);
                    refreshDurationTable(route.units, route.ghm_root,
                                         route.apply_coefficient, true);
                } break;
                case 'results': {
                    render(html`<div id="rt"></div>`, view_el);
                    refreshResults(route.units, route.page);
                } break;
            }

            view_el.toggleClass('busy', !mix_ready);
        }
    }
    this.runModule = runModule;

    function parsePeriod(str) {
        let period = (str || '').split('..');
        if (period.length == 2) {
            return period;
        } else {
            return [null, null];
        }
    }

    function periodToStr(period) {
        if (period && period[0]) {
            return period[0] + '..' + period[1];
        } else {
            return null;
        }
    }

    function parseRoute(route, path, parameters, hash)
    {
        // Model: mco_casemix/<view>/<json_parameters_in_base64>
        let path_parts = path.split('/', 4);

        // Get simple parameters
        route.view = path_parts[1] || 'ghm_roots';
        route.period = parsePeriod(path_parts[2]);
        route.prev_period = parsePeriod(parameters.prev_period);
        route.structure = parameters.structure || 0;
        route.regroup = parameters.regroup || 'none';
        route.mode = parameters.mode || 'none';
        route.algorithm = parameters.algorithm || null;
        route.filter = parameters.filter || null;
        route.ghm_root = parameters.ghm_root || null;
        route.apply_coefficient = !!parseInt(parameters.apply_coefficient, 10) || false;
        route.page = parseInt(parameters.page, 10) || 1;
        route.sort = parameters.sort || null;

        // Decode GHM roots
        route.ghm_roots = [];
        if (parameters.ghm_roots) {
            try {
                let encoded = atob(parameters.ghm_roots);

                let x;
                let y;
                for (let i = 0; i < encoded.length;) {
                    let z = encoded.charCodeAt(i++);
                    if (z & 0x40) {
                        z &= ~0x40;
                    } else {
                        x = z || 90;
                        y = encoded[i++];
                        z = encoded.charCodeAt(i++);
                    }

                    let ghm_root = ('' + x).padStart(2, '0') + y + ('' + z).padStart(2, '0');
                    route.ghm_roots.push(ghm_root);
                }
            } catch (err) {
                // Tolerate broken URLs
            }
        }

        // Decode units
        route.units = [];
        if (parameters.units) {
            try {
                let encoded = atob(parameters.units);

                let unit = 0;
                for (let i = 0; i < encoded.length; i) {
                    let x = encoded.charCodeAt(i++);

                    if (x & 0x40) {
                        x &= ~0x40;
                        unit += (x & ~0x40);
                    } else {
                        let y = encoded.charCodeAt(i++);
                        let z = encoded.charCodeAt(i++);
                        unit = (x << 12) + (y << 6) + z;
                    }

                    route.units.push(unit);
                }
            } catch (err) {
                // Tolerate broken URLs
            }
        }
    }
    this.parseRoute = parseRoute;

    function routeToUrl(args)
    {
        let new_route = thop.buildRoute(args);

        // Clean up some arguments
        if (args.page === undefined)
            new_route.page = pages[new_route.view];
        if (args.sort === undefined)
            new_route.sort = sorts[new_route.view];
        if (!new_route.algorithm)
            unspecified = true;

        // Encode GHM roots
        let ghm_roots_enc;
        if (new_route.ghm_roots) {
            let encoded = '';

            for (let i = 0; i < new_route.ghm_roots.length; i++) {
                let ghm_root = new_route.ghm_roots[i];

                if (i && ghm_root.substr(0, 3) == new_route.ghm_roots[i - 1].substr(0, 3)) {
                    let z = parseInt(ghm_root.substr(3), 10);
                    encoded += String.fromCharCode((z & 0x3F) | 0x40);
                } else {
                    let x = parseInt(ghm_root.substr(0, 2), 10);
                    if (x === 90)
                        x = 0;
                    let y = ghm_root[2];
                    let z = parseInt(ghm_root.substr(3), 10);
                    encoded += String.fromCharCode(x & 0x3F) + y +
                                     String.fromCharCode(z & 0x3F);
                }
            }

            try {
                ghm_roots_enc = btoa(encoded);
            } catch (err) {
                // Binary data handling in JS is crap, hence the restriction to ASCII plane
                // in the encoding above.
            }
        }

        // Encode units
        let units_enc;
        if (new_route.units) {
            let encoded = '';

            for (let i = 0; i < new_route.units.length; i++) {
                let unit = new_route.units[i];
                let delta = unit - (i ? new_route.units[i - 1] : 0);
                if (delta > 0 && delta < 64) {
                    let delta = unit - new_route.units[i - 1];
                    encoded += String.fromCharCode(delta | 0x40);
                } else {
                    encoded += String.fromCharCode((unit >> 12) & 0x3F) +
                                 String.fromCharCode((unit >> 6) & 0x3F) +
                                 String.fromCharCode(unit & 0x3F);
                }
            }

            try {
                units_enc = btoa(encoded);
            } catch (err) {
                // Binary data handling in JS is crap, hence the restriction to ASCII plane
                // in the encoding above.
            }
        }

        let url;
        {
            let url_parts = [thop.baseUrl('mco_casemix'), new_route.view, periodToStr(new_route.period)];
            while (!url_parts[url_parts.length - 1])
                url_parts.pop();

            url = url_parts.join('/');
        }

        url = util.buildUrl(url, {
            prev_period: new_route.mode !== 'none' ? periodToStr(new_route.prev_period) : null,
            ghm_roots: ghm_roots_enc || null,
            units: units_enc || null,
            structure: new_route.structure || null,
            regroup: new_route.regroup !== 'none' ? new_route.regroup : null,
            mode: new_route.mode !== 'none' ? new_route.mode : null,
            algorithm: new_route.algorithm,
            filter: new_route.filter,
            ghm_root: new_route.ghm_root || null,
            apply_coefficient: new_route.apply_coefficient ? 1 : null,
            page: (new_route.page !== 1) ? new_route.page : null,
            sort: new_route.sort || null
        });

        // Hide links if not available for user
        let settings = thop.updateMcoSettings();
        let allowed = !!settings.permissions &&
                      (new_route.view !== 'results' || settings.permissions.has('FullResults'));

        return {
            url: url,
            allowed: allowed
        };
    }
    this.routeToUrl = routeToUrl;

    // A true result actually means maybe (if we haven't download the relevant data yet)
    function checkCasemixGhmRoot(ghm_root)
    {
        return (!mix_ghm_roots.size || mix_ghm_roots.has(ghm_root)) &&
               (!mix_durations[ghm_root] || mix_durations[ghm_root].ghs.length);
    }

    function clearCasemix()
    {
        mix_rows.length = 0;
        mix_ghm_roots.clear();
        mix_durations = {};
        rt_results = [];

        mix_ready = false;
    }

    function updateCasemixParams(start, end, filter, mode, diff_start, diff_end,
                                 apply_coefficient, refresh)
    {
        let params = {
            period: (start && end) ? (start + '..' + end) : null,
            filter: filter || null,
            dispense_mode: mode,
            diff: (diff_start && diff_end) ? (diff_start + '..' + diff_end) : null,
            apply_coefficient: 0 + apply_coefficient,
            key: user.getUrlKey()
        };
        let url = util.buildUrl(thop.baseUrl('api/mco_aggregate.json'), params);

        mix_ready = (url === mix_url);
        if ((refresh && !mix_ready) || user.getUrlKey() !== mix_params.key) {
            clearCasemix();

            mix_url = url;
            mix_params = params;
        }
    }

    function updateCasemix()
    {
        if (!mix_rows.length) {
            data.get(mix_url, 'json', function(json) {
                mix_rows = json.rows;

                for (let row of mix_rows) {
                    row.ghm_root = row.ghm.substr(0, 5);
                    mix_ghm_roots.add(row.ghm_root);
                }
            });
        }
    }

    function updateCasemixGhmRoot(ghm_root)
    {
        if (ghm_root && !mix_durations[ghm_root]) {
            let params = Object.assign({ghm_root: ghm_root}, mix_params);
            let url = util.buildUrl(thop.baseUrl('api/mco_aggregate.json'), params);

            data.get(url, 'json', function(json) {
                let mismatch = false;
                {
                    let finished_ghms = new Set;
                    for (const ghs of json.ghs) {
                        if (finished_ghms.has(ghs.ghm)) {
                            mismatch = true;
                        } else if (!ghs.conditions) {
                            finished_ghms.add(ghs.ghm);
                        }
                    }
                }

                if (mismatch) {
                    let dedup_ghs = [];
                    {
                        let ghms_map = {};
                        for (let ghs of json.ghs) {
                            let prev_ghs = ghms_map[ghs.ghm];
                            if (prev_ghs) {
                                if (ghs.exh_treshold) {
                                    if (prev_ghs.exh_treshold) {
                                        prev_ghs.exh_treshold = Math.min(prev_ghs.exh_treshold, ghs.exh_treshold);
                                    } else {
                                        prev_ghs.exh_treshold = ghs.exh_treshold;
                                    }
                                }
                                if (ghs.exb_treshold) {
                                    if (prev_ghs.exb_treshold) {
                                        prev_ghs.exb_treshold = Math.max(prev_ghs.exb_treshold, ghs.exb_treshold);
                                    } else {
                                        prev_ghs.exb_treshold = ghs.exb_treshold;
                                    }
                                }
                            } else {
                                dedup_ghs.push(ghs);
                                ghms_map[ghs.ghm] = ghs;
                            }
                        }
                    }

                    json.ghs = dedup_ghs;
                    for (let ghs of json.ghs)
                        ghs.ghs = '2+';
                    for (let row of json.rows)
                        row.ghs = '2+';

                    mix_mismatched_roots.add(ghm_root);
                } else {
                    mix_mismatched_roots.delete(ghm_root);
                }

                mix_durations[ghm_root] = json;
            });
        }
    }

    function updateResults(ghm_root)
    {
        if (ghm_root) {
            let params = Object.assign({ghm_root: ghm_root}, mix_params);
            delete params.diff;
            let url = util.buildUrl(thop.baseUrl('api/mco_results.json'), params);

            if (url !== rt_url) {
                data.get(url, 'json', function(json) {
                    rt_results = json;
                    rt_url = url;

                    for (let result of json)
                        result.ghm_root = result.ghm.substr(0, 5);
                });
            }
        }
    }

    function refreshPeriodsPickers(period, prev_period, mode)
    {
        if (!settings.start_date) {
            period = [null, null];
            prev_period = [null, null];
        }

        let picker = query('#opt_periods > div:last-of-type');
        let prev_picker = query('#opt_periods > div:first-of-type');

        // Set main picker
        {
            let builder = wt_period_picker.create();
            builder.changeHandler = () => thop.go({period: builder.getDates()});
            builder.setLimitDates([settings.start_date, settings.end_date]);
            builder.setDates(period);
            builder.render(picker);
        }

        // Set diff picker
        {
            let builder = wt_period_picker.create();
            builder.changeHandler = () => thop.go({prev_period: builder.getDates()});
            builder.setLimitDates([settings.start_date, settings.end_date]);
            builder.setDates(prev_period);
            builder.render(prev_picker);
        }

        picker.style.width = (mode !== 'none') ? '49%' : '100%';
        prev_picker.style.width = '49%';
        prev_picker.toggleClass('hide', mode === 'none');
    }

    function refreshStructuresTree(units, structure_idx)
    {
        if (!thop.needsRefresh(refreshStructuresTree, settings.url_key, arguments))
            return;

        units = new Set(units);

        let select = query('#opt_units > div');

        let builder = wt_tree_selector.create();
        builder.changeHandler = function() {
            thop.go({units: this.getValues().sort(),
                     structure: this.getCurrentTab()});
        };
        builder.setPrefix('Unités : ');

        for (const structure of settings.structures) {
            builder.addTab(structure.name);

            let prev_groups = [];
            for (const ent of structure.entities) {
                let common_len = 0;
                while (common_len < ent.path.length - 1 && common_len < prev_groups.length &&
                       ent.path[common_len] === prev_groups[common_len])
                    common_len++;
                while (prev_groups.length > common_len) {
                    builder.endGroup();
                    prev_groups.pop();
                }
                for (let k = common_len; k < ent.path.length - 1; k++) {
                    builder.beginGroup(ent.path[k]);
                    prev_groups.push(ent.path[k]);
                }

                builder.addOption(ent.path[ent.path.length - 1], ent.unit,
                                  {selected: units.has(ent.unit)});
            }
        }
        builder.setCurrentTab(structure_idx);

        builder.render(select);
    }

    function refreshAlgorithmsMenu(algorithm)
    {
        let el = query('#opt_algorithm > select');

        render(settings.algorithms.map(algorithm => {
            let label = `Algorithme ${algorithm.name}${algorithm.name === settings.default_algorithm ? ' *' : ''}`;
            return html`<option value=${algorithm.name}>${label}</option>`;
        }), el);

        el.value = algorithm;
    }

    function refreshGhmRootsTree(ghm_roots, select_ghm_roots, regroup)
    {
        if (!thop.needsRefresh(refreshGhmRootsTree, settings.url_key, arguments))
            return;

        // TODO: This should probably not be hard-coded here
        const GroupTypes = [
            {key: 'none', text: 'Racines'},
            {key: 'cmd', text: 'CMD'},
            {key: 'da', text: 'DA'},
            {key: 'ga', text: 'GA'}
        ];

        select_ghm_roots = new Set(select_ghm_roots);

        let ghm_roots_map = catalog.update('mco_ghm_roots').map;

        let select = query('#opt_ghm_roots > div');

        let builder = wt_tree_selector.create();
        builder.changeHandler = function() {
            thop.go({ghm_roots: this.getValues(),
                     regroup: GroupTypes[this.getCurrentTab()].key});
        };
        builder.setPrefix('GHM : ');

        for (const group_type of GroupTypes) {
            ghm_roots = ghm_roots.slice().sort(function(ghm_root1, ghm_root2) {
                const group1 = ghm_root1[group_type.key];
                const group2 = ghm_root2[group_type.key];

                if (group1 !== group2) {
                    return (group1 < group2) ? -1 : 1;
                } else if (ghm_root1.ghm_root !== ghm_root2.ghm_root) {
                    return (ghm_root1.ghm_root < ghm_root2.ghm_root) ? -1 : 1;
                } else {
                    return 0;
                }
            });

            builder.addTab(group_type.text);

            let prev_group = null;
            for (const ghm_root of ghm_roots) {
                let group = ghm_root[group_type.key] || null;
                if (group !== prev_group) {
                    if (prev_group)
                        builder.endGroup();
                    builder.beginGroup(group + ' - ' + ghm_root[group_type.key + '_desc']);

                    prev_group = group;
                }

                builder.addOption(ghm_root.ghm_root + ' - ' + ghm_root.desc, ghm_root.ghm_root,
                                  {selected: select_ghm_roots.has(ghm_root.ghm_root)});
            }
        }
        builder.setCurrentTab(GroupTypes.findIndex(function(group_type) {
            return group_type.key === regroup;
        }));

        builder.render(select);
    }

    function refreshGhmRootsMenu(ghm_roots, select_ghm_root)
    {
        let el = query('#opt_ghm_root > select');

        render(ghm_roots.map(ghm_root_info => {
            let disabled = !checkCasemixGhmRoot(ghm_root_info.ghm_root);
            let label = `${ghm_root_info.ghm_root} – ${ghm_root_info.desc}${disabled ? ' *' : ''}`;

            return html`<option value=${ghm_root_info.ghm_root} ?disabled=${disabled}>${label}</option>`;
        }), el);

        el.value = select_ghm_root || el.value;
    }

    function refreshUnitsTable(filter_units, filter_ghm_roots, structure, page, sort)
    {
        let summary = summaries.units;
        if (!summary) {
            summary = wt_data_table.create();
            summary.sortHandler = function(sort) { thop.go({sort: sort}); };

            summaries.units = summary;
        }

        if (thop.needsRefresh(summary, mix_url, filter_units, filter_ghm_roots, structure)) {
            filter_units = new Set(filter_units);
            filter_ghm_roots = new Set(filter_ghm_roots);
            structure = settings.structures[structure];

            const rows = filterCasemix(mix_rows, filter_units, filter_ghm_roots);

            summary.clear();
            summary.addColumn('unit', 'Unité');
            addSummaryColumns(summary);

            // Aggregate
            let stat0;
            let stats1;
            {
                function rowToGroup(row)
                {
                    let values = row.unit.map(function(unit) { return structure.units[unit].path; });
                    return ['group', values];
                }
                function filterUnitParts(row)
                {
                    let values = row.unit.map(function(unit) { return filter_units.has(unit); });
                    return ['include', values];
                }

                stat0 = aggregateCasemix(rows, filterUnitParts).getList()[0];
                stats1 = aggregateCasemix(rows, rowToGroup, filterUnitParts);
            }

            if (stat0) {
                summary.beginRow();
                summary.addCell('Total');
                addSummaryCells(summary, stat0, stat0);

                let prev_groups = [];
                let prev_totals = [stat0];
                for (const ent of structure.entities) {
                    let unit_stat = stats1.find(ent.path[ent.path.length - 1]);
                    if (!unit_stat)
                        continue;

                    let common_len = 0;
                    while (common_len < ent.path.length - 1 && common_len < prev_groups.length &&
                           ent.path[common_len] === prev_groups[common_len])
                        common_len++;
                    while (prev_groups.length > common_len) {
                        summary.endRow();
                        prev_groups.pop();
                        prev_totals.pop();
                    }

                    for (let k = common_len; k < ent.path.length - 1; k++) {
                        let group_stat = stats1.find(ent.path[k]);

                        summary.beginRow();
                        summary.addCell(ent.path[k], {tooltip: ent.path[k]});
                        addSummaryCells(summary, group_stat, prev_totals[prev_totals.length - 1]);

                        prev_groups.push(ent.path[k]);
                        prev_totals.push(group_stat);
                    }

                    summary.beginRow();
                    summary.addCell(ent.path[ent.path.length - 1], {tooltip: ent.path[ent.path.length - 1]});
                    addSummaryCells(summary, unit_stat, prev_totals[prev_totals.length - 1]);
                    summary.endRow();
                }
            }
        }

        summary.sort(sort);

        render(html`
            <div class="dtab_pager"></div>
            <div class="dtab"></div>
            <div class="dtab_pager"></div>
        `, query('#cm_units'));

        let render_count = summary.render(query('#cm_units .dtab'),
                                          (page - 1) * page_len, page_len, {hide_empty: true});
        syncPagers(queryAll('#cm_units .dtab_pager'), page,
                   wt_pager.computeLastPage(render_count, summary.getRowCount(), page_len));
    }

    function refreshGhmRootsTable(filter_units, filter_ghm_roots, regroup, page, sort)
    {
        let summary = summaries.ghm_roots;
        if (!summary) {
            summary = wt_data_table.create();
            summary.sortHandler = function(sort) { thop.go({sort: sort}); };

            summaries.ghm_roots = summary;
        }

        if (thop.needsRefresh(summary, mix_url, filter_units, filter_ghm_roots, regroup)) {
            filter_units = new Set(filter_units);
            filter_ghm_roots = new Set(filter_ghm_roots);

            const rows = filterCasemix(mix_rows, filter_units, filter_ghm_roots);

            let ghm_roots = catalog.update('mco_ghm_roots')
                .concepts.slice().sort(function(ghm_root1, ghm_root2) {
                    if (ghm_root1[regroup] !== ghm_root2[regroup]) {
                        return (ghm_root1[regroup] < ghm_root2[regroup]) ? -1 : 1;
                    } else if (ghm_root1.ghm_root !== ghm_root2.ghm_root) {
                        return (ghm_root1.ghm_root < ghm_root2.ghm_root) ? -1 : 1;
                    } else {
                        return 0;
                    }
                });
            let ghm_roots_map = catalog.update('mco_ghm_roots').map;

            summary.clear();
            summary.addColumn('ghm_root', 'Racine', value => {
                let ghm_root_info = ghm_roots_map[value];
                if (ghm_root_info) {
                    return html`
                        <a href=${routeToUrl({view: 'durations', ghm_root: value}).url}>${value}</a>
                        ${ghm_root_info ? ` - ${ghm_root_info.desc}` : null}
                    `;
                } else {
                    return value;
                }
            });
            addSummaryColumns(summary);

            // Aggregate
            let stat0;
            let stats1;
            {
                function rowToGroup(row)
                {
                    let ghm_root_info = ghm_roots_map[row.ghm_root]
                    if (ghm_root_info) {
                        let values = row.unit.map(function(unit) { return [ghm_root_info[regroup], row.ghm_root]; });
                        return ['group', values];
                    } else {
                        return ['group', row.ghm_root];
                    }
                }
                function filterUnitParts(row)
                {
                    let values = row.unit.map(function(unit) { return filter_units.has(unit); });
                    return ['include', values];
                }

                stat0 = aggregateCasemix(rows, filterUnitParts).getList()[0];
                stats1 = aggregateCasemix(rows, rowToGroup, filterUnitParts);
            }

            if (stat0) {
                summary.beginRow();
                summary.addCell('Total');
                addSummaryCells(summary, stat0, stat0);

                let prev_group = null;
                let total = stat0;
                for (const ghm_root_info of ghm_roots) {
                    let ghm_root = ghm_root_info.ghm_root;
                    let stat = stats1.find(ghm_root);
                    if (!stat)
                        continue;

                    let group = ghm_root_info ? ghm_root_info[regroup] : null;
                    if (group !== prev_group) {
                        if (prev_group) {
                            summary.endRow();
                            total = stat0;
                        }

                        if (group) {
                            let stat = stats1.find(group);
                            summary.beginRow();
                            summary.addCell(`${group} - ${ghm_root_info[regroup + '_desc']}`);
                            addSummaryCells(summary, stat, total);
                            total = stat;
                        }

                        prev_group = group;
                    }

                    summary.beginRow();
                    summary.addCell(ghm_root,
                                    {tooltip: ghm_root_info ? `${ghm_root} - ${ghm_root_info.desc}` : null});
                    addSummaryCells(summary, stat, total);
                    summary.endRow();
                }
            }
        }

        summary.sort(sort);

        render(html`
            <div class="dtab_pager"></div>
            <div class="dtab"></div>
            <div class="dtab_pager"></div>
        `, query('#cm_ghm_roots'));

        let render_count = summary.render(query('#cm_ghm_roots .dtab'),
                                          (page - 1) * page_len, page_len, {hide_empty: true});
        syncPagers(queryAll('#cm_ghm_roots .dtab_pager'), page,
                   wt_pager.computeLastPage(render_count, summary.getRowCount(), page_len));
    }

    function addSummaryColumns(dtab)
    {
        if (mix_params.diff) {
            dtab.addColumn('rss', 'RSS', value => format.number(value, true), {format: '#,##0'});
            dtab.addColumn('rum', 'RUM', value => format.number(value, true), {format: '#,##0'});
            dtab.addColumn('total', 'Total', value => format.price(value, false, true), {format: '#,##0.00'});
            dtab.addColumn('pay', 'Rétribué', value => format.price(value, false, true), {format: '#,##0.00'});
            dtab.addColumn('deaths', 'Décès', value => format.number(value, true), {format: '#,##0'});
        } else {
            dtab.addColumn('rss', 'RSS', value => format.number(value), {format: '#,##0'});
            dtab.addColumn('rss_pct', '%', value => format.percent(value), {format: '0.00%'});
            dtab.addColumn('rum', 'RUM', value => format.number(value), {format: '#,##0'});
            dtab.addColumn('rum_pct', '%', value => format.percent(value), {format: '0.00%'});
            dtab.addColumn('total', 'Total', value => format.price(value, false), {format: '#,##0.00'});
            dtab.addColumn('total_pct', '%', value => format.percent(value), {format: '0.00%'});
            dtab.addColumn('pay', 'Rétribué', value => format.price(value, false), {format: '#,##0.00'});
            dtab.addColumn('pay_pct', '%', value => format.percent(value), {format: '0.00%'});
            dtab.addColumn('deaths', 'Décès', value => format.number(value), {format: '#,##0'});
            dtab.addColumn('deaths_pct', '%', value => format.percent(value), {format: '0.00%'});
        }
    }

    function addSummaryCells(dtab, stat, total)
    {
        if (mix_params.diff) {
            dtab.addCell(stat.count);
            dtab.addCell(stat.mono_count);
            dtab.addCell(stat.price_cents_total);
            dtab.addCell(stat.price_cents);
            dtab.addCell(stat.deaths);
        } else {
            dtab.addCell(stat.count);
            dtab.addCell(stat.count / total.count);
            dtab.addCell(stat.mono_count);
            dtab.addCell(stat.mono_count / total.mono_count);
            dtab.addCell(stat.price_cents_total);
            dtab.addCell(stat.price_cents_total / total.price_cents_total);
            dtab.addCell(stat.price_cents);
            dtab.addCell(stat.price_cents / total.price_cents);
            dtab.addCell(stat.deaths);
            dtab.addCell(stat.deaths / stat.count);
        }

    }

    function syncPagers(pagers, current_page, last_page)
    {
        pagers.forEach(function(pager) {
            if (last_page) {
                let builder = wt_pager.create();
                builder.hrefBuilder = page => routeToUrl({page: page}).url;
                builder.setLastPage(last_page);
                builder.setCurrentPage(current_page);
                builder.render(pager);

                pager.removeClass('hide');
            } else {
                pager.addClass('hide');
                render(html``, pager);
            }
        });
    }

    function refreshDurationTable(units, ghm_root, apply_coeff, merge_cells)
    {
        let table = query('#cm_table');
        table.replaceContent(
            dom.h('thead'),
            dom.h('tbody')
        );

        if (mix_durations[ghm_root] && mix_durations[ghm_root].rows.length) {
            units = new Set(units);

            const ghs = mix_durations[ghm_root].ghs;
            const rows = mix_durations[ghm_root].rows.filter(function(row) {
                for (const unit of row.unit) {
                    if (units.has(unit))
                        return true;
                }
                return false;
            });

            let thead = table.query('thead');
            let tbody = table.query('tbody');

            let stat0;
            let stats1;
            let stats1_units;
            let stats2;
            let stats2_units;
            let stats3;
            {
                function filterUnitParts(row)
                {
                    let values = row.unit.map(function(unit) { return units.has(unit); });
                    return ['include', values];
                }

                stat0 = aggregateCasemix(rows, filterUnitParts).getList()[0];
                stats1 = aggregateCasemix(rows, 'ghm', 'ghs', filterUnitParts);
                stats1_units = aggregateCasemix(rows, 'ghm', 'ghs', 'unit');
                stats2 = aggregateCasemix(rows, 'ghm', 'ghs', 'duration', filterUnitParts);
                stats2_units = aggregateCasemix(rows, 'ghm', 'ghs', 'duration', 'unit');
                stats3 = aggregateCasemix(rows, 'duration', filterUnitParts);
            }

            let max_duration = 10;
            let max_count = 0;
            let max_price_cents = 0;
            for (const duration_stat of stats2.getList()) {
                max_duration = Math.max(max_duration, duration_stat.duration + 1);
                max_count = Math.max(max_count, duration_stat.count);
                max_price_cents = Math.max(max_price_cents, duration_stat.price_cents);
            }

            render(html`
                <tr><th>GHM</th>${util.mapRLE(ghs.map(col => col.ghm),
                    (ghm, colspan) => html`<td class="desc" colspan=${colspan * 2}>${ghm}</td>`)}</tr>
                <tr><th>Niveau</th>${util.mapRLE(ghs.map(col => col.ghm.substr(5, 1)),
                    (mode, colspan) => html`<td class="desc" colspan=${colspan * 2}>Niveau ${mode}</td>`)}</tr>
                <tr><th>GHS</th>${ghs.map(col =>
                    html`<td class="desc" colspan="2">${col.ghs}${col.conditions.length ? '*' : ''}</td>`)}</tr>
            `, thead);

            function makeTooltip(title, col, col_stat, row_stat, duration_stat, unit_stats)
            {
                unit_stats = Object.values(unit_stats).sort(function(unit1, unit2) {
                    return unit1.unit - unit2.unit;
                });

                let tooltip = '';

                if (!mix_params.diff) {
                    tooltip += col.ghm + ' (' + col.ghs + ') – ' + title + ' :\n' +
                               '– ' + format.percent(duration_stat.count / stat0.count) + ' / ' +
                               format.percent(duration_stat.price_cents_total / stat0.price_cents_total) +
                               ' de la racine\n' +
                               '– ' + format.percent(duration_stat.count / col_stat.count) + ' / ' +
                               format.percent(duration_stat.price_cents_total / col_stat.price_cents_total) +
                               ' de la colonne\n' +
                               '– ' + format.percent(duration_stat.count / row_stat.count) + ' / ' +
                               format.percent(duration_stat.price_cents_total / row_stat.price_cents_total) +
                               ' de la ligne\n\n';
                }

                tooltip += 'Unités :';
                {
                    let missing_cents = duration_stat.price_cents_total;
                    for (const unit_stat of unit_stats) {
                        tooltip += '\n– ' + unit_stat.unit + ' : ' + format.price(unit_stat.price_cents, true, !!mix_params.diff);
                        if (!mix_params.diff)
                            tooltip += ' (' + format.percent(unit_stat.price_cents / duration_stat.price_cents_total) + ')';
                        missing_cents -= unit_stat.price_cents;
                    }

                    if (missing_cents) {
                        tooltip += '\n– Autres : ' + format.price(missing_cents);
                        if (!mix_params.diff)
                            tooltip += ' (' + format.percent(missing_cents / duration_stat.price_cents_total) + ')';
                    }
                }

                return tooltip;
            }

            function diffToClasses(value)
            {
                if (mix_params.diff) {
                    if (value > 0) {
                        return ['diff', 'higher'];
                    } else if (value < 0) {
                        return ['diff', 'lower'];
                    } else {
                        return ['diff', 'neutral'];
                    }
                } else {
                    return [null];
                }
            }

            // Totals
            {
                let tr = dom.h('tr',
                    dom.h('th', 'Total')
                );

                for (const col of ghs) {
                    let col_stat = stats1.find(col.ghm, col.ghs);

                    if (col_stat) {
                        let tooltip =
                            makeTooltip('Total', col, col_stat, stat0, col_stat,
                                        stats1_units.findPartial(col.ghm, col.ghs));

                        tr.appendContent(
                            dom.h('td', {class: ['count', 'total'].concat(diffToClasses(col_stat.count)),
                                         title: tooltip},
                                 '' + format.number(col_stat.count, !!mix_params.diff)),
                            dom.h('td', {class: ['price', 'total'].concat(diffToClasses(col_stat.price_cents_total)),
                                         title: tooltip},
                                 format.price(col_stat.price_cents_total, false, !!mix_params.diff))
                        );
                    } else {
                        tr.appendContent(
                            dom.h('td', {class: ['count', 'total', 'empty']}),
                            dom.h('td', {class: ['price', 'total', 'empty']})
                        );
                    }
                }

                tbody.appendContent(tr);
            }

            // Durations
            for (let duration = 0; duration < max_duration; duration++) {
                let row_stat = stats3.find(duration);

                let tr = dom.h('tr', {class: 'duration'},
                    dom.h('th', format.duration(duration))
                );
                for (const col of ghs) {
                    let col_stat = stats1.find(col.ghm, col.ghs);
                    let duration_stat = stats2.find(col.ghm, col.ghs, duration);

                    let cls;
                    if (col.exb_treshold && duration < col.exb_treshold) {
                        cls = 'exb';
                    } else if (col.exh_treshold && duration >= col.exh_treshold) {
                        cls = 'exh';
                    } else {
                        cls = 'noex';
                    }

                    if (duration_stat) {
                        let tooltip =
                            makeTooltip(format.duration(duration),
                                        col, col_stat, row_stat, duration_stat,
                                        stats2_units.findPartial(col.ghm, col.ghs, duration));

                        tr.appendContent(
                            dom.h('td', {class: ['count', cls].concat(diffToClasses(duration_stat.count)),
                                         title: tooltip},
                                 '' + format.number(duration_stat.count, !!mix_params.diff)),
                            dom.h('td', {class: ['price', cls].concat(diffToClasses(duration_stat.price_cents_total)),
                                         title: tooltip},
                                 format.price(duration_stat.price_cents_total, false, !!mix_params.diff))
                        );
                    } else if (mco_pricing.testDuration(col.durations, duration)) {
                        cls += ' empty';
                        tr.appendContent(
                            dom.h('td', {class: ['count', cls]}),
                            dom.h('td', {class: ['price', cls]})
                        );
                    } else {
                        tr.appendContent(
                            dom.h('td', {class: 'count'}),
                            dom.h('td', {class: 'price'})
                        );
                    }
                }
                tbody.appendContent(tr);
            }
        }
    }

    function refreshResults(units, page)
    {
        units = new Set(units);

        let cim10_map = catalog.update('cim10').map;
        let ccam_map = catalog.update('ccam').map;
        let ghm_roots_map = catalog.update('mco_ghm_roots').map;
        let errors_map = catalog.update('mco_errors').map;

        function handleIdClick(e)
        {
            let table = this.parentNode.parentNode.parentNode.parentNode;
            if (table.toggleClass('deploy')) {
                deploy_results.add(table.dataset.bill_id);
            } else {
                deploy_results.delete(table.dataset.bill_id);
            }

            e.preventDefault();
        }

        function codeWithDesc(map, code)
        {
            const desc = map[code];
            if (code !== null && code !== undefined) {
                return '' + code + (desc ? ' - ' + desc.desc : '');
            } else {
                return null;
            }
        }

        function unitPath(unit)
        {
            if (unit && settings.structures[0]) {
                let unit_info = settings.structures[0].units[unit];
                if (unit_info) {
                    return unit_info.path[unit_info.path.length - 1];
                } else {
                    return '' + unit;
                }
            } else if (unit) {
                return '' + unit;
            } else {
                return null;
            }
        }

        function createInfoRow(key, value, title)
        {
            if (value !== null && value !== undefined) {
                return dom.h('tr',
                    dom.h('th', key),
                    dom.h('td', {title: title}, '' + (value || ''))
                );
            } else {
                return null;
            }
        }

        let results = rt_results.filter(function(result) {
            for (const stay of result.stays) {
                if (units.has(stay.unit))
                    return true;
            }
            return false;
        });

        let offset = (page - 1) * page_len;
        let end = Math.min(offset + page_len, results.length);

        let elements = [];
        for (let i = offset; i < end; i++) {
            const result = results[i];

            let table = dom.h('table', {class: 'rt_result', 'data-bill_id': result.bill_id},
                dom.h('tbody',
                    dom.h('tr', {class: 'rt_header'},
                        dom.h('td',
                            dom.h('a', {class: 'rt_id', href: '#', click: handleIdClick},
                                  '' + result.bill_id)
                        ),
                        dom.h('td', (['♂ ', '♀ '][result.sex - 1] || '') + format.age(result.age)),
                        dom.h('td', (result.duration !== undefined ? format.duration(result.duration) : '') +
                                    (result.stays[result.stays.length - 1].exit_mode == '9' ? ' (✝)' : '')),
                        dom.h('td', {title: codeWithDesc(ghm_roots_map, result.ghm_root) + '\n\n' +
                                            'GHM : ' + result.ghm + '\n' +
                                            'Erreur : ' + codeWithDesc(errors_map, result.main_error) + '\n' +
                                            'GHS : ' + result.ghs},
                            dom.h('a', {href: mco_pricing.routeToUrl({view: 'table',
                                                                      date: result.index_date,
                                                                      ghm_root: result.ghm.substr(0, 5)}).url},
                                 result.ghm),
                            result.ghm.startsWith('90') ? (' [' + result.main_error + ']')
                                                        : (' (' + result.ghs + ')')
                        ),
                        dom.h('td', {style: 'text-align: right;'}, format.price(result.price_cents) + '€'),
                        dom.h('td', {style: 'text-align: right;'}, format.price(result.total_cents) + '€')
                    )
                )
            );
            let tbody = table.query('tbody');

            table.dataset.bill_id =result.bill_id;
            if (deploy_results.has(table.dataset.bill_id))
                table.addClass('deploy');

            for (let j = 0; j < result.stays.length; j++) {
                const stay = result.stays[j];

                tbody.appendContent(
                    dom.h('tr', {class: 'rt_stay'},
                        dom.h('td', 'RUM ' + (j + 1) + (j == result.main_stay ? ' *' : '')),
                        dom.h('th', {title: unitPath(stay.unit)}, '' + (stay.unit || '')),
                        dom.h('td', (stay.duration !== undefined ? format.duration(stay.duration) : '') +
                                    (stay.exit_mode == '9' ? ' (✝)' : '')),
                        dom.h('td'),
                        dom.h('td', {style: 'text-align: right;'},
                              stay.total_cents ? (format.price(stay.price_cents) + '€') : ''),
                        dom.h('td', {style: 'text-align: right;'},
                              stay.price_cents ? (format.price(stay.total_cents) + '€') : '')
                    )
                );

                if (stay.sex) {
                    let tr = dom.h('tr', {class: 'rt_details'},
                        dom.h('td', {colspan: 6},
                            dom.h('table'),
                            dom.h('table'),
                            dom.h('table')
                        )
                    );
                    let table0 = tr.firstChild.childNodes[0];
                    let table1 = tr.firstChild.childNodes[1];
                    let table2 = tr.firstChild.childNodes[2];

                    // Main info
                    table0.appendContent(
                        createInfoRow('Sexe', ['Homme', 'Femme'][stay.sex - 1] || ''),
                        createInfoRow('Date de naissance', (stay.birthdate || '') +
                                                           (stay.age !== undefined ? ' (' + format.age(stay.age) + ')' : '')),
                        createInfoRow('Entrée', '' + stay.entry_date + ' ' + stay.entry_mode +
                                                (stay.entry_origin ? '-' + stay.entry_origin : '')),
                        createInfoRow('Sortie', '' + stay.exit_date + ' ' + stay.exit_mode +
                                                (stay.exit_destination ? '-' + stay.exit_destination : '')),
                        createInfoRow('Autorisation de Lit', stay.bed_authorization),
                        createInfoRow('IGS 2', stay.igs2),
                        createInfoRow('Séances', stay.session_count),
                        createInfoRow('Dernières règles', stay.last_menstrual_period),
                        createInfoRow('Âge gestationnel',
                                      stay.gestational_age ? (stay.gestational_age + ' SA') : null),
                        createInfoRow('Poids du nouveau-né',
                                      stay.newborn_weight ? (stay.newborn_weight + ' grammes') : null),
                        createInfoRow('Confirmation', stay.confirm ? 'Oui' : null),
                        createInfoRow('Présence UCD', stay.ucd ? 'Oui' : null),
                        createInfoRow('RAAC', stay.raac ? 'Oui' : null),
                        createInfoRow('Séances de DIP', stay.dip_count)
                    );

                    // Diagnoses
                    {
                        table1.appendContent(
                            createInfoRow('Diagnostic principal', stay.main_diagnosis,
                                          codeWithDesc(cim10_map, stay.main_diagnosis)),
                            createInfoRow('Diagnostic relié', stay.linked_diagnosis,
                                          codeWithDesc(cim10_map, stay.linked_diagnosis))
                        );

                        let other_diagnoses = stay.other_diagnoses.slice();
                        other_diagnoses.sort(function(diag1, diag2) {
                            return util.compareValues(diag1.diag, diag2.diag);
                        });

                        for (let k = 0; k < other_diagnoses.length; k++) {
                            const diag = other_diagnoses[k];

                            let contents = [diag.diag];
                            if (diag.severity) {
                                let cls;
                                if (diag.exclude) {
                                    cls = 'rt_severity rt_severity_exclude';
                                } else {
                                    cls = 'rt_severity rt_severity_s' + diag.severity;
                                }

                                contents.push(' ');
                                contents.push(dom.h('span', {class: cls}, 'S' + (diag.severity + 1)));
                            }

                            table1.appendContent(
                                dom.h('tr',
                                    dom.h('th', k ? '' : 'Diagnostics associés'),
                                    dom.h('td', {title: codeWithDesc(cim10_map, diag.diag)}, contents)
                                )
                            );
                        }
                    }

                    // Procedures (deduplicated)
                    {
                        let procedures = [];
                        {
                            let procedures_map = {};

                            for (let proc of stay.procedures) {
                                let map_key = proc.proc + '/' + proc.phase + ':' + proc.activity;
                                let prev_proc = procedures_map[map_key];

                                if (prev_proc) {
                                    prev_proc.count += proc.count;
                                } else {
                                    proc = Object.assign({}, proc);
                                    proc.key = map_key;

                                    procedures_map[map_key] = proc;
                                    procedures.push(proc);
                                }
                            }

                            procedures.sort(function(proc1, proc2) {
                                return util.compareValues(proc1.key, proc2.key);
                            });
                        }

                        for (let k = 0; k < procedures.length; k++) {
                            const proc = procedures[k];

                            let text = proc.proc + (proc.phase ? '/' + proc.phase : '') +
                                       ' (' + proc.activity + ')' +
                                       (proc.count !== 1 ? ' * ' + proc.count : '');

                            table2.appendContent(
                                dom.h('tr',
                                    dom.h('th', k ? '' : 'Actes'),
                                    dom.h('td', {title: codeWithDesc(ccam_map, proc.proc)}, text)
                                )
                            );
                        }
                    }

                    tbody.appendContent(tr);
                }
            }

            elements.push(table);
        }

        let pagers = [
            dom.h('table', {class: 'pagr'}),
            dom.h('table', {class: 'pagr'})
        ];
        syncPagers(pagers, page, wt_pager.computeLastPage(end - offset, results.length, page_len));

        query('#rt').replaceContent(
            pagers[0],
            elements,
            pagers[1]
        );
    }

    function filterCasemix(rows, units, ghm_roots)
    {
        return rows.filter(function(row) {
            if (!ghm_roots.has(row.ghm_root))
                return false;
            for (const unit of row.unit) {
                if (units.has(unit))
                    return true;
            }
            return false;
        });
    }

    function aggregateCasemix(rows, by)
    {
        if (!Array.isArray(by))
            by = Array.prototype.slice.call(arguments, 1);

        let template = {
            count: 0,
            deaths: 0,
            mono_count_total: 0,
            price_cents_total: 0,
            mono_count: 0,
            price_cents: 0
        };

        function aggregateRow(row, col_idx, first, ptr)
        {
            if (first) {
                ptr.count += row.count;
                ptr.deaths += row.deaths;
                ptr.mono_count_total += row.mono_count_total;
                ptr.price_cents_total += row.price_cents_total;
            }
            ptr.mono_count += row.mono_count[col_idx];
            ptr.price_cents += row.price_cents[col_idx];
        }

        let agg = new Aggregator(template, aggregateRow, by);
        agg.aggregate(rows);

        return agg;
    }
}).call(mco_casemix);
