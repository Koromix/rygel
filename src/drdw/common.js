// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

function addClass(elements, cls)
{
    for (var i = 0; i < elements.length; i++)
        elements[i].classList.add(cls);
}

function removeClass(elements, cls)
{
    for (var i = 0; i < elements.length; i++)
        elements[i].classList.remove(cls);
}

function toggleClass(elements, cls)
{
    for (var i = 0; i < elements.length; i++)
        elements[i].classList.toggle(cls);
}

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

    function appendChild(child)
    {
        if (child === null || child === undefined) {
            // Skip
        } else if (typeof child === 'string') {
            el.appendChild(document.createTextNode(child));
        } else if (Array.isArray(child)) {
            for (var i = 0; i < child.length; i++)
                appendChild(child[i]);
        } else {
            el.appendChild(child);
        }
    }

    if (attr) {
        for (var key in attr) {
            value = attr[key];
            if (value !== null) {
                if (typeof value === 'function') {
                    el.addEventListener(key, value.bind(el));
                } else {
                    el.setAttribute(key, value);
                }
            }
        }
    }

    for (var i = 3; i < arguments.length; i++)
        appendChild(arguments[i]);

    return el;
}

function cloneAttributes(src_node, element) {
    var attributes = src_node.attributes;
    for (var i = 0; i < attributes.length; i++) {
        element.setAttribute(attributes[i].nodeName, attributes[i].nodeValue);
    }
}

function downloadJson(url, arguments, func)
{
    var keys = Object.keys(arguments);
    if (keys.length) {
        var query_arguments = [];
        for (var i = 0; i < keys.length; i++) {
            var value = arguments[keys[i]];
            if (value !== null && value !== undefined) {
                var arg = escape(keys[i]) + '=' + escape(arguments[keys[i]]);
                query_arguments.push(arg);
            }
        }
        url += '?' + query_arguments.sort().join('&');
    }

    if (downloadJson.queue.has(url))
        return;
    downloadJson.queue.add(url);
    downloadJson.run_lock++;

    var xhr = new XMLHttpRequest();
    xhr.open('get', url, true);
    xhr.responseType = 'json';
    xhr.timeout = 10000;
    xhr.onload = function(e) {
        downloadJson.run_lock--;
        func(this.status, xhr.response);
        downloadJson.queue.delete(url);
    };
    xhr.onerror = function(e) {
        downloadJson.run_lock--;
        func(503);
        downloadJson.queue.delete(url);
    };
    xhr.ontimeout = function(e) {
        downloadJson.run_lock--;
        func(504);
        downloadJson.queue.delete(url);
    };
    xhr.send();
}
downloadJson.queue = new Set();
downloadJson.run_lock = 0;

function markOutdated(selector, mark)
{
    var elements = document.querySelectorAll(selector);
    for (var i = 0; i < elements.length; i++) {
        elements[i].classList.toggle('outdated', mark);
    }
}

// ------------------------------------------------------------------------
// Navigation
// ------------------------------------------------------------------------

var url_base;
var url_page;
var module;

// Error accumulator
var errors = new Set();

// Shared routing values
var target_date = null;

function switchMenu(selector, enable)
{
    var el = document.querySelector(selector);
    if (enable === undefined)
        enable = !el.classList.contains('active');
    if (enable) {
        var els = document.querySelectorAll('nav');
        for (var i = 0; i < els.length; i++)
            els[i].classList.toggle('active', els[i] == el);
    } else {
        el.classList.remove('active');
    }
}

function switchPage(page_url, mark_history)
{
    if (mark_history === undefined)
        mark_history = true;

    if (mark_history && page_url !== url_page) {
        url_page = page_url;
        window.history.pushState(null, null, url_base + url_page);
    }

    var menu_anchors = document.querySelectorAll('#side_menu a');
    for (var i = 0; i < menu_anchors.length; i++) {
        var active = (page_url.startsWith(menu_anchors[i].getAttribute('href')) &&
                      !menu_anchors[i].classList.contains('category'));
        menu_anchors[i].classList.toggle('active', active);
    }
    switchMenu('#side_menu', false);

    removeClass(document.querySelectorAll('.page'), 'active');

    var module_name = page_url.split('/')[0];
    module = window[module_name];
    if (module !== undefined && module.run !== undefined)
        module.run();
}

function initNavigation()
{
    url_base = window.location.pathname.substr(0, window.location.pathname.indexOf('/')) +
               document.querySelector('base').getAttribute('href');
    url_page = window.location.pathname.substr(url_base.length);
    if (url_page === '') {
        var first_anchor = document.querySelector('#side_menu a');
        url_page = first_anchor.getAttribute('href');
        window.history.replaceState(null, null, url_base + url_page);
    }

    switchPage(url_page);

    window.addEventListener('popstate', function(e) {
        url_page = window.location.pathname.substr(url_base.length);
        switchPage(url_page, false);
    });
}

if (document.readyState === 'complete') {
    initNavigation();
} else {
    document.addEventListener('DOMContentLoaded', initNavigation);
}

// ------------------------------------------------------------------------
// Indexes
// ------------------------------------------------------------------------

var indexes_init = false;
var indexes = [];

function updateIndexes(func)
{
    if (indexes_init)
        return;

    downloadJson('api/indexes.json', {}, function(status, json) {
        var error = null;

        switch (status) {
            case 200: {
                if (json.length > 0) {
                    indexes = json;
                    for (var i = 0; i < indexes.length; i++)
                        indexes[i].init = false;
                } else {
                    error = 'Aucune table disponible';
                }
            } break;

            case 404: { error = 'Liste des indexes introuvable'; } break;
            case 502:
            case 503: { error = 'Service non accessible'; } break;
            case 504: { error = 'Délai d\'attente dépassé, réessayez'; } break;
            default: { error = 'Erreur inconnue ' + status; } break;
        }

        indexes_init = !error;
        if (error)
            errors.add(error);
        if (!downloadJson.run_lock)
            func();
    });
}

function refreshIndexesLine(selector, main_index)
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
            if (i == main_index) {
                radius++;
                var color = '#ff8900';
                var weight = 'bold';
            } else if (indexes[i].changed_prices) {
                var color = '#000';
                var weight = 'normal';
            } else {
                var color = '#888';
                var weight = 'normal';
            }
            var click_function = (function() {
                var index = i;
                return function(e) { module.route({date: indexes[index].begin_date}); };
            })();

            var node = createElementNS('svg', 'circle',
                                       {cx: x, cy: 20, r: radius, fill: color,
                                        style: 'cursor: pointer;',
                                        click: click_function},
                createElementNS('svg', 'title', {}, indexes[i].begin_date)
            );
            g.appendChild(node);

            if (indexes[i].changed_prices) {
                var text_y = text_above ? 10 : 40;
                text_above = !text_above;

                var text = createElementNS('svg', 'text',
                                           {x: x, y: text_y, 'text-anchor': 'middle', fill: color,
                                            style: 'cursor: pointer; font-weight: ' + weight,
                                            click: click_function},
                                           indexes[i].begin_date);
                g.appendChild(text);
            }
        }
    }

    var old_g = document.querySelector(selector + ' > g');
    old_g.parentNode.replaceChild(g, old_g);
}

function moveIndex(relative_index)
{
    var index = indexes.findIndex(function(index) { return index.begin_date === target_date; });
    if (index < 0)
        index = indexes.length - 1;

    var new_index = index + relative_index;
    if (new_index < 0 || new_index >= indexes.length)
        return;

    target_date = indexes[new_index].begin_date;
    module.route();
}
this.moveIndex = moveIndex;
