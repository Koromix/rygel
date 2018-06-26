var pricing = {};
(function() {
    // Cache
    var indexes = [];
    var ghm_roots = [];
    var ghm_roots_map = {};
    var pricings_map = {};
    this.pricings_map = pricings_map;

    var chart = null;

    function run(route, url, parameters, hash)
    {
        let errors = new Set(downloadJson.errors);

        // Parse route (model: pricing/<view>/[<diff>..]<date>/<ghm_root>/[coeff])
        let url_parts = url.split('/');
        route.view = url_parts[1] || route.view;
        if (url_parts[2]) {
            let date_parts = url_parts[2].split('..', 2);
            if (date_parts.length === 2) {
                route.date = date_parts[1];
                route.diff = date_parts[0];
            } else {
                route.date = date_parts[0];
                route.diff = null;
            }
        }
        route.ghm_root = url_parts[3] || route.ghm_root;
        route.apply_coefficient = parameters.apply_coefficient || false;

        // Resources
        indexes = getIndexes();
        updateGhmRoots();
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        if (!route.ghm_root && ghm_roots.length)
            route.ghm_root = ghm_roots[0].ghm_root;
        let main_index = indexes.findIndex(function(info) { return info.begin_date === route.date; });
        let diff_index = indexes.findIndex(function(info) { return info.begin_date === route.diff; });
        if (main_index >= 0 && !indexes[main_index].init)
            updatePriceMap(main_index);
        if (diff_index >= 0 && !indexes[diff_index].init)
            updatePriceMap(diff_index);

        // Errors
        if (route.view !== 'chart' && route.view !== 'table')
            errors.add('Mode d\'affichage incorrect');
        if (route.date !== null && indexes.length && main_index < 0)
            errors.add('Date incorrecte');
        if (route.diff !== null && indexes.length && diff_index < 0)
            errors.add('Date de comparaison incorrecte');
        if (route.ghm_root !== null && ghm_roots.length) {
            if (!ghm_roots_map[route.ghm_root]) {
                errors.add('Racine de GHM inconnue');
            } else {
                if (!checkIndexGhmRoot(main_index, route.ghm_root))
                    errors.add('Cette racine n\'existe pas dans la version \'' + indexes[main_index].begin_date + '\'');
                if (!checkIndexGhmRoot(diff_index, route.ghm_root))
                    errors.add('Cette racine n\'existe pas dans la version \'' + indexes[diff_index].begin_date + '\'');
            }
        }

        // Refresh settings
        removeClass(__('#opt_indexes, #opt_ghm_roots, #opt_diff_indexes, #opt_max_duration, #opt_apply_coefficient'), 'hide');
        refreshIndexesLine(_('#opt_indexes'), indexes, main_index);
        refreshGhmRoots(main_index, route.ghm_root);
        refreshIndexesDiff(diff_index, route.ghm_root);
        _('#opt_apply_coefficient').checked = route.apply_coefficient;

        // Refresh view
        if (!downloadJson.busy) {
            refreshErrors(Array.from(errors));
            downloadJson.errors = [];

            let ghm_root_info = ghm_roots_map[route.ghm_root];
            let pricing_info = pricings_map[route.ghm_root];
            let max_duration = parseInt(_('#opt_max_duration > input').value);

            switch (route.view) {
                case 'table': {
                    refreshPriceTable(pricing_info, main_index, diff_index, max_duration,
                                      route.apply_coefficient, true);
                } break;
                case 'chart': {
                    let chart_ctx = _('#pr_chart').getContext('2d');
                    chart = refreshChart(chart, chart_ctx, pricing_info, main_index,
                                         diff_index, max_duration, route.apply_coefficient);
                } break;
            }

            _('#pr_table').classList.toggle('hide', route.view !== 'table');
            _('#pr_chart').classList.toggle('hide', route.view !== 'chart');
            _('#pr').classList.remove('hide');
        }
        markBusy('#pr', downloadJson.busy);
    }
    this.run = run;

    function routeToUrl(args)
    {
        let new_route = buildRoute(args);

        let date = (new_route.diff ? new_route.diff + '..' : '') + (new_route.date || '');
        let url_parts = [buildModuleUrl('pricing'), new_route.view, date, new_route.ghm_root];
        while (!url_parts[url_parts.length - 1])
            url_parts.pop();
        let url = url_parts.join('/');
        if (new_route.apply_coefficient)
            url += '?apply_coefficient=1';

        return url;
    }
    this.routeToUrl = routeToUrl;

    function route(args)
    {
        go(routeToUrl(args));
    }
    this.route = route;

    // A true result actually means maybe (if we haven't download the relevant index yet)
    function checkIndexGhmRoot(index, ghm_root)
    {
        return index < 0 ||
               !indexes[index].init ||
               (pricings_map[ghm_root] && pricings_map[ghm_root][index]);
    }

    function updateGhmRoots()
    {
        if (!ghm_roots.length)
            [ghm_roots, ghm_roots_map] = getConcepts('ghm_roots');
        return [ghm_roots, ghm_roots_map];
    }
    this.updateGhmRoots = updateGhmRoots;

    function updatePriceMap(index)
    {
        let indexes = getIndexes();
        if (indexes[index].init)
            return;

        let begin_date = indexes[index].begin_date;
        let url = buildUrl(BaseUrl + 'api/ghm_ghs.json', {date: begin_date});
        downloadJson(url, function(json) {
            for (var i = 0; i < json.length; i++) {
                var ghm_root = json[i].ghm_root;
                var ghm_ghs = json[i];

                var pricing_info = pricings_map[ghm_root];
                if (pricing_info === undefined) {
                    pricing_info = Array.apply(null, Array(indexes.length));
                    pricings_map[ghm_root] = pricing_info;
                }

                if (pricing_info[index] === undefined) {
                    pricing_info[index] = {
                        'ghm_root': ghm_ghs.ghm_root,
                        'ghs': [],
                        'ghs_map': {}
                    };
                }
                pricing_info[index].ghs.push(ghm_ghs);
                pricing_info[index].ghs_map[ghm_ghs.ghs] = ghm_ghs;
            }
            indexes[index].init = true;
        });
    }
    this.updatePriceMap = updatePriceMap;

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
        var el = _("#opt_diff_indexes > select");
        el.innerHTML = '';

        el.appendChild(createElement('option', {value: ''}, 'Désactiver'));
        for (var i = 0; i < indexes.length; i++) {
            var opt = createElement('option', {value: indexes[i].begin_date},
                                    indexes[i].begin_date);
            if (i === diff_index)
                opt.setAttribute('selected', '');
            if (!checkIndexGhmRoot(i, test_ghm_root)) {
                opt.setAttribute('disabled', '');
                opt.text += '*';
            }
            el.appendChild(opt);
        }
    }

    function refreshGhmRoots(index, select_ghm_root)
    {
        var el = _('#opt_ghm_roots');
        el.innerHTML = '';

        for (var i = 0; i < ghm_roots.length; i++) {
            var ghm_root_info = ghm_roots[i];

            var opt = createElement('option', {value: ghm_root_info.ghm_root},
                                    ghm_root_info.ghm_root + ' – ' + ghm_root_info.desc);
            if (!checkIndexGhmRoot(index, ghm_root_info.ghm_root)) {
                opt.setAttribute('disabled', '');
                opt.text += '*';
            }

            el.appendChild(opt);
        }
        if (select_ghm_root)
            el.value = select_ghm_root;
    }

    function refreshPriceTable(pricing_info, main_index, diff_index, max_duration,
                               apply_coeff, merge_cells)
    {
        let table = null;
        if (pricing_info && pricing_info[main_index]) {
            table = createTable(pricing_info, main_index, diff_index, max_duration, apply_coeff,
                                merge_cells, function(col, duration) {
                if (diff_index < 0) {
                    var info = computePrice(col, duration, apply_coeff);
                } else {
                    var info = computePriceDelta(col, pricing_info[diff_index].ghs_map[col.ghs],
                                                 duration, apply_coeff);
                }
                if (info === null)
                    return null;

                var props = [priceText(info[0]), {class: info[1]}, false];
                if (duration == 0 && col.warn_cmd28) {
                    props[1].class += ' warn';
                    props[1].title = 'Devrait être orienté dans la CMD 28 (séance)';
                }
                return props;
            });
        } else {
            table = createElement('table');
        }

        let old_table = _('#pr_table');
        cloneAttributes(old_table, table);
        old_table.parentNode.replaceChild(table, old_table);
    }

    function refreshChart(chart, chart_ctx, pricing_info, main_index, diff_index,
                          max_duration, apply_coeff)
    {
        if (!pricing_info || !pricing_info[main_index]) {
            if (chart) {
                chart.destroy();
                chart = null;
            }

            return;
        }

        var ghs = pricing_info[main_index].ghs;

        function ghsLabel(ghs, conditions)
        {
            return '' + ghs.ghs + (conditions.length ? '*' : '') + ' (' + ghs.ghm + ')';
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

        var data = {
            labels: [],
            datasets: []
        };
        for (var i = 0; i < max_duration; i++)
            data.labels.push(durationText(i));

        var max_price = 0.0;
        for (var i = 0; i < ghs.length; i++) {
            var conditions = buildConditionsArray(ghs[i]);

            var dataset = {
                label: ghsLabel(ghs[i], conditions),
                data: [],
                borderColor: modeToColor(ghs[i].ghm.substr(5, 1)),
                backgroundColor: modeToColor(ghs[i].ghm.substr(5, 1)),
                borderDash: (conditions.length ? [5, 5] : undefined),
                fill: false
            };
            for (var duration = 0; duration < max_duration; duration++) {
                if (diff_index < 0) {
                    var info = computePrice(ghs[i], duration, apply_coeff);
                } else {
                    var info = computePriceDelta(ghs[i], pricing_info[diff_index].ghs_map[ghs[i].ghs],
                                                 duration, apply_coeff);
                }

                if (info !== null) {
                    dataset.data.push({
                        x: durationText(duration),
                        y: info[0] / 100
                    });
                    max_price = Math.max(max_price, Math.abs(info[0]));
                } else {
                    dataset.data.push(null);
                }
            }
            data.datasets.push(dataset);
        }

        var min_price;
        if (diff_index < 0) {
            min_price = 0.0;

            // Recalculate maximum price across all (loaded) indexes to stabilize Y axis
            for (var i = 0; i < pricing_info.length; i++) {
                if (i === main_index || !pricing_info[i])
                    continue;

                for (var j = 0; j < pricing_info[i].ghs.length; j++) {
                    p = computePrice(pricing_info[i].ghs[j], max_duration - 1, apply_coeff);
                    if (p && p[0] > max_price)
                        max_price = p[0];
                }
            }
            max_price /= 100.0;
        } else {
            max_price /= 100.0;
            min_price = -max_price;
        }

        if (chart) {
            chart.data = data;
            chart.options.scales.yAxes[0].ticks.suggestedMin = min_price;
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
                            {ticks: {suggestedMin: min_price, suggestedMax: max_price}}
                        ]
                    }
                },
            });
        }

        return chart;
    }

    function createTable(pricing_info, main_index, diff_index, max_duration,
                         apply_coeff, merge_cells, row_func)
    {
        var ghs = pricing_info[main_index].ghs;

        if (max_duration === undefined)
            max_duration = 200;
        if (merge_cells === undefined)
            merge_cells = true;

        function appendRow(parent, name, func)
        {
            var tr =
                createElement('tr', {},
                    createElement('th', {}, name)
                );

            var prev_cell = [document.createTextNode(''), {}, false];
            var prev_td = null;
            for (var i = 0; i < ghs.length; i++) {
                var cell = func(ghs[i], i) || [null, {}, false];
                if (cell[0] === null) {
                    cell[0] = document.createTextNode('');
                } else if (typeof cell[0] === 'string') {
                    cell[0] = document.createTextNode(cell[0]);
                }
                if (merge_cells && cell[2] && cell[0].isEqualNode(prev_cell[0]) &&
                        cell[1].class === prev_cell[1].class) {
                    var colspan = parseInt(prev_td.getAttribute('colspan') || 1);
                    prev_td.setAttribute('colspan', colspan + 1);
                } else {
                    prev_td = tr.appendChild(createElement('td', cell[1], cell[0]));
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

        thead.appendChild(createElement('tr', {},
            createElement('td', {colspan: 1 + ghs.length, class: 'ghm_root'},
                          pricing_info[main_index].ghm_root + ' - ' +
                          (ghm_roots_map[pricing_info[main_index].ghm_root].desc || ''))
        ));

        appendRow(thead, 'GHS', function(col) { return ['' + col.ghs, {class: 'desc'}, true]; });
        appendRow(thead, 'GHM', function(col) { return [col.ghm, {class: 'desc'}, true]; });
        appendRow(thead, 'Niveau', function(col) { return ['Niveau ' + col.ghm.substr(5, 1), {class: 'desc'}, true]; });
        appendRow(thead, 'Conditions', function(col) {
            var conditions = buildConditionsArray(col);

            var el =
                createElement('div', {title: conditions.join('\n')},
                    conditions.length ? conditions.length.toString() : ''
                );
            return [el, {class: 'conditions'}, true];
        });
        appendRow(thead, 'Borne basse', function(col) { return [durationText(col.exb_treshold), {class: 'exb'}, true]; });
        appendRow(thead, 'Borne haute',
                  function(col) { return [durationText(col.exh_treshold && col.exh_treshold - 1), {class: 'exh'}, true]; });
        appendRow(thead, 'Tarif €', function(col) {
            var cents = applyGhsCoefficient(col, col.ghs_cents, apply_coeff);
            return [priceText(cents), {class: 'price'}, true];
        });
        appendRow(thead, 'Forfait EXB €', function(col) {
            var cents = applyGhsCoefficient(col, col.exb_cents, apply_coeff);
            return [col.exb_once ? priceText(cents) : '', {class: 'exb'}, true];
        });
        appendRow(thead, 'Tarif EXB €', function(col) {
            var cents = applyGhsCoefficient(col, col.exb_cents, apply_coeff);
            return [!col.exb_once ? priceText(cents) : '', {class: 'exb'}, true];
        });
        appendRow(thead, 'Tarif EXH €', function(col) {
            var cents = applyGhsCoefficient(col, col.exh_cents, apply_coeff);
            return [priceText(cents), {class: 'exh'}, true];
        });
        appendRow(thead, 'Age', function(col) {
            var texts = [];
            var severity = col.ghm.charCodeAt(5) - '1'.charCodeAt(0);
            if (severity >= 0 && severity < 4) {
                if (severity < col.young_severity_limit)
                    texts.push('< ' + col.young_age_treshold.toString());
                if (severity < col.old_severity_limit)
                    texts.push('≥ ' + col.old_age_treshold.toString());
            }
            return [texts.join(', '), {class: 'age'}, true];
        });

        for (var duration = 0; duration < max_duration; duration++) {
            if (duration % 10 == 0) {
                let text = '' + duration + ' - ' + durationText(duration + 9);
                var tr = createElement('tr', {},
                    createElement('th', {class: 'repeat', colspan: ghs.length}, text)
                );
                tbody.appendChild(tr);
            }

            appendRow(tbody, durationText(duration),
                      function(col, i) { return row_func(col, duration); });
        }

        return table;
    }
    this.createTable = createTable;

    function computePrice(ghs, duration, apply_coeff)
    {
        if (!ghs.ghs_cents)
            return null;

        var duration_mask;
        if (duration < 32) {
            duration_mask = 1 << duration;
        } else {
            duration_mask = 1 << 31;
        }
        if (!(ghs.durations & duration_mask))
            return null;

        if (ghs.exb_treshold && duration < ghs.exb_treshold) {
            var price_cents = ghs.ghs_cents;
            if (ghs.exb_once) {
                price_cents -= ghs.exb_cents;
            } else {
                price_cents -= (ghs.exb_treshold - duration) * ghs.exb_cents;
            }
            var mode = 'exb';
        } else if (ghs.exh_treshold && duration >= ghs.exh_treshold) {
            var price_cents = ghs.ghs_cents + (duration - ghs.exh_treshold + 1) * ghs.exh_cents;
            var mode = 'exh';
        } else {
            var price_cents = ghs.ghs_cents;
            var mode = 'price';
        }

        price_cents = applyGhsCoefficient(ghs, price_cents, apply_coeff);
        return [price_cents, mode];
    }

    function computePriceDelta(ghs, prev_ghs, duration, apply_coeff)
    {
        var p1 = ghs ? computePrice(ghs, duration, apply_coeff) : null;
        var p2 = prev_ghs ? computePrice(prev_ghs, duration, apply_coeff) : null;

        if (p1 !== null && p2 !== null) {
            var delta = p1[0] - p2[0];
            if (delta < 0) {
                return [delta, 'lower'];
            } else if (delta > 0) {
                return [delta, 'higher'];
            } else {
                return [0, 'neutral'];
            }
        } else if (p1 !== null) {
            return [p1[0], 'added'];
        } else if (p2 !== null) {
            return [-p2[0], 'removed'];
        } else {
            return null;
        }
    }

    function applyGhsCoefficient(ghs, cents, apply_coeff)
    {
        return apply_coeff && cents ? (ghs.ghs_coefficient * cents) : cents;
    }

    function buildConditionsArray(ghs)
    {
        var conditions = [];

        if (ghs.unit_authorization)
            conditions.push('Autorisation Unité ' + ghs.unit_authorization);
        if (ghs.bed_authorization)
            conditions.push('Autorisation Lit ' + ghs.bed_authorization);
        if (ghs.minimum_duration)
            conditions.push('Durée ≥ ' + ghs.minimum_duration);
        if (ghs.minimum_age)
            conditions.push('Âge ≥ ' + ghs.minimum_age);
        if (ghs.main_diagnosis)
            conditions.push('DP ' + ghs.main_diagnosis);
        if (ghs.diagnoses)
            conditions.push('Diagnostic ' + ghs.diagnoses);
        if (ghs.procedures)
            conditions.push('Acte ' + ghs.procedures.join(', '));

        return conditions;
    }

    function durationText(duration)
    {
        if (duration !== undefined) {
            return duration.toString() + (duration >= 2 ? ' nuits' : ' nuit');
        } else {
            return '';
        }
    }
    this.durationText = durationText;

    function priceText(price_cents)
    {
        if (price_cents !== undefined) {
            // The replace() is a bit dirty, but it gets the job done
            return (price_cents / 100.0).toFixed(2).replace('.', ',');
        } else {
            return '';
        }
    }
    this.priceText = priceText;
}).call(pricing);
