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
    this.forEach(function(el) {
        el.classList.add(cls);
    });
};

Element.prototype.addClass = function(cls) {
    this.classList.add(cls);
};

NodeList.prototype.removeClass = function(cls) {
    this.forEach(function(el) {
        el.classList.remove(cls);
    });
};

Element.prototype.removeClass = function(cls, value) {
    this.classList.remove(cls);
};

NodeList.prototype.toggleClass = function(cls, value) {
    this.forEach(function(el) {
        el.classList.toggle(cls, value);
    });
};

Element.prototype.toggleClass = function(cls, value) {
    // 'return this.classList.toggle(cls, value);' should be enough but for some
    // reason it does not always work with old Chrome (42) versions.
    if (value !== undefined) {
        return this.classList.toggle(cls, value);
    } else {
        return this.classList.toggle(cls);
    }
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
        let self = this;
        els.forEach(function(el) {
            self.appendChildren(el);
        });
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

    for (const key in attributes) {
        const value = attributes[key];
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
            args[args_idx] instanceof Object &&
            !(args[args_idx] instanceof Node || Array.isArray(args[args_idx]))) {
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
    for (let i = 0; i < attributes.length; i++)
        this.setAttribute(attributes[i].nodeName, attributes[i].nodeValue);
}

Element.prototype.replaceWith = function(el) {
    this.parentNode.replaceChild(el, this);
}

// ------------------------------------------------------------------------
// Text
// ------------------------------------------------------------------------

function numberText(n)
{
    return n.toLocaleString('fr-FR');
}

function percentText(fraction)
{
    return fraction.toLocaleString('fr-FR',
                                   {style: 'percent', minimumFractionDigits: 1, maximumFractionDigits: 1});
}

function priceText(price_cents, format_cents)
{
    if (format_cents === undefined)
        format_cents = true;

    if (price_cents !== undefined) {
        let digits = format_cents ? 2 : 0;
        return (price_cents / 100.0).toLocaleString('fr-FR',
                                                    {minimumFractionDigits: digits, maximumFractionDigits: digits});
    } else {
        return '';
    }
}

// ------------------------------------------------------------------------
// Cookies
// ------------------------------------------------------------------------

// Why the f*ck is there still no good cookie API?
function getCookie(name)
{
    let cookie = ' ' + document.cookie;

    let start = cookie.indexOf(' ' + name + '=');
    if (start < 0)
        return null;

    start = cookie.indexOf('=', start) + 1;
    let end = cookie.indexOf(';', start);
    if (end < 0)
        end = cookie.length;

    let value = unescape(cookie.substring(start, end));

    return value;
}

// ------------------------------------------------------------------------
// URL
// ------------------------------------------------------------------------

function parseUrl(url)
{
    let a = document.createElement('a');
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
            let ret = {};
            let seg = a.search.replace(/^\?/, '').split('&');
            let len = seg.length;
            let s;
            for (let i = 0; i < len; i++) {
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
    for (const k in query_values) {
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

function needsRefresh(func, key, args)
{
    if (func.prev_args_json === undefined)
        func.prev_args_json = {};

    let args_json = JSON.stringify(args);

    if (args_json !== func.prev_args_json[key]) {
        func.prev_args_json[key] = args_json;
        return true;
    } else {
        return false;
    }
}

function generateRandomInt(min, max)
{
    min = Math.ceil(min);
    max = Math.floor(max);
    return Math.floor(Math.random() * (max - min)) + min;
}
