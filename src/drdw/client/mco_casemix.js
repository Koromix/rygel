// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var mco_casemix = {};
(function() {
    // Route
    var start_date = null;
    var end_date = null;
    var algorithms = [];
    var default_algorithm = null;
    var structures = [];

    // Cache
    var mix_url = null;
    var mix_rows = [];
    var mix_ghm_roots = new Set;

    function runCasemix(route, url, parameters, hash)
    {
        let errors = new Set(downloadJson.errors);

        // Parse route (model: casemix/<view>/<json_parameters_in_base64>)
        let url_parts = url.split('/', 3);
        if (url_parts[2])
            Object.assign(route, buildRoute(JSON.parse(window.atob(url_parts[2]))));
        route.cm_view = url_parts[1] || route.cm_view;
        if (!route.period[0])
            route.period = [start_date, end_date];
        if (!route.prev_period[0])
            route.prev_period = [start_date, end_date];

        // Casemix
        updateCaseMix();
        if (!route.algorithm)
            route.algorithm = default_algorithm;
        let new_classify_url = null;
        if (start_date) {
            let prev_period = (route.mode !== 'none') ? route.prev_period : [null, null];
            new_classify_url = buildClassifyUrl(route.period[0], route.period[1], route.units,
                                                route.algorithm, prev_period[0], prev_period[1]);
        }
        if ((!mix_url || route.apply) && new_classify_url) {
            updateResults(new_classify_url);
            route.apply = false;
        }

        // Resources
        let indexes = mco_common.updateIndexes();
        let ghm_roots = mco_common.updateConceptSet('mco_ghm_roots').concepts;
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        if (!route.ghm_root && ghm_roots.length)
            route.ghm_root = ghm_roots[0].ghm_root;
        let main_index = indexes.findIndex(function(info) { return info.begin_date === route.date; });
        if (main_index >= 0 && !indexes[main_index].init)
            mco_pricing.updatePriceMap(main_index);

        // Errors
        if (user.getSession()) {
            if (!(['summary', 'table'].includes(route.cm_view)))
                errors.add('Mode d\'affichage incorrect');
            if (!route.units.length && mix_url === new_classify_url)
                errors.add('Aucune unité sélectionnée');
            if (route.cm_view == 'table' && !route.ghm_root)
                errors.add('Aucune racine de GHM sélectionnée');
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
        toggleClass(__('#opt_units, #opt_periods, #opt_mode, #opt_algorithm, #opt_update'), 'hide',
                    !user.getSession());
        _('#opt_ghm_roots').classList.toggle('hide',
                                             !user.getSession() || route.cm_view !== 'table');
        refreshPeriods(route.period, route.prev_period, route.mode);
        refreshStructures(route.units);
        _('#opt_mode > select').value = route.mode;
        refreshAlgorithms(route.algorithm);
        refreshGhmRoots(ghm_roots, route.ghm_root);

        // Refresh view
        refreshErrors(Array.from(errors));
        if (!downloadJson.busy)
            downloadJson.errors = [];
        if (user.getSession()) {
            if (!downloadJson.busy) {
                switch (route.cm_view) {
                    case 'summary': {
                        refreshSummary();
                    } break;
                    case 'table': {
                        // refreshCmds(route.cmd);
                        if (main_index >= 0)
                            refreshTable(mco_pricing.pricings_map[route.ghm_root],
                                         main_index, route.ghm_root);
                    } break;
                }

                _('#cm_summary').classList.toggle('hide', route.cm_view !== 'summary');
                _('#cm_table').classList.toggle('hide', route.cm_view !== 'table');
                _('#cm').classList.remove('hide');
            }
        } else {
            _('#cm').classList.add('hide');
        }
        markBusy('#cm', downloadJson.busy || (mix_url !== new_classify_url));
    }
    this.runCasemix = runCasemix;

    function routeToUrl(args)
    {
        if (!user.getSession())
            return null;

        const KeepKeys = [
            'period',
            'prev_period',
            'units',
            'mode',
            'algorithm',
            'apply', // FIXME: Ugly?

            'date',
            'ghm_root'
        ];

        let new_route = buildRoute(args);

        let short_route = {};
        for (let i = 0; i < KeepKeys.length; i++) {
            let k = KeepKeys[i];
            short_route[k] = new_route[k] || null;
        }

        let url_parts = [buildModuleUrl('mco_casemix'), new_route.cm_view, window.btoa(JSON.stringify(short_route))];
        while (!url_parts[url_parts.length - 1])
            url_parts.pop();
        let url = url_parts.join('/');

        return url;
    }
    this.routeToUrl = routeToUrl;

    function route(args, delay)
    {
        go(routeToUrl(args), true, delay);
    }
    this.route = route;

    function updateCaseMix()
    {
        let url = buildUrl(BaseUrl + 'api/mco_casemix.json', {key: user.getUrlKey()});
        downloadJson('GET', url, function(json) {
            start_date = json.begin_date;
            end_date = json.end_date;
            algorithms = json.algorithms;
            default_algorithm = json.default_algorithm;
            structures = json.structures;
        });
    }

    function buildClassifyUrl(start, end, units, mode, diff_start, diff_end)
    {
        let params = {
            dates: (start && end) ? (start + '..' + end) : null,
            units: units.join('+'),
            mode: mode,
            diff: (diff_start && diff_end) ? (diff_start + '..' + diff_end) : null,
            durations: 1,
            key: user.getUrlKey()
        };
        let url = buildUrl(BaseUrl + 'api/mco_classify.json', params);

        return url;
    }

    function updateResults(url)
    {
        mix_rows = [];

        downloadJson('GET', url, function(json) {
            mix_url = url;

            mix_rows = json;
            mix_ghm_roots.clear();
            for (let i = 0; i < mix_rows.length; i++) {
                let row = mix_rows[i];

                row.cmd = parseInt(row.ghm.substr(0, 2), 10);
                row.type = row.ghm.substr(2, 1);
                row.ghm_root = row.ghm.substr(0, 5);
                switch (row.ghm.substr(5, 1)) {
                    case 'J':
                    case 'T': { row.short_mode = 'J/T'; } break;
                    case '1':
                    case 'A': { row.short_mode = '1'; } break;
                    case '2':
                    case '3':
                    case '4':
                    case 'B':
                    case 'C':
                    case 'D': { row.short_mode = '2/3/4'; } break;
                    case 'Z':
                    case 'E': { row.short_mode = 'Z/E'; } break;
                }

                mix_ghm_roots.add(row.ghm_root);
            }
        });
    }

    function refreshPeriods(period, prev_period, mode)
    {
        let picker;
        {
            let builder = new PeriodPicker(start_date, end_date,
                                           period ? period[0] : null, period ? period[1] : null);

            builder.changeHandler = function() {
                route({period: this.object.getValues()});
            };

            picker = builder.getWidget();
            let old_picker = _('#opt_periods > div:last-of-type');
            cloneAttributes(old_picker, picker);
            picker.classList.add('ppik');
            old_picker.parentNode.replaceChild(picker, old_picker);
        }

        let prev_picker;
        {
            let builder = new PeriodPicker(start_date, end_date,
                                           prev_period ? prev_period[0] : null, prev_period ? prev_period[1] : null);

            builder.changeHandler = function() {
                route({prev_period: this.object.getValues()});
            };

            prev_picker = builder.getWidget();
            let old_picker = _('#opt_periods > div:first-of-type');
            cloneAttributes(old_picker, prev_picker);
            prev_picker.classList.add('ppik');
            old_picker.parentNode.replaceChild(prev_picker, old_picker);
        }

        picker.style.width = (mode !== 'none') ? '49%' : '100%';
        prev_picker.style.width = '49%';
        prev_picker.classList.toggle('hide', mode === 'none');
    }

    function refreshStructures(units)
    {
        let builder = new TreeSelector('Unités médicales : ');

        for (let i = 0; i < structures.length; i++) {
            let structure = structures[i];

            let prev_groups = [];
            builder.createTab(structure.name);
            for (let j = 0; j < structure.units.length; j++) {
                let unit = structure.units[j];
                let parts = unit.path.substr(2).split('::');

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
            route({units: this.object.getValues()});
        };

        let old_select = _('#opt_units > div');
        if (old_select.object)
            builder.setActiveTab(old_select.object.getActiveTab());
        let select = builder.getWidget();
        cloneAttributes(old_select, select);
        select.classList.add('tsel');
        old_select.parentNode.replaceChild(select, old_select);
    }

    function refreshAlgorithms(algorithm)
    {
        let select = _('#opt_algorithm > select');

        select.innerHTML = '';
        for (let i = 0; i < algorithms.length; i++) {
            let algorithm = algorithms[i];

            let text ='Algorithme ' + algorithm.title;
            if (algorithm.name === default_algorithm)
                text += ' *';
            let option = createElement('option', {value: algorithm.name}, text);
            select.appendChild(option);
        }
        select.value = algorithm;
    }

    function refreshGhmRoots(ghm_roots, select_ghm_root)
    {
        var el = _('#opt_ghm_roots > select');
        el.innerHTML = '';

        for (var i = 0; i < ghm_roots.length; i++) {
            var ghm_root_info = ghm_roots[i];

            var opt = createElement('option', {value: ghm_root_info.ghm_root},
                                    ghm_root_info.ghm_root + ' – ' + ghm_root_info.desc);
            if (!mix_ghm_roots.has(ghm_root_info.ghm_root)) {
                opt.setAttribute('disabled', '');
                opt.text += '*';
            }

            el.appendChild(opt);
        }
        if (select_ghm_root)
            el.value = select_ghm_root;
    }

    function refreshSummary()
    {
        const ShortModes = ['J/T', '1', '2/3/4', 'Z/E'];

        let table = createElement('table', {class: 'ls_table'},
            createElement('thead'),
            createElement('tbody')
        );
        let thead = table.querySelector('thead');
        let tbody = table.querySelector('tbody');

        if (mix_rows.length) {
            // Header
            {
                let tr = createElement('tr', {},
                    createElement('th', {}, 'GHM'),
                    createElement('th', {colspan: 2}, 'Part'),
                    createElement('th', {colspan: 2}, 'Total')
                );
                for (let i = 0; i < ShortModes.length; i++) {
                    let th = createElement('th', {colspan: 2}, ShortModes[i]);
                    tr.appendChild(th);
                }
                tr.appendChild(createElement('th', {colspan: 2}, 'Décès'));
                thead.appendChild(tr);
            }

            let [stats1, stats_map1] = aggregate(mix_rows, ['ghm_root']);
            let [stats2, stats_map2] = aggregate(mix_rows, ['ghm_root', 'short_mode']);
            let ghm_roots = stats1
                .sort(function(a, b) { return b.count - a.count; })
                .map(function(stat) { return stat.ghm_root; });

            // FIXME: Ugly: precompute
            let total_count = 0;
            let total_price_cents = 0;
            for (let i = 0; i < mix_rows.length; i++) {
                total_count += mix_rows[i].count;
                total_price_cents += mix_rows[i].price_cents;
            }

            // Data
            for (let i = 0; i < ghm_roots.length; i++) {
                let stat1 = findAggregate(stats_map1, ghm_roots[i]);
                let tr = createElement('tr', {},
                    createElement('td', {},
                        createElement('a', {href: routeToUrl({cm_view: 'table', ghm_root: ghm_roots[i]})},
                                      ghm_roots[i])
                    ),
                    createElement('td', {class: 'cm_count'},
                                  percentText(stat1.count / total_count)),
                    createElement('td', {class: 'cm_price'},
                                  percentText(stat1.price_cents / total_price_cents)),
                    createElement('td', {class: 'cm_count'}, '' + stat1.count),
                    createElement('td', {class: 'cm_price'}, mco_pricing.priceText(stat1.price_cents, false))
                );
                for (let j = 0; j < ShortModes.length; j++) {
                    let stat2 = findAggregate(stats_map2, ghm_roots[i], ShortModes[j]);
                    if (stat2) {
                        tr.appendChild(createElement('td', {class: 'cm_count'}, '' + stat2.count));
                        tr.appendChild(createElement('td', {class: 'cm_price'},
                                                     mco_pricing.priceText(stat2.price_cents, false)));
                    } else {
                        tr.appendChild(createElement('td', {colspan: 2}));
                    }
                }
                tr.appendChild(createElement('td', {class: 'cm_count'}, '' + stat1.deaths));
                tr.appendChild(createElement('td', {class: 'cm_price'},
                                             percentText(stat1.deaths / stat1.count)));
                tbody.appendChild(tr);
            }
        }

        let old_table = _('#cm_summary');
        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
    }

    function percentText(fraction)
    {
        return fraction.toLocaleString('fr-FR',
                                       {style: 'percent', minimumFractionDigits: 1, maximumFractionDigits: 1});
    }

    function refreshCmds(cmd)
    {
        // FIXME: Avoid hard-coded CMD list
        const ValidCmds = ['01', '02', '03', '04', '05', '06', '07',
                           '08', '09', '10', '11', '12', '13', '14',
                           '15', '16', '17', '18', '19', '20', '21',
                           '22', '23', '25', '26', '27', '28', '90'];

        let table = createElement('table', {},
            createElement('tbody', {},
                createElement('tr')
            )
        );
        let tr = table.querySelector('tr');

        let density_max = 0;
        for (let i = 0; i < mix_rows.length; i++)
            density_max += Math.abs(mix_rows[i].price_cents);

        let [stats, stats_map] = aggregate(mix_rows, ['cmd']);
        for (let i = 0; i < ValidCmds.length; i++) {
            let cmd_num = parseInt(ValidCmds[i], 10);
            let stat = findAggregate(stats_map, cmd_num);

            if (stat) {
                let alpha = Math.abs(stat.price_cents) / density_max;
                let style = 'background: rgba(' + (stat.count > 0 ? '0, 255, 0, ' : '255, 0, 0, ') + alpha + ');';

                let td = createElement('td', {style: style},
                    createElement('a', {href: routeToUrl({cm_view: 'cmd', cmd: cmd_num})}, ValidCmds[i])
                );
                tr.appendChild(td);
            } else {
                let td = createElement('td', {}, ValidCmds[i]);
                tr.appendChild(td);
            }
        }

        let old_table = _('#cm_cmds');
        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
    }

    function refreshTable(pricing_info, main_index, ghm_root)
    {
        let rows = mix_rows.filter(function(row) { return row.ghm_root === ghm_root; });

        let table;
        if (rows.length) {
            let [stats, stats_map] = aggregate(rows, ['ghs', 'duration']);

            let max_duration = 20;
            let max_count = 0;
            let max_price_cents = 0;
            for (let i = 0; i < stats.length; i++) {
                let stat = stats[i];

                max_duration = Math.max(max_duration, stat.duration + 1);
                max_count = Math.max(max_count, stat.count);
                max_price_cents = Math.max(max_price_cents, stat.price_cents);
            }

            table = mco_pricing.createTable(ghm_root, pricing_info[main_index].ghs,
                                            max_duration, false, true,
                                            function(col, duration) {
                if (!mco_pricing.testDuration(col, duration))
                    return null;

                let stat = findAggregate(stats_map, col.ghs, duration);

                let style;
                let content;
                if (stat) {
                    style = 'padding: 0';
                    content = [
                        createElement('div', {style: 'float: left; width: calc(50% - 2px); text-align: left; padding: 1px;'},
                                      '' + stat.count),
                        createElement('div', {style: 'float: right; width: calc(50% - 2px); text-align: right; padding: 1px;'},
                                      mco_pricing.priceText(stat.price_cents))
                    ];
                } else {
                    style = 'filter: opacity(50%);';
                    content = '';
                }

                let mode;
                if (col.exb_treshold && duration < col.exb_treshold) {
                    mode = 'exb';
                } else if (col.exh_treshold && duration >= col.exh_treshold) {
                    mode = 'exh';
                } else {
                    mode = 'price';
                }

                return [content, {class: mode, style: style}, false];
            });
        } else {
            table = createElement('table');
        }

        let old_table = _('#cm_table');
        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
    }

    function aggregate(rows, by)
    {
        let list = [];
        let map = {};

        for (let i = 0; i < rows.length; i++) {
            let row = rows[i];

            let ptr = map;
            for (let j = 0; j < by.length; j++) {
                if (ptr[row[by[j]]] === undefined)
                    ptr[row[by[j]]] = {};
                ptr = ptr[row[by[j]]];
            }
            if (ptr.duration === undefined) {
                for (let k = 0; k < by.length; k++)
                    ptr[by[k]] = row[by[k]];
                ptr.duration = 0;
                ptr.count = 0;
                ptr.mono_count = 0;
                ptr.deaths = 0;
                ptr.price_cents = 0;

                list.push(ptr);
            }

            ptr.duration += row.duration;
            ptr.count += row.count;
            ptr.mono_count += row.mono_count;
            ptr.deaths += row.deaths;
            ptr.price_cents += row.price_cents;
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
        if (ptr.duration === undefined)
            return null;

        return ptr;
    }
}).call(mco_casemix);

registerUrl('mco_casemix', mco_casemix, mco_casemix.runCasemix);
