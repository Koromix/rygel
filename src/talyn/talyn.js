// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

function getFullNamespace(ns)
{
    return ({
        svg: 'http://www.w3.org/2000/svg',
        html: 'http://www.w3.org/1999/xhtml'
    })[ns] || ns;
}

function createElement(tag, attr)
{
    var args = [].slice.call(arguments);
    args.unshift('html');
    return createElementNS.apply(this, args);
}

function createElementNS(ns, tag, attr)
{
    ns = getFullNamespace(ns);
    var el = document.createElementNS(ns, tag);
    if (attr) {
        for (var key in attr) {
            value = attr[key];
            if (value !== null)
                el.setAttribute(key, attr[key]);
        }
    }
    for (var i = 3; i < arguments.length; i++) {
        if (typeof arguments[i] === 'string') {
            el.appendChild(document.createTextNode(arguments[i]));
        } else if (arguments[i] !== null) {
            el.appendChild(arguments[i]);
        }
    }
    return el;
}

function toggleNav(id)
{
    var el = document.querySelector('div#' + id + ' nav');
    if (el.classList.contains('active')) {
        el.classList.remove('active');
    } else {
        var els = document.querySelectorAll('div nav');
        for (var i = 0; i < els.length; i++)
            els[i].classList.toggle('active', els[i] == el);
    }
}

function downloadJson(url, func)
{
    var xhr = new XMLHttpRequest();
    xhr.open('get', url, true);
    xhr.responseType = 'json';
    xhr.onload = function(e) {
        if (this.status === 200) {
            func(xhr.response);
        } else {
            console.log("Statut de la réponse: %d (%s)", this.status, this.statusText);
        }
    };
    xhr.send();
}

// ------------------------------------------------------------------------
// Pages
// ------------------------------------------------------------------------

var url_base;
var url_name;

document.addEventListener('DOMContentLoaded', function(e) {
    url_base = window.location.pathname.substr(0, window.location.pathname.indexOf('/')) +
               document.querySelector('base').getAttribute('href');
    url_name = window.location.pathname.substr(url_base.length);

    downloadJson('api/pages.json', function(categories) {
        if (url_name == '') {
            url_name = categories[0].pages[0].url;
            window.history.replaceState(null, null, url_base + url_name);
        }

        var ul = document.querySelector('div#menu nav ul');
        function addMenuItem(name, url)
        {
            var li =
                createElement('li', {},
                    createElement('a', {href: url, class: url == url_name ? 'active' : null}, name)
                );
            ul.appendChild(li);
        }

        for (var i = 0; i < categories.length; i++) {
            var pages = categories[i].pages;

            var li =
                createElement('li', {},
                    createElement('a', {href: pages[0].url,
                                        onclick: 'switchPage("' + pages[0].url + '"); return false;',
                                        class: 'category'},
                                  categories[i].category)
                );
            ul.appendChild(li);

            for (var j = 0; j < pages.length; j++) {
                var li =
                    createElement('li', {},
                        createElement('a', {href: pages[j].url,
                                            onclick: 'switchPage("' + pages[j].url + '"); return false;'},
                                      pages[j].name)
                    );
                ul.appendChild(li);
            }
        }

        switchPage(url_name);
    });
});

function switchPage(page_url)
{
    var page_divs = document.querySelectorAll('main > div');
    for (var i = 0; i < page_divs.length; i++) {
        page_divs[i].classList.remove('active');
    }

    if (page_url == 'pricing/table') {
        document.querySelector('div#pricing').classList.add('active');
        document.querySelector('div#pricing > div.table').classList.remove('inactive');
        document.querySelector('div#pricing > div.chart').classList.add('inactive');
        // pricing.refreshPricing();
    } else if (page_url == 'pricing/chart') {
        document.querySelector('div#pricing').classList.add('active');
        document.querySelector('div#pricing > div.table').classList.add('inactive');
        document.querySelector('div#pricing > div.chart').classList.remove('inactive');
        // pricing.refreshPricing();
    }

    var menu_anchors = document.querySelectorAll('div#menu nav a');
    for (var i = 0; i < menu_anchors.length; i++) {
        var active = (menu_anchors[i].getAttribute('href') == page_url &&
                      !menu_anchors[i].classList.contains('category'));
        menu_anchors[i].classList.toggle('active', active);
    }

    if (page_url != url_name) {
        url_name = page_url;
        window.history.pushState(null, null, url_base + url_name);
    }
}

// ------------------------------------------------------------------------
// Pricing
// ------------------------------------------------------------------------

var pricing = {};
(function() {
    var ghm_roots = [];
    var ghm_roots_info = {};
    var chart = null;

    function updateDatabase()
    {
        var date = document.querySelector('input#pricing_date').value;

        downloadJson('api/catalog.json?date=' + date, function(json) {
            ghm_roots = [];
            ghm_roots_info = {};

            for (var i = 0; i < json.length; i++) {
                ghm_roots.push(json[i].ghm_root);
                ghm_roots_info[json[i].ghm_root] = json[i].info;
            }

            refreshGhmRoots();
            refreshPricing();
        });
    }
    this.updateDatabase = updateDatabase;

    function refreshGhmRoots()
    {
        var el = document.querySelector('select#pricing_ghm_roots');
        var previous_value = el.value;
        el.innerHTML = '';
        for (var i = 0; i < ghm_roots.length; i++) {
            var opt = document.createElement('option');
            opt.setAttribute('value', ghm_roots[i]);
            opt.textContent = ghm_roots[i];
            el.appendChild(opt);
        }
        if (previous_value)
            el.value = previous_value;
    }

    function refreshPricing()
    {
        var ghm_root_info = ghm_roots_info[document.querySelector('select#pricing_ghm_roots').value];
        var merge_cells = document.querySelector('input#pricing_merge_cells').checked;
        var max_duration = parseInt(document.querySelector('input#pricing_max_duration').value) + 1;

        var table = createTable(ghm_root_info, merge_cells, max_duration);
        var old_table = document.querySelector('div#pricing > div.table > table');
        old_table.parentNode.replaceChild(table, old_table);

        var chart_ctx = document.querySelector('div#pricing > div.chart > canvas').getContext('2d');
        chart = refreshChart(chart, chart_ctx, ghm_root_info, max_duration);
    }
    this.refreshPricing = refreshPricing;

    function refreshChart(chart, chart_ctx, ghm_root_info, max_duration)
    {
        data = {
            labels: [],
            datasets: []
        };

        for (var i = 0; i < max_duration; i++)
            data.labels.push(durationText(i));

        for (var i = 0; i < ghm_root_info.length; i++) {
            var dataset = {
                label: ghm_root_info[i].ghm,
                data: [],
                fill: false
            };
            for (var duration = 0; duration < max_duration; duration++) {
                p = computePrice(ghm_root_info[i], duration);
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

        if (chart) {
            chart.data = data;
            chart.update();
        } else {
            chart = new Chart(chart_ctx, {
                type: 'line',
                data: data,
                options: {
                    responsive: true,
                    tooltips: {
                        mode: 'x',
                        intersect: false,
                        position: 'nearest'
                    },
                    hover: {
                        mode: 'x',
                        intersect: false
                    },
                    elements: {
                        line: {
                            tension: 0
                        },
                        point: {
                            radius: 1,
                            hitRadius: 0
                        }
                    }
                },
            });
        }

        return chart;
    }

    function createTable(ghm_root_info, merge_cells, max_duration)
    {
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
            for (var i = 0; i < ghm_root_info.length; i++) {
                var cell = func(ghm_root_info[i]) || [null, null, false];
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

        appendRow(thead, 'GHM', function(col) { return [col.ghm, 'desc', true]; });
        appendRow(thead, 'Niveau', function(col) { return ['Niveau ' + col.ghm_mode, 'desc', true]; });
        appendRow(thead, 'GHS', function(col) { return ['GHS ' + col.ghs, 'desc', true]; });
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

document.addEventListener('DOMContentLoaded', function(e) {
    var date_input = document.querySelector('input#pricing_date');
    if (!date_input.value) {
        date_input.valueAsDate = new Date();
    }
    pricing.updateDatabase();
});
