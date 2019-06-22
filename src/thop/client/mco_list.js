// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_list = (function() {
    let self = this;

    const Lists = {
        ghm_roots: {
            path: 'api/mco_ghm_ghs.json',
            sector: true,
            catalog: 'mco_ghm_roots',

            groups: [
                {type: 'cmd', name: 'Catégories majeures de diagnostic',
                 func: (ghm_ghs1, ghm_ghs2) => util.compareValues(ghm_ghs1.ghm_root, ghm_ghs2.ghm_root)},
                {type: 'da', name: 'Domaines d\'activité',
                 func: (ghm_ghs1, ghm_ghs2, ghm_roots_map) => {
                    let ghm_root_info1 = ghm_roots_map[ghm_ghs1.ghm_root] || {};
                    let ghm_root_info2 = ghm_roots_map[ghm_ghs2.ghm_root] || {};

                    return util.compareValues(ghm_root_info1.da, ghm_root_info2.da) ||
                           util.compareValues(ghm_root_info1.ghm_root, ghm_root_info2.ghm_root);
                }},
                {type: 'ga', name: 'Groupes d\'activité',
                 func: (ghm_ghs1, ghm_ghs2, ghm_roots_map) => {
                    let ghm_root_info1 = ghm_roots_map[ghm_ghs1.ghm_root] || {};
                    let ghm_root_info2 = ghm_roots_map[ghm_ghs2.ghm_root] || {};

                    return util.compareValues(ghm_root_info1.ga, ghm_root_info2.ga) ||
                           util.compareValues(ghm_root_info1.ghm_root, ghm_root_info2.ghm_root);
                }}
            ],
            deduplicate: ghm_ghs => ghm_ghs.ghm_root,

            header: false,
            columns: [
                {func: (ghm_ghs, ghm_roots_map, group) => {
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
                 func: (ghm_ghs, ghm_roots_map) => {
                    return ghm_ghs.ghm_root + (ghm_roots_map[ghm_ghs.ghm_root] ?
                                              ' - ' + ghm_roots_map[ghm_ghs.ghm_root].desc : '');
                }}
            ],

            page_len: 800
        },

        ghm_ghs: {
            path: 'api/mco_ghm_ghs.json',
            sector: true,
            catalog: 'mco_ghm_roots',

            header: true,
            columns: [
                {key: 'ghm_root', func: (ghm_ghs, ghm_roots_map) => {
                    return ghm_ghs.ghm_root + (ghm_roots_map[ghm_ghs.ghm_root] ?
                                              ' - ' + ghm_roots_map[ghm_ghs.ghm_root].desc : '');
                }},
                {key: 'ghm', header: 'GHM', variable: 'ghm'},
                {key: 'ghs', header: 'GHS', variable: 'ghs'},
                {key: 'durations', header: 'Durées', tooltip: 'Durées (nuits)',
                 func: ghm_ghs => maskToRanges(ghm_ghs.durations)},
                {key: 'confirm', header: 'Confirmation', tooltip: 'Confirmation (nuits)',
                 func: ghm_ghs => ghm_ghs.confirm_treshold ? '< ' + ghm_ghs.confirm_treshold : null},
                {key: 'main_diagnosis', header: 'DP', variable: 'main_diagnosis'},
                {key: 'diagnoses', header: 'Diagnostics', variable: 'diagnoses'},
                {key: 'procedures', header: 'Actes', variable: 'procedures'},
                {key: 'authorizations', header: 'Autorisations', tooltip: 'Autorisations (unités et lits)',
                 func: ghm_ghs => {
                    let ret = [];
                    if (ghm_ghs.unit_authorization)
                        ret.push('Unité ' + ghm_ghs.unit_authorization);
                    if (ghm_ghs.bed_authorization)
                        ret.push('Lit ' + ghm_ghs.bed_authorization);
                    return ret;
                }},
                {key: 'old_severity', header: 'Sévérité âgé', func: ghm_ghs => {
                    if (ghm_ghs.old_age_treshold) {
                        return '≥ ' + ghm_ghs.old_age_treshold + ' et < ' +
                               (ghm_ghs.old_severity_limit + 1);
                    } else {
                        return null;
                    }
                }},
                {key: 'young_severity', header: 'Sévérité jeune', func: ghm_ghs => {
                    if (ghm_ghs.young_age_treshold) {
                        return '< ' + ghm_ghs.young_age_treshold + ' et < ' +
                               (ghm_ghs.young_severity_limit + 1);
                    } else {
                        return null;
                    }
                }}
            ],

            page_len: 300
        },

        diagnoses: {
            path: 'api/mco_diagnoses.json',
            catalog: 'cim10',

            header: true,
            columns: [
                {key: 'diag', header: 'Diagnostic',
                 func: (diag, cim10_map) => {
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
                 func: diag => diag.severity ? diag.severity + 1 : 1},
                {key: 'cmd' , header: 'CMD', variable: 'cmd'},
                {key: 'main_list', header: 'Liste principale', variable: 'main_list'}
            ],

            page_len: 300
        },

        procedures: {
            path: 'api/mco_procedures.json',
            catalog: 'ccam',

            header: true,
            columns: [
                {key: 'proc', header: 'Acte',
                 func: (proc, ccam_map) => {
                    let proc_phase = proc.proc + (proc.phase ? '/' + proc.phase : '');
                    return proc_phase + (ccam_map[proc.proc] ? ' - ' + ccam_map[proc.proc].desc : '');
                }},
                {key: 'dates', header: 'Dates', tooltip: 'Date de début incluse, date de fin exclue',
                 func: proc => proc.begin_date + ' -- ' + proc.end_date},
                {key: 'activities', header: 'Activités', variable: 'activities'},
                {key: 'extensions', header: 'Extensions', tooltip: 'Extensions (CCAM descriptive)',
                 variable: 'extensions'}
            ],

            page_len: 300
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

    this.runModule = function(route) {
        // Memorize route info
        specs[route.list] = route.spec;
        search[route.list] = route.search;
        groups[route.list] = route.group;
        pages[route.list + route.spec] = route.page;
        sorts[route.list + route.spec] = route.sort;

        // Resources
        let indexes = thop.updateMcoSettings().indexes;
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
            thop.error('Liste inconnue');
        if (route.date !== null && indexes.length && main_index < 0)
            thop.error('Date incorrecte');
        if (Lists[route.list] && Lists[route.list].sector && !['public', 'private'].includes(route.sector))
            thop.error('Secteur incorrect');
        if (route.group && Lists[route.list] && !group_info)
            thop.error('Critère de tri inconnu');

        // Refresh settings
        queryAll('#opt_index').removeClass('hide');
        refreshIndexesLine(indexes, main_index);
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
        if (!thop.isBusy() && Lists[route.list]) {
            let view_el = query('#view');

            render(html`
                <h1 id="ls_spec"></h1>
                <div id=${'ls_' + route.list} class="ls_table"></div>
            `, view_el);

            refreshHeader(route.spec);
            refreshListTable(route.list, group_info, route.search, route.page, route.sort);
        }
    };

    this.parseRoute = function(route, path, parameters, hash) {
        // Model: mco_list/<table>/<date>/<sector>
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
    };

    this.routeToUrl = function(args) {
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
        url = util.buildUrl(url, {
            search: new_route.search || null,
            group: new_route.group,
            page: new_route.page,
            sort: new_route.sort || null
        });

        return {
            url: url,
            allowed: true
        };
    };

    function updateList(list_name, date, sector, spec) {
        let params = {
            date: date,
            sector: Lists[list_name].sector ? sector : null,
            spec: spec
        };
        let url = util.buildUrl(thop.baseUrl(Lists[list_name].path), params);
        let list = list_cache[list_name];

        if (!list || url !== list.url) {
            catalog.update(Lists[list_name].catalog);

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

    function refreshHeader(spec) {
        let h1 = query('#ls_spec');

        if (spec) {
            render(html`
                Filtre : ${spec}
                <a href=${self.routeToUrl({spec: null}).url}>(retirer)</a>
            `, h1);
            h1.removeClass('hide');
        } else {
            render(html``, h1);
            h1.addClass('hide');
        }
    }

    function refreshGroupsMenu(list_info, select_group) {
        let el = query('#opt_group > select');

        render(list_info.groups.map(group =>
            html`<option value=${group.type}>${group.name}</option>`), el);

        if (select_group)
            el.value = select_group;
    }

    function refreshListTable(list_name, group_info, search, page, sort) {
        let test;
        if (search && search.match(/^\/.*\/$/)) {
            search = simplifyForSearch(search.substr(1, search.length - 2));
            try {
                search = new RegExp(search);
                test = function(content) { return !!simplifyForSearch(content).match(search); };
            } catch (err) {
                // TODO: Issue RegExp error message
                test = function(content) { return false; };
            }
        } else if (search) {
            search = simplifyForSearch(search);
            test = function(content) { return simplifyForSearch(content).indexOf(search) >= 0; };
        } else {
            test = function(content) { return true; };
        }

        let list_info = Lists[list_name];
        let list = list_cache[list_name];
        let concepts_map = catalog.update(list_info.catalog).map;

        let builder;
        if (thop.needsRefresh(list, group_info, search)) {
            builder = wt_data_table.create();
            builder.sortHandler = function(sort) { thop.go({sort: sort}); }
            list.builder = builder;

            // Special column
            let first_column = 0;
            if (!list_info.columns[0].header)
                first_column = 1;

            // Header
            for (let i = first_column; i < list_info.columns.length; i++) {
                let col = list_info.columns[i];
                builder.addColumn(col.key, col.header, self.addSpecLinks,
                                  {tooltip: col.tooltip || col.header});
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

                    show |= test(content);
                    cells.push(content);
                }

                if (show) {
                    if (first_column) {
                        if (cells[0] !== prev_cell0) {
                            if (prev_cell0)
                                builder.endRow();
                            builder.beginRow();
                            builder.addCell(cells[0], {colspan: cells.length - 1});
                            prev_cell0 = cells[0];
                        }
                    }

                    builder.beginRow();
                    for (let i = first_column; i < cells.length; i++) {
                        const cell = cells[i];
                        builder.addCell(cell);
                    }
                    builder.endRow();
                }
            }
        } else {
            builder = list.builder;
        }

        builder.sort(sort, false);

        render(html`
            <div class="dtab_pager"></div>
            <div class="dtab"></div>
            <div class="dtab_pager"></div>
        `, query('#ls_' + list_name));

        let render_count = builder.render(query('#ls_' + list_name + ' .dtab'),
                                          (page - 1) * list_info.page_len, list_info.page_len,
                                          {hide_header: !list_info.header, hide_parents: !!sort});
        syncPagers(queryAll('#ls_' + list_name + ' .dtab_pager'), page,
                   wt_pager.computeLastPage(render_count, builder.getRowCount(), list_info.page_len));
    }

    function createContent(column, item, concepts_map, group) {
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

    function simplifyForSearch(str) {
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

    function maskToRanges(mask) {
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

    function makeSpecAnchor(str) {
        if (str[0] === 'A') {
            let href = self.routeToUrl({list: 'procedures', spec: str}).url;
            return html`<a href=${href}>${str}</a>`;
        } else if (str[0] === 'D') {
            let href = self.routeToUrl({list: 'diagnoses', spec: str}).url;
            return html`<a href=${href}>${str}</a>`;
        } else if (str.match(/^[0-9]{2}[CMZKH][0-9]{2}[ZJT0-9ABCDE]?$/)) {
            let href = mco_pricing.routeToUrl({view: 'table', ghm_root: str.substr(0, 5)}).url;
            return html`<a class="ghm" href=${href}>${str}</a>`;
        } else if (str.match(/[Nn]oeud [0-9]+/)) {
            let href = mco_tree.routeToUrl().url + '#n' + str.substr(6);
            return html`<a href=${href}>${str}</a>`;
        } else {
            return str;
        }
    }

    this.addSpecLinks = function(str) {
        let elements = [];
        for (;;) {
            let m = str.match(/([AD](\-[0-9]+|\$[0-9]+\.[0-9]+)|[0-9]{2}[CMZKH][0-9]{2}[ZJT0-9ABCDE]?|[Nn]oeud [0-9]+)/);
            if (!m)
                break;

            elements.push(str.substr(0, m.index));
            elements.push(makeSpecAnchor(m[0]));
            str = str.substr(m.index + m[0].length);
        }
        elements.push(str);

        return elements;
    };

    function syncPagers(pagers, current_page, last_page) {
        pagers.forEach(function(pager) {
            if (last_page) {
                let builder = wt_pager.create();
                builder.hrefBuilder = page => self.routeToUrl({page: page}).url;
                builder.setLastPage(last_page);
                builder.setCurrentPage(current_page);
                builder.render(pager);

                pager.removeClass('hide');
            } else {
                pager.addClass('hide');
                render(html``, pager);
            }
        });
    }

    return this;
}).call({});
