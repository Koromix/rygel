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

document.addEventListener('DOMContentLoaded', function(e) {
    var url_base = window.location.pathname.substr(0, window.location.pathname.lastIndexOf('/'));
    var url_name = window.location.pathname.substr(url_base.length + 1);

    downloadJson('api/pages.json', function(pages) {
        if (url_name == '') {
            url_name = pages[0].url.substr(1);
            window.history.replaceState(null, null, url_base + '/' + url_name);
        }

        var ul = document.querySelector('div#menu nav ul');
        for (var i = 0; i < pages.length; i++) {
            var page_url = pages[i].url.substr(1);
            var li =
                createElement('li', {},
                    createElement('a', {href: page_url,
                                        class: page_url == url_name ? 'active' : null}, pages[i].name)
                );
            ul.appendChild(li);
        }
    });
});

// ------------------------------------------------------------------------
// Tables
// ------------------------------------------------------------------------

var pricing = {};
(function() {
    var ghm_roots = [];
    var ghm_roots_info = {};

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
            refreshTable();
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

    function refreshTable()
    {
        var ghm_root_info = ghm_roots_info[document.querySelector('select#pricing_ghm_roots').value];
        var merge_cells = document.querySelector('input#pricing_merge_cells').checked;
        var max_duration = parseInt(document.querySelector('input#pricing_max_duration').value) + 1;

        var table = createTable(ghm_root_info, merge_cells, max_duration);

        var old_table = document.querySelector('div#pricing table');
        old_table.parentNode.replaceChild(table, old_table);
    }
    this.refreshTable = refreshTable;

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
        })
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
                if (severity < col.young_severity_limit) {
                    texts.push('< ' + col.young_age_treshold.toString());
                }
                if (severity < col.old_severity_limit) {
                    texts.push('≥ ' + col.old_age_treshold.toString());
                }
            }
            return [texts.join(', '), 'age', true];
        });

        for (var duration = 0; duration < max_duration; duration++) {
            appendRow(tbody, durationText(duration), function(col) {
                if ((col.low_duration_limit && duration < col.low_duration_limit) ||
                    (col.high_duration_limit && duration >= col.high_duration_limit))
                    return null;
                if (col.exb_treshold && duration < col.exb_treshold) {
                    var price = col.price_cents;
                    if (col.exb_once) {
                        price -= col.exb_cents;
                    } else {
                        price -= (col.exb_treshold - duration) * col.exb_cents;
                    }
                    return [priceText(price), 'exb', false];
                }
                if (col.exh_treshold && duration >= col.exh_treshold) {
                    var price = col.price_cents + (duration - col.exh_treshold + 1) * col.exh_cents;
                    return [priceText(price), 'exh', false];
                }
                return [priceText(col.price_cents), 'price', false];
            });
        }

        return table;
    }
}).call(pricing);

document.addEventListener('DOMContentLoaded', function(e) {
    var date_input = document.querySelector('input#pricing_date');
    if (!date_input.value) {
        date_input.valueAsDate = new Date();
    }
    pricing.updateDatabase();
});
