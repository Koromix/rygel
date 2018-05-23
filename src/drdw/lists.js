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
            h1.innerText = 'Filtre : ' + target_specs[target_table];
        } else {
            h1.innerText = '';
        }
    }

    function route(args)
    {
        if (args !== undefined) {
            target_date = args.date || target_date;
            target_table = args.table || target_table;
            if (args.spec)
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
        function recurseNodes(idx, ignore_header)
        {
            var ul = createElement('ul');
            while (idx < nodes.length) {
                var click_function = function(e) {
                    // FIXME: this.classList.toggle('collapse');
                };

                if (nodes[idx].header && !ignore_header) {
                    var li = createElement('li', {click: click_function},
                                           addSpecLinks(nodes[idx].header));
                    var child_ul = recurseNodes(idx, true);
                    li.appendChild(child_ul);
                    ul.appendChild(li);

                    break;
                } else {
                    var li = createElement('li', {click: click_function},
                                           addSpecLinks(nodes[idx].text));

                    if (nodes[idx].children_idx) {
                        for (var i = 1; i < nodes[idx].children_count; i++) {
                            var child_ul = recurseNodes(nodes[idx].children_idx + i);
                            li.appendChild(child_ul);
                        }
                        ul.appendChild(li);

                        idx = nodes[idx].children_idx;
                    } else {
                        ul.appendChild(li);

                        break;
                    }
                }
            }

            return ul;
        }

        var ul = recurseNodes(0, false);

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
        if (str[0] === 'A') {
            page = 'tables/procedures/' + target_date + '/' + str;
        } else if (str[0] === 'D') {
            page = 'tables/diagnoses/' + target_date + '/' + str;
        } else {
            return str;
        }
        var click_function = function(e) {
            switchPage(page);
            e.preventDefault();
        };

        var link = createElement('a', {href: page,
                                       click: click_function}, str);
        return link;
    }
    this.makeSpecLink = makeSpecLink;

    function addSpecLinks(str)
    {
        var elements = [];
        for (;;) {
            var m = str.match(/[AD](\-[0-9]+|\$[0-9]+\.[0-9]+)/);
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
