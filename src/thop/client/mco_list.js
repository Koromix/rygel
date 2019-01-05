// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_list = {};
(function() {
    const Lists = {
        'ghm_roots': {
            'path': 'api/mco_ghm_ghs.json',
            'sector': true,
            'catalog': 'mco_ghm_roots',

            'groups': [
                {type: 'cmd', name: 'Catégories majeures de diagnostic',
                 func: function(ghm_ghs1, ghm_ghs2, ghm_roots_map) {
                    if (ghm_ghs1.ghm_root !== ghm_ghs2.ghm_root) {
                        return (ghm_ghs1.ghm_root < ghm_ghs2.ghm_root) ? -1 : 1;
                    } else {
                        return 0;
                    }
                }},
                {type: 'da', name: 'Domaines d\'activité',
                 func: function(ghm_ghs1, ghm_ghs2, ghm_roots_map) {
                    let ghm_root_info1 = ghm_roots_map[ghm_ghs1.ghm_root] || {};
                    let ghm_root_info2 = ghm_roots_map[ghm_ghs2.ghm_root] || {};

                    if (ghm_root_info1.da !== ghm_root_info2.da) {
                        return (ghm_root_info1.da < ghm_root_info2.da) ? -1 : 1;
                    } else if (ghm_root_info1.ghm_root !== ghm_root_info2.ghm_root) {
                        return (ghm_root_info1.ghm_root < ghm_root_info2.ghm_root) ? -1 : 1;
                    } else {
                        return 0;
                    }
                }},
                {type: 'ga', name: 'Groupes d\'activité',
                 func: function(ghm_ghs1, ghm_ghs2, ghm_roots_map) {
                    let ghm_root_info1 = ghm_roots_map[ghm_ghs1.ghm_root] || {};
                    let ghm_root_info2 = ghm_roots_map[ghm_ghs2.ghm_root] || {};

                    if (ghm_root_info1.ga !== ghm_root_info2.ga) {
                        return (ghm_root_info1.ga < ghm_root_info2.ga) ? -1 : 1;
                    } else if (ghm_root_info1.ghm_root !== ghm_root_info2.ghm_root) {
                        return (ghm_root_info1.ghm_root < ghm_root_info2.ghm_root) ? -1 : 1;
                    } else {
                        return 0;
                    }
                }}
            ],
            'deduplicate': function(ghm_ghs) { return ghm_ghs.ghm_root; },

            'header': false,
            'columns': [
                {func: function(ghm_ghs, ghm_roots_map, group) {
                    switch (group) {
                        case 'cmd': {
                            let ghm_root_info = ghm_roots_map[ghm_ghs.ghm_root];
                            if (ghm_root_info && ghm_root_info.cmd) {
                                return ghm_root_info.cmd + ' - ' + ghm_root_info.cmd_desc;
                            } else {
                                return 'CMD inconnue ??';
                            }
                        } break;

                        case 'da': {
                            let ghm_root_info = ghm_roots_map[ghm_ghs.ghm_root];
                            if (ghm_root_info && ghm_root_info.da) {
                                return ghm_root_info.da + ' - ' + ghm_root_info.da_desc;
                            } else {
                                return 'DA inconnu ??';
                            }
                        } break;

                        case 'ga': {
                            let ghm_root_info = ghm_roots_map[ghm_ghs.ghm_root];
                            if (ghm_root_info && ghm_root_info.ga) {
                                return ghm_root_info.ga + ' - ' + ghm_root_info.ga_desc;
                            } else {
                                return 'GA inconnu ??';
                            }
                        } break;
                    }
                }},
                {key: 'ghm_root', header: 'Racine de GHM',
                 func: function(ghm_ghs, ghm_roots_map) {
                    return ghm_ghs.ghm_root + (ghm_roots_map[ghm_ghs.ghm_root] ?
                                              ' - ' + ghm_roots_map[ghm_ghs.ghm_root].desc : '');
                }}
            ]
        },

        'ghm_ghs': {
            'path': 'api/mco_ghm_ghs.json',
            'sector': true,
            'catalog': 'mco_ghm_roots',

            'columns': [
                {key: 'ghm_root', func: function(ghm_ghs, ghm_roots_map) {
                    return ghm_ghs.ghm_root + (ghm_roots_map[ghm_ghs.ghm_root] ?
                                              ' - ' + ghm_roots_map[ghm_ghs.ghm_root].desc : '');
                }},
                {key: 'ghm', header: 'GHM', variable: 'ghm'},
                {key: 'ghs', header: 'GHS', variable: 'ghs'},
                {key: 'durations', header: 'Durées', title: 'Durées (nuits)',
                 func: function(ghm_ghs) {
                    return maskToRanges(ghm_ghs.durations);
                }},
                {key: 'confirm', header: 'Confirmation', title: 'Confirmation (nuits)',
                 func: function(ghm_ghs) {
                    return ghm_ghs.confirm_treshold ? '< ' + ghm_ghs.confirm_treshold : null;
                }},
                {key: 'main_diagnosis', header: 'DP', variable: 'main_diagnosis'},
                {key: 'diagnoses', header: 'Diagnostics', variable: 'diagnoses'},
                {key: 'procedures', header: 'Actes', variable: 'procedures'},
                {key: 'authorizations', header: 'Autorisations', title: 'Autorisations (unités et lits)',
                 func: function(ghm_ghs) {
                    let ret = [];
                    if (ghm_ghs.unit_authorization)
                        ret.push('Unité ' + ghm_ghs.unit_authorization);
                    if (ghm_ghs.bed_authorization)
                        ret.push('Lit ' + ghm_ghs.bed_authorization);
                    return ret;
                }},
                {key: 'old_severity', header: 'Sévérité âgé', func: function(ghm_ghs) {
                    if (ghm_ghs.old_age_treshold) {
                        return '≥ ' + ghm_ghs.old_age_treshold + ' et < ' +
                               (ghm_ghs.old_severity_limit + 1);
                    } else {
                        return null;
                    }
                }},
                {key: 'young_severity', header: 'Sévérité jeune', func: function(ghm_ghs) {
                    if (ghm_ghs.young_age_treshold) {
                        return '< ' + ghm_ghs.young_age_treshold + ' et < ' +
                               (ghm_ghs.young_severity_limit + 1);
                    } else {
                        return null;
                    }
                }}
            ]
        },

        'diagnoses': {
            'path': 'api/mco_diagnoses.json',
            'catalog': 'cim10',

            'columns': [
                {key: 'diag', header: 'Diagnostic',
                 func: function(diag, cim10_map) {
                    let text = diag.diag;
                    switch (diag.sex) {
                        case 'Homme': { text += ' (♂)'; } break;
                        case 'Femme': { text += ' (♀)'; } break;
                    }
                    if (cim10_map[diag.diag])
                        text += ' - ' + cim10_map[diag.diag].desc;
                    return text;
                }},
                {key: 'severity', header: 'Niveau',
                 func: function(diag) { return diag.severity ? diag.severity + 1 : 1; }},
                {key: 'cmd' , header: 'CMD', variable: 'cmd'},
                {key: 'main_list', header: 'Liste principale', variable: 'main_list'}
            ]
        },

        'procedures': {
            'path': 'api/mco_procedures.json',
            'catalog': 'ccam',

            'columns': [
                {key: 'proc', header: 'Acte',
                 func: function(proc, ccam_map) {
                    let proc_phase = proc.proc + (proc.phase ? '/' + proc.phase : '');
                    return proc_phase + (ccam_map[proc.proc] ? ' - ' + ccam_map[proc.proc].desc : '');
                }},
                {key: 'dates', header: 'Dates', title: 'Date de début incluse, date de fin exclue',
                 func: function(proc) { return proc.begin_date + ' -- ' + proc.end_date; }},
                {key: 'activities', header: 'Activités', variable: 'activities'},
                {key: 'extensions', header: 'Extensions', title: 'Extensions (CCAM descriptive)',
                 variable: 'extensions'}
            ]
        }
    };

    // Routes
    let specs = {};
    let groups = {};
    let search = {};
    let pages = {};
    let sorts = {};

    // Cache
    let list_cache = {};
    let reactor = {};

    function runList(route, path, parameters, hash, errors)
    {
        // Parse route (model: list/<table>/<date>/<sector>)
        let path_parts = path.split('/');
        route.list = path_parts[1] || 'ghm_roots';
        if (Lists[route.list] && Lists[route.list].sector) {
            route.sector = path_parts[2] || 'public';
            route.date = path_parts[3] || null;
            route.spec = (path_parts[3] && path_parts[4]) ? path_parts[4] : null;
        } else {
            route.date = path_parts[2] || null;
            route.spec = (path_parts[2] && path_parts[3]) ? path_parts[3] : null;
        }
        route.search = parameters.search || null;
        route.group = parameters.group || null;
        route.page = parseInt(parameters.page, 10) || 1;
        route.sort = parameters.sort || null;
        specs[route.list] = route.spec;
        search[route.list] = route.search;
        groups[route.list] = route.group;
        pages[route.list + route.spec] = route.page;
        sorts[route.list + route.spec] = route.sort;

        // Resources
        let indexes = mco_common.updateSettings().indexes;
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        let main_index = indexes.findIndex(function(info) { return info.begin_date === route.date; });
        let group_info = null;
        if (Lists[route.list] && Lists[route.list].groups) {
            if (route.group) {
                group_info = Lists[route.list].groups.find(function(group_info) {
                    return group_info.type == route.group;
                });
            } else {
                group_info = Lists[route.list].groups[0];
            }
        }
        if (main_index >= 0 && Lists[route.list])
            updateList(route.list, indexes[main_index].begin_date, route.sector, route.spec);

        // Errors
        if (!Lists[route.list])
            errors.add('Liste inconnue');
        if (route.date !== null && indexes.length && main_index < 0)
            errors.add('Date incorrecte');
        if (Lists[route.list] && Lists[route.list].sector && !['public', 'private'].includes(route.sector))
            errors.add('Secteur incorrect');
        if (route.group && Lists[route.list] && !group_info)
            errors.add('Critère de tri inconnu');

        // Refresh settings
        queryAll('#opt_index').removeClass('hide');
        mco_common.refreshIndexesLine(indexes, main_index);
        if (Lists[route.list] && Lists[route.list].sector) {
            query('#opt_sector').removeClass('hide');
            query('#opt_sector > select').value = route.sector;
        }
        query('#opt_search').removeClass('hide');
        {
            let search_input = query('#opt_search > input');
            if (route.search != search_input.value)
                search_input.value = route.search || '';
        }
        if (Lists[route.list] && Lists[route.list].groups !== undefined) {
            query('#opt_group').removeClass('hide');
            refreshGroupsMenu(Lists[route.list], route.group);
        }

        // Refresh view
        if (!data.isBusy()) {
            refreshHeader(route.spec);

            queryAll('.ls_table').addClass('hide');
            if (Lists[route.list]) {
                refreshListTable(route.list, group_info, route.search, route.page, route.sort);
                query('#ls_' + route.list).removeClass('hide');
            }

            query('#ls').removeClass('hide');
        } else if (query('#ls_' + route.list).hasClass('hide')) {
            query('#ls').addClass('hide');
        }
    }

    function routeToUrl(args)
    {
        let new_route = thop.buildRoute(args);
        if (args.spec === undefined)
            new_route.spec = specs[new_route.list];
        if (args.search === undefined)
            new_route.search = search[new_route.list];
        if (args.group === undefined)
            new_route.group = groups[new_route.list];
        if (args.page === undefined)
            new_route.page = pages[new_route.list + new_route.spec];
        if (args.sort === undefined)
            new_route.sort = sorts[new_route.list + new_route.spec];

        let url;
        {
            let url_parts;
            if (Lists[new_route.list] && Lists[new_route.list].sector) {
                url_parts = [thop.baseUrl('mco_list'), new_route.list, new_route.sector,
                             new_route.date, new_route.spec];
            } else {
                url_parts = [thop.baseUrl('mco_list'), new_route.list,
                             new_route.date, new_route.spec];
            }
            while (!url_parts[url_parts.length - 1])
                url_parts.pop();

            url = url_parts.join('/');
        }

        if (new_route.page && new_route.page === 1)
            new_route.page = null;
        if (new_route.group && Lists[new_route.list] && Lists[new_route.list].groups &&
                new_route.group === Lists[new_route.list].groups[0].type)
            new_route.group = null;
        url = buildUrl(url, {
            search: new_route.search || null,
            group: new_route.group,
            page: new_route.page,
            sort: new_route.sort || null
        });

        return {
            url: url,
            allowed: true
        };
    }
    this.routeToUrl = routeToUrl;

    function go(args, delay)
    {
        thop.route(routeToUrl(args).url, delay);
    }
    this.go = go;

    function updateList(list_name, date, sector, spec)
    {
        let params = {
            date: date,
            sector: Lists[list_name].sector ? sector : null,
            spec: spec
        };
        let url = buildUrl(thop.baseUrl(Lists[list_name].path), params);
        let list = list_cache[list_name];

        if (!list || url !== list.url) {
            mco_common.updateCatalog(Lists[list_name].catalog);

            list_cache[list_name] = {
                url: null,
                items: []
            };
            list = list_cache[list_name];

            data.get(url, 'json', function(json) {
                list.url = url;
                list.items = json;
            });
        }
    }

    function refreshHeader(spec)
    {
        let h1 = query('#ls_spec');

        if (spec) {
            h1.replaceContent(
                'Filtre : ' + spec + ' ',
                html('a', {href: routeToUrl({spec: null}).url}, '(retirer)')
            );
            h1.removeClass('hide');
        } else {
            h1.innerHTML = '';
            h1.addClass('hide');
        }
    }

    function refreshGroupsMenu(list_info, select_group)
    {
        let el = query('#opt_group > select');

        el.replaceContent(
            list_info.groups.map(function(group) {
                return html('option', {value: group.type}, group.name)
            })
        );

        if (select_group)
            el.value = select_group;
    }

    function refreshListTable(list_name, group_info, search, page, sort)
    {
        if (search)
            search = simplifyForSearch(search);

        let list_info = Lists[list_name];
        let list = list_cache[list_name];
        let concepts_map = mco_common.updateCatalog(list_info.catalog).map;

        if (!list || !thop.needsRefresh(refreshListTable, arguments))
            return;

        let builder;
        if (thop.needsRefresh(list, group_info, search)) {
            builder = createPagedDataTable(query('#ls_' + list_name));
            builder.sortHandler = function(sort) { go({sort: sort}); }
            list.builder = builder;

            // Special column
            let first_column = 0;
            if (!list_info.columns[0].header)
                first_column = 1;

            // Header
            for (let i = first_column; i < list_info.columns.length; i++) {
                let col = list_info.columns[i];
                builder.addColumn(col.key, null, {title: col.title || col.header}, col.header);
            }

            // Groups
            if (group_info)
                list.items.sort(function(v1, v2) { return group_info.func(v1, v2, concepts_map); });

            // Data
            let prev_cell0 = null;
            let prev_deduplicate_key = null;
            for (const item of list.items) {
                if (list_info.deduplicate) {
                    let deduplicate_key = list_info.deduplicate(item, concepts_map);
                    if (deduplicate_key === prev_deduplicate_key)
                        continue;
                    prev_deduplicate_key = deduplicate_key;
                }

                let show = false;
                let cells = [];
                for (const col of list_info.columns) {
                    let content = createContent(col, item, concepts_map,
                                                group_info ? group_info.type : null);

                    if (!search || simplifyForSearch(content).indexOf(search) >= 0)
                        show = true;
                    cells.push(content);
                }

                if (show) {
                    if (first_column) {
                        if (cells[0] !== prev_cell0) {
                            if (prev_cell0)
                                builder.endRow();
                            builder.beginRow();
                            builder.addCell(cells[0], {colspan: cells.length - 1},
                                            addSpecLinks(cells[0]));
                            prev_cell0 = cells[0];
                        }
                    }

                    builder.beginRow();
                    for (let i = first_column; i < cells.length; i++) {
                        const cell = cells[i];
                        builder.addCell(cell, addSpecLinks(cell));
                    }
                    builder.endRow();
                }
            }
        } else {
            builder = list.builder;
        }

        builder.sort(sort, false);

        let render_count = builder.render((page - 1) * TableLen, TableLen,
                                          {render_header: list_info.header, render_parents: !sort});
        syncPagers(queryAll('#ls_' + list_name + ' .pagr'), page,
                   computeLastPage(render_count, builder.getRowCount(), TableLen));
    }

    function createContent(column, item, concepts_map, group)
    {
        let content;
        if (column.variable) {
            content = item[column.variable];
        } else if (column.func) {
            content = column.func(item, concepts_map, group);
        }

        if (content === undefined || content === null) {
            content = '';
        } else if (Array.isArray(content)) {
            content = content.join(', ');
        }

        return '' + content;
    }

    function simplifyForSearch(str)
    {
        return str.toLowerCase().replace(/[êéèàâïùœ]/g, function(c) {
            switch (c) {
                case 'ê': return 'e';
                case 'é': return 'e';
                case 'è': return 'e';
                case 'à': return 'a';
                case 'â': return 'a';
                case 'ï': return 'i';
                case 'ù': return 'u';
                case 'œ': return 'oe';
            }
        });
    }

    function maskToRanges(mask)
    {
        let ranges = [];

        let i = 0;
        for (;;) {
            while (i < 32 && !(mask & (1 << i)))
                i++;
            if (i >= 32)
                break;

            let j = i + 1;
            while (j < 32 && (mask & (1 << j)))
                j++;
            j--;

            if (j == 31) {
                ranges.push('≥ ' + i);
            } else if (j > i) {
                ranges.push('' + i + '-' + j);
            } else {
                ranges.push('' + i);
            }

            i = j + 1;
        }
        return ranges.join(', ');
    }

    function makeSpecLink(str)
    {
        let url = null;
        let cls = null;
        if (str[0] === 'A') {
            url = routeToUrl({list: 'procedures', spec: str}).url;
        } else if (str[0] === 'D') {
            url = routeToUrl({list: 'diagnoses', spec: str}).url;
        } else if (str.match(/^[0-9]{2}[CMZKH][0-9]{2}[ZJT0-9ABCDE]?$/)) {
            url = mco_pricing.routeToUrl({view: 'table', ghm_root: str.substr(0, 5)}).url;
            cls = 'ghm';
        } else if (str.match(/[Nn]oeud [0-9]+/)) {
            url = mco_tree.routeToUrl().url + '#n' + str.substr(6);
        } else {
            return str;
        }

        let anchor = html('a', {href: url, class: cls}, str);

        return anchor;
    }
    this.makeSpecLink = makeSpecLink;

    function addSpecLinks(str)
    {
        let elements = [];
        for (;;) {
            let m = str.match(/([AD](\-[0-9]+|\$[0-9]+\.[0-9]+)|[0-9]{2}[CMZKH][0-9]{2}[ZJT0-9ABCDE]?|[Nn]oeud [0-9]+)/);
            if (!m)
                break;

            elements.push(str.substr(0, m.index));
            elements.push(makeSpecLink(m[0]));
            str = str.substr(m.index + m[0].length);
        }
        elements.push(str);

        return elements;
    }
    this.addSpecLinks = addSpecLinks;

    function syncPagers(pagers, active_page, last_page)
    {
        pagers.forEach(function(pager) {
            if (last_page) {
                let builder = new Pager(pager, active_page, last_page);
                builder.anchorBuilder = function(text, active_page) {
                    return html('a', {href: routeToUrl({page: active_page}).url}, '' + text);
                }
                builder.render();

                pager.removeClass('hide');
            } else {
                pager.addClass('hide');
                pager.innerHTML = '';
            }
        });
    }

    thop.registerUrl('mco_list', this, runList);
}).call(mco_list);
