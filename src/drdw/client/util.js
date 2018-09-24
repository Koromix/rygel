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
// Misc
// ------------------------------------------------------------------------

function generateRandomInt(min, max)
{
    min = Math.ceil(min);
    max = Math.floor(max);
    return Math.floor(Math.random() * (max - min)) + min;
}
