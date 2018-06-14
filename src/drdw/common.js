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

function toggleClass(elements, cls, value)
{
    for (var i = 0; i < elements.length; i++)
        elements[i].classList.toggle(cls, value);
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

function appendChildren(el, children)
{
    if (children === null || children === undefined) {
        // Skip
    } else if (typeof children === 'string') {
        el.appendChild(document.createTextNode(children));
    } else if (Array.isArray(children) || children instanceof NodeList) {
        for (var i = 0; i < children.length; i++)
            appendChildren(el, children[i]);
    } else {
        el.appendChild(children);
    }
}

function createElementNS(ns, tag, attr)
{
    ns = getFullNamespace(ns);
    var el = document.createElementNS(ns, tag);

    if (attr) {
        for (var key in attr) {
            value = attr[key];
            if (value !== null && value !== undefined) {
                if (typeof value === 'function') {
                    el.addEventListener(key, value.bind(el));
                } else {
                    el.setAttribute(key, value);
                }
            }
        }
    }

    for (var i = 3; i < arguments.length; i++)
        appendChildren(el, arguments[i]);

    return el;
}

function cloneAttributes(src_node, element)
{
    var attributes = src_node.attributes;
    for (var i = 0; i < attributes.length; i++) {
        element.setAttribute(attributes[i].nodeName, attributes[i].nodeValue);
    }
}

function _(selector)
{
    return document.querySelector(selector);
}

// ------------------------------------------------------------------------
// JSON
// ------------------------------------------------------------------------

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
    downloadJson.busy++;

    function handleFinishedRequest(status, response)
    {
        let error = null;
        switch (status) {
            case 200: { /* Success */ } break;
            case 404: { error = 'Adresse \'' + url + '\'introuvable'; } break;
            case 502:
            case 503: { error = 'Service non accessible'; } break;
            case 504: { error = 'Délai d\'attente dépassé, réessayez'; } break;
            default: { error = 'Erreur inconnue ' + status; } break;
        }

        if (!error) {
            func(response);
        } else {
            downloadJson.errors.push(error);
        }

        if (!--downloadJson.busy) {
            go();
            downloadJson.queue.clear();
        }
    }

    var xhr = new XMLHttpRequest();
    xhr.open('get', url, true);
    xhr.responseType = 'json';
    xhr.timeout = 10000;
    xhr.onload = function(e) { handleFinishedRequest(this.status, xhr.response); };
    xhr.onerror = function(e) { handleFinishedRequest(503); };
    xhr.ontimeout = function(e) { handleFinishedRequest(504); };
    xhr.send();
}
downloadJson.errors = [];
downloadJson.queue = new Set();
downloadJson.busy = 0;

// ------------------------------------------------------------------------
// Navigation
// ------------------------------------------------------------------------

// Routing
var route_url = null;
var route_url_parts = null;
var route = {
    'view': 'table',
    'date': null,
    'ghm_root': null,
    'diff': null,
    'apply_coefficient': false,

    'list': 'classifier_tree',
    'page': 1,
    'spec': null
};
var scroll_cache = {};
var module = null;

function markOutdated(selector, mark)
{
    var elements = document.querySelectorAll(selector);
    for (var i = 0; i < elements.length; i++) {
        elements[i].classList.toggle('outdated', mark);
    }
}

function toggleMenu(selector, enable)
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

function buildRoute(args)
{
    if (args === undefined)
        args = {};

    return Object.assign({}, route, args);
}

function parseUrl(url)
{
    var a = document.createElement('a');
    a.href = url;

    return {
        source: url,
        href: a.href,
        origin: a.origin,
        protocol: a.protocol.replace(':', ''),
        host: a.hostname,
        port: a.port,
        query: a.search,
        params: (function(){
            var ret = {};
            var seg = a.search.replace(/^\?/, '').split('&');
            var len = seg.length;
            var s;
            for (var i = 0; i < len; i++) {
                if (!seg[i])
                    continue;
                s = seg[i].split('=');
                ret[s[0]] = s[1];
            }
            return ret;
        })(),
        hash: a.hash.replace('#', ''),
        path: a.pathname.replace(/^([^/])/, '/$1')
    };
}

function go(new_url, mark_history)
{
    if (new_url === undefined)
        new_url = route_url;
    if (mark_history === undefined)
        mark_history = true;

    let url_parts = parseUrl(new_url);
    new_url = url_parts.href.substr(url_parts.origin.length + 1);

    if (new_url !== route_url) {
        if (route_url_parts)
            scroll_cache[route_url_parts.path] = [window.pageXOffset, window.pageYOffset];

        if (mark_history) {
            window.history.pushState(null, null, new_url);
        } else {
            window.history.replaceState(null, null, new_url);
        }
    }
    route_url = new_url;
    route_url_parts = url_parts;

    var menu_anchors = document.querySelectorAll('#side_menu a');
    for (var i = 0; i < menu_anchors.length; i++) {
        var active = (new_url.startsWith(menu_anchors[i].getAttribute('href')) &&
                      !menu_anchors[i].classList.contains('category'));
        menu_anchors[i].classList.toggle('active', active);
    }
    toggleMenu('#side_menu', false);

    removeClass(document.querySelectorAll('.page'), 'active');

    var module_name = new_url.split('/')[0];
    module = window[module_name];
    if (module !== undefined && module.run !== undefined)
        module.run(route, url_parts.path.substr(1), url_parts.params, url_parts.hash);

    var scroll_target = scroll_cache[url_parts.path];
    if (url_parts.hash) {
        window.location.hash = url_parts.hash;
    } else if (scroll_target) {
        window.scrollTo(scroll_target[0], scroll_target[1]);
    } else {
        window.scrollTo(0, 0);
    }
}
function redirect(new_url) { go(new_url, false); }

function initNavigation()
{
    let new_url;
    if (window.location.pathname !== '/') {
        new_url = window.location.href;
    } else {
        var first_anchor = document.querySelector('#side_menu a');
        new_url = first_anchor.getAttribute('href');
        window.history.replaceState(null, null, new_url);
    }
    go(new_url, false);

    window.addEventListener('popstate', function(e) {
        go(window.location.href, false);
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

function getIndexes()
{
    var indexes = getIndexes.indexes;

    if (!indexes.length) {
        downloadJson('api/indexes.json', {}, function(json) {
            if (json.length > 0) {
                indexes = json;
                for (var i = 0; i < indexes.length; i++)
                    indexes[i].init = false;
            } else {
                error = 'Aucune table disponible';
            }

            getIndexes.indexes = indexes;
        });
    }

    return indexes;
}
getIndexes.indexes = [];

function refreshIndexesLine(el, indexes, main_index)
{
    var old_g = el.querySelector('g');
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
                var color = '#ed6d0a';
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

    old_g.parentNode.replaceChild(g, old_g);
}

function moveIndex(relative_index)
{
    let indexes = getIndexes.indexes;

    let index = indexes.findIndex(function(index) { return index.begin_date === route.date; });
    if (index < 0)
        index = indexes.length - 1;

    let new_index = index + relative_index;
    if (new_index < 0 || new_index >= indexes.length)
        return;

    module.route({date: indexes[new_index].begin_date});
}
this.moveIndex = moveIndex;

// ------------------------------------------------------------------------
// Concepts
// ------------------------------------------------------------------------

function getConcepts(name)
{
    var sets = getConcepts.sets;
    var set = sets[name];

    if (!set.concepts.length) {
        downloadJson('concepts/' + name + '.json', {}, function(json) {
            set.concepts = json;
            set.map = {};
            for (var i = 0; i < json.length; i++) {
                var concept = json[i];
                set.map[concept[set.key]] = concept;
            }
        });
    }

    return [set.concepts, set.map];
}
getConcepts.sets = {
    'ccam': {key: 'procedure', concepts: [], map: {}},
    'cim10': {key: 'diagnosis', concepts: [], map: {}},
    'ghm_roots': {key: 'ghm_root', concepts: [], map: {}}
};
