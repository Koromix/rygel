// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ------------------------------------------------------------------------
// DOM
// ------------------------------------------------------------------------

Element.prototype.query = function(selector) { return this.querySelector(selector); }
Element.prototype.queryAll = function(selector) { return this.querySelectorAll(selector); }
function query(selector) { return document.querySelector(selector); }
function queryAll(selector) { return document.querySelectorAll(selector); }

NodeList.prototype.addClass = function(cls) {
    for (let i = 0; i < this.length; i++)
        this[i].classList.add(cls);
};

Element.prototype.addClass = function(cls) {
    this.classList.add(cls);
};

NodeList.prototype.removeClass = function(cls) {
    for (let i = 0; i < this.length; i++)
        this[i].classList.remove(cls);
};

Element.prototype.removeClass = function(cls, value) {
    this.classList.remove(cls);
};

NodeList.prototype.toggleClass = function(cls, value) {
    for (let i = 0; i < this.length; i++)
        this[i].classList.toggle(cls, value);
};

Element.prototype.toggleClass = function(cls, value) {
    return this.classList.toggle(cls, value);
};

Element.prototype.hasClass = function(cls) {
    return this.classList.contains(cls);
};

Element.prototype.appendChildren = function(els)
{
    if (els === null || els === undefined) {
        // Skip
    } else if (typeof els === 'string') {
        this.appendChild(document.createTextNode(els));
    } else if (Array.isArray(els) || els instanceof NodeList) {
        for (var i = 0; i < els.length; i++)
            this.appendChildren(els[i]);
    } else {
        this.appendChild(els);
    }
}

function expandNamespace(ns)
{
    return ({
        svg: 'http://www.w3.org/2000/svg',
        html: 'http://www.w3.org/1999/xhtml'
    })[ns] || ns;
}

function createElement(ns, tag, attributes, children)
{
    ns = expandNamespace(ns);
    let el = document.createElementNS(ns, tag);

    for (let key in attributes) {
        value = attributes[key];
        if (value !== null && value !== undefined) {
            if (typeof value === 'function') {
                el.addEventListener(key, value.bind(el));
            } else {
                el.setAttribute(key, value);
            }
        }
    }

    el.appendChildren(children);

    return el;
}

function createElementProxy(ns, tag, args, args_idx)
{
    let attributes;
    if (arguments.length > args_idx &&
            args[args_idx] instanceof Object && !(args[args_idx] instanceof Element)) {
        attributes = args[args_idx++];
    } else {
        attributes = {};
    }

    let el = createElement(ns, tag, attributes, []);
    for (let i = args_idx; i < args.length; i++)
        el.appendChildren(args[i]);

    return el;
}

function elem(ns, tag) { return createElementProxy(ns, tag, arguments, 2); }
function html(tag) { return createElementProxy('html', tag, arguments, 1); }
function svg(tag) { return createElementProxy('svg', tag, arguments, 1); }

Element.prototype.copyAttributesFrom = function(el) {
    let attributes = el.attributes;
    for (var i = 0; i < attributes.length; i++) {
        this.setAttribute(attributes[i].nodeName, attributes[i].nodeValue);
    }
}

Element.prototype.replaceWith = function(el) {
    this.parentNode.replaceChild(el, this);
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
// Reactor
// ------------------------------------------------------------------------

function Reactor()
{
    let values = {};

    this.changed = function(mode) {
        let ret = false;

        let mode_values = values[mode];
        if (mode_values === undefined) {
            mode_values = [];
            values[mode] = mode_values;
        }

        let common_length = Math.min(arguments.length - 1, mode_values.length);
        for (let i = 0; i < common_length; i++) {
            if (arguments[i + 1] !== mode_values[i]) {
                mode_values[i] = arguments[i + 1];
                ret = true;
            }
        }

        for (let i = mode_values.length; i < arguments.length - 1; i++) {
            mode_values.push(arguments[i + 1]);
            ret = true;
        }

        return ret;
    }
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
