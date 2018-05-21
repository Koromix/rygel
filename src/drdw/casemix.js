var casemix = {};
(function() {
    // URL settings (routing)
    var target_mode = 'roots';
    var target_start = null;
    var target_end = null;
    var target_units = null;
    var target_diff_start = null;
    var target_diff_end = null;
    var target_cmd = null;
    var target_ghm_root = null;

    // Casemix
    var mix_init = false;
    var mix_cmds = [];
    var mix_cmds_map = {};
    var mix_ghm_roots = [];
    var mix_ghm_roots_map = {};
    var mix_price_density_max = 0;

    function run(errors)
    {
        if (errors === undefined)
            errors = [];

        // Parse route (model: casemix/<mode>/<start>..<end>/<units>/<cmd>)
        var parts = url_page.split('/');
        target_mode = parts[1] || target_mode;
        /*
        if (parts[2]) {
            var date_parts = parts[2].split('..', 2);
            target_start = date_parts[0];
            target_end = date_parts[1];
        }
        target_units = parts[3] ? parts[3].split('_') : target_units;
        target_cmd = parts[4] || target_cmd;

        // Validate
        // TODO: Enforce date format (yyyy-mm-dd)
        if (target_mode !== 'roots' || target_mode !== 'ghs')
            errors.push('Mode d\'affichage incorrect');
        if (target_start === undefined || target_end === undefined) {
            errors.push('Dates incorrectes');
            target_start = target_start || null;
            target_end = target_end || null;
        }

        // Redirection (stable URLs)
        */

        // Resources
        if (!mix_init) {
            markOutdated('#casemix_view', true);
            updateCaseMix(target_start, target_end, target_units, target_diff_start,
                          target_diff_end, run);
        }

        // Refresh view
        document.querySelector('#casemix').classList.add('active');
        if (!downloadJson.run_lock) {
            if (target_start)
                document.querySelector('#casemix_start').value = target_start;
            if (target_end)
                document.querySelector('#casemix_end').value = target_end;

            document.querySelector('#casemix_cmds').classList.toggle('active', target_mode == 'roots');
            document.querySelector('#casemix_roots').classList.toggle('active', target_mode == 'roots');
            document.querySelector('#casemix_ghs').classList.toggle('active', target_mode == 'ghs');

            refreshCmds(target_cmd);
            refreshRoots(target_cmd);
            refreshGhmRoot(target_ghm_root);

            markOutdated('#casemix_view', false);
        }
    }
    this.run = run;

    function route(args)
    {
        if (args !== undefined) {
            target_mode = args.mode || target_mode;
            target_start = (args.start !== undefined) ? args.start : target_start;
            target_end = (args.end !== undefined) ? args.end : target_end;
            if (args.units === '')
                args.units = null;
            target_units = (args.units !== undefined) ? args.units : target_units;
            target_diff_start = (args.diff_start !== undefined) ? args.diff_start : target_diff_start;
            target_diff_end = (args.diff_end !== undefined) ? args.diff_end : target_diff_end;
            target_cmd = args.cmd || target_cmd;
            target_ghm_root = args.ghm_root || target_ghm_root;

            // FIXME: This will not behave correctly if an update is already running
            if (args.start !== undefined || args.end !== undefined || args.units !== undefined ||
                    args.diff_start !== undefined || args.diff_end !== undefined)
                mix_init = false;
        }

        switchPage('casemix/' + target_mode);
    }
    this.route = route;

    function updateCaseMix(start, end, units, diff_start, diff_end, func)
    {
        if (mix_init)
            return;
        mix_init = true;

        var dates = (start && end) ? (start + '..' + end) : null;
        var diff = (diff_start && diff_end) ? (diff_start + '..' + diff_end) : null;
        downloadJson('api/casemix.json', {dates: dates, units: units, diff: diff, duration_mode: 'full'},
                     function(status, json) {
            var errors = [];

            mix_cmds = [];
            mix_cmds_map = {};
            mix_ghm_roots = [];
            mix_ghm_roots_map = {};
            mix_price_density_max = 0;

            switch (status) {
                case 200: {
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
                } break;

                case 404: { errors.push('Casemix introuvable'); } break;
                case 502:
                case 503: { errors.push('Service non accessible'); } break;
                case 504: { errors.push('Délai d\'attente dépassé, réessayez'); } break;
                default: { errors.push('Erreur inconnue ' + status); } break;
            }

            mix_init = !errors.length;
            if (!downloadJson.queue.length)
                func(errors);
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

            var click_function = (function() {
                var cmd = cmd_num;
                return function(e) { route({cmd: cmd}); e.preventDefault(); };
            })();

            var td = createElement('td', {style: td_style},
                stays_count ? createElement('a', {href: stays_count ? '#' : null,
                                                  click: click_function}, '' + valid_cmds[i])
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
                types_tr.appendChild(createElement('th', {}, '' + ghm_types[i]));
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
                        var click_function = (function() {
                            var ghm_root = ghm_root_info.ghm_root;
                            return function(e) { route({mode: 'ghs',
                                                        ghm_root: ghm_root}); e.preventDefault(); };
                        })();
                        var text = ghm_root_info.ghm_root + ' = ' +
                                   pricing.priceText(ghm_root_info.ghs_price_cents);

                        var td = createElement('td', {},
                            createElement('a', {href: '#',
                                                title: '' + ghm_root_info.stays_count + ' RSS',
                                                click: click_function}, text)
                        );
                    } else {
                        var td = createElement('td');
                    }
                    tr.appendChild(td);
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
