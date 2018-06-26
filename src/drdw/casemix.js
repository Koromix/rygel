var casemix = {};
(function() {
    // Casemix
    var indexes = [];
    var ghm_roots = [];
    var ghm_roots_map = {};
    var mix_url = null;
    var mix_cmds = [];
    var mix_cmds_map = {};
    var mix_ghm_roots = [];
    var mix_ghm_roots_map = {};
    var mix_start_date = null;
    var mix_end_date = null;
    var mix_price_density_max = 0;

    function run(route, url, parameters, hash)
    {
        let errors = new Set(downloadJson.errors);

        // Parse route (model: casemix/<view>/<start>..<end>/<units>/<cmd>)
        let url_parts = url.split('/', 3);
        if (url_parts[2])
            Object.assign(route, buildRoute(JSON.parse(window.atob(url_parts[2]))));
        route.cm_view = url_parts[1] || route.cm_view;

        /*
        // Validate
        // TODO: Enforce date format (yyyy-mm-dd)
        if (route.cm_view !== 'roots' || route.cm_view !== 'ghs')
            errors.push('Mode d\'affichage incorrect');
        if (route.cm_start === undefined || route.cm_end === undefined) {
            errors.push('Dates incorrectes');
            route.cm_start = route.cm_start || null;
            route.cm_end = route.cm_end || null;
        }
        */

        // Resources
        indexes = getIndexes();
        [ghm_roots, ghm_roots_map] = pricing.updateGhmRoots();
        let main_index = indexes.findIndex(function(info) { return info.begin_date === route.date; });
        let diff_index = indexes.findIndex(function(info) { return info.begin_date === route.cm_diff; });
        if (main_index >= 0 && !indexes[main_index].init)
            pricing.updatePriceMap(main_index);
        if (main_index >= 0) {
            let end_date = indexes[main_index].begin_date;
            for (let i = main_index + 1; i < indexes.length; i++) {
                if (indexes[i].changed_prices) {
                    end_date = indexes[i].begin_date;
                    break;
                }
            }

            updateCaseMix(route.date, end_date, route.cm_units, route.cm_mode,
                          diff_index >= 0 ? indexes[diff_index].begin_date : null,
                          diff_index >= 0 ? indexes[diff_index].end_date : null);
        }

        // Refresh settings
        removeClass(__('#opt_indexes, #opt_diff_casemix, #opt_algorithm, #opt_units, #opt_update'), 'hide');
        refreshIndexesLine(_('#opt_indexes'), indexes, main_index, false);
        refreshIndexesDiff(diff_index, route.ghm_root);
        _('#opt_algorithm').value = route.cm_mode;

        // Refresh view
        if (!downloadJson.busy) {
            refreshErrors(Array.from(errors));
            downloadJson.errors = [];

            switch (route.cm_view) {
                case 'global': {
                    refreshCmds(route.cm_cmd);
                    refreshRoots(route.cm_cmd);
                } break;
                case 'ghm_root': {
                    refreshGhmRoot(pricing.pricings_map[route.ghm_root],
                                   main_index, diff_index, route.ghm_root);
                } break;
            }

            _('#cm_cmds').classList.toggle('hide', route.cm_view !== 'global');
            _('#cm_roots').classList.toggle('hide', route.cm_view !== 'global');
            _('#cm_ghs').classList.toggle('hide', route.cm_view !== 'ghm_root');
            _('#cm').classList.remove('hide');
        }
        markBusy('#cm', downloadJson.busy);
    }
    this.run = run;

    function routeToUrl(args)
    {
        const KeepKeys = [
            'date',
            'cm_diff',
            'cm_units',
            'cm_mode',
            'cm_cmd',
            'ghm_root'
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

    function updateCaseMix(start, end, units, mode, diff_start, diff_end)
    {
        let params = {
            dates: (start && end) ? (start + '..' + end) : null,
            units: units,
            mode: mode,
            diff: (diff_start && diff_end) ? (diff_start + '..' + diff_end) : null,
            durations: 1
        };
        let url = buildUrl(BaseUrl + 'api/casemix.json', params);

        if (url == mix_url)
            return;

        mix_cmds = [];
        mix_cmds_map = {};
        mix_ghm_roots = [];
        mix_ghm_roots_map = {};
        mix_price_density_max = 0;

        downloadJson(url, function(json) {
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

    function refreshErrors(errors)
    {
        var log = _('#log');

        if (errors.length) {
            log.innerHTML = errors.join('<br/>');
            log.classList.remove('hide');
        }
    }

    function refreshIndexesDiff(diff_index, test_ghm_root)
    {
        var el = _("#opt_diff_casemix > select");
        el.innerHTML = '';

        el.appendChild(createElement('option', {value: ''}, 'Désactiver'));
        for (var i = 0; i < indexes.length; i++) {
            if (indexes[i].changed_prices) {
                var opt = createElement('option', {value: indexes[i].begin_date},
                                        indexes[i].begin_date);
                if (i === diff_index)
                    opt.setAttribute('selected', '');
                /*if (!checkIndexGhmRoot(i, test_ghm_root)) {
                    opt.setAttribute('disabled', '');
                    opt.text += '*';
                }*/
                el.appendChild(opt);
            }
        }
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
                    stays_count ? createElement('a', {href: routeToUrl({cm_cmd: cmd_num})}, '' + valid_cmds[i])
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

            let max_duration = Math.max(ghm_root_info.max_duration, 20);
            var table = pricing.createTable(pricing_info, main_index, diff_index,
                                            max_duration, false, true, function(col, duration) {
                let ghs = ghm_root_info.ghs_map[col.ghs];

                let content = '';
                let style = 'filter: opacity(20%);';
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
                        let width = 60 + 40 * (info.ghs_price_cents / ghm_root_info.ghs_price_cents);
                        style = 'filter: opacity(' + width + '%)';
                        content = [
                            //createElement('div', {style: 'float: left; height: 1em; background: red; width: ' + width + '%;'}),
                            createElement('div', {style: 'float: left; width: 50%; text-align: left;'},
                                          '' + info.stays_count),
                            createElement('div', {style: 'float: right; width: 50%; text-align: right;'},
                                          pricing.priceText(info.ghs_price_cents))
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
