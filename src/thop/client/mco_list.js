// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_list = {};
(function() {
    'use strict';

    const Lists = {
        'ghm_roots': {
            'path': 'api/mco_ghm_ghs.json',
            'concept_set': 'mco_ghm_roots',

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
                    let ghm_root_info1 = ghm_roots_map[ghm_ghs1.ghm_root];
                    let ghm_root_info2 = ghm_roots_map[ghm_ghs2.ghm_root];

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
                    let ghm_root_info1 = ghm_roots_map[ghm_ghs1.ghm_root];
                    let ghm_root_info2 = ghm_roots_map[ghm_ghs2.ghm_root];

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
                                return 'CMD ' + (ghm_root_info.cmd < 10 ? '0' : '') + ghm_root_info.cmd +
                                                ' - ' + ghm_root_info.cmd_desc;
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
                {header: 'Racine de GHM', func: function(ghm_ghs, ghm_roots_map) {
                    return ghm_ghs.ghm_root + (ghm_roots_map[ghm_ghs.ghm_root] ?
                                              ' - ' + ghm_roots_map[ghm_ghs.ghm_root].desc : '');
                }}
            ]
        },

        'ghm_ghs': {
            'path': 'api/mco_ghm_ghs.json',
            'concept_set': 'mco_ghm_roots',

            'columns': [
                {func: function(ghm_ghs, ghm_roots_map) {
                    return ghm_ghs.ghm_root + (ghm_roots_map[ghm_ghs.ghm_root] ?
                                              ' - ' + ghm_roots_map[ghm_ghs.ghm_root].desc : '');
                }},
                {header: 'GHM', variable: 'ghm'},
                {header: 'GHS', variable: 'ghs'},
                {header: 'Durées', title: 'Durées (nuits)', func: function(ghm_ghs) {
                    return maskToRanges(ghm_ghs.durations);
                }},
                {header: 'Confirmation', title: 'Confirmation (nuits)', func: function(ghm_ghs) {
                    return ghm_ghs.confirm_treshold ? '< ' + ghm_ghs.confirm_treshold : null;
                }},
                {header: 'DP', variable: 'main_diagnosis'},
                {header: 'Diagnostics', variable: 'diagnoses'},
                {header: 'Actes', variable: 'procedures'},
                {header: 'Autorisations', title: 'Autorisations (unités et lits)', func: function(ghm_ghs) {
                    let ret = [];
                    if (ghm_ghs.unit_authorization)
                        ret.push('Unité ' + ghm_ghs.unit_authorization);
                    if (ghm_ghs.bed_authorization)
                        ret.push('Lit ' + ghm_ghs.bed_authorization);
                    return ret;
                }},
                {header: 'Sévérité âgé', func: function(ghm_ghs) {
                    if (ghm_ghs.old_age_treshold) {
                        return '≥ ' + ghm_ghs.old_age_treshold + ' et < ' +
                               (ghm_ghs.old_severity_limit + 1);
                    } else {
                        return null;
                    }
                }},
                {header: 'Sévérité jeune', func: function(ghm_ghs) {
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
            'concept_set': 'cim10',

            'columns': [
                {header: 'Diagnostic', func: function(diag, cim10_map) {
                    let text = diag.diag;
                    switch (diag.sex) {
                        case 'Homme': { text += ' (♂)'; } break;
                        case 'Femme': { text += ' (♀)'; } break;
                    }
                    if (cim10_map[diag.diag])
                        text += ' - ' + cim10_map[diag.diag].desc;
                    return text;
                }},
                {header: 'Niveau', func: function(diag) { return diag.severity ? diag.severity + 1 : 1; }},
                {header: 'CMD', variable: 'cmd'},
                {header: 'Liste principale', variable: 'main_list'}
            ]
        },

        'procedures': {
            'path': 'api/mco_procedures.json',
            'concept_set': 'ccam',

            'columns': [
                {header: 'Acte', func: function(proc, ccam_map) {
                    let proc_phase = proc.proc + (proc.phase ? '/' + proc.phase : '');
                    return proc_phase + (ccam_map[proc.proc] ? ' - ' + ccam_map[proc.proc].desc : '');
                }},
                {header: 'Dates', title: 'Date de début incluse, date de fin exclue',
                 func: function(proc) {
                    return proc.begin_date + ' -- ' + proc.end_date;
                }},
                {header: 'Activités', variable: 'activities'},
                {header: 'Extensions', title: 'Extensions (CCAM descriptive)', variable: 'extensions'}
            ]
        }
    };

    // Routes
    let specs = {};
    let groups = {};
    let search = {};
    let pages = {};

    // Cache
    let list_cache = {};
    let reactor = {};

    function runList(route, url, parameters, hash)
    {
        let errors = new Set(data.getErrors());

        // Parse route (model: list/<table>/<date>[/<spec>])
        let url_parts = url.split('/');
        route.list = url_parts[1] || 'ghm_roots';
        route.date = url_parts[2] || null;
        route.page = parseInt(parameters.page) || 1;
        route.spec = (url_parts[2] && url_parts[3]) ? url_parts[3] : null;
        route.group = parameters.group || null;
        route.search = parameters.search || null;
        specs[route.list] = route.spec;
        search[route.list] = route.search;
        groups[route.list] = route.group;
        pages[route.list + route.spec] = route.page;

        // Resources
        let indexes = mco_common.updateIndexes();
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
            updateList(route.list, indexes[main_index].begin_date,
                       route.spec, group_info, route.search);

        // Errors
        if (!Lists[route.list])
            errors.add('Liste inconnue');
        if (route.date !== null && indexes.length && main_index < 0)
            errors.add('Date incorrecte');
        if (route.group && Lists[route.list] && !group_info)
            errors.add('Critère de tri inconnu');

        // Refresh settings
        queryAll('#opt_indexes').removeClass('hide');
        mco_common.refreshIndexes(indexes, main_index);
        query('#opt_search').removeClass('hide');
        {
            let search_input = query('#opt_search > input');
            if (route.search != search_input.value)
                search_input.value = route.search || '';
        }
        if (Lists[route.list] && Lists[route.list].groups !== undefined) {
            query('#opt_groups').removeClass('hide');
            refreshGroups(Lists[route.list], route.group);
        }

        // Limit 'blinking' behavior
        let list_table = query('#ls_' + route.list);
        let show_table = list_table && !list_table.hasClass('hide');
        queryAll('.ls_table').addClass('hide');
        if (show_table) {
            list_table.removeClass('hide');
        } else {
            query('#ls').addClass('hide');
        }

        // Refresh view
        thop.refreshErrors(Array.from(errors));
        if (!data.isBusy()) {
            data.clearErrors();

            refreshHeader(route.spec);

            let list_info = Lists[route.list];
            if (list_info) {
                let concepts_map = mco_common.updateConceptSet(list_info.concept_set).map;
                refreshTable(route.list, concepts_map, route.page);
                query('#ls_' + route.list).removeClass('hide');
            }

            query('#ls').removeClass('hide');
        }
        thop.markBusy('#ls', data.isBusy());
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

        let url_parts = [thop.baseUrl('mco_list'), new_route.list, new_route.date, new_route.spec];
        while (!url_parts[url_parts.length - 1])
            url_parts.pop();
        let url = url_parts.join('/');

        let query = [];
        if (new_route.search)
            query.push('search=' + encodeURI(new_route.search));
        if (new_route.page && new_route.page !== 1)
            query.push('page=' + encodeURI(new_route.page));
        if (new_route.group && (!Lists[new_route.list] || !Lists[new_route.list].groups ||
                                new_route.group !== Lists[new_route.list].groups[0].type))
            query.push('group=' + encodeURI(new_route.group));
        if (query.length)
            url += '?' + query.join('&');

        return url;
    }
    this.routeToUrl = routeToUrl;

    function go(args, delay)
    {
        thop.route(routeToUrl(args), delay);
    }
    this.go = go;

    function updateList(list_name, date, spec, group_info, search)
    {
        let url = buildUrl(thop.baseUrl(Lists[list_name].path), {date: date, spec: spec});
        let list = list_cache[list_name];

        if (!list || url !== list.url) {
            mco_common.updateConceptSet(Lists[list_name].concept_set);

            list_cache[list_name] = {
                url: null, items: [], match_count: 0,
                init: false, cells: [], offsets: []
            };
            list = list_cache[list_name];

            data.get(url, function(json) {
                list.url = url;
                list.items = json;
            });
        } else if (!list.init || group_info !== list.group_info || search !== list.search) {
            if (search)
                search = simplifyForSearch(search);

            let list_info = Lists[list_name];
            let columns = list_info.columns;
            let concepts_map = mco_common.updateConceptSet(list_info.concept_set).map;

            // Groups
            if (group_info)
                list.items.sort(function(v1, v2) { return group_info.func(v1, v2, concepts_map); });

            // Filter
            list.cells = [];
            list.items_count = 0;
            list.match_count = 0;
            {
                let prev_deduplicate_key = null;
                for (const item of list.items) {
                    if (list_info.deduplicate) {
                        let deduplicate_key = list_info.deduplicate(item, concepts_map);
                        if (deduplicate_key === prev_deduplicate_key)
                            continue;
                        prev_deduplicate_key = deduplicate_key;
                    }
                    list.items_count++;

                    let show = false;
                    let prev_length = list.cells.length;
                    for (const col of columns) {
                        let content = createContent(col, item, concepts_map,
                                                    group_info ? group_info.type : null);
                        if (!search || simplifyForSearch(content).indexOf(search) >= 0)
                            show = true;
                        list.cells.push(content);
                    }

                    if (show) {
                        if (list.match_count % TableLen === 0)
                            list.offsets.push(prev_length);
                        list.match_count++;
                    } else {
                        list.cells.splice(prev_length);
                    }
                }
            }

            list.init = true;
            list.group_info = group_info;
            list.search = search;
        }
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

    function refreshHeader(spec)
    {
        let h1 = query('#ls_spec');

        if (spec) {
            h1.innerHTML = '';
            h1.appendChild(document.createTextNode('Filtre : ' + spec + ' '));
            h1.appendChild(html('a', {href: routeToUrl({spec: null})}, '(retirer)'));
            h1.removeClass('hide');
        } else {
            h1.innerText = '';
            h1.addClass('hide');
        }
    }

    function refreshGroups(list_info, select_group)
    {
        let el = query('#opt_groups > select');
        el.innerHTML = '';

        for (const group of list_info.groups) {
            let opt = html('option', {value: group.type}, group.name);
            el.appendChild(opt);
        }
        if (select_group)
            el.value = select_group;
    }

    function refreshTable(list_name, concepts_map, page)
    {
        let table = html('table',
            html('thead'),
            html('tbody')
        );
        let thead = table.query('thead');
        let tbody = table.query('tbody');

        let list_info = Lists[list_name];
        let list = list_cache[list_name];

        let stats_text = '';
        let last_page = null;
        if (list && list.url) {
            // Pagination
            let offset = (page >= 1 && page <= list.offsets.length) ? list.offsets[page - 1] : list.cells.length;

            // Pagination
            if (list.match_count && (list.match_count > TableLen || offset))
                last_page = Math.floor((list.match_count - 1) / TableLen + 1);

            let first_column = 0;
            if (!list_info.columns[0].header)
                first_column = 1;

            // Header
            if (list_info.header === undefined || list_info.header) {
                let tr = html('tr');
                for (let i = first_column; i < list_info.columns.length; i++) {
                    let col = list_info.columns[i];

                    let title = col.title ? col.title : col.header;
                    let th = html('th', {title: title, style: col.style}, col.header);
                    tr.appendChild(th);
                }
                thead.appendChild(tr);
            }

            // Data
            let visible_count = 0;
            let end = Math.min(offset + TableLen * list_info.columns.length, list.cells.length);
            let prev_heading_content = null;
            for (let i = offset; i < end; i += list_info.columns.length) {
                if (!list_info.columns[0].header) {
                    let content = list.cells[i];
                    if (content && content !== prev_heading_content) {
                        let tr = html('tr', {class: 'heading'},
                            html('td', {colspan: list_info.columns.length - 1, title: content},
                                 addSpecLinks(content))
                        );
                        tbody.appendChild(tr);
                        prev_heading_content = content;
                    }
                }

                let tr = html('tr');
                for (let j = first_column; j < list_info.columns.length; j++) {
                    let content = list.cells[i + j];
                    let td = html('td', {title: content}, addSpecLinks(content));
                    tr.appendChild(td);
                }
                tbody.appendChild(tr);
                visible_count++;
            }

            if (!visible_count) {
                let message = list.match_count ? 'Cette page n\'existe pas' : 'Aucun contenu à afficher';
                tbody.appendChild(html('tr',
                    html('td', {colspan: list_info.columns.length - first_column}, message)
                ));
            }

            if (visible_count)
                stats_text += ((page - 1) * TableLen + 1) + ' - ' + ((page - 1) * TableLen + visible_count) + ' ';
            stats_text += '(' + list.match_count + ' ' + (list.match_count > 1 ? 'lignes' : 'ligne');
            if (list.match_count < list.items_count)
                stats_text += ' sur ' + list.items_count;
            stats_text += ')';
        }

        for (let old_pager of queryAll('.ls_pager')) {
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

        query('#ls_stats').innerText = stats_text;

        let old_table = query('#ls_' + list_name);
        table.copyAttributesFrom(old_table);
        old_table.replaceWith(table);
    }

    function createPagination(page, last_page)
    {
        let builder = new Pager(page, last_page);
        builder.anchorBuilder = function(text, page) {
            return html('a', {href: routeToUrl({page: page})}, '' + text);
        }
        return builder.getWidget();
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
            url = routeToUrl({list: 'procedures', spec: str});
        } else if (str[0] === 'D') {
            url = routeToUrl({list: 'diagnoses', spec: str});
        } else if (str.match(/^[0-9]{2}[CMZK][0-9]{2}[ZJT0-9ABCDE]?$/)) {
            url = mco_pricing.routeToUrl({view: 'table', ghm_root: str.substr(0, 5)});
            cls = 'ghm';
        } else if (str.match(/[Nn]oeud [0-9]+/)) {
            url = mco_tree.routeToUrl() + '#n' + str.substr(6);
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
            let m = str.match(/([AD](\-[0-9]+|\$[0-9]+\.[0-9]+)|[0-9]{2}[CMZK][0-9]{2}[ZJT0-9ABCDE]?|[Nn]oeud [0-9]+)/);
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

    thop.registerUrl('mco_list', this, runList);
}).call(mco_list);
