// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ------------------------------------------------------------------------
// Simpler DOM
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

Element.prototype.appendContent = function() {
    for (const el of arguments) {
        if (Array.isArray(el) || el instanceof NodeList) {
            let self = this;
            el.forEach(function(el) {
                self.appendContent(el);
            });
        } else if (el === null || el === undefined) {
            // Skip
        } else if (typeof el === 'string') {
            this.appendChild(document.createTextNode(el));
        } else {
            this.appendChild(el);
        }
    }
}

Element.prototype.replaceContent = function() {
    if (this.innerHTML !== undefined) {
        this.innerHTML = '';
    } else {
        while (widget.lastChild)
            widget.removeChild(widget.lastChild);
    }

    this.appendContent.apply(this, arguments);
}

// ------------------------------------------------------------------------
// DOM creation
// ------------------------------------------------------------------------

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
        let value = attributes[key];

        if (Array.isArray(value)) {
            value = value.filter(function(x) { return !!x; })
                         .map(function(x) { return '' + x; })
                         .join(' ');
            el.setAttribute(key, value);
        } else if (value === null || value === undefined) {
            // Skip attribute
        } else if (typeof value === 'function') {
            el.addEventListener(key, value.bind(el));
        } else if (typeof value === 'boolean') {
            if (value)
                el.setAttribute(key, '');
        } else {
            el.setAttribute(key, value);
        }
    }

    el.appendContent(children);

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
        el.appendContent(args[i]);

    return el;
}

function elem(ns, tag) { return createElementProxy(ns, tag, arguments, 2); }
function html(tag) { return createElementProxy('html', tag, arguments, 1); }
function svg(tag) { return createElementProxy('svg', tag, arguments, 1); }

// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

let util = (function() {
    // Why the f*ck is there still no good cookie API?
    this.getCookie = function(name) {
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
    };

    this.parseUrl = function(url) {
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
            params: (function() {
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
    };

    this.buildUrl = function(url, query_values) {
        if (query_values === undefined)
            query_values = {};

        let query_fragments = [];
        for (const k in query_values) {
            let value = query_values[k];
            if (value !== null && value !== undefined) {
                let arg = encodeURIComponent(k) + '=' + encodeURIComponent(value);
                query_fragments.push(arg);
            }
        }
        if (query_fragments.length)
            url += '?' + query_fragments.sort().join('&');

        return url;
    };

    this.roundTo = function(n, digits) {
        if (digits === undefined)
            digits = 0;

        let multiplicator = Math.pow(10, digits);
        n = parseFloat((n * multiplicator).toFixed(11));
        return Math.round(n) / multiplicator;
    };

    this.compareValues = function(value1, value2) {
        if (value1 < value2) {
            return -1;
        } else if (value1 > value2) {
            return 1;
        } else {
            return 0;
        }
    };

    this.saveBlob = function(blob, filename) {
        let url = URL.createObjectURL(blob);
        let anchor = html('a', {
            download: filename,
            href: url
        });

        document.body.appendChild(anchor);
        anchor.click();
        document.body.removeChild(anchor);

        if(URL.revokeObjectURL)
            setTimeout(function() { URL.revokeObjectURL(url) }, 60000);
    };

    return this;
}).call({});
