var casemix = {};
(function() {
    // Casemix
    var mix_url = null;
    var mix_cmds = [];
    var mix_cmds_map = {};
    var mix_ghm_roots = [];
    var mix_ghm_roots_map = {};
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

        // Redirection (stable URLs)
        */

        // Resources
        updateCaseMix(route.cm_start, route.cm_end, route.cm_units, route.cm_mode,
                      route.cm_diff_start, route.cm_diff_end, run);

        // Refresh view
        document.querySelector('#casemix').classList.add('active');
        markOutdated('#casemix_view', downloadJson.busy);
        if (!downloadJson.busy) {
            if (route.cm_start)
                document.querySelector('#casemix_start').value = route.cm_start;
            if (route.cm_end)
                document.querySelector('#casemix_end').value = route.cm_end;

            document.querySelector('#casemix_cmds').classList.toggle('active', route.cm_view == 'global');
            document.querySelector('#casemix_roots').classList.toggle('active', route.cm_view == 'global');
            document.querySelector('#casemix_ghs').classList.toggle('active', route.cm_view == 'ghm_root');

            refreshCmds(route.cm_cmd);
            refreshRoots(route.cm_cmd);
            refreshGhmRoot(route.ghm_root);
        }
    }
    this.run = run;

    function buildUrl(args)
    {
        const keep_keys = [
            'cm_start',
            'cm_end',
            'cm_units',
            'cm_mode',
            'cm_diff_start',
            'cm_diff_end',
            'cm_cmd',
            'ghm_root'
        ];

        let new_route = buildRoute(args);

        let short_route = {};
        for (let i = 0; i < keep_keys.length; i++) {
            let k = keep_keys[i];
            short_route[k] = new_route[k] || null;
        }

        let url_parts = [buildModuleUrl('casemix'), new_route.cm_view, window.btoa(JSON.stringify(short_route))];
        while (!url_parts[url_parts.length - 1])
            url_parts.pop();
        let url = url_parts.join('/');

        return url;
    }
    this.buildUrl = buildUrl;

    function route(args, delay)
    {
        go(buildUrl(args), true, delay);
    }
    this.route = route;

    function updateCaseMix(start, end, units, mode, diff_start, diff_end, func)
    {
        let params = {
            dates: (start && end) ? (start + '..' + end) : null,
            units: units,
            mode: mode,
            diff: (diff_start && diff_end) ? (diff_start + '..' + diff_end) : null,
            durations: 1
        };

        if (JSON.stringify(params) === mix_url)
            return;

        downloadJson(BaseUrl + 'api/casemix.json', params, function(json) {
            mix_cmds = [];
            mix_cmds_map = {};
            mix_ghm_roots = [];
            mix_ghm_roots_map = {};
            mix_price_density_max = 0;

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

            for (var i = 0; i < mix_cmds.length; i++) {
                mix_cmds[i].ghm_roots = mix_cmds[i].ghm_roots.sort(
                        function(ghm_root_info1, ghm_root_info2) {
                    if (ghm_root_info1.ghm_root < ghm_root_info2.ghm_root) {
                        return -1;
                    } else if (ghm_root_info1.ghm_root > ghm_root_info2.ghm_root) {
                        return 1;
                    } else {
                        return 0;
                    }
                });
            }

            for (var i = 0; i < mix_ghm_roots.length; i++) {
                var ghm_root_info = mix_ghm_roots[i];

                ghm_root_info.ghs = ghm_root_info.ghs.sort(function(ghs_info1, ghs_info2) {
                    return ghs_info1.ghs - ghs_info2.ghs;
                });

                for (var j = 0; j < ghm_root_info.ghs.length; j++) {
                    ghm_root_info.ghs[j].details = ghm_root_info.ghs[j].details.sort(
                            function(details1, details2) {
                        return details1.duration - details2.duration;
                    });
                }
            }

            mix_url = JSON.stringify(params);
        });
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
                stays_count ? createElement('a', {href: buildUrl({cm_cmd: cmd_num})}, '' + valid_cmds[i])
                            : valid_cmds[i]
            );
            tr.appendChild(td);
        }

        var old_table = document.querySelector('#casemix_cmds');
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
            var ghm_roots = {};
            for (var i = 0; i < ghm_types.length; i++) {
                types_tr.appendChild(createElement('th', {colspan: 3}, '' + ghm_types[i]));
                ghm_roots[ghm_types[i]] = [];
            }
            thead.appendChild(types_tr);

            var rows_count = 0;
            for (var i = 0; i < mix_cmds_map[cmd].ghm_roots.length; i++) {
                var ghm_root_info = mix_cmds_map[cmd].ghm_roots[i];
                var ghm_root_type = ghm_root_info.ghm_root.substr(2, 1);

                ghm_roots[ghm_root_type].push(ghm_root_info);
                if (ghm_roots[ghm_root_type].length > rows_count)
                    rows_count = ghm_roots[ghm_root_type].length;
            }

            for (var i = 0; i < rows_count; i++) {
                var tr = createElement('tr');
                for (var j = 0; j < ghm_types.length; j++) {
                    var ghm_root_info = ghm_roots[ghm_types[j]][i];

                    if (ghm_root_info !== undefined) {
                        var text = ghm_root_info.ghm_root + ' = ' +
                                   pricing.priceText(ghm_root_info.ghs_price_cents);

                        tr.appendChild(createElement('td', {'style': 'border-right: 0;'},
                                                     '' + ghm_root_info.stays_count));
                        tr.appendChild(createElement('td', {'style': 'border-left: 0; border-right: 0;'},
                            createElement('a', {href: buildUrl({cm_view: 'ghm_root',
                                                                ghm_root: ghm_root_info.ghm_root})},
                                          ghm_root_info.ghm_root)
                        ));
                        tr.appendChild(createElement('td', {'style': 'border-left: 0;'},
                                                     pricing.priceText(ghm_root_info.ghs_price_cents)));
                    } else {
                        tr.appendChild(createElement('td', {colspan: 3}));
                    }
                }
                tbody.appendChild(tr);
            }
        }

        var old_table = document.querySelector('#casemix_roots');
        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
    }

    function refreshGhmRoot(ghm_root)
    {
        var table = createElement('table', {},
            createElement('thead'),
            createElement('tbody')
        );
        var thead = table.querySelector('thead');
        var tbody = table.querySelector('tbody');

        if (ghm_root && mix_ghm_roots_map[ghm_root]) {
            var ghm_root_info = mix_ghm_roots_map[ghm_root];

            var tr = createElement('tr');
            tr.appendChild(createElement('th', {}, ''));
            for (var i = 0; i < ghm_root_info.ghs.length; i++)
                tr.appendChild(createElement('th', {}, '' + ghm_root_info.ghs[i].ghs));
            thead.appendChild(tr);

            for (var i = 0; i < ghm_root_info.max_duration; i++) {
                var tr = createElement('tr');
                tr.appendChild(createElement('th', {}, pricing.durationText(i)));
                for (var j = 0; j < ghm_root_info.ghs.length; j++) {
                    // FIXME: All of this is getting really ugly
                    var info = null;
                    for (var k = 0; k < ghm_root_info.ghs[j].details.length; k++) {
                        if (ghm_root_info.ghs[j].details[k].duration == i) {
                            info = ghm_root_info.ghs[j].details[k];
                            break;
                        }
                    }

                    if (info !== null) {
                        var td = createElement('td', {title: info.stays_count},
                                               pricing.priceText(info.ghs_price_cents));
                    } else {
                        var td = createElement('td');
                    }
                    tr.appendChild(td);
                }
                tbody.appendChild(tr);
            }
        }

        var old_table = document.querySelector('#casemix_ghs');
        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
    }
}).call(casemix);
