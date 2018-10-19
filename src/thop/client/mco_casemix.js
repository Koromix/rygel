// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_casemix = {};
(function() {
    'use strict';

    // Route
    let pages = {};
    let sorts = {};

    // Casemix
    let prev_url_key = null;
    let start_date = null;
    let end_date = null;
    let algorithms = [];
    let default_algorithm = null;
    let structures = [];

    // Results
    let mix_ready = false;
    let mix_params = {};
    let mix_url = null;
    let mix_rows = [];
    let mix_ghm_roots = new Set;
    let mix_durations = {};
    let mix_mismatched_roots = new Set;

    // Cache
    let units_summary = null;
    let ghm_roots_summary = null;

    function runCasemix(route, url, parameters, hash, errors)
    {
        if (!user.isConnected()) {
            errors.add('Vous n\'êtes pas connecté(e)');
            query('#cm').addClass('hide');
            return;
        }

        // Parse route (model: casemix/<view>/<json_parameters_in_base64>)
        let url_parts = url.split('/', 3);
        if (url_parts[2])
            Object.assign(route, thop.buildRoute(JSON.parse(window.atob(url_parts[2]))));
        route.view = url_parts[1] || 'ghm_roots';
        route.period = (route.period && route.period[0]) ? route.period : [start_date, end_date];
        route.prev_period = (route.prev_period && route.prev_period[0]) ?
                            route.prev_period : [start_date, end_date];
        route.units = route.units || [];
        route.structure = route.structure || 0;
        route.mode = route.mode || 'none';
        route.algorithm = route.algorithm || null;
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
        let ghm_roots = [];
        if (route.view === 'durations') {
            ghm_roots = mco_common.updateConceptSet('mco_ghm_roots').concepts;
            if (!route.ghm_root && ghm_roots.length)
                route.ghm_root = ghm_roots[0].ghm_root;
        }

        // Casemix
        let prev_period = (route.mode !== 'none') ? route.prev_period : [null, null];
        if (user.isConnected()) {
            updateSettings();
            if (!route.algorithm)
                route.algorithm = default_algorithm;

            if (start_date) {
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
                }
            }
        }
        delete route.refresh;

        // Errors
        if (!(['ghm_roots', 'units', 'durations'].includes(route.view)))
            errors.add('Mode d\'affichage incorrect');
        if (!route.units.length && mix_ready)
            errors.add('Aucune unité sélectionnée');
        if (route.structure > structures.length && structures.length)
            errors.add('Structure inexistante');
        if (route.view === 'durations') {
            if (!route.ghm_root)
                errors.add('Aucune racine de GHM sélectionnée');
            if (!checkCasemixGhmRoot(route.ghm_root))
                errors.add('Cette racine n\'existe pas dans cette période');
            if (mix_mismatched_roots.has(route.ghm_root))
                errors.add('Regroupement des GHS suite à changement')
        }
        if (!(['none', 'absolute'].includes(route.mode)))
            errors.add('Mode de comparaison inconnu');
        if (algorithms.length &&
                !algorithms.find(function(algorithm) { return algorithm.name === route.algorithm; }))
            errors.add('Algorithme inconnu');

        // Refresh settings
        queryAll('#opt_units, #opt_ghm_roots, #opt_periods, #opt_mode, #opt_algorithm, #opt_update, #opt_apply_coefficient')
            .removeClass('hide');
        query('#opt_ghm_root').toggleClass('hide', route.view !== 'durations');

        refreshPeriodsPickers(route.period, route.prev_period, route.mode);
        query('#opt_mode > select').value = route.mode;
        refreshAlgorithmsMenu(route.algorithm);
        query('#opt_apply_coefficient > input').checked = route.apply_coefficient;
        query('#opt_update').disabled = mix_ready;
        refreshStructuresMenu(route.units, route.structure);
        if (route.view === 'durations')
            refreshGhmRootsMenu(ghm_roots, route.ghm_root);

        // Refresh view
        if (!data.isBusy()) {
            switch (route.view) {
                case 'ghm_roots': {
                    refreshGhmRootsTable(route.units, route.page, route.sort);
                } break;

                case 'units': {
                    refreshUnitsTable(route.units, route.structure, route.page, route.sort);
                } break;

                case 'durations': {
                    refreshDurationTable(route.units, route.ghm_root,
                                         route.apply_coefficient, true);
                } break;
            }
        }

        query('#cm_ghm_roots').toggleClass('hide', route.view !== 'ghm_roots');
        query('#cm_units').toggleClass('hide', route.view !== 'units');
        query('#cm_table').toggleClass('hide', route.view !== 'durations');
        query('#cm').toggleClass('busy', !data.isBusy() && !mix_ready);
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
            'structure',
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

        let short_route = {};
        for (const k of KeepKeys)
            short_route[k] = new_route[k] || null;

        let url;
        {
            let url_parts = [thop.baseUrl('mco_casemix'), new_route.view,
                             window.btoa(JSON.stringify(short_route))];
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
        if (user.getUrlKey() !== prev_url_key) {
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

                prev_url_key = user.getUrlKey();
            });
        }
    }

    function clearCasemix()
    {
        mix_rows.length = 0;
        mix_ghm_roots.clear();
        mix_durations = {};

        mix_ready = false;
    }

    function updateCasemixParams(start, end, mode, diff_start, diff_end, apply_coefficient, refresh)
    {
        let params = {
            dates: (start && end) ? (start + '..' + end) : null,
            mode: mode,
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
                    row.cmd = parseInt(row.ghm.substr(0, 2), 10);
                    row.type = row.ghm.substr(2, 1);
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

    function refreshPeriodsPickers(period, prev_period, mode)
    {
        if (!start_date) {
            period = [null, null];
            prev_period = [null, null];
        }

        let picker;
        {
            let builder = new PeriodPicker(start_date, end_date, period[0], period[1]);

            builder.changeHandler = function() {
                go({period: this.object.getValues()});
            };

            picker = builder.getWidget();
            let old_picker = query('#opt_periods > div:last-of-type');
            picker.copyAttributesFrom(old_picker);
            picker.addClass('ppik');
            old_picker.replaceWith(picker);
        }

        let prev_picker;
        {
            let builder = new PeriodPicker(start_date, end_date, prev_period[0], prev_period[1]);

            builder.changeHandler = function() {
                go({prev_period: this.object.getValues()});
            };

            prev_picker = builder.getWidget();
            let old_picker = query('#opt_periods > div:first-of-type');
            prev_picker.copyAttributesFrom(old_picker);
            prev_picker.addClass('ppik');
            old_picker.replaceWith(prev_picker);
        }

        picker.style.width = (mode !== 'none') ? '49%' : '100%';
        prev_picker.style.width = '49%';
        prev_picker.toggleClass('hide', mode === 'none');
    }

    function refreshStructuresMenu(units, structure_idx)
    {
        units = new Set(units);

        let builder = new TreeSelector('Unités : ');

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

        builder.changeHandler = function() {
            go({units: this.object.getValues(),
                structure: this.object.getActiveTab()});
        };

        let old_select = query('#opt_units > div');
        let select = builder.getWidget();
        select.copyAttributesFrom(old_select);
        select.addClass('tsel');
        old_select.replaceWith(select);
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

    function refreshUnitsTable(units, structure_idx, page, sort)
    {
        if (!needsRefresh(refreshUnitsTable, null, [mix_url].concat(Array.from(arguments))))
            return;

        if (!units_summary ||
                needsRefresh(refreshUnitsTable, 'init', [mix_url, units, structure_idx])) {
            let structure = structures[structure_idx];
            units = new Set(units);

            units_summary = createPagedTable(query('#cm_units'));
            units_summary.addColumn('unit', 'Unité');
            units_summary.addColumn('rss', 'RSS');
            if (!mix_params.diff)
                units_summary.addColumn('rss_pct', '%');
            units_summary.addColumn('rum', 'RUM');
            if (!mix_params.diff)
                units_summary.addColumn('rum_pct', '%');
            units_summary.addColumn('total', 'Total');
            if (!mix_params.diff)
                units_summary.addColumn('total_pct', '%');
            units_summary.addColumn('partial', 'Partiel');
            if (!mix_params.diff)
                units_summary.addColumn('partial_pct', '%');
            units_summary.addColumn('deaths', 'Décès');
            if (!mix_params.diff)
                units_summary.addColumn('deaths_pct', '%');

            // Aggregate
            let stat0;
            let stats1, stats1_map;
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

                stat0 = aggregate(mix_rows, filterUnitParts)[0][0];
                [stats1, stats1_map] = aggregate(mix_rows, unitToEntities, filterUnitParts);
            }

            if (stat0) {
                units_summary.beginRow();
                units_summary.addCell('Total');
                addSummaryCells(units_summary, stat0, stat0);

                let prev_groups = [];
                let prev_totals = [stat0];
                for (const ent of structure.entities) {
                    let unit_stat = findAggregate(stats1_map, ent.path[ent.path.length - 1]);
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
                        let group_stat = findAggregate(stats1_map, ent.path[k]);

                        units_summary.beginRow();
                        units_summary.addCell(ent.path[k]);
                        addSummaryCells(units_summary, group_stat, prev_totals[prev_totals.length - 1]);

                        prev_groups.push(ent.path[k]);
                        prev_totals.push(group_stat);
                    }

                    units_summary.beginRow();
                    units_summary.addCell(ent.path[ent.path.length - 1]);
                    addSummaryCells(units_summary, unit_stat, prev_totals[prev_totals.length - 1]);
                    units_summary.endRow();
                }
            }
        }

        units_summary.sort(sort);

        let render_count = units_summary.render((page - 1) * TableLen, TableLen);
        syncPagers(queryAll('#cm_units .pagr'), render_count, units_summary.getRowCount(), page);
    }

    function refreshGhmRootsTable(units, page, sort)
    {
        if (!needsRefresh(refreshGhmRootsTable, null, [mix_url].concat(Array.from(arguments))))
            return;

        if (!ghm_roots_summary || needsRefresh(refreshGhmRootsTable, 'init', [mix_url, units])) {
            units = new Set(units);

            ghm_roots_summary = createPagedTable(query('#cm_ghm_roots'));
            ghm_roots_summary.addColumn('ghm_root', 'Racine');
            ghm_roots_summary.addColumn('rss', 'RSS');
            if (!mix_params.diff)
                ghm_roots_summary.addColumn('rss_pct', '%');
            ghm_roots_summary.addColumn('rum', 'RUM');
            if (!mix_params.diff)
                ghm_roots_summary.addColumn('rum_pct', '%');
            ghm_roots_summary.addColumn('total', 'Total');
            if (!mix_params.diff)
                ghm_roots_summary.addColumn('total_pct', '%');
            ghm_roots_summary.addColumn('partial', 'Partiel');
            if (!mix_params.diff)
                ghm_roots_summary.addColumn('partial_pct', '%');
            ghm_roots_summary.addColumn('deaths', 'Décès');
            if (!mix_params.diff)
                ghm_roots_summary.addColumn('deaths_pct', '%');

            // Aggregate
            let stat0;
            let stats1, stats1_map;
            {
                function filterUnitParts(row)
                {
                    let values = row.unit.map(function(unit) { return units.has(unit); });
                    return ['include', values];
                }

                stat0 = aggregate(mix_rows, filterUnitParts)[0][0];
                [stats1, stats1_map] = aggregate(mix_rows, 'ghm_root', filterUnitParts);
            }

            if (stat0) {
                let ghm_roots = stats1.map(function(stat) { return stat.ghm_root; });
                let ghm_roots_map = mco_common.updateConceptSet('mco_ghm_roots').map;

                ghm_roots_summary.beginRow();
                ghm_roots_summary.addCell('Total');
                addSummaryCells(ghm_roots_summary, stat0, stat0);

                for (const ghm_root of ghm_roots) {
                    let root_stat = findAggregate(stats1_map, ghm_root);

                    let desc = ghm_roots_map[ghm_root];
                    let header = html('a', {href: routeToUrl({view: 'durations', ghm_root: ghm_root}),
                                            title: desc ? desc.desc : null}, ghm_root);

                    ghm_roots_summary.beginRow();
                    ghm_roots_summary.addCell(ghm_root, header);
                    addSummaryCells(ghm_roots_summary, root_stat, stat0);
                    ghm_roots_summary.endRow();
                }
            }
        }

        ghm_roots_summary.sort(sort);

        let render_count = ghm_roots_summary.render((page - 1) * TableLen, TableLen);
        syncPagers(queryAll('#cm_ghm_roots .pagr'), render_count,
                   ghm_roots_summary.getRowCount(), page);
    }

    function createPagedTable(el)
    {
        if (!el.childNodes.length) {
            el.appendChildren([
                html('table', {class: 'pagr'}),
                html('table', {class: 'dtab'}),
                html('table', {class: 'pagr'})
            ]);
        }

        let dtab = new DataTable(el.query('.dtab'));
        dtab.sortHandler = function(sort) { go({sort: sort}); };

        return dtab;
    }

    function addSummaryCells(dtab, stat, total)
    {
        dtab.addCell(stat.count, numberText(stat.count));
        if (!mix_params.diff)
            dtab.addCell(stat.count / total.count, percentText(stat.count / total.count));

        dtab.addCell(stat.mono_count, numberText(stat.mono_count));
        if (!mix_params.diff)
            dtab.addCell(stat.mono_count / total.mono_count,
                         percentText(stat.mono_count / total.mono_count));

        dtab.addCell(stat.price_cents_total, priceText(stat.price_cents_total, false));
        if (!mix_params.diff)
            dtab.addCell(stat.price_cents_total / total.price_cents_total,
                         percentText(stat.price_cents_total / total.price_cents_total));

        dtab.addCell(stat.price_cents, priceText(stat.price_cents, false));
        if (!mix_params.diff)
            dtab.addCell(stat.price_cents / total.price_cents,
                         percentText(stat.price_cents / total.price_cents));

        dtab.addCell(stat.deaths, numberText(stat.deaths));
        if (!mix_params.diff)
            dtab.addCell(stat.deaths / stat.count, percentText(stat.deaths / stat.count));
    }

    function syncPagers(pagers, render_count, row_count, page)
    {
        let last_page = Math.floor((row_count - 1) / TableLen + 1);
        if (last_page === 1 && render_count === row_count)
            last_page = null;

        for (let pager of pagers) {
            if (last_page) {
                let new_pager;
                {
                    let builder = new Pager(page, last_page);
                    builder.anchorBuilder = function(text, page) {
                        return html('a', {href: routeToUrl({page: page})}, '' + text);
                    }
                    new_pager = builder.getWidget();
                    new_pager.addClass('pagr');
                }

                pager.replaceWith(new_pager);
            } else {
                pager.innerHTML = '';
                pager.addClass('hide');
            }
        }
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
            let stats1_map;
            let stats1_units_map;
            let stats2, stats2_map;
            let stats2_units_map;
            let stats3_map;
            {
                function filterUnitParts(row)
                {
                    let values = row.unit.map(function(unit) { return units.has(unit); });
                    return ['include', values];
                }

                stat0 = aggregate(rows, filterUnitParts)[0][0];
                stats1_map = aggregate(rows, 'ghm', 'ghs', filterUnitParts)[1];
                stats1_units_map = aggregate(rows, 'ghm', 'ghs', 'unit')[1];
                [stats2, stats2_map] = aggregate(rows, 'ghm', 'ghs', 'duration', filterUnitParts);
                stats2_units_map = aggregate(rows, 'ghm', 'ghs', 'duration', 'unit')[1];
                stats3_map = aggregate(rows, 'duration', filterUnitParts)[1];
            }

            let max_duration = 10;
            let max_count = 0;
            let max_price_cents = 0;
            for (const duration_stat of stats2) {
                max_duration = Math.max(max_duration, duration_stat.duration + 1);
                max_count = Math.max(max_count, duration_stat.count);
                max_price_cents = Math.max(max_price_cents, duration_stat.price_cents);
            }

            mco_pricing.addPricingHeader(thead, ghm_root, columns, false, apply_coeff, merge_cells);
            for (let td of thead.queryAll('td'))
                td.setAttribute('colspan', parseInt(td.getAttribute('colspan') || 1) * 2);

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
                    let col_stat = findAggregate(stats1_map, col.ghm, col.ghs);

                    if (col_stat) {
                        let tooltip =
                            makeTooltip('Total', col, col_stat, stat0, col_stat,
                                        findPartialAggregate(stats1_units_map, col.ghm, col.ghs));

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
                let row_stat = findAggregate(stats3_map, duration);

                if (duration % 10 == 0) {
                    let text = '' + duration + ' - ' +
                                    mco_common.durationText(Math.min(max_duration - 1, duration + 9));
                    let tr = html('tr',
                        html('th', {class: 'repeat', colspan: columns.length}, text)
                    );
                    tbody.appendChild(tr);
                }

                let tr = html('tr',
                    html('th', mco_common.durationText(duration))
                );
                for (const col of columns) {
                    let col_stat = findAggregate(stats1_map, col.ghm, col.ghs);
                    let duration_stat = findAggregate(stats2_map, col.ghm, col.ghs, duration);

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
                                        findPartialAggregate(stats2_units_map, col.ghm, col.ghs, duration));

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
                    [key, value] = key(row);
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

        return [list, map];
    }

    function findPartialAggregate(map, values)
    {
        if (!Array.isArray(values))
            values = Array.prototype.slice.call(arguments, 1);

        let ptr = map;
        for (const value of values) {
            ptr = ptr[value];
            if (ptr === undefined)
                return null;
        }

        return ptr;
    }

    function findAggregate(map, values)
    {
        let ptr = findPartialAggregate.apply(null, arguments);
        if (ptr && ptr.count === undefined)
            return null;

        return ptr;
    }

    // Clear casemix data when user changes or disconnects
    user.addChangeHandler(function() {
        clearCasemix();
        refreshUnitsTable([], 0, 1);
        refreshGhmRootsTable([], 1);
        refreshDurationTable([]);
    });

    thop.registerUrl('mco_casemix', this, runCasemix);
}).call(mco_casemix);
