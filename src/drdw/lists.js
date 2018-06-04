var tables = {};
(function() {
    // URL settings (routing)
    var target_table = 'classifier_tree';
    var target_specs = {};

    var table_index = null;
    var table_type = null;
    var table_spec = null;
    var items_init = false;
    var items = {};

    var collapse_nodes = new Set();

    const TableColumns = {
        'ghm_ghs': [
            'ghm',
            'ghs',
            'durations',
            'ages',
            'confirm_treshold',
            'main_diagnosis',
            'diagnoses',
            'procedures',
            'unit_authorization',
            'bed_authorization',
            'old_age_treshold',
            'old_severity_limit',
            'young_age_treshold',
            'young_severity_limit'
        ],
        'diagnoses': [
            'diag',
            'sex',
            'cmd',
            'main_list'
        ],
        'procedures': [
            'proc',
            'begin_date',
            'end_date',
            'phase',
            'activities',
            'extensions'
        ]
    };

    function run()
    {
        // Parse route (model: tables/<table>/<date>[/<spec>])
        var parts = url_page.split('/');
        target_table = parts[1] || target_table;
        target_date = parts[2] || target_date;
        target_specs[target_table] = parts[3] || target_specs[target_table];

        // Validate
        var main_index = indexes.findIndex(function(info) { return info.begin_date === target_date; });
        if (target_date !== null && indexes.length && main_index < 0)
            errors.add('Date incorrecte');
        // TODO: Validation is not complete

        // Redirection (stable URLs)
        if (target_date === null && indexes.length) {
            route({date: indexes[indexes.length - 1].begin_date});
            return;
        }

        // Common resources
        updateIndexes(run);
        var force_refresh = false;
        if (main_index >= 0 && (table_index !== main_index || table_type !== target_table ||
                                table_spec !== target_specs[target_table])) {
            force_refresh = (table_type !== target_table);
            items_init = false;

            markOutdated('#tables_view', true);
            updateTable(target_table, main_index, target_specs[target_table], run);
        }

        // Refresh display
        document.querySelector('#tables').classList.add('active');
        document.querySelector('#tables_tree').classList.toggle('active', target_table === 'classifier_tree');
        document.querySelector('#tables_table').classList.toggle('active', target_table !== 'classifier_tree');
        refreshIndexesLine('#tables_indexes', main_index);
        if (!downloadJson.run_lock || force_refresh) {
            refreshHeader(Array.from(errors));
            errors.clear();

            if (target_table === 'classifier_tree') {
                refreshClassifierTree(items);
            } else {
                refreshTable(items, TableColumns[target_table]);
            }

            if (!downloadJson.run_lock)
                markOutdated('#tables_view', false);
        }
    }
    this.run = run;

    function refreshHeader(errors)
    {
        var log = document.querySelector('#tables .log');
        var h1 = document.querySelector('#tables_spec');

        if (errors.length) {
            log.style.display = 'block';
            log.innerHTML = errors.join('<br/>');
        } else {
            log.style.display = 'none';
        }

        if (target_specs[target_table]) {
            var remove_spec = function(e) {
                tables.route({spec: null});
                e.preventDefault();
            };

            h1.innerHTML = '';
            h1.appendChild(document.createTextNode('Filtre : ' + target_specs[target_table] + ' '));
            h1.appendChild(createElement('a', {href: '#',
                                               click: remove_spec}, '(retirer)'));
        } else {
            h1.innerText = '';
        }
    }

    function route(args)
    {
        if (args !== undefined) {
            target_date = args.date || target_date;
            target_table = args.table || target_table;
            if (args.spec !== undefined)
                target_specs[target_table] = args.spec;
        }

        if (target_date !== null) {
            switchPage('tables/' + target_table + '/' + target_date +
                       (target_specs[target_table] ? '/' + target_specs[target_table] : ''));
        } else {
            switchPage('tables/' + target_table);
        }
    }
    this.route = route;

    function updateTable(table, index, spec, func)
    {
        if (items_init)
            return true;

        items = [];

        var begin_date = indexes[index].begin_date;
        downloadJson('api/' + table + '.json', {date: begin_date, spec: spec},
                     function(status, json) {
            var error = null;

            switch (status) {
                case 200: { items = json; } break;

                case 404: { error = 'Table introuvable'; } break;
                case 502:
                case 503: { error = 'Service non accessible'; } break;
                case 504: { error = 'Délai d\'attente dépassé, réessayez'; } break;
                default: { error = 'Erreur inconnue ' + status; } break;
            }

            if (!error) {
                items_init = true;
                table_type = table;
                table_index = index;
                table_spec = spec;
            } else {
                items_init = false;
            }

            if (error)
                errors.add(error);
            if (!downloadJson.run_lock)
                func();
        });
    }

    function refreshClassifierTree(nodes)
    {
        var click_function = function(e) {
            var li = this.parentNode.parentNode;
            var collapse_id = target_date + li.id;
            if (li.classList.toggle('collapse')) {
                collapse_nodes.add(collapse_id);
            } else {
                collapse_nodes.delete(collapse_id);
            }
            e.preventDefault();
        };

        function createNodeLi(idx, text, parent)
        {
            var content = addSpecLinks(text);

            if (parent) {
                var li = createElement('li', {id: 'n' + idx, class: 'parent'},
                    createElement('span', {},
                        createElement('span', {class: 'n',
                                               click: click_function}, '' + idx + ' '),
                        content
                    )
                );
                if (collapse_nodes.has(target_date + 'n' + idx))
                    li.classList.add('collapse');
            } else {
                var li = createElement('li', {id: 'n' + idx, class: 'leaf'},
                    createElement('span', {},
                        createElement('span', {class: 'n'}, '' + idx + ' '),
                        content
                    )
                );
            }

            return li;
        }

        function recurseNodes(node_idx, parent_next_idx)
        {
            var ul = createElement('ul');

            for (var i = 0; node_idx !== undefined; i++) {
                var node = nodes[node_idx];
                var next_idx = node.children_idx;

                if (node.children_count > 2 && nodes[node.children_idx + 1].header) {
                    // Here we deal with jump lists (mainly D-xx and D-xxxx)
                    for (var j = 1; j < node.children_count; j++) {
                        var children = recurseNodes(node.children_idx + j, next_idx);

                        var pseudo_idx = (j > 1) ? ('' + node_idx + '-' + (j - 1)) : node_idx;
                        var pseudo_text = node.text + ' ' + nodes[node.children_idx + j].header;
                        var li = createNodeLi(pseudo_idx, pseudo_text, children.tagName === 'UL');

                        appendChildren(li, children);
                        ul.appendChild(li);
                    }

                    node_idx = node.children_idx;
                } else if (node.children_count === 2 && node.reverse) {
                    next_idx++;
                    var children = recurseNodes(node.children_idx, next_idx);

                    var li = createNodeLi(node_idx, node.reverse, children.tagName === 'UL');

                    appendChildren(li, children);
                    ul.appendChild(li);
                } else if (node.children_count === 2) {
                    var children = recurseNodes(node.children_idx + 1, next_idx);

                    var li = createNodeLi(node_idx, node.text, children.tagName === 'UL');

                    // Simplify OR GOTO chains
                    while (next_idx && nodes[next_idx].children_count === 2 &&
                           nodes[nodes[next_idx].children_idx + 1].goto_idx === node.children_idx + 1) {
                        li.appendChild(createElement('br'));

                        var li2 = createNodeLi(next_idx, nodes[next_idx].text, true);
                        appendChildren(li, li2.childNodes);

                        next_idx = nodes[next_idx].children_idx;
                    }

                    appendChildren(li, children);
                    ul.appendChild(li);
                } else if (next_idx || node.goto_idx !== parent_next_idx) {
                    // The test above hides GOTO nodes at the end of chains that
                    // the classifier uses to jump back to go back one level.
                    var li = createNodeLi(node_idx, node.text,
                                          node.children_count && node.children_count > 1);
                    ul.appendChild(li);

                    for (var j = 1; j < node.children_count; j++) {
                        var children = recurseNodes(node.children_idx + j, next_idx);
                        appendChildren(li, children);
                    }
                }

                node_idx = next_idx;
            }

            // Simplify when there is only one leaf children
            if (ul.querySelectorAll('.n').length == 1) {
                var ul = Array.prototype.slice.call(ul.querySelector('li > span').childNodes);
                ul.unshift(' → ');
            }

            return ul;
        }

        if (nodes.length) {
            var ul = recurseNodes(0);
        } else {
            var ul = createElement('ul', {});
        }

        // Make sure the corresponding node is visible
        if (url_hash && url_hash.match(/^n[0-9]+$/)) {
            var el = ul.querySelector('#' + url_hash);
            while (el && el !== ul) {
                if (el.tagName === 'LI')
                    el.classList.remove('collapse');
                el = el.parentNode;
            }
        }

        var old_ul = document.querySelector('#tables_tree');
        cloneAttributes(old_ul, ul);
        old_ul.parentNode.replaceChild(ul, old_ul);
    }

    function refreshTable(items, columns)
    {
        var table = createElement('table', {},
            createElement('thead'),
            createElement('tbody')
        );
        var thead = table.querySelector('thead');
        var tbody = table.querySelector('tbody');

        if (items.length) {
            var tr = createElement('tr');
            for (var i = 0; i < columns.length; i++) {
                var th = createElement('th', {title: columns[i]}, columns[i]);
                tr.appendChild(th);
            }
            thead.appendChild(tr);

            for (var i = 0; i < items.length; i++) {
                var item = items[i];

                var tr = createElement('tr');
                for (var j = 0; j < columns.length; j++) {
                    var column = columns[j];

                    if (item[column] !== null && item[column] !== undefined) {
                        // FIXME: Put this meta-info in TableColumns
                        if (column === 'durations' || column === 'ages') {
                            var content = maskToRanges(item[column]);
                        } else if (Array.isArray(item[column])) {
                            var content = addSpecLinks(item[column].join(', '));
                        } else {
                            var content = addSpecLinks('' + item[column]);
                        }
                        var td = createElement('td', {}, content);
                    } else {
                        var td = createElement('td', {});
                    }
                    tr.appendChild(td);
                }
                tbody.appendChild(tr);
            }
        }

        var old_table = document.querySelector('#tables_table');
        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
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
        var page;
        var klass = null;
        if (str[0] === 'A') {
            page = 'tables/procedures/' + target_date + '/' + str;
        } else if (str[0] === 'D') {
            page = 'tables/diagnoses/' + target_date + '/' + str;
        } else if (str.match(/^[0-9]{2}[CMZK][0-9]{2}[ZJT0-9ABCDE]?$/)) {
            page = 'pricing/table/' + target_date + '/' + str.substr(0, 5);
            klass = 'ghm';
        } else if (str.match(/noeud [0-9]+/)) {
            page = 'tables/classifier_tree/' + target_date + '#n' + str.substr(6);
        } else {
            return str;
        }
        var click_function = function(e) {
            switchPage(page);
            e.preventDefault();
        };

        var link = createElement('a', {href: page, class: klass,
                                       click: click_function}, str);
        return link;
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
}).call(tables);
