// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const {render, html, svg} = lithtml;

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

// For new code, prefer lighterhtml to this crap, unless you need
// compatibility with older browsers (e.g. for THOP).

let dom = (function() {
    let self = this;

    function expandNamespace(ns) {
        return ({
            svg: 'http://www.w3.org/2000/svg',
            html: 'http://www.w3.org/1999/xhtml'
        })[ns] || ns;
    }

    function create(ns, tag, attributes, children) {
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

    this.createProxy = function(ns, tag, args, args_idx) {
        let attributes;
        if (arguments.length > args_idx &&
                args[args_idx] instanceof Object &&
                !(args[args_idx] instanceof Node || Array.isArray(args[args_idx]))) {
            attributes = args[args_idx++];
        } else {
            attributes = {};
        }

        let el = create(ns, tag, attributes, []);
        for (let i = args_idx; i < args.length; i++)
            el.appendContent(args[i]);

        return el;
    };

    this.h = function(tag) { return self.createProxy('html', tag, arguments, 1); };
    this.s = function(tag) { return self.createProxy('svg', tag, arguments, 1); };

    return this;
}).call({});

// ------------------------------------------------------------------------
// Log
// ------------------------------------------------------------------------

let log = (function() {
    let self = this;

    this.defaultHandler = function(type, msg) {
        switch (type) {
            case 'debug':
            case 'info':
            case 'success': { console.log(msg); } break;
            case 'error': { console.error(msg); } break;
        }
    };

    let handlers = [self.defaultHandler];

    this.pushHandler = function(func) { handlers.push(func); };
    this.popHandler = function() { handlers.pop(); };

    function log(type, msg) {
        let func = handlers[handlers.length - 1];
        func(type, msg);
    }

    this.debug = function(msg) { log('debug', msg); };
    this.info = function(msg) { log('info', msg); };
    this.success = function(msg) { log('success', msg); };
    this.error = function(msg) { log('error', msg); };

    return this;
}).call({});

// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

let util = (function() {
    this.mapRange = function(start, end, func) {
        let len = end - start;
        let arr = Array.from({length: len});

        for (let i = 0; i < len; i++)
            arr[i] = func(start + i);

        return arr;
    };

    this.mapRLE = function(arr, func) {
        let arr2 = Array.from({length: arr.length});

        let arr2_len = 0;
        for (let i = 0; i < arr.length;) {
            let j = i + 1;
            while (j < arr.length && arr[j] === arr[i]) {
                j++;
            }

            arr2[arr2_len++] = func(arr[i], j - i);
            i = j;
        }
        arr2.length = arr2_len;

        return arr2;
    };

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
                    ret[s[0]] = decodeURIComponent(s[1]);
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

    this.strToValue = function(str) {
        try {
            return JSON.parse(str);
        } catch (err) {
            return undefined;
        }
    };
    this.valueToStr = function(value) {
        return JSON.stringify(value);
    };

    function encode30bits(value, buf, offset, len) {
        let characters = '0123456789ABCDEFGHJKMNPQRSTVWXYZ';

        buf[offset + 0] = characters[(value >>> 25) & 0x1F];
        buf[offset + 1] = characters[(value >>> 20) & 0x1F];
        buf[offset + 2] = characters[(value >>> 15) & 0x1F];
        buf[offset + 3] = characters[(value >>> 10) & 0x1F];
        buf[offset + 4] = characters[(value >>> 5) & 0x1F];
        buf[offset + 5] = characters[value & 0x1F];
    }

    this.makeULID = function() {
        let ulid = new Array(26).fill(0);

        // Generate time part
        {
            let time = Date.now();
            if (time < 0 || time >= 0x1000000000000)
                throw new Error('Cannot generate ULID with current UTC time');

            let now0 = Math.floor(time / 0x100000);
            let now1 = time % 0x100000;

            encode30bits(now1, ulid, 4);
            encode30bits(now0, ulid, 0);
        }

        // Generate random part
        {
            let rnd32 = new Uint32Array(3);
            crypto.getRandomValues(rnd32);

            encode30bits(rnd32[0], ulid, 10);
            encode30bits(rnd32[1], ulid, 16);
            encode30bits(rnd32[2], ulid, 20);
        }

        return ulid.join('');
    };

    // TODO: React to onerror?
    this.loadScript = function(url) {
        let head = document.querySelector('script');
        let script = document.createElement('script');

        return new Promise((resolve, reject) => {
            script.type = 'text/javascript';
            script.src = url;
            script.onreadystatechange = resolve;
            script.onload = resolve;

            head.appendChild(script);
        });
    };

    this.saveBlob = function(blob, filename) {
        let url = URL.createObjectURL(blob);

        let a = document.createElement('a');
        a.download = filename;
        a.href = url;

        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);

        if(URL.revokeObjectURL)
            setTimeout(function() { URL.revokeObjectURL(url) }, 60000);
    };

    return this;
}).call({});
