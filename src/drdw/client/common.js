// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
    if (query_fragments.length)
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
    user.runSession();

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

    data.idleHandler = go;
}

if (document.readyState === 'complete') {
    initNavigation();
} else {
    document.addEventListener('DOMContentLoaded', initNavigation);
}

// ------------------------------------------------------------------------
// View
// ------------------------------------------------------------------------

function refreshErrors(errors)
{
    var log = _('#log');

    log.innerHTML = errors.join('<br/>');
    log.classList.toggle('hide', !errors.length);
}
