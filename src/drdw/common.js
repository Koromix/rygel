const BaseUrl = '/';

// ------------------------------------------------------------------------
// DOM
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

function __(selector)
{
    return document.querySelectorAll(selector);
}

// ------------------------------------------------------------------------
// URL
// ------------------------------------------------------------------------

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
                ret[s[0]] = decodeURI(s[1]);
            }
            return ret;
        })(),
        hash: a.hash.replace('#', ''),
        path: a.pathname.replace(/^([^/])/, '/$1')
    };
}

function buildUrl(url, query_values)
{
    if (query_values === undefined)
        query_values = {};

    let query_fragments = [];
    for (k in query_values) {
        let value = query_values[k];
        if (value !== null && value !== undefined) {
            let arg = escape(k) + '=' + escape(value);
            query_fragments.push(arg);
        }
    }
    if (query_fragments)
        url += '?' + query_fragments.sort().join('&');

    return url;
}

// ------------------------------------------------------------------------
// JSON
// ------------------------------------------------------------------------

function downloadJson(url, func)
{
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
    'spec': null,

    'cm_view': 'global',
    'cm_diff': null,
    'cm_units': null,
    'cm_mode': 'exj2',
    'cm_cmd': null
};
var scroll_cache = {};
var module = null;

function markBusy(selector, busy)
{
    var elements = __(selector);
    for (var i = 0; i < elements.length; i++)
        elements[i].classList.toggle('busy', busy);
}

function toggleMenu(selector, enable)
{
    var el = _(selector);
    if (enable === undefined)
        enable = !el.classList.contains('active');
    if (enable) {
        var els = __('nav');
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

function buildModuleUrl(module_name)
{
    return BaseUrl + module_name;
}

var go_timer_id = null;
function go(new_url, mark_history, delay)
{
    if (new_url === undefined)
        new_url = null;
    if (mark_history === undefined)
        mark_history = true;

    if (go_timer_id) {
        clearTimeout(go_timer_id);
        go_timer_id = null;
    }
    if (delay) {
        go_timer_id = setTimeout(function() {
            go(new_url, mark_history);
        }, delay);
        return;
    }

    // Parse new URL
    let url_parts = new_url ? parseUrl(new_url) : route_url_parts;
    let app_url = url_parts.path.substr(BaseUrl.length);

    // Update scroll cache and history
    if (!route_url_parts || url_parts.href !== route_url_parts.href) {
        if (route_url_parts)
            scroll_cache[route_url_parts.path] = [window.pageXOffset, window.pageYOffset];

        if (mark_history) {
            window.history.pushState(null, null, url_parts.href);
        } else {
            window.history.replaceState(null, null, url_parts.href);
        }
    }

    // Hide all module-specific views
    addClass(__('main > div'), 'hide');
    addClass(__('#opt_menu > *'), 'hide');

    // Find relevant module and run
    var module_name = app_url.split('/')[0];
    module = window[module_name];
    if (module !== undefined && module.run !== undefined)
        module.run(route, app_url, url_parts.params, url_parts.hash);

    // Update URL to reflect real state (module may have set default values, etc.)
    let real_url = module.routeToUrl({}) + (url_parts.hash ? ('#' + url_parts.hash) : '');
    window.history.replaceState(null, null, real_url);
    route_url_parts = parseUrl(real_url);
    route_url = real_url.substr(BaseUrl.length);

    // Update menu state and links
    var menu_anchors = __('#side_menu a');
    for (var i = 0; i < menu_anchors.length; i++) {
        let anchor = menu_anchors[i];

        anchor.href = eval(anchor.dataset.url);
        let active = (route_url_parts.href.startsWith(anchor.href) &&
                      !anchor.classList.contains('category'));
        anchor.classList.toggle('active', active);
    }
    toggleMenu('#side_menu', false);

    // Update scroll target
    var scroll_target = scroll_cache[route_url_parts.path];
    if (route_url_parts.hash) {
        window.location.hash = route_url_parts.hash;
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
    if (window.location.pathname !== BaseUrl) {
        new_url = window.location.href;
    } else {
        var first_anchor = _('#side_menu a');
        new_url = eval(first_anchor.dataset.url);
        window.history.replaceState(null, null, new_url);
    }
    go(new_url, false);

    window.addEventListener('popstate', function(e) {
        go(window.location.href, false);
    });

    _('body').addEventListener('click', function(e) {
        if (e.target && e.target.tagName == 'A') {
            let href = e.target.getAttribute('href');
            if (href && !href.match(/^(?:[a-z]+:)?\/\//) && href[0] != '#') {
                go(href);
                e.preventDefault();
            }
        }
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
        downloadJson(BaseUrl + 'api/indexes.json', function(json) {
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

function refreshIndexesLine(el, indexes, main_index, show_table_changes)
{
    if (show_table_changes === undefined)
        show_table_changes = true;

    var old_g = el.querySelector('g');
    if (!old_g) {
        el.appendChild(createElementNS('svg', 'text',
                                       {x: 0, y: 25, 'text-anchor': 'start',
                                        style: 'cursor: pointer; font-size: 1.5em;'}, '≪'));
        el.appendChild(createElementNS('svg', 'line',
                                       {x1: '6%', y1: 20, x2: '94%', y2: 20, style: 'stroke: #888; stroke-width: 1;'}));
        el.appendChild(createElementNS('svg', 'g'));
        el.appendChild(createElementNS('svg', 'text',
                                       {x: '100%', y: 25, 'text-anchor': 'end',
                                        style: 'cursor: pointer; font-size: 1.5em;'}, '≫'));

        old_g = el.querySelector('g');
    }

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
            } else if (show_table_changes) {
                var color = '#888';
                var weight = 'normal';
            } else {
                continue;
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
        downloadJson(BaseUrl + 'concepts/' + name + '.json', function(json) {
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
