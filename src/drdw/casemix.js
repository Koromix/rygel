// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var casemix = {};
(function() {
    // Casemix
    var indexes = [];
    var ghm_roots = [];
    var ghm_roots_map = {};
    var structures = [];
    var mix_url = null;
    var mix_cmds = [];
    var mix_cmds_map = {};
    var mix_ghm_roots = [];
    var mix_ghm_roots_map = {};
    var mix_start_date = null;
    var mix_end_date = null;
    var mix_price_density_max = 0;

    function runCasemix(route, url, parameters, hash)
    {
        let errors = new Set(downloadJson.errors);

        // Parse route (model: casemix/<view>/<start>..<end>/<units>/<cmd>)
        let url_parts = url.split('/', 3);
        if (url_parts[2])
            Object.assign(route, buildRoute(JSON.parse(window.atob(url_parts[2]))));
        route.cm_view = url_parts[1] || route.cm_view;

        // Validate
        if (!(['global', 'ghm_root'].includes(route.cm_view)))
            errors.add('Mode d\'affichage incorrect');
        if (!(['none', 'absolute', 'relative'].includes(route.mode)))
            errors.add('Mode de comparaison inconnu');
        if (!user.getSession())
            errors.add('Vous n\'êtes pas connecté(e)');

        // Resources
        indexes = getIndexes();
        [ghm_roots, ghm_roots_map] = pricing.updateGhmRoots();
        let main_index = indexes.findIndex(function(info) { return info.begin_date === route.date; });
        let diff_index = indexes.findIndex(function(info) { return info.begin_date === route.diff; });
        if (main_index >= 0 && !indexes[main_index].init)
            pricing.updatePriceMap(main_index);
        updateStructures();
        if (route.period) {
            let prev_period = (route.mode !== 'none') ? route.prev_period.slice() : null;
            updateCaseMix(route.period[0], route.period[1], route.units, route.algorithm,
                          prev_period ? prev_period[0] : null, prev_period ? prev_period[1] : null);
        }

        // Refresh settings
        toggleClass(__('#opt_units, #opt_periods, #opt_mode, #opt_algorithm, #opt_update'), 'hide',
                    !user.getSession());
        refreshPeriods(route.period, route.prev_period, route.mode);
        refreshStructures(route.units);
        _('#opt_mode > select').value = route.mode;
        _('#opt_algorithm > select').value = route.algorithm;

        // Refresh view
        refreshErrors(Array.from(errors));
        if (!downloadJson.busy) {
            downloadJson.errors = [];

            switch (route.cm_view) {
                case 'global': {
                    refreshCmds(route.cmd);
                    refreshRoots(route.cmd);
                } break;
                case 'ghm_root': {
                    refreshCmds(route.cmd);
                    refreshGhmRoot(pricing.pricings_map[route.ghm_root],
                                   main_index, diff_index, route.ghm_root);
                } break;
            }

            _('#cm_cmds').classList.remove('hide');
            _('#cm_roots').classList.toggle('hide', route.cm_view !== 'global');
            _('#cm_ghs').classList.toggle('hide', route.cm_view !== 'ghm_root');
            _('#cm').classList.remove('hide');
        }
        markBusy('#cm', downloadJson.busy);
    }
    this.runCasemix = runCasemix;

    function routeToUrl(args)
    {
        const KeepKeys = [
            'period',
            'prev_period',
            'units',
            'mode',
            'algorithm',

            'cmd',
            'ghm_root',

            'date'
        ];

        let new_route = buildRoute(args);

        let short_route = {};
        for (let i = 0; i < KeepKeys.length; i++) {
            let k = KeepKeys[i];
            short_route[k] = new_route[k] || null;
        }

        let url_parts = [buildModuleUrl('casemix'), new_route.cm_view, window.btoa(JSON.stringify(short_route))];
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

    function updateStructures()
    {
        let url = buildUrl(BaseUrl + 'api/structures.json', {key: user.getUrlKey()});
        downloadJson('GET', url, function(json) {
            structures = json;
        });
    }

    function updateCaseMix(start, end, units, mode, diff_start, diff_end)
    {
        let params = {
            dates: (start && end) ? (start + '..' + end) : null,
            units: units.join('+'),
            mode: mode,
            diff: (diff_start && diff_end) ? (diff_start + '..' + diff_end) : null,
            durations: 1,
            key: user.getUrlKey()
        };
        let url = buildUrl(BaseUrl + 'api/casemix.json', params);

        if (url == mix_url)
            return;

        mix_cmds = [];
        mix_cmds_map = {};
        mix_ghm_roots = [];
        mix_ghm_roots_map = {};
        mix_price_density_max = 0;

        downloadJson('GET', url, function(json) {
            for (var i = 0; i < json.length; i++) {
                var cmd = parseInt(json[i].ghm.substr(0, 2), 10);
                var ghm_root = json[i].ghm.substr(0, 5);

                var cmd_info = mix_cmds_map[cmd];
                if (cmd_info === undefined) {
                    cmd_info = {
                        cmd: cmd,
                        stays_count: 0,
                        ghs_price_cents: 0,
                        ghm_roots: [],
                        ghm_roots_map: {}
                    };
                    mix_cmds.push(cmd_info);
                    mix_cmds_map[cmd] = cmd_info;
                }

                var ghm_root_info = cmd_info.ghm_roots_map[ghm_root];
                if (ghm_root_info === undefined) {
                    ghm_root_info = {
                        ghm_root: ghm_root,
                        stays_count: 0,
                        ghs_price_cents: 0,
                        max_duration: 0,
                        ghs: [],
                        ghs_map: {}
                    };
                    cmd_info.ghm_roots.push(ghm_root_info);
                    cmd_info.ghm_roots_map[ghm_root] = ghm_root_info;
                    mix_ghm_roots.push(ghm_root_info);
                    mix_ghm_roots_map[ghm_root] = ghm_root_info;
                }

                var ghs_info = ghm_root_info.ghs_map[json[i].ghs];
                if (ghs_info === undefined) {
                    ghs_info = {
                        ghs: json[i].ghs,
                        details: []
                    };
                    ghm_root_info.ghs.push(ghs_info);
                    ghm_root_info.ghs_map[json[i].ghs] = ghs_info;
                }

                cmd_info.stays_count += json[i].stays_count;
                cmd_info.ghs_price_cents += json[i].ghs_price_cents;
                ghm_root_info.stays_count += json[i].stays_count;
                ghm_root_info.ghs_price_cents += json[i].ghs_price_cents;
                if (json[i].duration >= ghm_root_info.max_duration)
                    ghm_root_info.max_duration = json[i].duration + 1;
                ghs_info.details.push(json[i]);

                mix_price_density_max += Math.abs(json[i].ghs_price_cents);
            }

            mix_url = url;
        });
    }

    function refreshPeriods(period, prev_period, mode)
    {
        let picker;
        {
            let builder = new PeriodPickerBuilder('2012-01-01', '2018-01-01',
                                                  period ? period[0] : null,
                                                  period ? period[1] : null);
            picker = builder.getWidget();

            let old_picker = _('#opt_periods > div:first-of-type');
            cloneAttributes(old_picker, picker);
            picker.classList.add('ppik');
            old_picker.parentNode.replaceChild(picker, old_picker);
        }

        let prev_picker;
        {
            let builder = new PeriodPickerBuilder('2012-01-01', '2018-01-01',
                                                  prev_period ? prev_period[0] : null,
                                                  prev_period ? prev_period[1] : null);
            prev_picker = builder.getWidget();

            let old_picker = _('#opt_periods > div:last-of-type');
            cloneAttributes(old_picker, prev_picker);
            prev_picker.classList.add('ppik');
            prev_picker.style.width = '44%';
            old_picker.parentNode.replaceChild(prev_picker, old_picker);
        }

        picker.style.width = (mode !== 'none') ? '44%' : '90%';
        prev_picker.classList.toggle('hide', mode === 'none');
    }

    function refreshStructures(units)
    {
        let builder = new TreeSelectorBuilder('Unités médicales : ');

        for (let i = 0; i < structures.length; i++) {
            let structure = structures[i];

            let prev_groups = [];
            builder.beginGroup(structure.name);
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
            for (let j = 0; j <= prev_groups.length; j++)
                builder.endGroup();
        }

        let select = builder.getWidget();

        let old_select = _('#opt_units > div');
        cloneAttributes(old_select, select);
        select.classList.add('tsel');
        old_select.parentNode.replaceChild(select, old_select);
    }

    function refreshCmds(cmd)
    {
        // FIXME: Avoid hard-coded CMD list
        var valid_cmds = ['01', '02', '03', '04', '05', '06', '07',
                          '08', '09', '10', '11', '12', '13', '14',
                          '15', '16', '17', '18', '19', '20', '21',
                          '22', '23', '25', '26', '27', '28', '90'];

        var table = createElement('table', {},
            createElement('tbody', {},
                createElement('tr')
            )
        );
        var tr = table.querySelector('tr');

        if (mix_cmds.length) {
            for (var i = 0; i < valid_cmds.length; i++) {
                var cmd_num = parseInt(valid_cmds[i], 10);
                var mix_cmd = mix_cmds_map[cmd_num];
                var stays_count = mix_cmd ? mix_cmd.stays_count : 0;

                if (stays_count) {
                    var td_style = 'background: rgba(' + (stays_count > 0 ? '0, 255, 0, ' : '255, 0, 0, ') +
                                                         (Math.abs(mix_cmd.ghs_price_cents) / mix_price_density_max) + ');';
                } else {
                    var td_style = null;
                }

                var td = createElement('td', {style: td_style},
                    stays_count ? createElement('a', {href: routeToUrl({cm_view: 'global',
                                                                        cmd: cmd_num})}, '' + valid_cmds[i])
                                : valid_cmds[i]
                );
                tr.appendChild(td);
            }
        }

        var old_table = _('#cm_cmds');
        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
    }

    function refreshRoots(cmd)
    {
        // FIXME: Avoid hard-coded GHM types
        var ghm_types = ['M', 'C', 'K', 'Z'];

        var table = createElement('table', {},
            createElement('thead'),
            createElement('tbody')
        );
        var thead = table.querySelector('thead');
        var tbody = table.querySelector('tbody');

        if (cmd && mix_cmds_map[cmd]) {
            var types_tr = createElement('tr');
            var ghm_roots_by_type = {};
            for (var i = 0; i < ghm_types.length; i++) {
                types_tr.appendChild(createElement('th', {colspan: 3}, '' + ghm_types[i]));
                ghm_roots_by_type[ghm_types[i]] = [];
            }
            thead.appendChild(types_tr);

            let rows_count = 0;
            let ghm_roots_by_type_map = {};
            for (let i = 0; i < ghm_roots.length; i++) {
                let ghm_root_cmd = parseInt(ghm_roots[i].ghm_root.substr(0, 2));
                if (ghm_root_cmd === cmd) {
                    let type = ghm_roots[i].ghm_root.substr(2, 1);
                    let ghm_root_info = {ghm_root: ghm_roots[i].ghm_root};
                    if (ghm_roots_by_type[type]) {
                        ghm_roots_by_type[type].push(ghm_root_info);
                        ghm_roots_by_type_map[ghm_roots[i].ghm_root] = ghm_root_info;
                        rows_count = Math.max(rows_count, ghm_roots_by_type[type].length);
                    }
                }
            }

            // FIXME: Missing if missing from ghm_roots_by_type_map
            for (var i = 0; i < mix_cmds_map[cmd].ghm_roots.length; i++) {
                var ghm_root_info = mix_cmds_map[cmd].ghm_roots[i];
                var ghm_root_type = ghm_root_info.ghm_root.substr(2, 1);

                Object.assign(ghm_roots_by_type_map[ghm_root_info.ghm_root], ghm_root_info);
            }

            for (var i = 0; i < rows_count; i++) {
                var tr = createElement('tr');
                for (var j = 0; j < ghm_types.length; j++) {
                    var ghm_root_info = ghm_roots_by_type[ghm_types[j]][i];

                    if (ghm_root_info !== undefined) {
                        var text = ghm_root_info.ghm_root + ' = ' +
                                   pricing.priceText(ghm_root_info.ghs_price_cents);

                        tr.appendChild(createElement('td', {'style': 'border-right: 0; text-align: left;'},
                                                     ghm_root_info.stays_count ? '' + ghm_root_info.stays_count : ''));
                        tr.appendChild(createElement('td', {'style': ' border-left: 0; border-right: 0; text-align: center;'},
                            createElement('a', {href: routeToUrl({cm_view: 'ghm_root',
                                                                  ghm_root: ghm_root_info.ghm_root})},
                                          ghm_root_info.ghm_root)
                        ));
                        tr.appendChild(createElement('td', {'style': 'border-left: 0; text-align: right;'},
                                                     pricing.priceText(ghm_root_info.ghs_price_cents)));
                    } else {
                        tr.appendChild(createElement('td', {colspan: 3}));
                    }
                }
                tbody.appendChild(tr);
            }
        }

        var old_table = _('#cm_roots');
        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
    }

    function refreshGhmRoot(pricing_info, main_index, diff_index, ghm_root)
    {
        if (ghm_root && mix_ghm_roots_map[ghm_root]) {
            var ghm_root_info = mix_ghm_roots_map[ghm_root];

            // function duration_transform(x) { return x + 1; }
            function duration_transform(x) { return Math.max(x, 1); }

            let max_stays_count = 0;
            let max_price_cents = 0;
            for (let i = 0; i < ghm_root_info.ghs.length; i++) {
                let ghs = ghm_root_info.ghs[i];
                for (let j = 0; j < ghs.details.length; j++) {
                    max_stays_count = Math.max(max_stays_count, ghs.details[j].stays_count);
                    max_price_cents = Math.max(max_price_cents, ghs.details[j].ghs_price_cents /
                                                                duration_transform(ghs.details[j].duration));
                }
            }

            let max_duration = Math.max(ghm_root_info.max_duration, 20);
            var table = pricing.createTable(pricing_info, main_index, diff_index,
                                            max_duration, false, true, function(col, duration) {
                let ghs = ghm_root_info.ghs_map[col.ghs];

                if (!pricing.testDuration(col, duration))
                    return null;

                let content = '';
                let style = 'filter: opacity(50%);';
                if (ghs) {
                    // FIXME: Ugly search loop
                    var info = null;
                    for (var i = 0; i < ghs.details.length; i++) {
                        if (ghs.details[i].duration == duration) {
                            info = ghs.details[i];
                            break;
                        }
                    }

                    if (info) {
                        let width1 = 100 * (info.stays_count / max_stays_count);
                        let width2 = 100 * (info.ghs_price_cents / duration_transform(duration) / max_price_cents);
                        // let width = 100 * (info.ghs_price_cents / max_price_cents);
                        style = 'padding: 0';
                        content = [
                            //createElement('div', {style: 'float: left; height: 1em; background: red; width: ' + width + '%;'}),
                            createElement('div', {style: 'float: left; width: calc(50% - 2px); text-align: left; padding: 1px;'},
                                          '' + info.stays_count),
                            createElement('div', {style: 'float: right; width: calc(50% - 2px); text-align: right; padding: 1px;'},
                                          pricing.priceText(info.ghs_price_cents)),
                            createElement('div', {style: 'clear: both; height: 3px; padding: 0;' +
                                                          'background: linear-gradient(to right, ' +
                                                                                      '#0000ff 0%, ' +
                                                                                      '#0000ff ' + width1 + '%, ' +
                                                                                      'rgba(0, 0, 0, 0) ' + width1 + '%, ' +
                                                                                      'rgba(0, 0, 0, 0) 100%);'}),
                            createElement('div', {style: 'clear: both; height: 3px; padding: 0;' +
                                                          'background: linear-gradient(to right, ' +
                                                                                      '#ff0000 0%, ' +
                                                                                      '#ff0000 ' + width2 + '%, ' +
                                                                                      'rgba(0, 0, 0, 0) ' + width2 + '%, ' +
                                                                                      'rgba(0, 0, 0, 0) 100%);'})
                        ];
                    }
                }

                if (col.exb_treshold && duration < col.exb_treshold) {
                    var mode = 'exb';
                } else if (col.exh_treshold && duration >= col.exh_treshold) {
                    var mode = 'exh';
                } else {
                    var mode = 'price';
                }

                return [content, {class: mode, style: style}, false];
            });
        } else {
            var table = createElement('table');
        }

        var old_table = _('#cm_ghs');
        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
    }
}).call(casemix);

registerUrl('casemix', casemix, casemix.runCasemix);
