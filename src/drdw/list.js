var list = {};
(function() {
    const PageLen = 200;

    // Routes
    var specs = {};
    var sort = {};
    var search = {};
    var pages = {};

    // Cache
    var indexes = [];
    var list_cache = {};

    const Lists = {
        'ghm_roots': {
            'table': 'ghm_ghs',
            'concepts': 'ghm_roots',

            'sort': [
                {type: 'ghm_roots', name: 'Racines',
                 func: function(ghm_ghs1, ghm_ghs2) {
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
                {func: function(ghm_ghs, ghm_roots_map, sort_type) {
                    switch (sort_type) {
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

                        case 'ghm_roots': { return null; } break;
                    }
                }},
                {header: 'Racine de GHM', func: function(ghm_ghs, ghm_roots_map) {
                    return ghm_ghs.ghm_root + (ghm_roots_map[ghm_ghs.ghm_root] ?
                                              ' - ' + ghm_roots_map[ghm_ghs.ghm_root].desc : '');
                }}
            ]
        },

        'ghm_ghs': {
            'concepts': 'ghm_roots',

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
                    var ret = [];
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
            'concepts': 'cim10',

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
            'concepts': 'ccam',

            'columns': [
                {header: 'Acte', func: function(proc, ccam_map) {
                    var proc_phase = proc.proc + (proc.phase ? '/' + proc.phase : '');
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

    function run(route, url, parameters, hash)
    {
        let errors = new Set(downloadJson.errors);

        // Parse route (model: list/<table>/<date>[/<spec>])
        let url_parts = url.split('/');
        route.list = url_parts[1] || route.list;
        route.date = url_parts[2] || route.date;
        route.page = parseInt(parameters.page) || 1;
        route.spec = (url_parts[2] && url_parts[3]) ? url_parts[3] : null;
        route.sort = parameters.sort;
        route.search = parameters.search;
        specs[route.list] = route.spec;
        search[route.list] = route.search;
        sort[route.list] = route.sort;
        pages[route.list + route.spec] = route.page;

        // Resources
        indexes = getIndexes();
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        let main_index = indexes.findIndex(function(info) { return info.begin_date === route.date; });
        let sort_info = null;
        if (Lists[route.list] && Lists[route.list].sort) {
            if (route.sort) {
                sort_info = Lists[route.list].sort.find(function(sort_info) {
                    return sort_info.type == route.sort;
                });
            } else {
                sort_info = Lists[route.list].sort[0];
            }
        }
        if (main_index >= 0 && Lists[route.list])
            updateList(route.list, main_index, route.spec, sort_info, route.search);

        // Errors
        if (!Lists[route.list])
            errors.add('Liste inconnue');
        if (route.date !== null && indexes.length && main_index < 0)
            errors.add('Date incorrecte');
        if (route.sort && Lists[route.list] && !sort_info)
            errors.add('Critère de tri inconnu');

        // Refresh settings
        removeClass(__('#opt_indexes'), 'hide');
        refreshIndexesLine(_('#opt_indexes'), indexes, main_index);
        _('#opt_search').classList.remove('hide');
        {
            let search_input = _('#opt_search > input');
            if (route.search != search_input.value)
                search_input.value = route.search || '';
        }
        if (Lists[route.list] && Lists[route.list].sort !== undefined) {
            _('#opt_sort').classList.remove('hide');
            refreshSortList(Lists[route.list], route.sort);
        }

        // Refresh view
        let list_table = _('#ls_' + route.list);
        {
            let list_tables = __('.ls_table');
            for (let i = 0; i < list_tables.length; i++) {
                if (list_tables[i] != list_table)
                    list_tables[i].classList.add('hide');
            }
        }
        if (list_table && list_table.classList.contains('hide')) {
            _('#ls_spec').classList.add('hide');
            addClass(__('.ls_pages'), 'hide');
        }
        if (!downloadJson.busy) {
            refreshHeader(route.spec, Array.from(errors));
            downloadJson.errors = [];

            let list_info = Lists[route.list];
            if (list_info) {
                list_table.classList.remove('hide');
                refreshTable(list_table, route.list,
                             list_info.concepts ? getConcepts(list_info.concepts)[1] : null,
                             route.page);
            }
        }
        _('#ls').classList.remove('hide');
        markBusy('#ls', downloadJson.busy);
    }
    this.run = run;

    function routeToUrl(args)
    {
        let new_route = buildRoute(args);
        if (args.spec === undefined)
            new_route.spec = specs[new_route.list];
        if (args.search === undefined)
            new_route.search = search[new_route.list];
        if (args.sort === undefined)
            new_route.sort = sort[new_route.list];
        if (args.page === undefined)
            new_route.page = pages[new_route.list + new_route.spec];

        let url_parts = [buildModuleUrl('list'), new_route.list, new_route.date, new_route.spec];
        while (!url_parts[url_parts.length - 1])
            url_parts.pop();
        let url = url_parts.join('/');

        let query = [];
        if (new_route.search)
            query.push('search=' + encodeURI(new_route.search));
        if (new_route.page && new_route.page !== 1)
            query.push('page=' + encodeURI(new_route.page));
        if (new_route.sort && (!Lists[new_route.list] || !Lists[new_route.list].sort ||
                               new_route.sort !== Lists[new_route.list].sort[0].type))
            query.push('sort=' + encodeURI(new_route.sort));
        if (query.length)
            url += '?' + query.join('&');

        return url;
    }
    this.routeToUrl = routeToUrl;

    function route(args, delay)
    {
        go(routeToUrl(args), true, delay);
    }
    this.route = route;

    function updateList(list_name, index, spec, sort_info, search)
    {
        let url = buildUrl(BaseUrl + 'api/' + (Lists[list_name].table || list_name) + '.json',
                           {date: indexes[index].begin_date, spec: spec});
        let list = list_cache[list_name];

        if (!list || url !== list.url) {
            if (Lists[list_name].concepts)
                getConcepts(Lists[list_name].concepts);

            list_cache[list_name] = {
                url: null, items: [],
                init: false, cells: [], offsets: []
            };
            list = list_cache[list_name];

            downloadJson(url, function(json) {
                list.url = url;
                list.items = json;
            });
        } else if (!list.init || sort_info !== list.sort_info || search !== list.search) {
            if (search)
                search = simplifyForSearch(search);

            let list_info = Lists[list_name];
            let columns = list_info.columns;
            let concepts_map = undefined;
            if (list_info.concepts)
                concepts_map = getConcepts(list_info.concepts)[1];

            // Sort
            if (sort_info) {
                list.items.sort(function(v1, v2) { return sort_info.func(v1, v2, concepts_map); });
                list.sort = sort_info.type;
            }

            // Filter
            list.cells = [];
            list.match_count = 0;
            {
                let prev_deduplicate_key = null;
                for (let i = 0; i < list.items.length; i++) {
                    if (list_info.deduplicate) {
                        let deduplicate_key = list_info.deduplicate(list.items[i], concepts_map);
                        if (deduplicate_key === prev_deduplicate_key)
                            continue;
                        prev_deduplicate_key = deduplicate_key;
                    }

                    let show = false;
                    let prev_length = list.cells.length;
                    for (var j = 0; j < columns.length; j++) {
                        let content = createContent(columns[j], list.items[i], concepts_map,
                                                    sort_info ? sort_info.type : null);
                        if (!search || simplifyForSearch(content).indexOf(search) >= 0)
                            show = true;
                        list.cells.push(content);
                    }

                    if (show) {
                        if (list.match_count % PageLen === 0)
                            list.offsets.push(prev_length);
                        list.match_count++;
                    } else {
                        list.cells.splice(prev_length);
                    }
                }
            }

            list.init = true;
            list.sort_info = sort_info;
            list.search = search;
        }
    }

    function createContent(column, item, concepts_map, sort_type)
    {
        if (column.variable) {
            var content = item[column.variable];
        } else if (column.func) {
            var content = column.func(item, concepts_map, sort_type);
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

    function refreshHeader(spec, errors)
    {
        var log = _('#log');
        var h1 = _('#ls_spec');

        if (errors.length) {
            log.innerHTML = errors.join('<br/>');
            log.classList.remove('hide');
        }

        if (spec) {
            h1.innerHTML = '';
            h1.appendChild(document.createTextNode('Filtre : ' + spec + ' '));
            h1.appendChild(createElement('a', {href: routeToUrl({spec: null})}, '(retirer)'));
            h1.classList.remove('hide');
        } else {
            h1.innerText = '';
            h1.classList.add('hide');
        }
    }

    function refreshSortList(list_info, select_sort)
    {
        var el = _('#opt_sort > select');
        el.innerHTML = '';

        for (let i = 0; i < list_info.sort.length; i++) {
            let sort_info = list_info.sort[i];
            let opt = createElement('option', {value: sort_info.type}, sort_info.name);
            el.appendChild(opt);
        }
        if (select_sort)
            el.value = select_sort;
    }

    function refreshTable(old_table, list_name, concepts_map, page)
    {
        var table = createElement('table', {},
            createElement('thead'),
            createElement('tbody')
        );
        var thead = table.querySelector('thead');
        var tbody = table.querySelector('tbody');

        var pages = createElement('div');

        let list_info = Lists[list_name];
        let list = list_cache[list_name];

        if (list.url) {
            let offset = (page >= 1 && page <= list.offsets.length) ? list.offsets[page - 1] : list.cells.length;

            // Pagination
            if (list.match_count && (list.match_count > PageLen || offset)) {
                let last_page = Math.floor((list.match_count / PageLen) + 1);
                pages = createPagination(page, last_page);
            }

            let first_column = 0;
            if (!list_info.columns[0].header)
                first_column = 1;

            // Header
            if (list_info.header === undefined || list_info.header) {
                var tr = createElement('tr');
                for (var i = first_column; i < list_info.columns.length; i++) {
                    let title = list_info.columns[i].title ? list_info.columns[i].title : list_info.columns[i].header;
                    var th = createElement('th', {title: title, style: list_info.columns[i].style},
                                           list_info.columns[i].header);
                    tr.appendChild(th);
                }
                thead.appendChild(tr);
            }

            // Data
            let visible_count = 0;
            let end = Math.min(offset + PageLen * list_info.columns.length, list.cells.length);
            let prev_heading_content = null;
            for (var i = offset; i < end; i += list_info.columns.length) {
                if (!list_info.columns[0].header) {
                    let content = list.cells[i];
                    if (content && content !== prev_heading_content) {
                        var tr = createElement('tr', {class: 'heading'},
                            createElement('td', {colspan: list_info.columns.length - 1,
                                                 title: content}, addSpecLinks(content))
                        );
                        tbody.appendChild(tr);
                        prev_heading_content = content;
                    }
                }

                var tr = createElement('tr');
                for (var j = first_column; j < list_info.columns.length; j++) {
                    let content = list.cells[i + j];
                    let td = createElement('td', {title: content}, addSpecLinks(content));
                    tr.appendChild(td);
                }
                tbody.appendChild(tr);
                visible_count++;
            }

            if (!visible_count) {
                let message = list.match_count ? 'Cette page n\'existe pas' : 'Aucun contenu à afficher';
                tbody.appendChild(createElement('tr', {},
                    createElement('td', {colspan: list_info.columns.length - first_column},
                                  message)
                ));
            }
        }

        let old_pages = __('.ls_pages');
        for (let i = 0; i < old_pages.length; i++) {
            let pages_copy = pages.cloneNode(true);
            cloneAttributes(old_pages[i], pages_copy);
            pages_copy.classList.toggle('hide', !pages.children.length);
            old_pages[i].parentNode.replaceChild(pages_copy, old_pages[i]);
        }

        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
    }

    function createPagination(page, last_page)
    {
        let pages = createElement('table', {},
            createElement('tr')
        );
        let tr = pages.querySelector('tr');

        function addPageLink(text, page)
        {
            if (page) {
                tr.appendChild(createElement('td', {},
                    createElement('a', {href: routeToUrl({page: page})}, '' + text)
                ));
            } else {
                tr.appendChild(createElement('td', {}, '' + text));
            }
        }

        let start_page, end_page;
        if (last_page < 5 || page < 5) {
            start_page = 1;
            end_page = Math.min(5, last_page);
        } else if (page > last_page - 4) {
            start_page = last_page - 4;
            end_page = last_page;
        } else {
            start_page = page - 1;
            end_page = page + 1;
        }

        addPageLink('≪', (page > 1) ? (page - 1) : null);
        if (start_page > 1) {
            addPageLink(1, 1);
            addPageLink(' … ');
        }
        for (let i = start_page; i <= end_page; i++)
            addPageLink(i, (page !== i) ? i : null);
        if (end_page < last_page) {
            addPageLink(' … ');
            addPageLink(last_page, last_page);
        }
        addPageLink('≫', (page < last_page) ? (page + 1) : null);

        return pages;
    }

    function maskToRanges(mask)
    {
        var ranges = [];

        var i = 0;
        for (;;) {
            while (i < 32 && !(mask & (1 << i)))
                i++;
            if (i >= 32)
                break;

            var j = i + 1;
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
            url = pricing.routeToUrl({view: 'table', ghm_root: str.substr(0, 5)});
            cls = 'ghm';
        } else if (str.match(/noeud [0-9]+/)) {
            url = tree.routeToUrl() + '#n' + str.substr(6);
        } else {
            return str;
        }

        let anchor = createElement('a', {href: url, class: cls}, str);

        return anchor;
    }
    this.makeSpecLink = makeSpecLink;

    function addSpecLinks(str)
    {
        var elements = [];
        for (;;) {
            var m = str.match(/([AD](\-[0-9]+|\$[0-9]+\.[0-9]+)|[0-9]{2}[CMZK][0-9]{2}[ZJT0-9ABCDE]?|noeud [0-9]+)/);
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
}).call(list);
