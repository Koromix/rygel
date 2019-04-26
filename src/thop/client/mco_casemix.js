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

    function runCasemix(route, path, parameters, hash, errors)
    {
        // Parse route (model: casemix/<view>/<json_parameters_in_base64>)
        let path_parts = path.split('/', 3);
        if (path_parts[2])
            Object.assign(route, thop.buildRoute(JSON.parse(window.atob(path_parts[2]))));
        route.view = path_parts[1] || 'ghm_roots';
        route.period = route.period || [null, null];
        route.prev_period = route.prev_period || [null, null];
        route.structure = route.structure || 0;
        route.regroup = route.regroup || 'none';
        route.mode = route.mode || 'none';
        route.algorithm = route.algorithm || null;
        route.filter = route.filter || null;
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
        settings = thop.updateMcoSettings();
        let indexes = settings.indexes;
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        let ghm_roots = catalog.update('mco_ghm_roots').concepts;
        if (['durations', 'results'].includes(route.view) && !route.ghm_root && ghm_roots.length)
            route.ghm_root = ghm_roots[0].ghm_root;
        if (unspecified && settings.structures.length && ghm_roots.length) {
            if (!route.units.length && settings.structures[route.structure])
                route.units = settings.structures[route.structure].entities.map(function(ent) { return ent.unit; }).sort();
            if (!route.ghm_roots.length)
                route.ghm_roots = ghm_roots.map(function(ghm_root) { return ghm_root.ghm_root; }).sort();

            unspecified = false;
        }

        // Permissions
        if (!user.isConnected() || !settings.permissions) {
            if (!user.isConnected())
                errors.add('Vous n\'êtes pas connecté(e)');
            query('#cm').addClass('hide');
            return;
        }
        if (route.view === 'results' && !settings.permissions.has('FullResults')) {
            errors.add('Vous n\'avez pas les droits pour utiliser cette page');
            query('#cm').addClass('hide');
            return;
        }

        // Casemix
        if (!route.period[0]) {
            let year = parseInt(settings.end_date.split('-')[0], 10);
            if (settings.end_date.endsWith('-01-01')) {
                route.period = ['' + (year - 1) + '-01-01', settings.end_date];
                route.prev_period = ['' + (year - 2) + '-01-01', '' + (year - 1) + '-01-01'];
            } else {
                route.period = ['' + year + '-01-01', settings.end_date];
                route.prev_period = ['' + (year - 1) + '-01-01', '' + (year - 1) + settings.end_date.substring(4)];
            }
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
            errors.add('Mode d\'affichage incorrect');
        if (!route.units.length && mix_ready)
            errors.add('Aucune unité sélectionnée');
        if (['ghm_roots', 'units'].includes(route.view) && !route.ghm_roots.length && mix_ready)
            errors.add('Aucune racine sélectionnée');
        if (route.structure > settings.structures.length && settings.structures.length)
            errors.add('Structure inexistante');
        if (!['none', 'cmd', 'da', 'ga'].includes(route.regroup))
            errors.add('Regroupement incorrect');
        if (['durations', 'results'].includes(route.view)) {
            if (!checkCasemixGhmRoot(route.ghm_root))
                errors.add('Cette racine n\'existe pas dans cette période');
            if (route.view === 'durations' && mix_mismatched_roots.has(route.ghm_root))
                errors.add('Regroupement des GHS suite à changement')
        }
        if (!(['none', 'absolute'].includes(route.mode)))
            errors.add('Mode de comparaison inconnu');
        if (settings.algorithms.length &&
                !settings.algorithms.find(function(algorithm) { return algorithm.name === route.algorithm; }))
            errors.add('Algorithme inconnu');

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

        // Update visible view
        {
            const ViewNodes = {
                'ghm_roots': query('#cm_ghm_roots'),
                'units': query('#cm_units'),
                'durations': query('#cm_table'),
                'results': query('#rt')
            };

            for (const k in ViewNodes) {
                if (k === route.view) {
                    if (!thop.isBusy())
                        ViewNodes[k].removeClass('hide');
                } else {
                    ViewNodes[k].addClass('hide');
                }
            }
        }

        // Reveal casemix
        if (!thop.isBusy() && !mix_ready) {
            query('#cm').addClass('busy');
        } else if (!thop.isBusy()) {
            query('#cm').removeClass('busy');
        }
        query('#cm').removeClass('hide');
    }

    function routeToUrl(args)
    {
        const KeepKeys = [
            'period',
            'prev_period',
            'units',
            'ghm_roots',
            'structure',
            'regroup',
            'mode',
            'algorithm',
            'filter',
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

        url = util.buildUrl(url, {
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

    function go(args, delay)
    {
        thop.route(routeToUrl(args).url, delay);
    }
    this.go = go;

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
            let builder = new PeriodPicker(picker, settings.start_date, settings.end_date,
                                           period[0], period[1]);
            builder.changeHandler = function() {
                go({period: this.object.getValues()});
            };
            builder.render();
        }

        // Set diff picker
        {
            let builder = new PeriodPicker(prev_picker, settings.start_date, settings.end_date,
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
        if (!thop.needsRefresh(refreshStructuresTree, settings.url_key, arguments))
            return;

        units = new Set(units);

        let select = query('#opt_units > div');

        let builder = new TreeSelector(select, 'Unités : ');
        builder.changeHandler = function() {
            go({units: this.object.getValues(),
                structure: this.object.getActiveTab()});
        };

        for (const structure of settings.structures) {
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
        let el = query('#opt_algorithm > select');

        el.replaceContent(
            settings.algorithms.map(function(algorithm) {
                return dom.h('option',
                    {value: algorithm.name},
                    'Algorithme ' + algorithm.name + (algorithm.name === settings.default_algorithm ? ' *' : '')
                );
            })
        );
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

        el.replaceContent(
            ghm_roots.map(function(ghm_root_info) {
                let disabled = !checkCasemixGhmRoot(ghm_root_info.ghm_root);
                return dom.h('option',
                    {value: ghm_root_info.ghm_root, disabled: disabled},
                    ghm_root_info.ghm_root + ' – ' + ghm_root_info.desc + (disabled ? ' *' : '')
                );
            })
        );
        el.value = select_ghm_root || el.value;
    }

    function refreshUnitsTable(filter_units, filter_ghm_roots, structure, page, sort)
    {
        if (!thop.needsRefresh(refreshUnitsTable, mix_url, arguments))
            return;

        let summary = summaries.units;
        if (!summary) {
            summary = createPagedDataTable(query('#cm_units'));
            summary.sortHandler = function(sort) { go({sort: sort}); };

            summaries.units = summary;
        }

        if (thop.needsRefresh(summary, mix_url, filter_units, filter_ghm_roots, structure)) {
            filter_units = new Set(filter_units);
            filter_ghm_roots = new Set(filter_ghm_roots);
            structure = settings.structures[structure];

            const rows = filterCasemix(mix_rows, filter_units, filter_ghm_roots);

            summary.clear();

            summary.addColumn('unit', null, 'Unité');
            summary.addColumn('rss', '#,##0', 'RSS');
            if (!mix_params.diff)
                summary.addColumn('rss_pct', '0.00%', '%');
            summary.addColumn('rum', '#,##0', 'RUM');
            if (!mix_params.diff)
                summary.addColumn('rum_pct', '0.00%', '%');
            summary.addColumn('total', '#,##0.00', 'Total');
            if (!mix_params.diff)
                summary.addColumn('total_pct', '0.00%', '%');
            summary.addColumn('pay', '#,##0.00', 'Rétribué');
            if (!mix_params.diff)
                summary.addColumn('pay_pct', '0.00%', '%');
            summary.addColumn('deaths', '#,##0', 'Décès');
            if (!mix_params.diff)
                summary.addColumn('deaths_pct', '0.00%', '%');

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
                summary.addCell('td', 'Total');
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
                        summary.addCell('td', ent.path[k], {title: ent.path[k]}, ent.path[k]);
                        addSummaryCells(summary, group_stat, prev_totals[prev_totals.length - 1]);

                        prev_groups.push(ent.path[k]);
                        prev_totals.push(group_stat);
                    }

                    summary.beginRow();
                    summary.addCell('td', ent.path[ent.path.length - 1],
                                    {title: ent.path[ent.path.length - 1]}, ent.path[ent.path.length - 1]);
                    addSummaryCells(summary, unit_stat, prev_totals[prev_totals.length - 1]);
                    summary.endRow();
                }
            }
        }

        summary.sort(sort);

        let render_count = summary.render((page - 1) * TableLen, TableLen, {render_empty: false});
        syncPagers(queryAll('#cm_units .pagr'), page,
                   computeLastPage(render_count, summary.getRowCount(), TableLen));
    }

    function refreshGhmRootsTable(filter_units, filter_ghm_roots, regroup, page, sort)
    {
        if (!thop.needsRefresh(refreshGhmRootsTable, mix_url, arguments))
            return;

        let summary = summaries.ghm_roots;
        if (!summary) {
            summary = createPagedDataTable(query('#cm_ghm_roots'));
            summary.sortHandler = function(sort) { go({sort: sort}); };

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

            summary.addColumn('ghm_root', null, 'Racine');
            summary.addColumn('rss', '#,##0', 'RSS');
            if (!mix_params.diff)
                summary.addColumn('rss_pct', '0.00%', '%');
            summary.addColumn('rum', '#,##0', 'RUM');
            if (!mix_params.diff)
                summary.addColumn('rum_pct', '0.00%', '%');
            summary.addColumn('total', '#,##0.00', 'Total');
            if (!mix_params.diff)
                summary.addColumn('total_pct', '0.00%', '%');
            summary.addColumn('pay', '#,##0.00', 'Rétribué');
            if (!mix_params.diff)
                summary.addColumn('pay_pct', '0.00%', '%');
            summary.addColumn('deaths', '#,##0', 'Décès');
            if (!mix_params.diff)
                summary.addColumn('deaths_pct', '0.00%', '%');

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
                summary.addCell('td', 'Total');
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
                            summary.addCell('td', group + ' - ' + ghm_root_info[regroup + '_desc']);
                            addSummaryCells(summary, stat, total);
                            total = stat;
                        }

                        prev_group = group;
                    }

                    let elements = [
                        dom.h('a', {href: routeToUrl({view: 'durations', ghm_root: ghm_root}).url}, ghm_root),
                        ghm_root_info ? ' - ' + ghm_root_info.desc : null
                    ];
                    let title = ghm_root_info ? (ghm_root + ' - ' + ghm_root_info.desc) : null;

                    summary.beginRow();
                    summary.addCell('td', ghm_root, {title: title}, elements);
                    addSummaryCells(summary, stat, total);
                    summary.endRow();
                }
            }
        }

        summary.sort(sort);

        let render_count = summary.render((page - 1) * TableLen, TableLen, {render_empty: false});
        syncPagers(queryAll('#cm_ghm_roots .pagr'), page,
                   computeLastPage(render_count, summary.getRowCount(), TableLen));
    }

    function addSummaryCells(dtab, stat, total)
    {
        function addPercentCell(value)
        {
            if (!isNaN(value)) {
                dtab.addCell('td', value, format.percent(value));
            } else {
                dtab.addCell('td', null, '-');
            }
        }

        dtab.addCell('td', stat.count, format.number(stat.count, !!mix_params.diff));
        if (!mix_params.diff)
            addPercentCell(stat.count / total.count);
        dtab.addCell('td', stat.mono_count, format.number(stat.mono_count, !!mix_params.diff));
        if (!mix_params.diff)
            addPercentCell(stat.mono_count / total.mono_count);
        dtab.addCell('td', stat.price_cents_total / 100.0, format.price(stat.price_cents_total, false, !!mix_params.diff));
        if (!mix_params.diff)
            addPercentCell(stat.price_cents_total / total.price_cents_total);
        dtab.addCell('td', stat.price_cents / 100.0, format.price(stat.price_cents, false, !!mix_params.diff));
        if (!mix_params.diff)
            addPercentCell(stat.price_cents / total.price_cents);
        dtab.addCell('td', stat.deaths, format.number(stat.deaths, !!mix_params.diff));
        if (!mix_params.diff)
            addPercentCell(stat.deaths / stat.count);
    }

    function syncPagers(pagers, active_page, last_page)
    {
        pagers.forEach(function(pager) {
            if (last_page) {
                let builder = new Pager(pager, active_page, last_page);
                builder.anchorBuilder = function(text, active_page) {
                    return dom.h('a', {href: routeToUrl({page: active_page}).url}, '' + text);
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
        if (!thop.needsRefresh(refreshDurationTable, mix_url, arguments))
            return;

        let table = query('#cm_table');
        table.replaceContent(
            dom.h('thead'),
            dom.h('tbody')
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

                for (const col of columns) {
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

                if (duration % 10 == 0) {
                    let text = '' + duration + ' - ' +
                                    format.duration(Math.min(max_duration - 1, duration + 9));
                    let tr = dom.h('tr',
                        dom.h('th', {class: 'repeat', colspan: columns.length * 2 + 1}, text)
                    );
                    tbody.appendContent(tr);
                }

                let tr = dom.h('tr',
                    dom.h('th', format.duration(duration))
                );
                for (const col of columns) {
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
        if (!thop.needsRefresh(refreshResults, rt_url, arguments))
            return;

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

        let offset = (page - 1) * TableLen;
        let end = Math.min(offset + TableLen, results.length);

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
                                if (diag.exclude) {
                                    contents.push(' ');
                                    contents.push(dom.h('s', '(' + (diag.severity + 1) + ')'));
                                } else {
                                    contents.push(' (' + (diag.severity + 1) + ')');
                                }
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
        syncPagers(pagers, page, computeLastPage(end - offset, results.length, TableLen));

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

    // Clear casemix data when user changes or disconnects
    user.addChangeHandler(function() {
        clearCasemix();

        deploy_results.clear();
        summaries = {};

        queryAll('#cm > *').forEach(function(el) {
            if (el.innerHTML)
                el.innerHTML = '';
        });
    });

    thop.registerUrl('mco_casemix', this, runCasemix);
}).call(mco_casemix);
