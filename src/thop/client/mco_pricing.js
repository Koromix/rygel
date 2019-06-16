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
    let chart_el;
    let chart;

    function runModule(route)
    {
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
            thop.error('Mode d\'affichage incorrect');
        if (route.date && indexes.length && main_index < 0)
            thop.error('Date incorrecte');
        if (route.diff && indexes.length && diff_index < 0)
            thop.error('Date de comparaison incorrecte');
        if (!['public', 'private'].includes(route.sector))
            thop.error('Secteur incorrect');
        if (route.ghm_root !== null && ghm_roots.length) {
            if (!ghm_roots_map[route.ghm_root]) {
                thop.error('Racine de GHM inconnue');
            } else {
                if (!checkIndexGhmRoot(indexes, main_index, route.sector, route.ghm_root))
                    thop.error('Cette racine n\'existe pas dans la version \'' + indexes[main_index].begin_date + '\'');
                if (!checkIndexGhmRoot(indexes, diff_index, route.sector, route.ghm_root))
                    thop.error('Cette racine n\'existe pas dans la version \'' + indexes[diff_index].begin_date + '\'');
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
            let view_el = query('#view');

            const ghm_root_info = ghm_roots_map[route.ghm_root];
            let pricing_info = pricings_map[route.ghm_root];
            if (pricing_info)
                pricing_info = pricing_info[route.sector];
            const max_duration = parseInt(query('#opt_max_duration > input').value, 10);

            switch (route.view) {
                case 'table': {
                    refreshPriceTable(route.ghm_root, pricing_info, main_index, diff_index,
                                      max_duration, route.apply_coefficient);
                } break;

                case 'chart': {
                    if (!chart_el) {
                        chart_el = document.createElement('canvas');
                        chart_el.setAttribute('id', 'pr_chart');
                        chart_el.setAttribute('width', 400);
                        chart_el.setAttribute('height', 300);
                    }

                    render(chart_el, view_el);
                    refreshPriceChart(pricing_info, main_index, diff_index,
                                      max_duration, route.apply_coefficient);
                } break;
            }
        }
    }
    this.runModule = runModule;

    function parseRoute(route, path, parameters, hash)
    {
        // Model: mco_pricing/<view>/[<diff>..]<date>/<ghm_root>/<sector>)
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
        route.apply_coefficient = !!parseInt(parameters.apply_coefficient, 10) || false;
    }
    this.parseRoute = parseRoute;

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

        url = util.buildUrl(url, {
            apply_coefficient: new_route.apply_coefficient ? 1 : null
        });

        return {
            url: url,
            allowed: true
        };
    }
    this.routeToUrl = routeToUrl;

    // A true result actually means maybe (if we haven't download the relevant index yet)
    function checkIndexGhmRoot(indexes, index, sector, ghm_root)
    {
        if (index >= 0) {
            const date_key = indexes[index].begin_date + '@' + sector;

            return !available_dates.has(date_key) ||
                   (pricings_map[ghm_root] && pricings_map[ghm_root][sector] && pricings_map[ghm_root][sector][index]);
       } else {
            return true;
       }
    }

    function updatePriceMap(index, sector)
    {
        const indexes = thop.updateMcoSettings().indexes;
        const date_key = indexes[index].begin_date + '@' + sector;

        if (!available_dates.has(date_key)) {
            let url = util.buildUrl(thop.baseUrl('api/mco_ghm_ghs.json'),
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

        render(ghm_roots.map(ghm_root_info => {
            let disabled = !checkIndexGhmRoot(indexes, main_index, sector, ghm_root_info.ghm_root);
            let label = `${ghm_root_info.ghm_root} – ${ghm_root_info.desc}${disabled ? ' *' : ''}`;

            return html`<option value=${ghm_root_info.ghm_root} ?disabled=${disabled}>${label}</option>`;
        }), el);

        el.value = select_ghm_root || el.value;
    }

    function refreshDiffMenu(indexes, diff_index, sector, test_ghm_root)
    {
        let el = query("#opt_diff_index > select");

        render(html`
            <option value="">Désactiver</option>
            ${indexes.map((index, idx) => {
                let disabled = !checkIndexGhmRoot(indexes, idx, sector, test_ghm_root);
                let label = `${index.begin_date}${disabled ? ' *' : ''}`;

                return html`<option value=${index.begin_date} ?disabled=${disabled}>${label}</option>`;
            })}
        `, el);

        if (diff_index >= 0)
            el.value = indexes[diff_index].begin_date;
    }

    function refreshPriceTable(ghm_root, pricing_info, main_index, diff_index,
                               max_duration, apply_coeff)
    {
        let root_el = query('#view');

        let title = ghm_root;
        {
            let ghm_roots_map = catalog.update('mco_ghm_roots').map;
            if (ghm_roots_map[ghm_root])
                title += ' - '  + ghm_roots_map[ghm_root].desc;
        }

        let ghs;
        if (pricing_info && pricing_info[main_index] &&
                (diff_index < 0 || pricing_info[diff_index])) {
            let ghs = pricing_info[main_index].ghs;

            render(html`
                <table id="pr_table" class="pr_grid">
                    <thead>
                        <tr><td class="ghm_root" colspan=${ghs.length + 1}>${title}</td></tr>

                        <tr><th>GHM</th>${util.mapRLE(ghs.map(col => col.ghm),
                            (ghm, colspan) => html`<td class="desc" colspan=${colspan}>${ghm}</td>`)}</tr>
                        <tr><th>Niveau</th>${util.mapRLE(ghs.map(col => col.ghm.substr(5, 1)),
                            (mode, colspan) => html`<td class="desc" colspan=${colspan}>Niveau ${mode}</td>`)}</tr>
                        <tr><th>GHS</th>${ghs.map(col =>
                            html`<td class="desc">${col.ghs}${col.conditions.length ? '*' : ''}</td>`)}</tr>
                        <tr><th>Conditions</th>${ghs.map(col =>
                            html`<td class="conditions">${col.conditions.map(cond => html`${mco_list.addSpecLinks(cond)}<br/>`)}</td>`)}</tr>
                        <tr><th>Borne basse</th>${util.mapRLE(ghs.map(col => col.exb_treshold),
                            (treshold, colspan) => html`<td class="exb" colspan=${colspan}>${format.duration(treshold)}</td>`)}</tr>
                        <tr><th>Borne haute</th>${util.mapRLE(ghs.map(col => col.exh_treshold ? (col.exh_treshold - 1) : null),
                            (treshold, colspan) => html`<td class="exh" colspan=${colspan}>${format.duration(treshold)}</td>`)}</tr>
                        <tr><th>Tarif €</th>${util.mapRLE(ghs.map(col =>
                            applyCoefficient(col.ghs_cents, !apply_coeff || col.ghs_coefficient)),
                            (cents, colspan) => html`<td class="noex" colspan=${colspan}>${format.price(cents)}</td>`)}</tr>
                        <tr><th>Forfait EXB €</th>${util.mapRLE(ghs.map(col =>
                            applyCoefficient(col.exb_once ? col.exb_cents : null, !apply_coeff || col.ghs_coefficient)),
                            (cents, colspan) => html`<td class="exb" colspan=${colspan}>${format.price(cents)}</td>`)}</tr>
                        <tr><th>Tarif EXB €</th>${util.mapRLE(ghs.map(col =>
                            applyCoefficient(col.exb_once ? null : col.exb_cents, !apply_coeff || col.ghs_coefficient)),
                            (cents, colspan) => html`<td class="exb" colspan=${colspan}>${format.price(cents)}</td>`)}</tr>
                        <tr><th>Tarif EXH €</th>${util.mapRLE(ghs.map(col =>
                            applyCoefficient(col.exh_cents, !apply_coeff || col.ghs_coefficient)),
                            (cents, colspan) => html`<td class="exh" colspan=${colspan}>${format.price(cents)}</td>`)}</tr>
                        <tr><th>Age</th>${util.mapRLE(ghs.map(col => {
                            let texts = [];
                            let severity = col.ghm.charCodeAt(5) - '1'.charCodeAt(0);
                            if (severity >= 0 && severity < 4) {
                                if (severity < col.young_severity_limit)
                                    texts.push('< ' + col.young_age_treshold.toString());
                                if (severity < col.old_severity_limit)
                                    texts.push('≥ ' + col.old_age_treshold.toString());
                            }

                            return texts.join(', ');
                        }), (text, colspan) => html`<td class="age" colspan=${colspan}>${text}</td>`)}</tr>
                    </thead>

                    <tbody>${util.mapRange(0, max_duration, duration =>
                        html`<tr>
                            <th>${format.duration(duration)}</th>
                            ${ghs.map(col => {
                                let info;
                                if (diff_index < 0) {
                                    info = computePrice(col, duration, apply_coeff);
                                } else {
                                    let prev_ghs = pricing_info[diff_index].ghs_map[col.ghs];
                                    info = computeDelta(col, prev_ghs, duration, apply_coeff);
                                }

                                if (info) {
                                    let cls = info.mode;
                                    let tooltip = '';
                                    if (!duration && col.warn_cmd28) {
                                        cls += ' warn';
                                        tooltip += 'Devrait être orienté dans la CMD 28 (séance)\n';
                                    }
                                    if (testDuration(col.raac_durations || 0, duration)) {
                                        cls += ' warn';
                                        tooltip += 'Accessible en cas de RAAC\n';
                                    }
                                    if (col.warn_ucd) {
                                        cls += ' info';
                                        tooltip += 'Possibilité de minoration UCD (40 €)\n';
                                    }

                                    let text = format.price(info.price, true, diff_index >= 0);
                                    return html`<td class=${cls} title=${tooltip}>${text}</td>`;
                                } else {
                                    return html`<td></td>`;
                                }
                            })}
                        </tr>`
                    )}</tbody>
                </table>
            `, root_el);
        } else {
            render(html``, root_el);
        }
    }

    function modeToColor(mode)
    {
        switch (mode) {
            case 'J': return '#1b9e77';
            case 'T': return '#1b9e77';
            case '1': return '#9a9a9a';
            case '2': return '#0070c0';
            case '3': return '#ff6600';
            case '4': return '#ff0000';
            case 'A': return '#9a9a9a';
            case 'B': return '#0070c0';
            case 'C': return '#ff6600';
            case 'D': return '#ff0000';
            case 'E': return '#7f2704';
            case 'Z': return '#9a9a9a';
            default: return 'black';
        };
    }

    function refreshPriceChart(pricing_info, main_index, diff_index, max_duration, apply_coeff)
    {
        if (typeof Chart === 'undefined') {
            data.lazyLoad('chartjs');
            return;
        }

        if (!pricing_info || !pricing_info[main_index]) {
            if (chart) {
                chart.destroy();
                chart = null;
            }

            return;
        }

        let ghs = pricing_info[main_index].ghs;

        let datasets = [];
        let max_price = 0.0;
        for (let i = ghs.length - 1; i >= 0; i--) {
            let col = ghs[i];

            let dataset = {
                label: `${col.ghs}${col.conditions.length ? '*' : ''} (${col.ghm})`,
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
                    legend: {
                        reverse: true,
                        onClick: null
                    },
                    tooltips: {
                        mode: 'index',
                        intersect: false,
                        callbacks: {
                            title: function(items, data) {
                                return format.duration(items[0].xLabel)
                            },
                            label: function(item, data) {
                                return 'GHS ' + data.datasets[item.datasetIndex].label + ': ' +
                                       format.price(item.yLabel);
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
                                callback: function(value) { return format.duration(value); }
                            }
                        }],
                        yAxes: [{
                            type: 'linear',
                            ticks: {
                                suggestedMin: min_price,
                                suggestedMax: max_price,
                                callback: function(value) { return format.price(value); }
                            }
                        }]
                    }
                },
            });
        }
    }

    function testDuration(mask, duration)
    {
        let duration_mask = (duration < 32) ? (1 << duration) : (1 << 31);
        return !!(mask & duration_mask);
    }
    this.testDuration = testDuration;

    function computePrice(ghs, duration, apply_coeff)
    {
        if (!ghs.ghs_cents)
            return null;
        if (!testDuration(ghs.durations, duration))
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

        price_cents = applyCoefficient(price_cents, !apply_coeff || ghs.ghs_coefficient);
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

    function applyCoefficient(cents, coefficient)
    {
        return cents ? (cents * coefficient) : cents;
    }

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
        switch (ghs.special_mode) {
            case 'diabetes': { conditions.push('FI diabète < ' + ghs.special_duration + ' nuits'); } break;
        }
        if (ghs.main_diagnosis)
            conditions.push('DP ' + ghs.main_diagnosis);
        if (ghs.diagnoses)
            conditions.push('Diagnostic ' + ghs.diagnoses);
        if (ghs.procedures)
            conditions.push('Acte ' + ghs.procedures.join(', '));

        return conditions;
    }
}).call(mco_pricing);
