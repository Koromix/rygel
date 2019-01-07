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
// Data
// ------------------------------------------------------------------------

let data = {};
(function() {
    this.busyHandler = null;

    let self = this;
    let errors = [];
    let queue = new Set();
    let busy = 0;

    function get(url, type, proceed, fail)
    {
        let xhr = openRequest('GET', url, type, proceed, fail);
        if (!xhr)
            return;

        xhr.send();
    }
    this.get = get;

    function post(url, type, parameters, proceed, fail)
    {
        let xhr = openRequest('POST', url, type, proceed, fail);
        if (!xhr)
            return;

        let encoded_parameters = buildUrl('', parameters).substr(1);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send(encoded_parameters || null);
    }
    this.post = post;

    function openRequest(method, url, type, proceed, fail)
    {
        if (queue.has(url))
            return;
        queue.add(url);
        busy++;

        function handleFinishedRequest(status, response)
        {
            callRequestHandlers(url, proceed, fail, status, response);
            if (!--busy)
                callIdleHandler();
        }

        let xhr = new XMLHttpRequest();
        xhr.timeout = 14000;
        xhr.onload = function(e) { handleFinishedRequest(this.status, xhr.response); };
        xhr.onerror = function(e) { handleFinishedRequest(503); };
        xhr.ontimeout = function(e) { handleFinishedRequest(504); };
        if (type)
            xhr.responseType = type;
        xhr.open(method, url, true);

        if (self.busyHandler)
            self.busyHandler(true);

        return xhr;
    }

    function callRequestHandlers(url, proceed, fail, status, response)
    {
        let error = null;
        switch (status) {
            case 200: { /* Success */ } break;
            case 403: { error = 'Accès refusé'; } break;
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
            errors.push(error);
            if (fail)
                fail(error);
        }
    }

    function callIdleHandler()
    {
        if (self.busyHandler) {
            setTimeout(function() {
                self.busyHandler(false);
                if (!busy)
                    queue.clear();
            }, 0);
        } else {
            queue.clear();
        }
    }

    this.isBusy = function() { return !!busy; }
    this.getErrors = function() { return errors; }
    this.clearErrors = function() { errors = []; }
}).call(data);

// ------------------------------------------------------------------------
// Misc
// ------------------------------------------------------------------------

function compareValues(value1, value2)
{
    if (value1 < value2) {
        return -1;
    } else if (value1 > value2) {
        return 1;
    } else {
        return 0;
    }
}

function generateRandomInt(min, max)
{
    min = Math.ceil(min);
    max = Math.floor(max);
    return Math.floor(Math.random() * (max - min)) + min;
}

function lazyLoad(module, func)
{
    if (typeof LazyModules !== 'object' || !LazyModules[module]) {
        console.log('Cannot load module ' + module);
        return;
    }

    data.get(LazyModules[module], null, function(js) {
        eval.call(window, js);
        if (func)
            func();
    });
}
