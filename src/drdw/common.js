// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
// Random
// ------------------------------------------------------------------------

function generateRandomInt(min, max)
{
    min = Math.ceil(min);
    max = Math.floor(max);
    return Math.floor(Math.random() * (max - min)) + min;
}

// ------------------------------------------------------------------------
// JSON
// ------------------------------------------------------------------------

function downloadJson(method, url, proceed, fail)
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
            case 404: { error = 'Adresse \'' + url + '\' introuvable'; } break;
            case 422: { error = 'Paramètres incorrects'; } break;
            case 502:
            case 503: { error = 'Service non accessible'; } break;
            case 504: { error = 'Délai d\'attente dépassé, réessayez'; } break;
            default: { error = 'Erreur inconnue ' + status; } break;
        }

        if (!error) {
            proceed(response);
        } else {
            downloadJson.errors.push(error);
            if (fail)
                fail(error);
        }

        if (!--downloadJson.busy) {
            go();
            if (!downloadJson.busy)
                downloadJson.queue.clear();
        }
    }

    var xhr = new XMLHttpRequest();
    xhr.responseType = 'json';
    xhr.timeout = 14000;
    xhr.onload = function(e) { handleFinishedRequest(this.status, xhr.response); };
    xhr.onerror = function(e) { handleFinishedRequest(503); };
    xhr.ontimeout = function(e) { handleFinishedRequest(504); };
    if (method === 'POST') {
        parts = url.split('?', 2);
        xhr.open(method, parts[0], true);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send(parts.length > 1 ? parts[1] : null);
    } else {
        xhr.open(method, url, true);
        xhr.send();
    }
}
downloadJson.errors = [];
downloadJson.queue = new Set();
downloadJson.busy = 0;

// ------------------------------------------------------------------------
// Navigation
// ------------------------------------------------------------------------

// Routing
var route_modules = {};
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

    'cm_view': 'summary',
    'period': [null, null],
    'prev_period': [null, null],
    'mode': 'none',
    'units': [],
    'algorithm': null,

    'apply': false
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

function registerUrl(prefix, object, func)
{
    route_modules[prefix] = {
        object: object,
        func: func
    };
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
        if (mark_history)
            window.history.pushState(null, null, url_parts.href);
    }

    // Update user stuff
    {
        user.runSessionBox();
        let session = user.getSession();

        let menu_item;
        if (session) {
            menu_item = createElement('li', {},
                createElement('a', {href: '#',
                                    click: function(e) { user.disconnect(); e.preventDefault(); }},
                              'Se déconnecter (' + session.username + ')')
            );
        } else {
            menu_item = createElement('li', {},
                createElement('a', {href: user.routeToUrl()}, 'Se connecter')
            );
        }

        let old_menu_item = _('#side_session_menu');
        cloneAttributes(old_menu_item, menu_item);
        old_menu_item.parentNode.replaceChild(menu_item, old_menu_item);
    }

    // Find relevant module and run
    {
        let new_module_name = app_url.split('/')[0];
        let new_module = route_modules[new_module_name];

        if (new_module !== module)
            addClass(__('main > div'), 'hide');
        addClass(__('#opt_menu > *'), 'hide');

        module = new_module;
        if (module)
            module.func(route, app_url, url_parts.params, url_parts.hash);
    }

    // Update URL to reflect real state (module may have set default values, etc.)
    {
        let real_url = null;
        if (module)
            real_url = module.object.routeToUrl({});
        if (real_url) {
            if (url_parts.hash)
                real_url += '#' + url_parts.hash;

            window.history.replaceState(null, null, real_url);
            route_url_parts = parseUrl(real_url);
            route_url = real_url.substr(BaseUrl.length);
        } else {
            route_url_parts = url_parts;
            route_url = app_url;
        }
    }

    // Update side menu state and links
    var menu_anchors = __('#side_menu li a');
    for (var i = 0; i < menu_anchors.length; i++) {
        let anchor = menu_anchors[i];

        if (anchor.dataset.url) {
            let url = eval(anchor.dataset.url);
            anchor.classList.toggle('hide', !url);
            if (url)
                anchor.href = url;
        }

        let active = (route_url_parts.href.startsWith(anchor.href) &&
                      !anchor.classList.contains('category'));
        anchor.classList.toggle('active', active);
    }
    toggleMenu('#side_menu', false);

    // Hide page menu if empty
    let opt_hide = true;
    {
        let els = __('#opt_menu > *');
        for (let i = 0; i < els.length; i++) {
            if (!els[i].classList.contains('hide')) {
                opt_hide = false;
                break;
            }
        }
    }
    toggleClass(__('#opt_deploy, #opt_menu'), 'hide', opt_hide);

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
        var first_anchor = _('#side_menu a[data-url]');
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
// Resources
// ------------------------------------------------------------------------

function getIndexes()
{
    var indexes = getIndexes.indexes;

    if (!indexes.length) {
        let url = BaseUrl + 'api/indexes.json';
        downloadJson('GET', url, function(json) {
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

function getConcepts(name)
{
    var sets = getConcepts.sets;
    var set = sets[name];

    if (!set.concepts.length) {
        let url = BaseUrl + 'concepts/' + name + '.json';
        downloadJson('GET', url, function(json) {
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

// ------------------------------------------------------------------------
// View
// ------------------------------------------------------------------------

function refreshIndexesLine(el, indexes, main_index)
{
    let builder = new VersionLine;

    for (let i = 0; i < indexes.length; i++) {
        let index = indexes[i];
        builder.addVersion(index.begin_date, index.begin_date, index.changed_prices);
    }
    if (main_index >= 0)
        builder.setValue(indexes[main_index].begin_date);

    builder.changeHandler = function() {
        module.object.route({date: this.object.getValue()});
    };

    let svg = builder.getWidget();
    cloneAttributes(el, svg);
    el.parentNode.replaceChild(svg, el);
}

function refreshErrors(errors)
{
    var log = _('#log');

    log.innerHTML = errors.join('<br/>');
    log.classList.toggle('hide', !errors.length);
}
