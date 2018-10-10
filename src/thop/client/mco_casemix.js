// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_casemix = {};
(function() {
    'use strict';

    // Route
    let pages = {};
    let sorts = {};
    let descendings = {};

    // Casemix
    let prev_url_key = null;
    let start_date = null;
    let end_date = null;
    let algorithms = [];
    let default_algorithm = null;
    let structures = [];

    // Results
    let mix_url = null;
    let mix_rows = [];
    let mix_ghm_roots = new Set;

    // Cache
    let reactor = new Reactor;
    let summary_table = null;

    function runCasemix(route, url, parameters, hash)
    {
        let errors = new Set(data.getErrors());

        // Parse route (model: casemix/<view>/<json_parameters_in_base64>)
        let url_parts = url.split('/', 3);
        if (url_parts[2])
            Object.assign(route, thop.buildRoute(JSON.parse(window.atob(url_parts[2]))));
        route.view = url_parts[1] || 'summary';
        route.period = (route.period && route.period[0]) ? route.period : [start_date, end_date];
        route.prev_period = (route.prev_period && route.prev_period[0]) ?
                            route.prev_period : [start_date, end_date];
        route.units = route.units || [];
        route.mode = route.mode || 'none';
        route.algorithm = route.algorithm || null;
        route.refresh = route.refresh || false;
        route.ghm_root = route.ghm_root || null;
        route.apply_coefficient = route.apply_coefficient || false;
        route.page = parseInt(parameters.page, 10) || 1;
        route.sort = parseInt(parameters.sort, 10) || null;
        route.descending = !!parseInt(parameters.descending, 10) || false;
        pages[route.view] = route.page;
        sorts[route.view] = route.sort;
        descendings[route.view] = route.descending;

        // Casemix
        let new_classify_url = null;
        if (user.isConnected()) {
            updateSettings();
            if (!route.algorithm)
                route.algorithm = default_algorithm;
            if (start_date) {
                let prev_period = (route.mode !== 'none') ? route.prev_period : [null, null];
                new_classify_url = buildCasemixUrl(route.period[0], route.period[1], route.units,
                                                   route.algorithm, prev_period[0], prev_period[1]);
            }
            if (route.refresh && new_classify_url) {
                clearCasemix();
                updateCasemix(new_classify_url);
            }
        }
        delete route.refresh;

        // Resources
        let indexes = mco_common.updateIndexes();
        let ghm_roots = mco_common.updateConceptSet('mco_ghm_roots').concepts;
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        if (!route.ghm_root && ghm_roots.length)
            route.ghm_root = ghm_roots[0].ghm_root;
        let main_index = indexes.findIndex(function(info) { return info.begin_date === route.date; });
        let pricings_map = null;
        if (main_index >= 0)
            pricings_map = mco_pricing.updatePriceMap(main_index);

        // Errors
        if (user.isConnected()) {
            if (!(['summary', 'table'].includes(route.view)))
                errors.add('Mode d\'affichage incorrect');
            if (!route.units.length && mix_url === new_classify_url)
                errors.add('Aucune unité sélectionnée');
            if (route.view == 'table') {
                if (!route.ghm_root)
                    errors.add('Aucune racine de GHM sélectionnée');
                if (mix_ghm_roots.size && !mix_ghm_roots.has(route.ghm_root))
                    errors.add('Aucun séjour dans cette racine');
            }
            if (!(['none', 'absolute'].includes(route.mode)))
                errors.add('Mode de comparaison inconnu');
            if (route.date !== null && indexes.length && main_index < 0)
                errors.add('Date incorrecte');
            if (algorithms.length &&
                    !algorithms.find(function(algorithm) { return algorithm.name === route.algorithm; }))
                errors.add('Algorithme inconnu');
        } else {
            errors.add('Vous n\'êtes pas connecté(e)');
        }

        // Refresh settings
        queryAll('#opt_units, #opt_periods, #opt_mode, #opt_algorithm, #opt_update')
            .toggleClass('hide', !user.isConnected());
        query('#opt_ghm_roots').toggleClass('hide', !user.isConnected() || route.view !== 'table');
        refreshPeriods(route.period, route.prev_period, route.mode);
        refreshStructures(route.units);
        query('#opt_mode > select').value = route.mode;
        refreshAlgorithms(route.algorithm);
        refreshGhmRoots(ghm_roots, route.ghm_root);

        // Refresh view
        thop.refreshErrors(Array.from(errors));
        if (!data.isBusy())
            data.clearErrors();
        if (user.isConnected()) {
            if (!data.isBusy()) {
                switch (route.view) {
                    case 'summary': {
                        if (reactor.changed('summary', mix_url, mix_rows.length, route.page,
                                            route.sort, route.descending))
                            refreshSummary(route.page, route.sort, route.descending);
                    } break;
                    case 'table': {
                        if (main_index >= 0 &&
                                reactor.changed('table', mix_url, mix_rows.length, main_index,
                                                route.ghm_root, route.apply_coefficient))
                            refreshTable(pricings_map[route.ghm_root], main_index, route.ghm_root,
                                         route.apply_coefficient, true);
                    } break;
                }

                queryAll('#cm_summary').toggleClass('hide', route.view !== 'summary');
                queryAll('.cm_pager').toggleClass('hide', route.view !== 'summary' ||
                                                  !query('.cm_pager').childNodes.length);
                query('#cm_table').toggleClass('hide', route.view !== 'table');
                query('#cm').removeClass('hide');
            }

            if (!data.isBusy() && mix_url !== new_classify_url) {
                query('#cm').addClass('busy');
            } else if (!data.isBusy()) {
                query('#cm').removeClass('busy');
            }
            query('#opt_update').disabled = (mix_url === new_classify_url);
        } else {
            query('#cm').addClass('hide');
        }
    }

    function routeToUrl(args)
    {
        if (!user.isConnected())
            return null;

        const KeepKeys = [
            'period',
            'prev_period',
            'units',
            'mode',
            'algorithm',
            'ghm_root',
            'apply_coefficient',
            'refresh'
        ];

        let new_route = thop.buildRoute(args);
        if (args.page === undefined)
            new_route.page = pages[new_route.view];
        if (args.sort === undefined)
            new_route.sort = sorts[new_route.view];
        if (args.descending === undefined)
            new_route.descending = descendings[new_route.view];

        let short_route = {};
        for (const k of KeepKeys)
            short_route[k] = new_route[k] || null;

        let url;
        {
            let url_parts = [thop.baseUrl('mco_casemix'), new_route.view, window.btoa(JSON.stringify(short_route))];
            while (!url_parts[url_parts.length - 1])
                url_parts.pop();

            url = url_parts.join('/');
        }

        if (new_route.page && new_route.page === 1)
            new_route.page = null;
        url = buildUrl(url, {
            page: new_route.page,
            sort: new_route.sort,
            descending: new_route.descending ? 1 : null
        });

        return url;
    }
    this.routeToUrl = routeToUrl;

    function go(args, delay)
    {
        thop.route(routeToUrl(args), delay);
    }
    this.go = go;

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

                prev_url_key = user.getUrlKey();
            });
        }
    }

    function buildCasemixUrl(start, end, units, mode, diff_start, diff_end)
    {
        let params = {
            dates: (start && end) ? (start + '..' + end) : null,
            units: units.join('+'),
            mode: mode,
            diff: (diff_start && diff_end) ? (diff_start + '..' + diff_end) : null,
            durations: 1,
            key: user.getUrlKey()
        };
        let url = buildUrl(thop.baseUrl('api/mco_casemix.json'), params);

        return url;
    }

    function updateCasemix(url)
    {
        data.get(url, function(json) {
            clearCasemix();

            mix_url = url;
            mix_rows = json;
            for (let row of mix_rows) {
                row.cmd = parseInt(row.ghm.substr(0, 2), 10);
                row.type = row.ghm.substr(2, 1);
                row.ghm_root = row.ghm.substr(0, 5);

                mix_ghm_roots.add(row.ghm_root);
            }

            initSummaryTable();
        });
    }

    function clearCasemix()
    {
        mix_rows = [];
        mix_ghm_roots.clear();
    }

    function refreshPeriods(period, prev_period, mode)
    {
        let picker;
        {
            let builder = new PeriodPicker(start_date, end_date,
                                           period ? period[0] : null, period ? period[1] : null);

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
            let builder = new PeriodPicker(start_date, end_date,
                                           prev_period ? prev_period[0] : null, prev_period ? prev_period[1] : null);

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

    function refreshStructures(units)
    {
        let builder = new TreeSelector('Unités médicales : ');

        for (const structure of structures) {
            let prev_groups = [];
            builder.createTab(structure.name);
            for (const unit of structure.units) {
                let parts = unit.path.substr(1).split('|');

                let common_len = 0;
                while (common_len < parts.length - 1 && common_len < prev_groups.length &&
                       parts[common_len] === prev_groups[common_len])
                    common_len++;
                while (prev_groups.length > common_len) {
                    builder.endGroup();
                    prev_groups.pop();
                }
                for (let k = common_len; k < parts.length - 1; k++) {
                    builder.beginGroup(parts[k]);
                    prev_groups.push(parts[k]);
                }

                builder.addOption(parts[parts.length - 1], unit.unit,
                                  {selected: units.includes(unit.unit.toString())});
            }
        }

        builder.changeHandler = function() {
            go({units: this.object.getValues()});
        };

        let old_select = query('#opt_units > div');
        if (old_select.object)
            builder.setActiveTab(old_select.object.getActiveTab());
        let select = builder.getWidget();
        select.copyAttributesFrom(old_select);
        select.addClass('tsel');
        old_select.replaceWith(select);
    }

    function refreshAlgorithms(algorithm)
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

    function refreshGhmRoots(ghm_roots, select_ghm_root)
    {
        let el = query('#opt_ghm_roots > select');
        el.innerHTML = '';

        for (const ghm_root of ghm_roots) {
            let opt = html('option', {value: ghm_root.ghm_root},
                           ghm_root.ghm_root + ' – ' + ghm_root.desc);

            if (mix_ghm_roots.size && !mix_ghm_roots.has(ghm_root.ghm_root)) {
                opt.setAttribute('disabled', '');
                opt.text += '*';
            }

            el.appendChild(opt);
        }
        if (select_ghm_root)
            el.value = select_ghm_root;
    }

    function initSummaryTable()
    {
        // FIXME: Ugly as hell
        let diff = (mix_url.indexOf('diff=') >= 0);

        summary_table = new DataTable(query('#cm_summary'));
        summary_table.sortHandler = function(col_idx, descending) {
            go({sort: col_idx, descending: descending});
        };

        // Avoid headers with no data
        if (!mix_rows.length)
            return;

        // Header
        summary_table.addColumns([
            'GHM',
            'RSS', !diff ? '%' : null,
            'Total', !diff ? '%' : null,
            'Partiel', !diff ? '%' : null,
            'Décès', !diff ? '%' : null
        ]);

        // Aggregate
        let stat1 = aggregate(mix_rows)[0][0];
        let [stats2, stats2_map] = aggregate(mix_rows, ['ghm_root']);
        let ghm_roots = stats2
            // .sort(function(a, b) { return b.count - a.count; })
            .map(function(stat) { return stat.ghm_root; });

        // Resources
        let ghm_roots_map = mco_common.updateConceptSet('mco_ghm_roots').map;

        function addStatCells(stat)
        {
            summary_table.addCell(stat.count, numberText(stat.count));
            if (!diff)
                summary_table.addCell(stat.count / stat1.count, percentText(stat.count / stat1.count));
            summary_table.addCell(stat.price_cents, priceText(stat.price_cents, false));
            if (!diff)
                summary_table.addCell(stat.price_cents / stat1.price_cents,
                                      percentText(stat.price_cents / stat1.price_cents));
            summary_table.addCell(stat.partial_price_cents,
                                  priceText(stat.partial_price_cents, false));
            if (!diff)
                summary_table.addCell(stat.partial_price_cents / stat1.partial_price_cents,
                                      percentText(stat.partial_price_cents / stat1.partial_price_cents));
            summary_table.addCell(stat.deaths, numberText(stat.deaths));
            if (!diff)
                summary_table.addCell(stat.deaths / stat.count, percentText(stat.deaths / stat.count));
        }

        // Add stats
        summary_table.beginRow();
        summary_table.addCell('Total');
        addStatCells(stat1);
        for (let ghm_root of ghm_roots) {
            let ghm_root_desc = ghm_roots_map[ghm_root];
            let header = html('a', {href: routeToUrl({view: 'table', ghm_root: ghm_root}),
                                    title: ghm_root_desc ? ghm_root_desc.desc : null}, ghm_root);

            let stat2 = findAggregate(stats2_map, ghm_root);

            summary_table.beginRow();
            summary_table.addCell(ghm_root, header);
            addStatCells(stat2);
            summary_table.endRow();
        }
        summary_table.endRow();
    }

    function refreshSummary(page, sort, descending)
    {
        if (!summary_table)
            return;

        if (sort !== null && sort !== undefined)
            summary_table.sort(sort, descending);

        let render_count = summary_table.render((page - 1) * TableLen, TableLen);
        let row_count = summary_table.getRowCount();

        let last_page = Math.floor((row_count - 1) / TableLen + 1);
        if (last_page === 1 && render_count === row_count)
            last_page = null;

        for (let old_pager of queryAll('.cm_pager')) {
            if (last_page) {
                let pager = createPagination(page, last_page);
                pager.copyAttributesFrom(old_pager);
                pager.addClass('pagr');
                pager.removeClass('hide');
                old_pager.replaceWith(pager);
            } else {
                old_pager.innerHTML = '';
                old_pager.addClass('hide');
            }
        }
    }

    function createPagination(page, last_page)
    {
        let builder = new Pager(page, last_page);
        builder.anchorBuilder = function(text, page) {
            return html('a', {href: routeToUrl({page: page})}, '' + text);
        }
        return builder.getWidget();
    }

    function refreshTable(pricing_info, main_index, ghm_root, apply_coeff, merge_cells)
    {
        let table = html('table',
            html('thead'),
            html('tbody')
        );

        let rows = mix_rows.filter(function(row) { return row.ghm_root === ghm_root; });

        if (rows.length) {
            let thead = table.query('thead');
            let tbody = table.query('tbody');

            // FIXME: Ugly as hell
            let diff = (mix_url.indexOf('diff=') >= 0);

            let [stats, stats_map] = aggregate(rows, ['ghm', 'duration']);

            let max_duration = 20;
            let max_count = 0;
            let max_price_cents = 0;
            for (const stat of stats) {
                max_duration = Math.max(max_duration, stat.duration + 1);
                max_count = Math.max(max_count, stat.count);
                max_price_cents = Math.max(max_price_cents, stat.price_cents);
            }

            let ghs = pricing_info[main_index].ghs;
            let ghms = [Object.assign({}, ghs[0])];
            for (let i = 1; i < ghs.length; i++) {
                if (ghs[i].ghm !== ghms[ghms.length - 1].ghm) {
                    ghms.push(Object.assign({}, ghs[i]));
                } else {
                    ghms[ghms.length - 1].ghs = '2+';
                    ghms[ghms.length - 1].conditions = [];
                }
            }

            mco_pricing.addPricingHeader(thead, ghm_root, ghms, apply_coeff, merge_cells);
            for (let td of thead.queryAll('td'))
                td.setAttribute('colspan', parseInt(td.getAttribute('colspan') || 1) * 2);

            for (let duration = 0; duration < max_duration; duration++) {
                if (duration % 10 == 0) {
                    let text = '' + duration + ' - ' +
                                    mco_common.durationText(Math.min(max_duration - 1, duration + 9));
                    let tr = html('tr',
                        html('th', {class: 'repeat', colspan: ghs.length}, text)
                    );
                    tbody.appendChild(tr);
                }

                let tr = html('tr',
                    html('th', mco_common.durationText(duration))
                );
                for (const col of ghms) {
                    let stat = findAggregate(stats_map, col.ghm, duration);

                    let cls;
                    if (col.exb_treshold && duration < col.exb_treshold) {
                        cls = 'exb';
                    } else if (col.exh_treshold && duration >= col.exh_treshold) {
                        cls = 'exh';
                    } else {
                        cls = 'price';
                    }

                    if (stat) {
                        function addDiffClass(cls, value)
                        {
                            if (diff) {
                                if (value > 0) {
                                    return cls + ' higher';
                                } else if (value < 0) {
                                    return cls + ' lower';
                                } else {
                                    return cls + ' neutral';
                                }
                            } else {
                                return cls;
                            }
                        }

                        tr.appendChildren([
                            html('td', {class: 'count ' + addDiffClass(cls, stat.count)},
                                 '' + stat.count),
                            html('td', {class: addDiffClass(cls, stat.price_cents)},
                                 priceText(stat.price_cents, false))
                        ]);
                    } else if (mco_pricing.testDuration(col, duration)) {
                        cls += ' empty';
                        tr.appendChild(html('td', {class: cls, colspan: 2}));
                    } else {
                        tr.appendChild(html('td', {colspan: 2}));
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
        if (by === undefined)
            by = [];

        let list = [];
        let map = {};

        for (const row of rows) {
            let ptr = map;
            for (const k of by) {
                if (ptr[row[k]] === undefined)
                    ptr[row[k]] = {};
                ptr = ptr[row[k]];
            }
            if (ptr.count === undefined) {
                for (const k of by)
                    ptr[k] = row[k];
                ptr.count = 0;
                ptr.mono_count = 0;
                ptr.partial_mono_count = 0;
                ptr.deaths = 0;
                ptr.price_cents = 0;
                ptr.partial_price_cents = 0;

                list.push(ptr);
            }

            ptr.count += row.count;
            ptr.mono_count += row.mono_count;
            ptr.partial_mono_count += row.partial_mono_count;
            ptr.deaths += row.deaths;
            ptr.price_cents += row.price_cents;
            ptr.partial_price_cents += row.partial_price_cents;
        }

        return [list, map];
    }

    function findAggregate(map)
    {
        let ptr = map;
        for (let i = 1; i < arguments.length; i++) {
            ptr = ptr[arguments[i]];
            if (ptr === undefined)
                return null;
        }
        if (ptr.count === undefined)
            return null;

        return ptr;
    }

    // Clear casemix data when user changes or disconnects
    user.addChangeHandler(function() {
        clearCasemix();
        refreshSummary(1);
        refreshTable();
    });

    thop.registerUrl('mco_casemix', this, runCasemix);
}).call(mco_casemix);
