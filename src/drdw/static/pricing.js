var pricing = {};
(function() {
    var update_state = 'indexes';
    var data_error = null;

    // Reset in updateIndexes()
    var indexes;
    var target_index;
    var current_index;
    var ghm_roots_map;

    var chart = null;

    function run()
    {
        switch (update_state) {
            case 'indexes': {
                update_state = 'busy';
                updateIndexes(function() {
                    target_index = indexes.length - 1;
                    update_state = 'view';
                    run();
                });
            } break;

            case 'view': {
                var index = target_index;
                if (current_index !== index) {
                    refreshIndexes(index);

                    markOutdated('#pricing_view', true);
                    if (!indexes[index].loaded) {
                        update_state = 'busy';
                        updatePriceMap(index, function() {
                            current_index = index;
                            refreshGhmRoots();
                            update_state = 'view';

                            run();
                        });
                    } else {
                        current_index = index;
                        refreshGhmRoots();
                        run();
                    }
                } else {
                    refreshView();
                    markOutdated('#pricing_view', false);
                }
            } break;

            case 'busy': { /* Working */ } break;
        }
    }
    this.run = run;

    function prevIndex()
    {
        if (target_index <= 0)
            return;
        target_index--;
        run();
    }
    this.prevIndex = prevIndex;

    function nextIndex()
    {
        if (target_index >= indexes.length - 1)
            return;
        target_index++;
        run();
    }
    this.nextIndex = nextIndex;

    function updateIndexes(func)
    {
        indexes = [];
        target_index = -1;
        current_index = -1;
        ghm_roots_map = {};
        ghm_roots = [];

        downloadJson('get', 'api/indexes.json', {}, function(status, json) {
            data_error = null;
            switch (status) {
                case 200: {
                    if (json.length > 0) {
                        indexes = json;
                        for (var i = 0; i < indexes.length; i++)
                            indexes[i].loaded = false;
                    } else {
                        data_error = 'Aucune table disponible';
                    }
                } break;

                case 503: { data_error = 'Service non accessible'; } break;
                case 504: { data_error = 'Délai d\'attente dépassé, réessayez'; } break;
                default: { data_error = 'Erreur inconnue ' + status; } break;
            }

            func();
        });
    }

    function updatePriceMap(index, func)
    {
        var begin_date = indexes[index].begin_date;
        downloadJson('get', 'api/price_map.json?date=' + begin_date, {}, function(status, json) {
            ghm_roots = [];

            data_error = null;
            switch (status) {
                case 200: {
                    if (json.length > 0) {
                        for (var i = 0; i < json.length; i++) {
                            var ghm_root_info = ghm_roots_map[json[i].ghm_root];
                            if (ghm_root_info === undefined) {
                                ghm_root_info = Array.apply(null, Array(indexes.length));
                                ghm_roots_map[json[i].ghm_root] = ghm_root_info;
                            }
                            ghm_root_info[index] = json[i];
                        }
                        indexes[index].loaded = true;
                    } else {
                        data_error = 'Aucune racine de GHM dans cette table';
                    }
                } break;

                case 404: { data_error = 'Aucune table disponible à cette date'; } break;
                case 503: { data_error = 'Service non accessible'; } break;
                case 504: { data_error = 'Délai d\'attente dépassé, réessayez'; } break;
                default: { data_error = 'Erreur inconnue ' + status; } break;
            }

            func();
        });
    }

    function refreshView()
    {
        var ghm_root_info = ghm_roots_map[document.querySelector('#pricing_ghm_roots').value];
        var merge_cells = document.querySelector('#pricing_merge_cells').checked;
        var max_duration = parseInt(document.querySelector('#pricing_max_duration').value) + 1;

        var h1 = document.querySelector('#pricing_menu > h1');
        var log = document.querySelector('#pricing .log');
        var old_table = document.querySelector('#pricing_table');
        var chart_ctx = document.querySelector('#pricing_chart').getContext('2d');

        if (ghm_root_info[current_index]) {
            log.style.display = 'none';
            h1.innerText = ghm_root_info[current_index].ghm_root + ' : ' + ghm_root_info[current_index].ghm_root_desc;

            if (document.querySelector('.page_pricing_table').classList.contains('active')) {
                var table = createTable(ghm_root_info, current_index, merge_cells, max_duration);
                cloneAttributes(old_table, table);
                old_table.parentNode.replaceChild(table, old_table);
            }

            if (document.querySelector('.page_pricing_chart').classList.contains('active')) {
                chart = refreshChart(chart, chart_ctx, ghm_root_info, current_index, max_duration);
            }
        } else {
            log.style.display = 'block';
            log.textContent = data_error;
            h1.innerText = '';

            var table = createElement('table');
            cloneAttributes(old_table, table);
            old_table.parentNode.replaceChild(table, old_table);

            if (chart) {
                chart.destroy();
                chart = null;
            }
        }
    }

    function refreshIndexes(view_index)
    {
        var g = createElementNS('svg', 'g', {});

        if (indexes.length >= 2) {
            var first_date = new Date(indexes[0].begin_date);
            var last_date = new Date(indexes[indexes.length - 1].begin_date);
            var max_delta = last_date - first_date;

            var text_above = true;
            for (var i = 0; i < indexes.length; i++) {
                var date = new Date(indexes[i].begin_date);

                var x = (9.0 + (date - first_date) / max_delta * 82.0).toFixed(1) + '%';
                var radius = indexes[i].changed_prices ? 5 : 4;
                if (i == view_index) {
                    var color = '#ff8900';
                } else if (indexes[i].changed_prices) {
                    var color = '#000';
                } else {
                    var color = '#888';
                }
                var click_function = (function() {
                    var index = i;
                    return (function(e) {
                        target_index = index;
                        run();
                    });
                })();

                var node = createElementNS('svg', 'circle',
                                           {cx: x, cy: 20, r: radius, fill: color,
                                            style: 'cursor: pointer;'},
                    createElementNS('svg', 'title', {}, indexes[i].begin_date)
                );
                node.addEventListener('click', click_function);
                g.appendChild(node);

                if (indexes[i].changed_prices) {
                    var text_y = text_above ? 10 : 40;
                    text_above = !text_above;

                    var text = createElementNS('svg', 'text',
                                               {x: x, y: text_y, 'text-anchor': 'middle', fill: color,
                                                style: 'cursor: pointer;'}, indexes[i].begin_date);
                    text.addEventListener('click', click_function);
                    g.appendChild(text);
                }
            }
        }

        var old_g = document.querySelector('#pricing_indexes > g');
        old_g.parentNode.replaceChild(g, old_g);
    }

    function refreshGhmRoots()
    {
        var el = document.querySelector('#pricing_ghm_roots');
        var previous_value = el.value;
        el.innerHTML = '';

        var ghm_roots = Object.keys(ghm_roots_map).sort();
        for (var i = 0; i < ghm_roots.length; i++) {
            var opt = document.createElement('option');
            opt.setAttribute('value', ghm_roots[i]);
            opt.textContent = ghm_roots[i] + (ghm_roots_map[ghm_roots[i]][current_index] ? '' : ' (?)');
            el.appendChild(opt);
        }
        if (previous_value)
            el.value = previous_value;
    }

    function refreshChart(chart, chart_ctx, ghm_root_info, index, max_duration)
    {
        var ghs = ghm_root_info[index].ghs;

        function ghsLabel(ghs)
        {
            return '' + ghs.ghs + (ghs.conditions.length ? '*' : '') + ' (' + ghs.ghm + ')';
        }

        function modeToColor(mode)
        {
            return {
                "J": "#fdae6b",
                "T": "#fdae6b",
                "1": "#fd8d3c",
                "2": "#f16913",
                "3": "#d94801",
                "4": "#a63603",
                "A": "#fd8d3c",
                "B": "#f16913",
                "C": "#d94801",
                "D": "#a63603",
                "E": "#7f2704",
                "Z": "#9a9a9a"
            }[mode] || "black";
        }

        data = {
            labels: [],
            datasets: []
        };

        for (var i = 0; i < max_duration; i++)
            data.labels.push(durationText(i));

        for (var i = 0; i < ghs.length; i++) {
            var dataset = {
                label: ghsLabel(ghs[i]),
                data: [],
                borderColor: modeToColor(ghs[i].ghm_mode),
                backgroundColor: modeToColor(ghs[i].ghm_mode),
                borderDash: (ghs[i].conditions.length ? [5, 5] : undefined),
                fill: false
            };
            for (var duration = 0; duration < max_duration; duration++) {
                p = computePrice(ghs[i], duration);
                if (p !== null) {
                    dataset.data.push({
                        x: durationText(duration),
                        y: p[0] / 100
                    });
                } else {
                    dataset.data.push(null);
                }
            }
            data.datasets.push(dataset);
        }

        // Calculate maximum price across all (loaded) indexes to stabilize Y axis
        var max_price = 0.0;
        for (var i = 0; i < ghm_root_info.length; i++) {
            if (!ghm_root_info[i])
                continue;

            for (var j = 0; j < ghm_root_info[i].ghs.length; j++) {
                p = computePrice(ghm_root_info[i].ghs[j], max_duration - 1);
                if (p && p[0] > max_price)
                    max_price = p[0];
            }
        }
        max_price /= 100;

        if (chart) {
            chart.data = data;
            chart.options.scales.yAxes[0].ticks.suggestedMin = 0;
            chart.options.scales.yAxes[0].ticks.suggestedMax = max_price;
            chart.update({duration: 0});
        } else {
            chart = new Chart(chart_ctx, {
                type: 'line',
                data: data,
                options: {
                    responsive: true,
                    tooltips: {mode: 'index', intersect: false},
                    hover: {mode: 'x', intersect: true},
                    elements: {
                        line: {tension: 0},
                        point: {radius: 0, hitRadius: 0}
                    },
                    scales: {
                        yAxes: [
                            {ticks: {suggestedMin: 0, suggestedMax: max_price}}
                        ]
                    }
                },
            });
        }

        return chart;
    }

    function createTable(ghm_root_info, index, merge_cells, max_duration)
    {
        var ghs = ghm_root_info[index].ghs;

        if (merge_cells === undefined)
            merge_cells = true;
        if (max_duration === undefined)
            max_duration = 200;

        function appendRow(parent, name, func)
        {
            var tr =
                createElement('tr', {},
                    createElement('th', {}, name)
                );

            var prev_cell = [document.createTextNode(''), null, false];
            var prev_td = null;
            for (var i = 0; i < ghs.length; i++) {
                var cell = func(ghs[i]) || [null, null, false];
                if (cell[0] === null) {
                    cell[0] = document.createTextNode('');
                } else if (typeof cell[0] === 'string') {
                    cell[0] = document.createTextNode(cell[0]);
                }
                if (merge_cells && cell[2] && cell[0].isEqualNode(prev_cell[0]) && cell[1] == prev_cell[1]) {
                    var colspan = parseInt(prev_td.getAttribute('colspan') || 1);
                    prev_td.setAttribute('colspan', colspan + 1);
                } else {
                    prev_td = tr.appendChild(createElement('td', {class: cell[1]}, cell[0]));
                }
                prev_cell = cell;
            }

            parent.appendChild(tr);
        }

        var table =
            createElement('table', {},
                createElement('thead'),
                createElement('tbody')
            );
        var thead = table.querySelector('thead');
        var tbody = table.querySelector('tbody');

        appendRow(thead, 'GHS', function(col) { return ['GHS ' + col.ghs, 'desc', true]; });
        appendRow(thead, 'GHM', function(col) { return [col.ghm, 'desc', true]; });
        appendRow(thead, 'Niveau', function(col) { return ['Niveau ' + col.ghm_mode, 'desc', true]; });
        appendRow(thead, 'Conditions', function(col) {
            var el =
                createElement('div', {title: col.conditions.join('\n')},
                    col.conditions.length ? col.conditions.length.toString() : ''
                );
            return [el, 'conditions', true];
        });
        appendRow(thead, 'Borne basse', function(col) { return [durationText(col.exb_treshold), 'exb', true]; });
        appendRow(thead, 'Borne haute',
                  function(col) { return [durationText(col.exh_treshold && col.exh_treshold - 1), 'exh', true]; });
        appendRow(thead, 'Tarif €', function(col) { return [priceText(col.price_cents), 'price', true]; });
        appendRow(thead, 'Forfait EXB €',
                  function(col) { return [col.exb_once ? priceText(col.exb_cents) : '', 'exb', true]; });
        appendRow(thead, 'Tarif EXB €',
                  function(col) { return [!col.exb_once ? priceText(col.exb_cents) : '', 'exb', true]; });
        appendRow(thead, 'Tarif EXH €', function(col) { return [priceText(col.exh_cents), 'exh', true]; });
        appendRow(thead, 'Age', function(col) {
            var texts = [];
            if (col.ghm_mode >= '1' && col.ghm_mode < '5') {
                var severity = col.ghm_mode.charCodeAt(0) - '1'.charCodeAt(0);
                if (severity < col.young_severity_limit)
                    texts.push('< ' + col.young_age_treshold.toString());
                if (severity < col.old_severity_limit)
                    texts.push('≥ ' + col.old_age_treshold.toString());
            }
            return [texts.join(', '), 'age', true];
        });

        for (var duration = 0; duration < max_duration; duration++) {
            appendRow(tbody, durationText(duration), function(col) {
                info = computePrice(col, duration);
                if (info === null)
                    return null;
                info[0] = priceText(info[0]);
                info.push(false);
                return info;
            });
        }

        return table;
    }

    function computePrice(col, duration)
    {
        var duration_mask_test;
        if (duration < 32) {
            duration_mask_test = 1 << duration;
        } else {
            duration_mask_test = 1 << 31;
        }
        if (!(col.duration_mask & duration_mask_test))
            return null;

        if (col.exb_treshold && duration < col.exb_treshold) {
            var price = col.price_cents;
            if (col.exb_once) {
                price -= col.exb_cents;
            } else {
                price -= (col.exb_treshold - duration) * col.exb_cents;
            }
            return [price, 'exb'];
        }
        if (col.exh_treshold && duration >= col.exh_treshold) {
            var price = col.price_cents + (duration - col.exh_treshold + 1) * col.exh_cents;
            return [price, 'exh'];
        }
        return [col.price_cents, 'price'];
    }

    function durationText(duration)
    {
        if (duration !== undefined) {
            return duration.toString() + (duration >= 2 ? ' nuits' : ' nuit');
        } else {
            return '';
        }
    }

    function priceText(price_cents)
    {
        if (price_cents !== undefined) {
            // The replace() is a bit dirty, but it gets the job done
            return (price_cents / 100.0).toFixed(2).replace('.', ',');
        } else {
            return '';
        }
    }
}).call(pricing);
