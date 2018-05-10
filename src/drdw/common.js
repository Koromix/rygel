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
    if (attr) {
        for (var key in attr) {
            value = attr[key];
            if (value !== null) {
                if (typeof value === 'function') {
                    el.addEventListener(key, value);
                } else {
                    el.setAttribute(key, value);
                }
            }
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
            if (value !== null) {
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

    var module_name = page_url.split('/')[0];
    removeClass(document.querySelectorAll('.page'), 'active');
    addClass(document.querySelectorAll('.page.page_' + module_name), 'active');

    var module = window[module_name];
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
