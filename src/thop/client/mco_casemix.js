// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_casemix = {};
(function() {
    // Route
    let unspecified = false;
    let pages = {};
    let sorts = {};

    // Settings
    let st_url_key = null;
    let start_date = null;
    let end_date = null;
    let algorithms = [];
    let default_algorithm = null;
    let structures = [];

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
    let units_summary = null;
    let ghm_roots_summary = null;
    let deploy_results = new Set;

    function runCasemix(route, path, parameters, hash, errors)
    {
        if (!user.isConnected()) {
            errors.add('Vous n\'êtes pas connecté(e)');
            query('#cm').addClass('hide');
            return;
        }

        // Parse route (model: casemix/<view>/<json_parameters_in_base64>)
        let path_parts = path.split('/', 3);
        if (path_parts[2])
            Object.assign(route, thop.buildRoute(JSON.parse(window.atob(path_parts[2]))));
        route.view = path_parts[1] || 'ghm_roots';
        route.period = (route.period && route.period[0]) ? route.period : [start_date, end_date];
        route.prev_period = (route.prev_period && route.prev_period[0]) ?
                            route.prev_period : [start_date, end_date];
        route.structure = route.structure || 0;
        route.regroup = route.regroup || 'none';
        route.mode = route.mode || 'none';
        route.algorithm = route.algorithm || default_algorithm;
        route.units = route.units || [];
        route.ghm_roots = route.ghm_roots || [];
        route.refresh = route.refresh || false;
        route.ghm_root = route.ghm_root || null;
        route.apply_coefficient = route.apply_coefficient || false;
        route.page = parseInt(parameters.page, 10) || 1;
        route.sort = parameters.sort || null;
        pages[route.view] = route.page;
        sorts[route.view] = route.sort;

        // Resources
        let indexes = mco_common.updateIndexes();
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        let ghm_roots = mco_common.updateCatalog('mco_ghm_roots').concepts;
        if (['durations', 'results'].includes(route.view) && !route.ghm_root && ghm_roots.length)
            route.ghm_root = ghm_roots[0].ghm_root;
        if (unspecified && structures.length && ghm_roots.length) {
            if (!route.units.length && structures[route.structure])
                route.units = structures[route.structure].entities.map(function(ent) { return ent.unit; }).sort();
            if (!route.ghm_roots.length)
                route.ghm_roots = ghm_roots.map(function(ghm_root) { return ghm_root.ghm_root; }).sort();

            unspecified = false;
        }
        if (route.view === 'results') {
            mco_common.updateCatalog('cim10');
            mco_common.updateCatalog('ccam');
        }
        updateSettings();

        // Casemix
        if (st_url_key) {
            let prev_period = (route.mode !== 'none') ? route.prev_period : [null, null];
            updateCasemixParams(route.period[0], route.period[1], route.algorithm,
                                prev_period[0], prev_period[1], route.apply_coefficient,
                                route.refresh);

            switch (route.view) {
                case 'ghm_roots':
                case 'units': {
                    updateCasemixUnits();
                } break;
                case 'durations': {
                    updateCasemixDuration(route.ghm_root);
                } break;
                case 'results': {
                    updateResults(route.ghm_root);
                } break;
            }
        }
        delete route.refresh;

        // Errors
        if (!(['ghm_roots', 'units', 'durations', 'results'].includes(route.view)))
            errors.add('Mode d\'affichage incorrect');
        if (!route.units.length && mix_ready)
            errors.add('Aucune unité sélectionnée');
        if (['ghm_roots', 'units'].includes(route.view) && !route.ghm_roots.length && mix_ready)
            errors.add('Aucune racine sélectionnée');
        if (route.structure > structures.length && structures.length)
            errors.add('Structure inexistante');
        if (!['none', 'cmd', 'da', 'ga'].includes(route.regroup))
            errors.add('Regroupement incorrect');
        if (['durations', 'results'].includes(route.view)) {
            if (!route.ghm_root)
                errors.add('Aucune racine de GHM sélectionnée');
            if (!checkCasemixGhmRoot(route.ghm_root))
                errors.add('Cette racine n\'existe pas dans cette période');
            if (route.view === 'durations' && mix_mismatched_roots.has(route.ghm_root))
                errors.add('Regroupement des GHS suite à changement')
        }
        if (!(['none', 'absolute'].includes(route.mode)))
            errors.add('Mode de comparaison inconnu');
        if (algorithms.length &&
                !algorithms.find(function(algorithm) { return algorithm.name === route.algorithm; }))
            errors.add('Algorithme inconnu');

        // Refresh settings
        queryAll('#opt_units, #opt_periods, #opt_algorithm, #opt_update, #opt_apply_coefficient')
            .removeClass('hide');
        query('#opt_mode').toggleClass('hide', !['units', 'ghm_roots', 'durations'].includes(route.view));
        query('#opt_ghm_roots').toggleClass('hide', !['units', 'ghm_roots'].includes(route.view));
        query('#opt_ghm_root').toggleClass('hide', !['durations', 'results'].includes(route.view));
        refreshPeriodsPickers(route.period, route.prev_period,
                              route.view !== 'results' ? route.mode : 'none');
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
        if (!data.isBusy()) {
            switch (route.view) {
                case 'ghm_roots': {
                    refreshGhmRootsTable(route.units, route.ghm_roots, route.regroup,
                                         route.page, route.sort);
                } break;
                case 'units': {
                    refreshUnitsTable(route.units, route.ghm_roots, route.structure,
                                      route.page, route.sort);
                } break;
                case 'durations': {
                    refreshDurationTable(route.units, route.ghm_root,
                                         route.apply_coefficient, true);
                } break;
                case 'results': {
                    refreshResults(route.units, route.page);
                } break;
            }
        }

        query('#cm_ghm_roots').toggleClass('hide', route.view !== 'ghm_roots');
        query('#cm_units').toggleClass('hide', route.view !== 'units');
        query('#cm_table').toggleClass('hide', route.view !== 'durations');
        query('#rt').toggleClass('hide', route.view !== 'results');
        if (!data.isBusy() && !mix_ready) {
            query('#cm').addClass('busy');
        } else if (!data.isBusy()) {
            query('#cm').removeClass('busy');
        }
        query('#cm').removeClass('hide');
    }

    function routeToUrl(args)
    {
        if (!user.isConnected())
            return null;

        const KeepKeys = [
            'period',
            'prev_period',
            'units',
            'ghm_roots',
            'structure',
            'regroup',
            'mode',
            'algorithm',
            'ghm_root',
            'apply_coefficient',
            'refresh'
        ];

        if (args.units)
            args.units = args.units.map(function(str) { return parseInt(str, 10); });

        let new_route = thop.buildRoute(args);
        if (args.page === undefined)
            new_route.page = pages[new_route.view];
        if (args.sort === undefined)
            new_route.sort = sorts[new_route.view];
        if (!new_route.algorithm)
            unspecified = true;

        let short_route_str;
        {
            let short_route = {};
            for (const k of KeepKeys)
                short_route[k] = new_route[k] || null;

            short_route_str = window.btoa(JSON.stringify(short_route));
        }

        let url;
        {
            let url_parts = [thop.baseUrl('mco_casemix'), new_route.view, short_route_str];
            while (!url_parts[url_parts.length - 1])
                url_parts.pop();

            url = url_parts.join('/');
        }

        url = buildUrl(url, {
            page: (new_route.page !== 1) ? new_route.page : null,
            sort: new_route.sort || null
        });

        return url;
    }
    this.routeToUrl = routeToUrl;

    function go(args, delay)
    {
        thop.route(routeToUrl(args), delay);
    }
    this.go = go;

    // A true result actually means maybe (if we haven't download the relevant data yet)
    function checkCasemixGhmRoot(ghm_root)
    {
        return (!mix_ghm_roots.size || mix_ghm_roots.has(ghm_root)) &&
               (!mix_durations[ghm_root] || mix_durations[ghm_root].ghs.length);
    }

    function updateSettings()
    {
        if (user.getUrlKey() !== st_url_key) {
            let url = buildUrl(thop.baseUrl('api/mco_settings.json'), {key: user.getUrlKey()});
            data.get(url, function(json) {
                start_date = json.begin_date;
                end_date = json.end_date;
                algorithms = json.algorithms;
                default_algorithm = json.default_algorithm;
                structures = json.structures;

                for (let structure of structures) {
                    structure.units = {};
                    for (let ent of structure.entities) {
                        ent.path = ent.path.substr(1).split('|');
                        structure.units[ent.unit] = ent;
                    }
                }

                st_url_key = user.getUrlKey();
            });
        }
    }

    function clearCasemix()
    {
        mix_rows.length = 0;
        mix_ghm_roots.clear();
        mix_durations = {};
        rt_results = [];

        mix_ready = false;
    }

    function updateCasemixParams(start, end, mode, diff_start, diff_end, apply_coefficient, refresh)
    {
        let params = {
            dates: (start && end) ? (start + '..' + end) : null,
            dispense_mode: mode,
            diff: (diff_start && diff_end) ? (diff_start + '..' + diff_end) : null,
            apply_coefficient: 0 + apply_coefficient,
            key: user.getUrlKey()
        };
        let url = buildUrl(thop.baseUrl('api/mco_casemix_units.json'), params);

        mix_ready = (url === mix_url);
        if ((refresh && !mix_ready) || user.getUrlKey() !== mix_params.key) {
            clearCasemix();

            mix_url = url;
            mix_params = params;
        }
    }

    function updateCasemixUnits()
    {
        if (!mix_rows.length) {
            data.get(mix_url, function(json) {
                mix_rows = json.rows;

                for (let row of mix_rows) {
                    row.ghm_root = row.ghm.substr(0, 5);
                    mix_ghm_roots.add(row.ghm_root);
                }
            });
        }
    }

    function updateCasemixDuration(ghm_root)
    {
        if (!mix_durations[ghm_root]) {
            let params = Object.assign({ghm_root: ghm_root}, mix_params);
            let url = buildUrl(thop.baseUrl('api/mco_casemix_duration.json'), params);

            data.get(url, function(json) {
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
        let params = Object.assign({ghm_root: ghm_root}, mix_params);
        delete params.diff;
        let url = buildUrl(thop.baseUrl('api/mco_results.json'), params);

        if (url !== rt_url) {
            data.get(url, function(json) {
                rt_results = json;
                rt_url = url;

                for (let result of json)
                    result.ghm_root = result.ghm.substr(0, 5);
            });
        }
    }

    function refreshPeriodsPickers(period, prev_period, mode)
    {
        if (!start_date) {
            period = [null, null];
            prev_period = [null, null];
        }

        let picker = query('#opt_periods > div:last-of-type');
        let prev_picker = query('#opt_periods > div:first-of-type');

        // Set main picker
        {
            let builder = new PeriodPicker(picker, start_date, end_date, period[0], period[1]);
            builder.changeHandler = function() {
                go({period: this.object.getValues()});
            };
            builder.render();
        }

        // Set diff picker
        {
            let builder = new PeriodPicker(prev_picker, start_date, end_date,
                                           prev_period[0], prev_period[1]);
            builder.changeHandler = function() {
                go({prev_period: this.object.getValues()});
            };
            builder.render();
        }

        picker.style.width = (mode !== 'none') ? '49%' : '100%';
        prev_picker.style.width = '49%';
        prev_picker.toggleClass('hide', mode === 'none');
    }

    function refreshStructuresTree(units, structure_idx)
    {
        if (!needsRefresh(refreshStructuresTree, null, [st_url_key].concat(Array.from(arguments))))
            return;

        units = new Set(units);

        let select = query('#opt_units > div');

        let builder = new TreeSelector(select, 'Unités : ');
        builder.changeHandler = function() {
            go({units: this.object.getValues(),
                structure: this.object.getActiveTab()});
        };

        for (const structure of structures) {
            builder.createTab(structure.name);

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
        builder.setActiveTab(structure_idx);

        builder.render();
    }

    function refreshAlgorithmsMenu(algorithm)
    {
        let select = query('#opt_algorithm > select');

        select.innerHTML = '';
        for (const algorithm of algorithms) {
            let text ='Algorithme ' + algorithm.title;
            if (algorithm.name === default_algorithm)
                text += ' *';
            let option = html('option', {value: algorithm.name}, text);
            select.appendChild(option);
        }
        select.value = algorithm;
    }

    function refreshGhmRootsTree(ghm_roots, select_ghm_roots, regroup)
    {
        if (!needsRefresh(refreshGhmRootsTree, null, [st_url_key].concat(Array.from(arguments))))
            return;

        // TODO: This should probably not be hard-coded here
        const GroupTypes = [
            {key: 'none', text: 'Racines'},
            {key: 'cmd', text: 'CMD'},
            {key: 'da', text: 'DA'},
            {key: 'ga', text: 'GA'}
        ];

        select_ghm_roots = new Set(select_ghm_roots);

        let ghm_roots_map = mco_common.updateCatalog('mco_ghm_roots').map;

        let select = query('#opt_ghm_roots > div');

        let builder = new TreeSelector(select, 'GHM : ');
        builder.changeHandler = function() {
            go({ghm_roots: this.object.getValues(),
                regroup: GroupTypes[this.object.getActiveTab()].key});
        };

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

            builder.createTab(group_type.text);

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
        builder.setActiveTab(GroupTypes.findIndex(function(group_type) {
            return group_type.key === regroup;
        }));

        builder.render();
    }

    function refreshGhmRootsMenu(ghm_roots, select_ghm_root)
    {
        let el = query('#opt_ghm_root > select');
        el.innerHTML = '';

        for (const ghm_root of ghm_roots) {
            let opt = html('option', {value: ghm_root.ghm_root},
                           ghm_root.ghm_root + ' – ' + ghm_root.desc);

            if (!checkCasemixGhmRoot(ghm_root.ghm_root)) {
                opt.setAttribute('disabled', '');
                opt.text += '*';
            }

            el.appendChild(opt);
        }
        if (select_ghm_root)
            el.value = select_ghm_root;
    }

    function refreshUnitsTable(units, ghm_roots, structure, page, sort)
    {
        if (!needsRefresh(refreshUnitsTable, null, [mix_url].concat(Array.from(arguments))))
            return;

        if (!units_summary ||
                needsRefresh(refreshUnitsTable, 'init', [mix_url, units, ghm_roots, structure])) {
            units = new Set(units);
            ghm_roots = new Set(ghm_roots);
            structure = structures[structure];

            const rows = mix_rows.filter(function(row) {
                if (!ghm_roots.has(row.ghm_root))
                    return false;
                for (const unit of row.unit) {
                    if (units.has(unit))
                        return true;
                }
                return false;
            });

            units_summary = createPagedDataTable(query('#cm_units'));
            units_summary.sortHandler = function(sort) { go({sort: sort}); };

            units_summary.addColumn('unit', null, 'Unité');
            units_summary.addColumn('rss', '#,##0', 'RSS');
            if (!mix_params.diff)
                units_summary.addColumn('rss_pct', '0.00%', '%');
            units_summary.addColumn('rum', '#,##0', 'RUM');
            if (!mix_params.diff)
                units_summary.addColumn('rum_pct', '0.00%', '%');
            units_summary.addColumn('total', '#,##0', 'Total');
            if (!mix_params.diff)
                units_summary.addColumn('total_pct', '0.00%', '%');
            units_summary.addColumn('pay', '#,##0', 'Rétribué');
            if (!mix_params.diff)
                units_summary.addColumn('pay_pct', '0.00%', '%');
            units_summary.addColumn('deaths', '#,##0', 'Décès');
            if (!mix_params.diff)
                units_summary.addColumn('deaths_pct', '0.00%', '%');

            // Aggregate
            let stat0;
            let stats1;
            {
                function unitToEntities(row)
                {
                    let values = row.unit.map(function(unit) { return structure.units[unit].path; });
                    return ['entities', values];
                }
                function filterUnitParts(row)
                {
                    let values = row.unit.map(function(unit) { return units.has(unit); });
                    return ['include', values];
                }

                stat0 = aggregate(rows, filterUnitParts).list[0];
                stats1 = aggregate(rows, unitToEntities, filterUnitParts);
            }

            if (stat0) {
                units_summary.beginRow();
                units_summary.addCell('Total');
                addSummaryCells(units_summary, stat0, stat0);

                let prev_groups = [];
                let prev_totals = [stat0];
                for (const ent of structure.entities) {
                    let unit_stat = findAggregate(stats1, ent.path[ent.path.length - 1]);
                    if (!unit_stat)
                        continue;

                    let common_len = 0;
                    while (common_len < ent.path.length - 1 && common_len < prev_groups.length &&
                           ent.path[common_len] === prev_groups[common_len])
                        common_len++;
                    while (prev_groups.length > common_len) {
                        units_summary.endRow();
                        prev_groups.pop();
                        prev_totals.pop();
                    }

                    for (let k = common_len; k < ent.path.length - 1; k++) {
                        let group_stat = findAggregate(stats1, ent.path[k]);

                        units_summary.beginRow();
                        units_summary.addCell(ent.path[k], {title: ent.path[k]}, ent.path[k]);
                        addSummaryCells(units_summary, group_stat, prev_totals[prev_totals.length - 1]);

                        prev_groups.push(ent.path[k]);
                        prev_totals.push(group_stat);
                    }

                    units_summary.beginRow();
                    units_summary.addCell(ent.path[ent.path.length - 1],
                                          {title: ent.path[ent.path.length - 1]}, ent.path[ent.path.length - 1]);
                    addSummaryCells(units_summary, unit_stat, prev_totals[prev_totals.length - 1]);
                    units_summary.endRow();
                }
            }
        }

        units_summary.sort(sort);

        let render_count = units_summary.render((page - 1) * TableLen, TableLen,
                                                {render_empty: false});
        syncPagers(queryAll('#cm_units .pagr'), page,
                   computeLastPage(render_count, units_summary.getRowCount(), TableLen));
    }

    function refreshGhmRootsTable(units, ghm_roots, regroup, page, sort)
    {
        if (!needsRefresh(refreshGhmRootsTable, null, [mix_url].concat(Array.from(arguments))))
            return;

        if (!ghm_roots_summary ||
                needsRefresh(refreshGhmRootsTable, 'init', [mix_url, units, ghm_roots, regroup])) {
            units = new Set(units);
            ghm_roots = new Set(ghm_roots);

            const rows = mix_rows.filter(function(row) {
                if (!ghm_roots.has(row.ghm_root))
                    return false;
                for (const unit of row.unit) {
                    if (units.has(unit))
                        return true;
                }
                return false;
            });

            let ghm_roots_map = mco_common.updateCatalog('mco_ghm_roots').map;

            ghm_roots_summary = createPagedDataTable(query('#cm_ghm_roots'));
            ghm_roots_summary.sortHandler = function(sort) { go({sort: sort}); };

            ghm_roots_summary.addColumn('ghm_root', null, 'Racine');
            ghm_roots_summary.addColumn('rss', '#,##0', 'RSS');
            if (!mix_params.diff)
                ghm_roots_summary.addColumn('rss_pct', '0.00%', '%');
            ghm_roots_summary.addColumn('rum', '#,##0', 'RUM');
            if (!mix_params.diff)
                ghm_roots_summary.addColumn('rum_pct', '0.00%', '%');
            ghm_roots_summary.addColumn('total', '#,##0', 'Total');
            if (!mix_params.diff)
                ghm_roots_summary.addColumn('total_pct', '0.00%', '%');
            ghm_roots_summary.addColumn('pay', '#,##0', 'Rétribué');
            if (!mix_params.diff)
                ghm_roots_summary.addColumn('pay_pct', '0.00%', '%');
            ghm_roots_summary.addColumn('deaths', '#,##0', 'Décès');
            if (!mix_params.diff)
                ghm_roots_summary.addColumn('deaths_pct', '0.00%', '%');

            // Aggregate
            let stat0;
            let stats1;
            let stats2;
            {
                function ghmRootToGroup(row)
                {
                    let ghm_root_info = ghm_roots_map[row.ghm_root];
                    let group = ghm_root_info ? (ghm_root_info[regroup] || null) : null;
                    return ['group', group];
                }
                function filterUnitParts(row)
                {
                    let values = row.unit.map(function(unit) { return units.has(unit); });
                    return ['include', values];
                }

                stat0 = aggregate(rows, filterUnitParts).list[0];
                stats1 = aggregate(rows, 'ghm_root', filterUnitParts);
                stats2 = aggregate(rows, ghmRootToGroup, filterUnitParts);
            }

            if (stat0) {
                let ghm_roots = stats1.list.map(function(stat) { return stat.ghm_root; });
                ghm_roots = ghm_roots.sort(function(ghm_root1, ghm_root2) {
                    const ghm_root_info1 = ghm_roots_map[ghm_root1] || {};
                    const ghm_root_info2 = ghm_roots_map[ghm_root2] || {};

                    if (ghm_root_info1[regroup] !== ghm_root_info2[regroup]) {
                        return (ghm_root_info1[regroup] < ghm_root_info2[regroup]) ? -1 : 1;
                    } else if (ghm_root_info1.ghm_root !== ghm_root_info2.ghm_root) {
                        return (ghm_root_info1.ghm_root < ghm_root_info2.ghm_root) ? -1 : 1;
                    } else {
                        return 0;
                    }
                });

                ghm_roots_summary.beginRow();
                ghm_roots_summary.addCell('Total');
                addSummaryCells(ghm_roots_summary, stat0, stat0);

                let prev_group = null;
                let total = stat0;
                for (const ghm_root of ghm_roots) {
                    let root_stat = findAggregate(stats1, ghm_root);
                    let ghm_root_info = ghm_roots_map[ghm_root];
                    let group = ghm_root_info ? ghm_root_info[regroup] : null;

                    if (group !== prev_group) {
                        if (prev_group) {
                            ghm_roots_summary.endRow();
                            total = stat0;
                        }

                        if (group) {
                            let group_stat = findAggregate(stats2, group);

                            ghm_roots_summary.beginRow();
                            ghm_roots_summary.addCell(group + ' - ' + ghm_root_info[regroup + '_desc']);
                            addSummaryCells(ghm_roots_summary, group_stat, total);

                            total = group_stat;
                        }

                        prev_group = group;
                    }

                    let elements = [
                        html('a', {href: routeToUrl({view: 'durations', ghm_root: ghm_root})}, ghm_root),
                        ghm_root_info ? ' - ' + ghm_root_info.desc : null
                    ];
                    let title = ghm_root_info ? (ghm_root + ' - ' + ghm_root_info.desc) : null;

                    ghm_roots_summary.beginRow();
                    ghm_roots_summary.addCell(ghm_root, {title: title}, elements);
                    addSummaryCells(ghm_roots_summary, root_stat, total);
                    ghm_roots_summary.endRow();
                }
            }
        }

        ghm_roots_summary.sort(sort);

        let render_count = ghm_roots_summary.render((page - 1) * TableLen, TableLen,
                                                    {render_empty: false});
        syncPagers(queryAll('#cm_ghm_roots .pagr'), page,
                   computeLastPage(render_count, ghm_roots_summary.getRowCount(), TableLen));
    }

    function addSummaryCells(dtab, stat, total)
    {
        function addPercentCell(value)
        {
            if (!isNaN(value)) {
                dtab.addCell(value, percentText(value));
            } else {
                dtab.addCell(null, null);
            }
        }

        dtab.addCell(stat.count, numberText(stat.count));
        if (!mix_params.diff)
            addPercentCell(stat.count / total.count);

        dtab.addCell(stat.mono_count, numberText(stat.mono_count));
        if (!mix_params.diff)
            addPercentCell(stat.mono_count / total.mono_count);

        dtab.addCell(stat.price_cents_total, priceText(stat.price_cents_total, false));
        if (!mix_params.diff)
            addPercentCell(stat.price_cents_total / total.price_cents_total);

        dtab.addCell(stat.price_cents, priceText(stat.price_cents, false));
        if (!mix_params.diff)
            addPercentCell(stat.price_cents / total.price_cents);

        dtab.addCell(stat.deaths, numberText(stat.deaths));
        if (!mix_params.diff)
            addPercentCell(stat.deaths / stat.count);
    }

    function syncPagers(pagers, active_page, last_page)
    {
        pagers.forEach(function(pager) {
            if (last_page) {
                let builder = new Pager(pager, active_page, last_page);
                builder.anchorBuilder = function(text, active_page) {
                    return html('a', {href: routeToUrl({page: active_page})}, '' + text);
                }
                builder.render();

                pager.removeClass('hide');
            } else {
                pager.addClass('hide');
                pager.innerHTML = '';
            }
        });
    }

    function refreshDurationTable(units, ghm_root, apply_coeff, merge_cells)
    {
        if (!needsRefresh(refreshDurationTable, null, [mix_url, units, ghm_root, merge_cells]))
            return;

        let table = html('table',
            html('thead'),
            html('tbody')
        );

        if (mix_durations[ghm_root] && mix_durations[ghm_root].rows.length) {
            units = new Set(units);

            const columns = mix_durations[ghm_root].ghs;
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

                stat0 = aggregate(rows, filterUnitParts).list[0];
                stats1 = aggregate(rows, 'ghm', 'ghs', filterUnitParts);
                stats1_units = aggregate(rows, 'ghm', 'ghs', 'unit');
                stats2 = aggregate(rows, 'ghm', 'ghs', 'duration', filterUnitParts);
                stats2_units = aggregate(rows, 'ghm', 'ghs', 'duration', 'unit');
                stats3 = aggregate(rows, 'duration', filterUnitParts);
            }

            let max_duration = 10;
            let max_count = 0;
            let max_price_cents = 0;
            for (const duration_stat of stats2.list) {
                max_duration = Math.max(max_duration, duration_stat.duration + 1);
                max_count = Math.max(max_count, duration_stat.count);
                max_price_cents = Math.max(max_price_cents, duration_stat.price_cents);
            }

            mco_pricing.addPricingHeader(thead, ghm_root, columns, false, apply_coeff, merge_cells);
            thead.queryAll('td').forEach(function(td) {
                td.setAttribute('colspan', parseInt(td.getAttribute('colspan') || 1) * 2);
            });

            function makeTooltip(title, col, col_stat, row_stat, duration_stat, unit_stats)
            {
                unit_stats = Object.values(unit_stats).sort(function(unit1, unit2) {
                    return unit1.unit - unit2.unit;
                });

                let tooltip = '';

                if (!mix_params.diff) {
                    tooltip += col.ghm + ' (' + col.ghs + ') – ' + title + ' :\n' +
                               '– ' + percentText(duration_stat.count / stat0.count) + ' / ' +
                               percentText(duration_stat.price_cents_total / stat0.price_cents_total) +
                               ' de la racine\n' +
                               '– ' + percentText(duration_stat.count / col_stat.count) + ' / ' +
                               percentText(duration_stat.price_cents_total / col_stat.price_cents_total) +
                               ' de la colonne\n' +
                               '– ' + percentText(duration_stat.count / row_stat.count) + ' / ' +
                               percentText(duration_stat.price_cents_total / row_stat.price_cents_total) +
                               ' de la ligne\n\n';
                }

                tooltip += 'Unités :';
                {
                    let missing_cents = duration_stat.price_cents_total;
                    for (const unit_stat of unit_stats) {
                        tooltip += '\n– ' + unit_stat.unit + ' : ' + priceText(unit_stat.price_cents);
                        if (!mix_params.diff)
                            tooltip += ' (' + percentText(unit_stat.price_cents / duration_stat.price_cents_total) + ')';
                        missing_cents -= unit_stat.price_cents;
                    }

                    if (missing_cents) {
                        tooltip += '\n– Autres : ' + priceText(missing_cents);
                        if (!mix_params.diff)
                            tooltip += ' (' + percentText(missing_cents / duration_stat.price_cents_total) + ')';
                    }
                }

                return tooltip;
            }

            function diffClass(value)
            {
                if (mix_params.diff) {
                    if (value > 0) {
                        return ' diff higher';
                    } else if (value < 0) {
                        return ' diff lower';
                    } else {
                        return ' diff neutral';
                    }
                } else {
                    return '';
                }
            }

            // Totals
            {
                let tr = html('tr',
                    html('th', 'Total')
                );

                for (const col of columns) {
                    let col_stat = findAggregate(stats1, col.ghm, col.ghs);

                    if (col_stat) {
                        let tooltip =
                            makeTooltip('Total', col, col_stat, stat0, col_stat,
                                        findPartialAggregate(stats1_units, col.ghm, col.ghs));

                        tr.appendChildren([
                            html('td', {class: 'count total' + diffClass(col_stat.count),
                                        title: tooltip},
                                 '' + col_stat.count),
                            html('td', {class: 'price total' + diffClass(col_stat.price_cents_total),
                                        title: tooltip},
                                 priceText(col_stat.price_cents_total, false))
                        ]);
                    } else {
                        tr.appendChildren([
                            html('td', {class: 'count total empty'}),
                            html('td', {class: 'price total empty'})
                        ]);
                    }
                }

                tbody.appendChild(tr);
            }

            // Durations
            for (let duration = 0; duration < max_duration; duration++) {
                let row_stat = findAggregate(stats3, duration);

                if (duration % 10 == 0) {
                    let text = '' + duration + ' - ' +
                                    mco_common.durationText(Math.min(max_duration - 1, duration + 9));
                    let tr = html('tr',
                        html('th', {class: 'repeat', colspan: columns.length * 2 + 1}, text)
                    );
                    tbody.appendChild(tr);
                }

                let tr = html('tr',
                    html('th', mco_common.durationText(duration))
                );
                for (const col of columns) {
                    let col_stat = findAggregate(stats1, col.ghm, col.ghs);
                    let duration_stat = findAggregate(stats2, col.ghm, col.ghs, duration);

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
                            makeTooltip(mco_common.durationText(duration),
                                        col, col_stat, row_stat, duration_stat,
                                        findPartialAggregate(stats2_units, col.ghm, col.ghs, duration));

                        tr.appendChildren([
                            html('td', {class: 'count ' + cls + diffClass(duration_stat.count),
                                        title: tooltip},
                                 '' + duration_stat.count),
                            html('td', {class: 'price ' + cls + diffClass(duration_stat.price_cents_total),
                                        title: tooltip},
                                 priceText(duration_stat.price_cents_total, false))
                        ]);
                    } else if (mco_pricing.testDuration(col, duration)) {
                        cls += ' empty';
                        tr.appendChildren([
                            html('td', {class: 'count ' + cls}),
                            html('td', {class: 'price ' + cls})
                        ]);
                    } else {
                        tr.appendChildren([
                            html('td', {class: 'count'}),
                            html('td', {class: 'price'})
                        ]);
                    }
                }
                tbody.appendChild(tr);
            }
        }

        let old_table = query('#cm_table');
        table.copyAttributesFrom(old_table);
        old_table.replaceWith(table);
    }

    function refreshResults(units, page)
    {
        if (!needsRefresh(refreshResults, null, [mix_url, rt_url, units, page]))
            return;

        units = new Set(units);

        let cim10_map = mco_common.updateCatalog('cim10').map;
        let ccam_map = mco_common.updateCatalog('ccam').map;
        let ghm_roots_map = mco_common.updateCatalog('mco_ghm_roots').map;
        let errors_map = mco_common.updateCatalog('mco_errors').map;

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
            if (unit && structures[0]) {
                let unit_info = structures[0].units[unit];
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

        let results = rt_results.filter(function(result) {
            for (const stay of result.stays) {
                if (units.has(stay.unit))
                    return true;
            }
            return false;
        });

        let offset = (page - 1) * TableLen;
        let end = Math.min(offset + TableLen, results.length);

        let elements = [];
        for (let i = offset; i < end; i++) {
            const result = results[i];

            let table = html('table', {class: 'rt_result', 'data-bill_id': result.bill_id},
                html('tbody',
                    html('tr', {class: 'rt_header'},
                        html('td',
                            html('a', {class: 'rt_id', href: '#', click: handleIdClick},
                                 '' + result.bill_id)
                        ),
                        html('td', (['♂', '♀'][result.sex - 1] || '?') + ' ' + result.age + ' ans'),
                        html('td', result.duration >= 0 ? mco_common.durationText(result.duration) : null),
                        html('td', {title: codeWithDesc(ghm_roots_map, result.ghm_root) + '\n\n' +
                                           'GHM : ' + result.ghm + '\n' +
                                           'Erreur : ' + codeWithDesc(errors_map, result.main_error) + '\n' +
                                           'GHS : ' + result.ghs},
                            html('a', {href: mco_pricing.routeToUrl({view: 'table',
                                                                     date: result.index_date,
                                                                     ghm_root: result.ghm.substr(0, 5)})},
                                 result.ghm),
                            result.ghm.startsWith('90') ? (' [' + result.main_error + ']')
                                                        : (' (' + result.ghs + ')')
                        ),
                        html('td', {style: 'text-align: right;'},
                             priceText(result.price_cents) + '€'),
                        html('td', {style: 'text-align: right;'},
                             priceText(result.total_cents) + '€')
                    )
                )
            );
            let tbody = table.query('tbody');

            table.dataset.bill_id =result.bill_id;
            if (deploy_results.has(table.dataset.bill_id))
                table.addClass('deploy');

            for (let j = 0; j < result.stays.length; j++) {
                const stay = result.stays[j];

                tbody.appendChildren(html('tr', {class: 'rt_stay'},
                    html('td', 'RUM ' + (j + 1) + (j == result.main_stay ? ' *' : '')),
                    html('th', {title: unitPath(stay.unit)}, '' + (stay.unit || '')),
                    html('td', stay.duration >= 0 ? mco_common.durationText(stay.duration) : null),
                    html('td'),
                    html('td', {style: 'text-align: right;'},
                         stay.total_cents ? (priceText(stay.price_cents) + '€') : ''),
                    html('td', {style: 'text-align: right;'},
                         stay.price_cents ? (priceText(stay.total_cents) + '€') : '')
                ));

                if (stay.sex) {
                    let tr = html('tr', {class: 'rt_details'},
                        html('td', {colspan: 6},
                            html('div',
                                html('div', html('table')),
                                html('div', html('table')),
                                html('div', html('table'))
                            )
                        )
                    );
                    let div = tr.query('div');
                    let table0 = div.childNodes[0].firstChild;
                    let table1 = div.childNodes[1].firstChild;
                    let table2 = div.childNodes[2].firstChild;

                    table0.appendChildren([
                        html('tr',
                            html('th', 'Sexe'),
                            html('td', ['Homme', 'Femme'][stay.sex - 1] || '')
                        ),
                        html('tr',
                            html('th', 'Date de naissance'),
                            html('td', stay.birthdate)
                        ),
                        html('tr',
                            html('th', 'Entrée'),
                            html('td', '' + stay.entry_date + ' ' + stay.entry_mode +
                                       (stay.entry_origin ? '-' + stay.entry_origin : ''))
                        ),
                        html('tr',
                            html('th', 'Sortie'),
                            html('td', '' + stay.exit_date + ' ' + stay.exit_mode +
                                       (stay.exit_destination ? '-' + stay.exit_destination : ''))
                        ),
                        stay.bed_authorization ? html('tr',
                            html('th', 'Autorisation de Lit'),
                            html('td', '' + stay.bed_authorization)
                        ) : null,
                        stay.igs2 ? html('tr',
                            html('th', 'IGS 2'),
                            html('td', '' + stay.igs2)
                        ) : null,
                        stay.session_count ? html('tr',
                            html('th', 'Séances'),
                            html('td', '' + stay.session_count)
                        ) : null,
                        stay.last_menstrual_period ? html('tr',
                            html('th', 'Dernières règles'),
                            html('td', '' + stay.last_menstrual_period)
                        ) : null,
                        stay.gestational_age ? html('tr',
                            html('th', 'Âge gestationnel'),
                            html('td', '' + stay.gestational_age + ' SA')
                        ) : null,
                        stay.newborn_weight ? html('tr',
                            html('th', 'Poids du nouveau-né'),
                            html('td', '' + stay.newborn_weight + ' grammes')
                        ) : null,
                        stay.confirm ? html('tr',
                            html('th', 'Confirmation'),
                            html('td', 'Oui')
                        ) : null,
                        stay.dip_count ? html('tr',
                            html('th', 'Séances de DIP'),
                            html('td', '' + stay.dip_count)
                        ) : null,
                        stay.ucd ? html('tr',
                            html('th', 'Présence UCD'),
                            html('td', 'Oui')
                        ) : null
                    ]);

                    table1.appendChildren([
                        stay.main_diagnosis ? html('tr',
                            html('th', 'Diagnostic principal'),
                            html('td', {title: codeWithDesc(cim10_map, stay.main_diagnosis)},
                                 stay.main_diagnosis)
                        ) : null,
                        stay.linked_diagnosis ? html('tr',
                            html('th', 'Diagnostic relié'),
                            html('td', {title: codeWithDesc(cim10_map, stay.linked_diagnosis)},
                                 stay.linked_diagnosis)
                        ) : null
                    ]);
                    for (let k = 0; k < stay.other_diagnoses.length; k++) {
                        const diag = stay.other_diagnoses[k];

                        let contents = [diag.diag];
                        if (diag.severity) {
                            if (diag.exclude) {
                                contents.push(' ');
                                contents.push(html('s', '(' + (diag.severity + 1) + ')'));
                            } else {
                                contents.push(' (' + (diag.severity + 1) + ')');
                            }
                        }

                        let tr = html('tr',
                            !k ? html('th', 'Diagnostics associés') : html('td'),
                            html('td', {title: codeWithDesc(cim10_map, diag.diag)}, contents)
                        );

                        table1.appendChild(tr);
                    }

                    for (let k = 0; k < stay.procedures.length; k++) {
                        const proc = stay.procedures[k];
                        let text = proc.proc + (proc.phase ? '/' + proc.phase : '') +
                                   ' (' + proc.activity + ')' +
                                   (proc.count !== 1 ? ' * ' + proc.count : '');

                        let tr = html('tr',
                            !k ? html('th', 'Actes') : html('td'),
                            html('td', {title: codeWithDesc(ccam_map, proc.proc)}, text)
                        );

                        table2.appendChild(tr);
                    }

                    tbody.appendChild(tr);
                }
            }

            elements.push(table);
        }

        let pagers = [
            html('table', {class: 'pagr'}),
            html('table', {class: 'pagr'})
        ];
        syncPagers(pagers, page, computeLastPage(end - offset, results.length, TableLen));

        query('#rt').innerHTML = '';
        query('#rt').appendChildren([
            pagers[0],
            elements,
            pagers[1]
        ]);
    }

    function aggregate(rows, by)
    {
        if (!Array.isArray(by))
            by = Array.prototype.slice.call(arguments, 1);

        let list = [];
        let map = {};

        let template = {
            count: 0,
            deaths: 0,
            mono_count_total: 0,
            price_cents_total: 0,
            mono_count: 0,
            price_cents: 0
        };

        function aggregateRec(row, row_ptrs, ptr, col_idx, key_idx)
        {
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

            if (ptr.count === undefined) {
                Object.assign(ptr, template);
                list.push(ptr);
            }

            if (!row_ptrs.has(ptr)) {
                ptr.count += row.count;
                ptr.deaths += row.deaths;
                ptr.mono_count_total += row.mono_count_total;
                ptr.price_cents_total += row.price_cents_total;

                row_ptrs.add(ptr);
            }

            ptr.mono_count += row.mono_count[col_idx];
            ptr.price_cents += row.price_cents[col_idx];
        }

        for (const row of rows) {
            const max_idx = row.mono_count.length;

            let row_ptrs = new Set;
            for (let i = 0; i < max_idx; i++)
                aggregateRec(row, row_ptrs, map, i, 0);
        }

        return {list: list, map: map};
    }

    function findPartialAggregate(agg, values)
    {
        if (!Array.isArray(values))
            values = Array.prototype.slice.call(arguments, 1);

        let ptr = agg.map;
        for (const value of values) {
            ptr = ptr[value];
            if (ptr === undefined)
                return null;
        }

        return ptr;
    }

    function findAggregate(agg, values)
    {
        let ptr = findPartialAggregate.apply(null, arguments);
        if (ptr && ptr.count === undefined)
            return null;

        return ptr;
    }

    // Clear casemix data when user changes or disconnects
    user.addChangeHandler(function() {
        clearCasemix();
        deploy_results.clear();

        queryAll('#cm > *').forEach(function(el) {
            el.innerHTML = '';
        });
    });

    thop.registerUrl('mco_casemix', this, runCasemix);
}).call(mco_casemix);
