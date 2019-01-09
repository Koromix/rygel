// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_pricing = {};
(function() {
    // Cache
    let ghm_roots = [];
    let ghm_roots_map = {};
    let available_dates = new Set;
    let pricings_map = {};

    // Chart.js
    let chart = null;

    function runPricing(route, path, parameters, hash, errors)
    {
        // Parse route (model: pricing/<view>/[<diff>..]<date>/<ghm_root>/<sector>)
        let path_parts = path.split('/');
        route.view = path_parts[1] || 'table';
        route.sector = path_parts[2] || 'public';
        if (path_parts[3]) {
            let date_parts = path_parts[3].split('..', 2);
            if (date_parts.length === 2) {
                route.date = date_parts[1];
                route.diff = date_parts[0];
            } else {
                route.date = date_parts[0];
                route.diff = null;
            }
        }
        route.ghm_root = path_parts[4] || null;
        route.apply_coefficient = !!parseInt(parameters.apply_coefficient) || false;

        // Resources
        let indexes = thop.updateMcoSettings().indexes;
        {
            let ret = catalog.update('mco_ghm_roots');
            ghm_roots = ret.concepts;
            ghm_roots_map = ret.map;
        }
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        if (!route.ghm_root && ghm_roots.length)
            route.ghm_root = ghm_roots[0].ghm_root;
        let main_index = indexes.findIndex(function(info) { return info.begin_date === route.date; });
        let diff_index = indexes.findIndex(function(info) { return info.begin_date === route.diff; });
        if (main_index >= 0)
            updatePriceMap(main_index, route.sector);
        if (diff_index >= 0)
            updatePriceMap(diff_index, route.sector);

        // Errors
        if (route.view !== 'chart' && route.view !== 'table')
            errors.add('Mode d\'affichage incorrect');
        if (route.date && indexes.length && main_index < 0)
            errors.add('Date incorrecte');
        if (route.diff && indexes.length && diff_index < 0)
            errors.add('Date de comparaison incorrecte');
        if (!['public', 'private'].includes(route.sector))
            errors.add('Secteur incorrect');
        if (route.ghm_root !== null && ghm_roots.length) {
            if (!ghm_roots_map[route.ghm_root]) {
                errors.add('Racine de GHM inconnue');
            } else {
                if (!checkIndexGhmRoot(indexes, main_index, route.sector, route.ghm_root))
                    errors.add('Cette racine n\'existe pas dans la version \'' + indexes[main_index].begin_date + '\'');
                if (!checkIndexGhmRoot(indexes, diff_index, route.sector, route.ghm_root))
                    errors.add('Cette racine n\'existe pas dans la version \'' + indexes[diff_index].begin_date + '\'');
            }
        }

        // Refresh settings
        queryAll('#opt_index, #opt_sector, #opt_ghm_root, #opt_diff_index, #opt_max_duration, #opt_apply_coefficient')
            .removeClass('hide');
        refreshIndexesLine(indexes, main_index);
        query('#opt_sector > select').value = route.sector;
        refreshGhmRootsMenu(indexes, main_index, route.sector, route.ghm_root);
        refreshDiffMenu(indexes, diff_index, route.sector, route.ghm_root);
        query('#opt_apply_coefficient > input').checked = route.apply_coefficient;

        // Refresh view
        if (!thop.isBusy()) {
            const ghm_root_info = ghm_roots_map[route.ghm_root];
            let pricing_info = pricings_map[route.ghm_root];
            if (pricing_info)
                pricing_info = pricing_info[route.sector];
            const max_duration = parseInt(query('#opt_max_duration > input').value);

            switch (route.view) {
                case 'table': {
                    refreshPriceTable(route.ghm_root, pricing_info, main_index, diff_index,
                                      max_duration, route.apply_coefficient, true);
                } break;

                case 'chart': {
                    refreshPriceChart(pricing_info, main_index, diff_index,
                                      max_duration, route.apply_coefficient);
                } break;
            }
        }

        query('#pr_table').toggleClass('hide', route.view !== 'table');
        query('#pr_chart').toggleClass('hide', route.view !== 'chart');
        query('#pr').removeClass('hide');
    }

    function routeToUrl(args)
    {
        let new_route = thop.buildRoute(args);

        let url;
        {
            let date = (new_route.diff ? new_route.diff + '..' : '') + (new_route.date || '');
            let url_parts = [thop.baseUrl('mco_pricing'), new_route.view, new_route.sector,
                             date, new_route.ghm_root];
            while (!url_parts[url_parts.length - 1])
                url_parts.pop();

            url = url_parts.join('/');
        }

        url = buildUrl(url, {
            apply_coefficient: new_route.apply_coefficient ? 1 : null
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

    // A true result actually means maybe (if we haven't download the relevant index yet)
    function checkIndexGhmRoot(indexes, index, sector, ghm_root)
    {
        if (index < 0)
            return true;

        const date_key = indexes[index].begin_date + '@' + sector;

        return !available_dates.has(date_key) ||
               (pricings_map[ghm_root] && pricings_map[ghm_root][sector] && pricings_map[ghm_root][sector][index]);
    }

    function updatePriceMap(index, sector)
    {
        const indexes = thop.updateMcoSettings().indexes;
        const date_key = indexes[index].begin_date + '@' + sector;

        if (!available_dates.has(date_key)) {
            let url = buildUrl(thop.baseUrl('api/mco_ghm_ghs.json'),
                               {date: indexes[index].begin_date, sector: sector});
            data.get(url, 'json', function(json) {
                for (let ghm_ghs of json) {
                    const ghm_root = ghm_ghs.ghm_root;

                    ghm_ghs.conditions = buildConditionsArray(ghm_ghs);

                    let pricing_info = pricings_map[ghm_root];
                    if (pricing_info === undefined) {
                        pricing_info = {
                            public: Array.apply(null, Array(indexes.length)),
                            private: Array.apply(null, Array(indexes.length))
                        };
                        pricings_map[ghm_root] = pricing_info;
                    }
                    pricing_info = pricing_info[sector];

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

                available_dates.add(date_key);
            });
        }

        return pricings_map;
    }
    this.updatePriceMap = updatePriceMap;

    function refreshGhmRootsMenu(indexes, main_index, sector, select_ghm_root)
    {
        let el = query('#opt_ghm_root > select');

        el.replaceContent(
            ghm_roots.map(function(ghm_root_info) {
                let disabled = !checkIndexGhmRoot(indexes, main_index, sector, ghm_root_info.ghm_root);
                return html('option',
                    {value: ghm_root_info.ghm_root, disabled: disabled},
                    ghm_root_info.ghm_root + ' – ' + ghm_root_info.desc + (disabled ? ' *' : '')
                );
            })
        );
        el.value = select_ghm_root || el.value;
    }

    function refreshDiffMenu(indexes, diff_index, sector, test_ghm_root)
    {
        let el = query("#opt_diff_index > select");

        el.replaceContent(
            html('option', {value: ''}, 'Désactiver'),
            indexes.map(function(index, i) {
                let disabled = !checkIndexGhmRoot(indexes, i, sector, test_ghm_root);
                return html('option',
                    {value: indexes[i].begin_date, disabled: disabled},
                    indexes[i].begin_date + (disabled ? ' *' : '')
                );
            })
        );
        if (diff_index >= 0)
            el.value = indexes[diff_index].begin_date;
    }

    function refreshPriceTable(ghm_root, pricing_info, main_index, diff_index,
                               max_duration, apply_coeff, merge_cells)
    {
        if (!thop.needsRefresh(refreshPriceTable, arguments))
            return;

        let table = query('#pr_table');
        table.replaceContent(
            html('thead'),
            html('tbody')
        );

        if (pricing_info && pricing_info[main_index] &&
                (diff_index < 0 || pricing_info[diff_index])) {
            let thead = table.query('thead');
            let tbody = table.query('tbody');

            let ghs = pricing_info[main_index].ghs;

            addPricingHeader(thead, ghm_root, pricing_info[main_index].ghs, true,
                             apply_coeff, merge_cells);

            for (let duration = 0; duration < max_duration; duration++) {
                if (duration % 10 == 0) {
                    let text = '' + duration + ' - ' +
                                    durationText(Math.min(max_duration - 1, duration + 9));
                    let tr = html('tr',
                        html('th', {class: 'repeat', colspan: ghs.length + 1}, text)
                    );
                    tbody.appendContent(tr);
                }

                let tr = html('tr',
                    html('th', durationText(duration))
                );
                for (const col of ghs) {
                    let info;
                    if (diff_index < 0) {
                        info = computePrice(col, duration, apply_coeff);
                    } else {
                        let prev_ghs = pricing_info[diff_index].ghs_map[col.ghs];
                        info = computeDelta(col, prev_ghs, duration, apply_coeff);
                    }

                    let td;
                    if (info) {
                        td = html('td', {class: info.mode}, priceText(info.price));

                        let title = '';
                        if (!duration && col.warn_cmd28) {
                            td.addClass('warn');
                            title += 'Devrait être orienté dans la CMD 28 (séance)\n';
                        }
                        if (col.warn_ucd) {
                            td.addClass('info');
                            title += 'Possibilité de minoration UCD (40 €)\n';
                        }
                        if (title)
                            td.title = title;
                    } else {
                        td = html('td');
                    }
                    tr.appendContent(td);
                }
                tbody.appendContent(tr);
            }
        }
    }

    function addPricingHeader(thead, ghm_root, columns, show_pricing, apply_coeff, merge_cells)
    {
        if (apply_coeff === undefined)
            apply_coeff = false;
        if (merge_cells === undefined)
            merge_cells = true;

        let title;
        {
            let ghm_roots_map = catalog.update('mco_ghm_roots').map;

            title = ghm_root;
            if (ghm_roots_map[ghm_root])
                title += ' - '  + ghm_roots_map[ghm_root].desc;
        }

        thead.appendContent(
            html('tr',
                html('td', {colspan: 1 + columns.length, class: 'ghm_root'}, title)
            )
        );

        function appendRow(name, func)
        {
            let tr = html('tr',
                html('th', name)
            );

            let prev_cell = [document.createTextNode(''), {}, false];
            let prev_td = null;
            for (let i = 0; i < columns.length; i++) {
                let cell = func(columns[i], i) || [null, {}, false];
                if (cell[0] === null) {
                    cell[0] = document.createTextNode('');
                } else if (typeof cell[0] === 'string') {
                    cell[0] = document.createTextNode(cell[0]);
                }

                if (merge_cells && cell[2] && cell[0].isEqualNode(prev_cell[0]) &&
                        cell[1].class === prev_cell[1].class) {
                    let colspan = parseInt(prev_td.getAttribute('colspan') || 1);
                    prev_td.setAttribute('colspan', colspan + 1);
                } else {
                    prev_td = tr.appendChild(html('td', cell[1], cell[0]));
                }

                prev_cell = cell;
            }

            thead.appendContent(tr);
        }

        appendRow('GHS', function(col) { return ['' + col.ghs, {class: 'desc'}, false]; });
        appendRow('GHM', function(col) { return [col.ghm, {class: 'desc'}, true]; });
        appendRow('Niveau', function(col) { return ['Niveau ' + col.ghm.substr(5, 1), {class: 'desc'}, true]; });
        if (show_pricing) {
            appendRow('Conditions', function(col) {
                let el = html('div', {title: col.conditions.join('\n')},
                              col.conditions.length ? col.conditions.length.toString() : '');
                return [el, {class: 'conditions'}, true];
            });
            appendRow('Borne basse', function(col) { return [durationText(col.exb_treshold), {class: 'exb'}, true]; });
            appendRow('Borne haute',
                      function(col) { return [durationText(col.exh_treshold && col.exh_treshold - 1), {class: 'exh'}, true]; });
            appendRow('Tarif €', function(col) {
                let cents = applyGhsCoefficient(col, col.ghs_cents, apply_coeff);
                return [priceText(cents), {class: 'noex'}, true];
            });
            appendRow('Forfait EXB €', function(col) {
                let cents = applyGhsCoefficient(col, col.exb_cents, apply_coeff);
                return [col.exb_once ? priceText(cents) : '', {class: 'exb'}, true];
            });
            appendRow('Tarif EXB €', function(col) {
                let cents = applyGhsCoefficient(col, col.exb_cents, apply_coeff);
                return [!col.exb_once ? priceText(cents) : '', {class: 'exb'}, true];
            });
            appendRow('Tarif EXH €', function(col) {
                let cents = applyGhsCoefficient(col, col.exh_cents, apply_coeff);
                return [priceText(cents), {class: 'exh'}, true];
            });
            appendRow('Age', function(col) {
                let texts = [];
                let severity = col.ghm.charCodeAt(5) - '1'.charCodeAt(0);
                if (severity >= 0 && severity < 4) {
                    if (severity < col.young_severity_limit)
                        texts.push('< ' + col.young_age_treshold.toString());
                    if (severity < col.old_severity_limit)
                        texts.push('≥ ' + col.old_age_treshold.toString());
                }

                return [texts.join(', '), {class: 'age'}, true];
            });
        }
    }
    this.addPricingHeader = addPricingHeader;

    function refreshPriceChart(pricing_info, main_index, diff_index, max_duration, apply_coeff)
    {
        if (!thop.needsRefresh(refreshPriceChart, Array.from(arguments)))
            return;

        if (typeof Chart === 'undefined') {
            lazyLoad('chartjs');
            return;
        }

        if (!pricing_info || !pricing_info[main_index]) {
            if (chart)
                chart.destroy();
            return;
        }

        let ghs = pricing_info[main_index].ghs;

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

        let datasets = [];

        let max_price = 0.0;
        for (const col of ghs) {
            let dataset = {
                label: ghsLabel(col, col.conditions),
                data: [],
                borderColor: modeToColor(col.ghm.substr(5, 1)),
                backgroundColor: modeToColor(col.ghm.substr(5, 1)),
                borderDash: (col.conditions.length ? [5, 5] : undefined),
                fill: false
            };

            for (let duration = 0; duration < max_duration; duration++) {
                let info;
                if (diff_index < 0 || !pricing_info[diff_index]) {
                    info = computePrice(col, duration, apply_coeff);
                } else {
                    info = computeDelta(col, pricing_info[diff_index].ghs_map[col.ghs],
                                        duration, apply_coeff);
                }

                if (info !== null) {
                    dataset.data.push({
                        x: duration,
                        y: info.price
                    });
                    max_price = Math.max(max_price, Math.abs(info.price));
                } else {
                    dataset.data.push(null);
                }
            }
            datasets.push(dataset);
        }

        let min_price;
        if (diff_index < 0) {
            min_price = 0.0;

            // Recalculate maximum price across all (loaded) indexes to stabilize Y axis
            for (let i = 0; i < pricing_info.length; i++) {
                if (i === main_index || !pricing_info[i])
                    continue;

                for (const col of pricing_info[i].ghs) {
                    let p = computePrice(col, max_duration - 1, apply_coeff);
                    if (p && p.price > max_price)
                        max_price = p.price;
                }
            }
        } else {
            min_price = -max_price;
        }

        if (chart) {
            chart.data.datasets = datasets;
            chart.options.scales.yAxes[0].ticks.suggestedMin = min_price;
            chart.options.scales.yAxes[0].ticks.suggestedMax = max_price;
            chart.update({duration: 0});
        } else {
            let chart_ctx = query('#pr_chart').getContext('2d');
            chart = new Chart(chart_ctx, {
                type: 'line',
                data: {
                    datasets: datasets
                },
                options: {
                    responsive: true,
                    tooltips: {
                        mode: 'index',
                        intersect: false,
                        callbacks: {
                            title: function(items, data) {
                                return durationText(items[0].xLabel)
                            },
                            label: function(item, data) {
                                return 'GHS ' + data.datasets[item.datasetIndex].label + ': ' +
                                       priceText(item.yLabel);
                            }
                        }
                    },
                    hover: {mode: 'x', intersect: true},
                    elements: {
                        line: {tension: 0},
                        point: {radius: 0, hitRadius: 0}
                    },
                    scales: {
                        xAxes: [{
                            type: 'linear',
                            ticks: {
                                stepSize: 10,
                                callback: function(value) { return durationText(value); }
                            }
                        }],
                        yAxes: [{
                            type: 'linear',
                            ticks: {
                                suggestedMin: min_price,
                                suggestedMax: max_price,
                                callback: function(value) { return priceText(value); }
                            }
                        }]
                    },
                    legend: {onClick: null}
                },
            });
        }
    }

    function testDuration(ghs, duration)
    {
        let duration_mask = (duration < 32) ? (1 << duration) : (1 << 31);
        return !!(ghs.durations & duration_mask);
    }
    this.testDuration = testDuration;

    function computePrice(ghs, duration, apply_coeff)
    {
        if (!ghs.ghs_cents)
            return null;
        if (!testDuration(ghs, duration))
            return null;

        let price_cents;
        let mode;
        if (ghs.exb_treshold && duration < ghs.exb_treshold) {
            price_cents = ghs.ghs_cents;
            if (ghs.exb_once) {
                price_cents -= ghs.exb_cents;
            } else {
                price_cents -= (ghs.exb_treshold - duration) * ghs.exb_cents;
            }
            mode = 'exb';
        } else if (ghs.exh_treshold && duration >= ghs.exh_treshold) {
            price_cents = ghs.ghs_cents + (duration - ghs.exh_treshold + 1) * ghs.exh_cents;
            mode = 'exh';
        } else {
            price_cents = ghs.ghs_cents;
            mode = 'noex';
        }

        price_cents = applyGhsCoefficient(ghs, price_cents, apply_coeff);
        return {price: price_cents, mode: mode};
    }

    function computeDelta(ghs, prev_ghs, duration, apply_coeff)
    {
        let p = ghs ? computePrice(ghs, duration, apply_coeff) : null;
        let p2 = prev_ghs ? computePrice(prev_ghs, duration, apply_coeff) : null;

        let delta;
        let mode;
        if (p !== null && p2 !== null) {
            p.price -= p2.price;
            if (p.price < 0) {
                p.mode += ' diff lower';
            } else if (p.price > 0) {
                p.mode += ' diff higher';
            } else {
                p.mode += ' diff neutral';
            }
        } else if (p !== null) {
            p.mode += ' added';
        } else if (p2 !== null) {
            p = {price: -p2.price, mode: p2.mode + ' removed'};
        } else {
            return null;
        }

        return p;
    }

    function applyGhsCoefficient(ghs, cents, apply_coeff)
    {
        return apply_coeff && cents ? (ghs.ghs_coefficient * cents) : cents;
    }
    this.applyGhsCoefficient = applyGhsCoefficient;

    function buildConditionsArray(ghs)
    {
        let conditions = [];

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
    this.buildConditionsArray = buildConditionsArray;

    thop.registerUrl('mco_pricing', this, runPricing);
}).call(mco_pricing);
