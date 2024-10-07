// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import './common.css';

// ------------------------------------------------------------------------
// Compatibility
// ------------------------------------------------------------------------

if (String.prototype.format == null) {
    String.prototype.format = function(...args) {
        let str = this.replace(/{(\d+)}/g, (match, idx) => {
            idx = parseInt(idx, 10) - 1;

            if (idx >= args.length)
                return match;

            let arg = args[idx];
            return (arg != null) ? arg : '';
        });

        return str;
    }
}

if (typeof Blob !== 'undefined' && !Blob.prototype.text) {
    Blob.prototype.text = function() {
        return new Promise((resolve, reject) => {
            var reader = new FileReader;

            reader.onload = e => resolve(e.target.result);
            reader.onerror = e => reject(new Error(e.target.error));

            reader.readAsText(this);
        });
    };
}

// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

const Util = new function() {
    let self = this;

    this.clamp = function(value, min, max) {
        return Math.min(Math.max(value, min), max);
    };

    this.map = function*(obj, func) {
        for (let it of obj)
            yield func(it);
    };

    this.mapRange = function*(start, end, func) {
        for (let i = start; i < end; i++)
            yield func(i);
    };

    this.mapRLE = function*(arr, key_func, func) {
        for (let i = 0; i < arr.length;) {
            let current_key = key_func(arr[i]);

            let j = i + 1;
            while (j < arr.length && key_func(arr[j]) === current_key)
                j++;

            yield func(current_key, i, j - i);
            i = j;
        }
    };

    this.arrayToObject = function(iter, key_func, value_func = it => it) {
        let obj = {};

        for (let it of iter) {
            let key = key_func(it);
            obj[key] = value_func(it);
        }

        return obj;
    };

    this.assignDeep = function(obj, ...sources) {
        for (let src of sources)
            assignDeep1(obj, src);

        return obj;
    };

    function assignDeep1(obj, src) {
        for (let key in src) {
            let from = src[key];

            if (self.isPodObject(from) && !Object.isFrozen(from)) {
                let to = obj[key];

                if (!self.isPodObject(to)) {
                    to = {};
                    obj[key] = to;
                }

                assignDeep1(to, from);
            } else {
                obj[key] = from;
            }
        }
    }

    this.isPodObject = function(value) {
        return !!value && value.constructor === Object;
    };

    this.isNumeric = function(n) {
        return !isNaN(parseFloat(n)) && isFinite(n);
    };

    this.deepFreeze = function(obj) {
        Object.freeze(obj);

        for (let key in obj) {
            let value = obj[key];
            if (!Object.isFrozen(value) && (typeof value === 'object' ||
                                            typeof value === 'function'))
                self.deepFreeze(value);
        }

        return obj;
    };

    this.pasteURL = function(url, params = {}, hash = null) {
        let query = new URLSearchParams;

        for (let key in params) {
            let value = params[key];
            if (value != null)
                query.set(key, value.toString());
        }

        let query_str = query.toString();
        return `${url}${query_str ? '?' : ''}${query_str || ''}${hash ? '#' : ''}${hash || ''}`;
    };

    // Why the f*ck is there still no good cookie API?
    this.getCookie = function(key, default_value = undefined) {
        let cookies = document.cookie;
        let offset = 0;

        while (offset < cookies.length) {
            let name_end = cookies.indexOf('=', offset);
            let value_end = cookies.indexOf(';', name_end + 1);
            if (value_end < 0)
                value_end = cookies.length;

            let name = cookies.substring(offset, name_end);
            name = decodeURIComponent(name);

            if (name === key) {
                let value = cookies.substring(name_end + 1, value_end);
                value = decodeURIComponent(value);

                return value;
            }

            // Find next cookie
            offset = value_end + 1;
            while (cookies[offset] === ' ')
                offset++;
        }

        return default_value;
    };

    this.setCookie = function(name, value, path, seconds = null) {
        let cookie = `${name}=${encodeURIComponent(value)}; Path=${path}; SameSite=Lax;`;

        if (seconds != null)
            cookie += ` Max-Age=${seconds};`;

        document.cookie = cookie;
    };

    this.deleteCookie = function(name, path) {
        self.setCookie(name, '', path, 0);
    };

    this.roundTo = function(n, digits) {
        if (digits === undefined)
            digits = 0;

        let multiplicator = Math.pow(10, digits);
        n = parseFloat((n * multiplicator).toFixed(11));
        return Math.round(n) / multiplicator;
    };

    this.makeComparator = function(key_func, locales, options = {}) {
        let collator = new Intl.Collator(locales, options);

        if (key_func != null) {
            let func = (a, b) => {
                a = key_func(a);
                b = key_func(b);

                return collator.compare(a, b);
            };

            return func;
        } else {
            return collator.compare;
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

    function encode30bits(value, buf, offset) {
        let characters = '0123456789ABCDEFGHJKMNPQRSTVWXYZ';

        buf[offset + 0] = characters[(value >>> 25) & 0x1F];
        buf[offset + 1] = characters[(value >>> 20) & 0x1F];
        buf[offset + 2] = characters[(value >>> 15) & 0x1F];
        buf[offset + 3] = characters[(value >>> 10) & 0x1F];
        buf[offset + 4] = characters[(value >>> 5) & 0x1F];
        buf[offset + 5] = characters[value & 0x1F];
    }

    this.decodeULIDTime = function(ulid) {
        if (ulid.length !== 26)
            throw new Error('Invalid ULID string (must have 26 characters)');

        let time = decodeByte(ulid[0]) * 35184372088832 +
                   decodeByte(ulid[1]) * 1099511627776 +
                   decodeByte(ulid[2]) * 34359738368 +
                   decodeByte(ulid[3]) * 1073741824 +
                   decodeByte(ulid[4]) * 33554432 +
                   decodeByte(ulid[5]) * 1048576 +
                   decodeByte(ulid[6]) * 32768 +
                   decodeByte(ulid[7]) * 1024 +
                   decodeByte(ulid[8]) * 32 +
                   decodeByte(ulid[9]);

        return time;
    };

    function decodeByte(c) {
        switch (c) {
            case '0': return 0;
            case '1': return 1;
            case '2': return 2;
            case '3': return 3;
            case '4': return 4;
            case '5': return 5;
            case '6': return 6;
            case '7': return 7;
            case '8': return 8;
            case '9': return 9;
            case 'A': return 10;
            case 'B': return 11;
            case 'C': return 12;
            case 'D': return 13;
            case 'E': return 14;
            case 'F': return 15;
            case 'G': return 16;
            case 'H': return 17;
            case 'J': return 18;
            case 'K': return 19;
            case 'M': return 20;
            case 'N': return 21;
            case 'P': return 22;
            case 'Q': return 23;
            case 'R': return 24;
            case 'S': return 25;
            case 'T': return 26;
            case 'V': return 27;
            case 'W': return 28;
            case 'X': return 29;
            case 'Y': return 30;
            case 'Z': return 31;
        }

        throw new Error('Invalid ULID string (incorrect character)');
    }

    this.saveFile = function(blob, filename) {
        let url = URL.createObjectURL(blob);

        let a = document.createElement('a');
        a.download = filename;
        a.href = url;

        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);

        if (URL.revokeObjectURL)
            setTimeout(() => URL.revokeObjectURL(url), 60000);
    };
    this.saveBlob = this.saveFile;

    this.loadFile = async function() {
        let file = await new Promise((resolve, reject) => {
            let input = document.createElement('input');
            input.type = 'file';

            let cancel = true;

            // Detect file selection
            input.onchange = async (e) => {
                let file = e.target.files[0];

                if (file != null) {
                    resolve(file);
                    cancel = false;
                } else {
                    reject();
                }
            };

            // Hack to detect cancellation... great API!
            let teardown = () => {
                document.body.removeEventListener('focus', teardown, true);

                setTimeout(() => {
                    if (cancel)
                        reject();
                }, 200);
            };
            document.body.addEventListener('focus', teardown, true);

            input.click();
        });

        return file;
    };

    this.locateEvalError = function(err) {
        let location = null;

        if (err instanceof SyntaxError && err.lineNumber != null) {
            // At least Firefox seems to do well in this case, it's better than nothing!

            location = {
                line: err.lineNumber - 2,
                column: err.columnNumber || 1
            };
        } else if (err.stack) {
            let lines = String(err.stack).split('\n');

            for (let str of lines) {
                let m = null;

                if (m = str.match(/ > (?:Async)?Function:([0-9]+):([0-9]+)/) ||
                        str.match(/, <anonymous>:([0-9]+):([0-9]+)/)) {
                    // Can someone explain to me why do I have to offset by -2?
                    location = {
                        line: parseInt(m[1], 10) - 2,
                        column: parseInt(m[2], 10)
                    };
                    break;
                } else if (m = str.match(/Function code:([0-9]+):([0-9]+)/)) {
                    location = {
                        line: parseInt(m[1], 10),
                        column: parseInt(m[2], 10)
                    };
                    break;
                }
            }
        }

        return location;
    };

    this.findParent = function(el, func) {
        while (el && !func(el))
            el = el.parentElement;
        return el;
    };

    this.interceptLocalAnchors = function(func) {
        document.body.addEventListener('click', e => {
            if (!e.defaultPrevented && !e.ctrlKey) {
                let target = self.findParent(e.target, el => el.tagName === 'a' ||
                                                             el.tagName === 'A');

                if (target == null)
                    return;
                if (target.getAttribute('download') != null)
                    return;
                if (target.getAttribute('target') == '_blank')
                    return;

                let href = target.getAttribute('href');

                if (href) {
                    if (!href.match(/^[a-z]+:/)) {
                        func(e, href);
                        e.preventDefault();
                    }
                } else {
                    e.preventDefault();
                }
            }
        });
    };

    this.formatDiskSize = function(size) {
        if (size >= 1000 * 1000 * 1000) {
            return `${(size / 1000 / 1000 / 1000).toFixed(1)} GB`;
        } else if (size >= 1000 * 1000) {
            return `${(size / 1000 / 1000).toFixed(1)} MB`;
        } else if (size >= 1000) {
            return `${(size / 1000).toFixed(1)} kB`;
        } else {
            return `${size} B`;
        }
    };

    this.formatDiskMemorySize = function(size) {
        if (size >= 1024 * 1024 * 1024) {
            return `${(size / 1024 / 1024 / 1024).toFixed(1)} GB`;
        } else if (size >= 1024 * 1024) {
            return `${(size / 1024 / 1024).toFixed(1)} MB`;
        } else if (size >= 1024) {
            return `${(size / 1024).toFixed(1)} kB`;
        } else {
            return `${size} B`;
        }
    };

    this.formatDay = function(day) {
        switch (day) {
            case 1: { return 'Lundi'; } break;
            case 2: { return 'Mardi'; } break;
            case 3: { return 'Mercredi'; } break;
            case 4: { return 'Jeudi'; } break;
            case 5: { return 'Vendredi'; } break;
            case 6: { return 'Samedi'; } break;
            case 7: { return 'Dimanche'; } break;
        }
    };

    this.formatTime = function(time) {
        let hour = Math.floor(time / 100);
        let min = time % 100;

        return `${hour}h${min ? ('' + min).padStart(2, '0') : ''}`
    };

    this.formatDuration = function(min) {
        let hours = Math.floor(min / 60);
        min %= 60;

        if (hours >= 1 && !min) {
            return `${hours} heure${hours > 1 ? 's' : ''}`;
        } else if (hours >= 1) {
            return `${hours}h${min}`;
        } else {
            return `${min} minute${min > 1 ? 's' : ''}`;
        }
    };

    this.getRandomInt = function(min, max) {
        min = Math.ceil(min);
        max = Math.floor(max);

        let rnd = Math.floor(Math.random() * (max - min)) + min;
        return rnd;
    };

    this.waitFor = function(ms) {
        return new Promise((resolve, reject) => {
            setTimeout(resolve, ms);
        });
    };

    this.capitalize = function(str) {
        if (str.length > 1) {
            return str[0].toUpperCase() + str.substr(1);
        } else {
            return str.toUpperCase();
        }
    };

    this.serialize = function(func, mutex = new Mutex) {
        return (...args) => mutex.run(() => func(...args));
    };
};

// ------------------------------------------------------------------------
// Mutex
// ------------------------------------------------------------------------

function Mutex() {
    let self = this;

    let root = {
        prev: null,
        next: null
    };
    root.prev = root;
    root.next = root;

    let chain = false;

    this.run = async function(func) {
        let locked = await lock();

        try {
            let ret = await func();
            return ret;
        } catch (err) {
            throw err;
        } finally {
            if (locked)
                unlock();
        }
    };

    function lock() {
        if (chain) {
            chain = false;
            return false;
        }

        return new Promise((resolve, reject) => {
            let node = {
                prev: root.prev,
                next: root,
                resolve: resolve
            };

            root.prev.next = node;
            root.prev = node;

            if (node.prev === root)
                resolve(true);
        });
    };

    function unlock() {
        if (root.next !== root) {
            let node = root.next;

            node.next.prev = node.prev;
            node.prev.next = node.next;
        }

        if (root.next !== root) {
            let node = root.next;
            node.resolve(true);
        }
    };

    this.chain = function(func) {
        chain = true;
        return func();
    };
}

// ------------------------------------------------------------------------
// Log
// ------------------------------------------------------------------------

const Log = new function() {
    let self = this;

    let handlers = [];

    // Log to console
    this.defaultHandler = function(action, entry) {
        if (action !== 'close') {
            switch (entry.type) {
                case 'debug':
                case 'info':
                case 'success':
                case 'progress': { console.log(entry.msg); } break;
                case 'warning':
                case 'error': { console.error(entry.msg); } break;
            }
        }
    };
    handlers.push(self.defaultHandler);

    this.defaultTimeout = 3000;

    this.pushHandler = function(func) { handlers.push(func); };
    this.popHandler = function() { handlers.pop(); };

    function updateEntry(entry, type, msg, timeout) {
        let func = handlers[handlers.length - 1];
        let is_new = (entry.type == null);

        if (typeof msg == 'object' && msg != null)
            msg = msg.msg || msg.message || msg;

        entry.type = type;
        entry.msg = msg;

        if (entry.timer_id != null) {
            clearTimeout(entry.timer_id);
            entry.timer_id = null;
        }
        if (timeout >= 0)
            entry.timer_id = setTimeout(() => closeEntry(entry), timeout);

        func(is_new ? 'open' : 'edit', entry);

        return entry;
    }

    function closeEntry(entry) {
        if (entry.type != null) {
            let func = handlers[handlers.length - 1];
            func('close', entry);
            entry.type = null;
        }
    }

    this.Entry = function() {
        let self = this;

        this.type = null;
        this.msg = null;
        this.timer_id = null;

        this.debug = function(msg, timeout = Log.defaultTimeout) { return updateEntry(self, 'debug', msg, timeout); };
        this.info = function(msg, timeout = Log.defaultTimeout) { return updateEntry(self, 'info', msg, timeout); };
        this.success = function(msg, timeout = Log.defaultTimeout) { return updateEntry(self, 'success', msg, timeout); };
        this.warning = function(msg, timeout = Log.defaultTimeout) { return updateEntry(self, 'warning', msg, timeout); };
        this.error = function(msg, timeout = Log.defaultTimeout) { return updateEntry(self, 'error', msg, timeout); };

        this.progress = function(action, value = null, max = null) {
            if (value != null) {
                let msg = `${action}: ${value}${max != null ? ('/' + max) : ''}`;
                return updateEntry(self, 'progress', msg, -1);
            } else {
                return updateEntry(self, 'progress', action, -1);
            }
        };

        this.close = function() { closeEntry(self); };
    };

    this.debug = function(msg, timeout = Log.defaultTimeout) { return (new self.Entry).debug(msg, timeout); };
    this.info = function(msg, timeout = Log.defaultTimeout) { return (new self.Entry).info(msg, timeout); };
    this.success = function(msg, timeout = Log.defaultTimeout) { return (new self.Entry).success(msg, timeout); };
    this.warning = function(msg, timeout = Log.defaultTimeout) { return (new self.Entry).warning(msg, timeout); };
    this.error = function(msg, timeout = Log.defaultTimeout) { return (new self.Entry).error(msg, timeout); };
    this.progress = function(action, value = null, max = null) { return (new self.Entry).progress(action, value, max); };
};

// ------------------------------------------------------------------------
// Network
// ------------------------------------------------------------------------

class NetworkError extends Error {
    constructor() {
        super('Request failure: network error');
    }
}

class HttpError extends Error {
    constructor(status, msg) {
        super(msg);
        this.status = status;
    }
};

const Net = new function() {
    let self = this;

    this.retryHandler = response => false;

    this.fetch = async function(url, options = {}) {
        options = Object.assign({}, options);

        if (options.credentials == null)
            options.credentials = 'same-origin';
        if (options.headers == null)
            options.headers = {};
        if (!options.hasOwnProperty('timeout') && options.signal == null)
            options.timeout = 15000;
        if (!options.headers.hasOwnProperty('X-Requested-With'))
            options.headers['X-Requested-With'] = 'XMLHTTPRequest';

        for (;;) {
            let timer;
            let response;

            if (typeof AbortController !== 'undefined' && options.timeout != null) {
                let controller = new AbortController;
                options.signal = controller.signal;

                timer = setTimeout(() => controller.abort(), options.timeout);
            }

            try {
                response = await fetch(url, options);
            } catch (err) {
                throw new NetworkError;
            }

            if (timer != null)
                clearTimeout(timer);

            if (!response.ok) {
                let retry = await self.retryHandler(response);

                if (retry)
                    continue;
            }

            return response;
        }
    };

    this.get = async function(url, options = {}) {
        let response = await self.fetch(url, options);

        if (!response.ok) {
            let msg = await self.readError(response);
            throw new HttpError(response.status, msg);
        }

        let json = await response.json();
        return json;
    };

    this.post = async function(url, obj = null, options = {}) {
        options = Object.assign({}, options, {
            method: 'POST',
            headers: {
                'X-Requested-With': 'XMLHTTPRequest',
                'Content-Type': 'application/json'
            },
            body: (obj != null) ? JSON.stringify(obj) : null
        });

        let response = await self.fetch(url, options);

        if (!response.ok) {
            let msg = await self.readError(response);
            throw new HttpError(response.status, msg);
        }

        let json = await response.json();
        return json;
    };

    this.cache = async function(key, url) {
        let entry = caches[key];
        let now = performance.now();

        if (entry == null) {
            entry = {
                url: null,
                time: -self.cache_duration,
                data: null
            };
            caches[key] = entry;
        }

        if (entry.url != url || entry.time < now - self.cache_duration) {
            entry.data = await self.get(url);

            if (entry.data == null) {
                delete(caches[key]);
                return null;
            }

            entry.time = performance.now();
            entry.url = url;
        }

        return entry.data;
    };

    this.loadScript = function(url) {
        return new Promise((resolve, reject) => {
            let script = document.createElement('script');
            script.type = 'text/javascript';
            script.src = url;

            script.onload = e => resolve(script);
            script.onerror = e => reject(new Error(`Failed to load '${url}' script`));

            document.head.appendChild(script);
        });
    };

    this.loadImage = async function(url) {
        let response = await self.fetch(url);

        let blob = await response.blob();
        let img = await createImageBitmap(blob);

        return img;
    };

    this.readError = async function(response, nice = true) {
        let text = (await response.text()).trim();

        if (nice && text.match(/^Error [0-9]+:/)) {
            let idx = text.indexOf('\n');
            if (idx >= 0)
                text = text.substr(idx + 1).trim();
        }

        return text;
    };
};

// ------------------------------------------------------------------------
// Containers
// ------------------------------------------------------------------------

function LruMap(limit) {
    if (limit == null || limit < 2)
        throw new Error('LruMap limit must be >= 2');

    let self = this;

    let map = {};
    let size = 0;

    Object.defineProperties(this, {
        size: { get: () => size, enumerable: true }
    });

    let root_bucket = {
        prev: null,
        next: null
    };
    root_bucket.prev = root_bucket;
    root_bucket.next = root_bucket;

    function link(bucket) {
        bucket.prev = root_bucket.prev;
        bucket.next = root_bucket;
        root_bucket.prev.next = bucket;
        root_bucket.prev = bucket;
    }

    function unlink(bucket) {
        bucket.prev.next = bucket.next;
        bucket.next.prev = bucket.prev;
    }

    this.set = function(key, value) {
        let bucket = map[key];

        if (bucket) {
            bucket.value = value;

            if (root_bucket.prev !== bucket) {
                unlink(bucket);
                link(bucket);
            }
        } else {
            if (size >= limit)
                deleteBucket(root_bucket.next);

            bucket = {
                key: key,
                value: value,
                prev: null,
                next: null
            };

            map[key] = bucket;
            link(bucket);
            size++;
        }
    };

    this.delete = function(key) {
        let bucket = map[key];
        if (bucket)
            deleteBucket(bucket);
    };

    function deleteBucket(bucket) {
        unlink(bucket);
        delete map[bucket.key];
        size--;
    }

    this.get = function(key) {
        let bucket = map[key];

        if (bucket) {
            if (bucket.next !== root_bucket) {
                unlink(bucket);
                link(bucket);
            }

            return bucket.value;
        } else {
            return undefined;
        }
    };

    this.has = function(key) {
        let bucket = map[key];
        return bucket !== undefined;
    };

    this.newest = function() { return root_bucket.prev.key; };
    this.oldest = function() { return root_bucket.next.key; };

    this.clear = function() {
        root_bucket.prev = root_bucket;
        root_bucket.next = root_bucket;

        map = {};
        size = 0;
    };

    this.entries = function*() {
        let it = root_bucket.next;
        while (it !== root_bucket) {
            yield [it.key, it.value];
            it = it.next;
        }
    };
    this[Symbol.iterator] = self.entries;

    this.keys = function*() {
        let it = root_bucket.next;
        while (it !== root_bucket) {
            yield it.key;
            it = it.next;
        }
    };

    this.values = function*() {
        let it = root_bucket.next;
        while (it !== root_bucket) {
            yield it.value;
            it = it.next;
        }
    };
}

function BTree(order = 64) {
    if (order < 3)
        throw new Error('BTree order must be at least 3');

    let self = this;

    let min_node_keys = Math.floor((order - 1) / 2);
    let max_node_keys = order - 1;
    let min_leaf_keys = Math.floor(order / 2);
    let max_leaf_keys = order;

    let root;
    let leaf0;

    this.size = 0;

    // Also used for initialization
    this.clear = function() {
        root = {
            keys: [],
            values: [],
            next: null
        };
        leaf0 = root;

        self.size = 0;
    };

    this.get = function(key) {
        let leaf = findLeaf(key);
        let idx = leaf.keys.indexOf(key);
        return (idx >= 0) ? leaf.values[idx] : undefined;
    };

    this.has = function(key) {
        let leaf = findLeaf(key);
        return leaf.keys.includes(key);
    };

    function findLeaf(key) {
        let node = root;

        while (!node.values) {
            let idx = node.keys.findIndex(key2 => key2 > key);

            if (idx >= 0) {
                node = node.children[idx];
            } else {
                node = node.children[node.children.length - 1];
            }
        }

        return node;
    }

    function findLeafPath(key) {
        let path = [root];
        let node = root;

        while (!node.values) {
            let idx = node.keys.findIndex(key2 => key2 > key);

            if (idx >= 0) {
                node = node.children[idx];
            } else {
                node = node.children[node.children.length - 1];
            }

            path.push(node);
        }

        return path;
    }

    this.entries = function*() {
        let leaf = leaf0;

        while (leaf) {
            for (let i = 0; i < leaf.keys.length; i++) {
                let it = [leaf.keys[i], leaf.values[i]];
                yield it;
            }
            leaf = leaf.next;
        }
    };
    this[Symbol.iterator] = self.entries;

    this.keys = function*() {
        let leaf = leaf0;

        while (leaf) {
            for (let i = 0; i < leaf.keys.length; i++)
                yield leaf.keys[i];
            leaf = leaf.next;
        }
    };

    this.values = function*() {
        let leaf = leaf0;

        while (leaf) {
            for (let i = 0; i < leaf.keys.length; i++)
                yield leaf.values[i];
            leaf = leaf.next;
        }
    };

    this.set = function(key, value) {
        let path = findLeafPath(key);
        let leaf = path[path.length - 1];

        let insert_idx = leaf.keys.findIndex(key2 => key2 >= key);
        if (insert_idx < 0)
            insert_idx = leaf.keys.length;

        if (leaf.keys[insert_idx] === key) {
            leaf.values[insert_idx] = value;
        } else {
            leaf.keys.splice(insert_idx, 0, key);
            leaf.values.splice(insert_idx, 0, value);

            if (leaf.keys.length > max_leaf_keys)
                splitLeaf(path);

            self.size++;
        }
    };

    function splitLeaf(path) {
        let leaf0 = path[path.length - 1];
        let leaf1 = {
            keys: leaf0.keys.splice(min_leaf_keys, leaf0.keys.length - min_leaf_keys),
            values: leaf0.values.splice(min_leaf_keys, leaf0.values.length - min_leaf_keys),
            next: leaf0.next
        };
        leaf0.next = leaf1;

        insertChild(path, path.length - 2, leaf1);
    }

    function insertChild(path, idx, child) {
        let key = child.keys[0];

        while (idx >= 0) {
            let node = path[idx];

            let insert_idx = node.keys.findIndex(key2 => key2 > key);
            if (insert_idx < 0)
                insert_idx = node.keys.length;

            node.keys.splice(insert_idx, 0, key);
            node.children.splice(insert_idx + 1, 0, child);

            if (node.keys.length > max_node_keys) {
                let split_idx = min_node_keys + 1;

                child = {
                    keys: node.keys.splice(split_idx, node.keys.length - split_idx),
                    children: node.children.splice(split_idx, node.children.length - split_idx)
                };
                key = node.keys.pop();
            } else {
                return;
            }

            idx--;
        }

        // Root node is full, split it
        root = {
            keys: [key],
            children: [root, child]
        };
    }

    this.delete = function(key) {
        let path = findLeafPath(key);
        let leaf = path[path.length - 1];

        let delete_idx = leaf.keys.indexOf(key);

        if (delete_idx >= 0) {
            leaf.keys.splice(delete_idx, 1);
            leaf.values.splice(delete_idx, 1);

            if (leaf !== root) {
                if (!delete_idx)
                    fixInternalKeys(path, key, leaf.keys[0]);
                if (leaf.keys.length < min_leaf_keys)
                    balanceLeaf(path);
            }

            self.size--;
        }
    };

    function balanceLeaf(path) {
        let leaf = path[path.length - 1];
        let parent = path[path.length - 2];

        let leaf_idx = parent.children.indexOf(leaf);
        let sibling0 = parent.children[leaf_idx - 1];
        let sibling1 = parent.children[leaf_idx + 1];

        if (sibling0 && sibling0.keys.length > min_leaf_keys) {
            leaf.keys.unshift(sibling0.keys.pop());
            leaf.values.unshift(sibling0.values.pop());

            fixInternalKeys(path, leaf.keys[1], leaf.keys[0]);
        } else if (sibling1 && sibling1.keys.length > min_leaf_keys) {
            fixInternalKeys(path, sibling1.keys[0], sibling1.keys[1]);

            leaf.keys.push(sibling1.keys.shift());
            leaf.values.push(sibling1.values.shift());
        } else if (sibling0) {
            sibling0.keys.push(...leaf.keys);
            sibling0.values.push(...leaf.values);
            sibling0.next = leaf.next;

            parent.keys.splice(leaf_idx - 1, 1);
            parent.children.splice(leaf_idx, 1);
        } else {
            leaf.keys.push(...sibling1.keys);
            leaf.values.push(...sibling1.values);
            leaf.next = sibling1.next;

            parent.keys.splice(leaf_idx, 1);
            parent.children.splice(leaf_idx + 1, 1);
        }

        if (parent.keys.length < min_node_keys)
            balanceNode(path, path.length - 2);
    }

    function fixInternalKeys(path, old_key, new_key) {
        for (let i = path.length - 2; i >= 0; i--) {
            let parent = path[i];

            let fix_idx = parent.keys.indexOf(old_key);

            if (!fix_idx) {
                parent.keys[fix_idx] = new_key;
            } else if (fix_idx > 0) {
                parent.keys[fix_idx] = new_key;
                break;
            }
        }
    }

    function balanceNode(path, idx) {
        while (idx > 0) {
            let node = path[idx];
            let parent = path[idx - 1];

            let node_idx = parent.children.indexOf(node);
            let sibling0 = parent.children[node_idx - 1];
            let sibling1 = parent.children[node_idx + 1];

            if (sibling0 && sibling0.keys.length > min_node_keys) {
                node.keys.unshift(parent.keys[node_idx - 1]);
                parent.keys[node_idx - 1] = sibling0.keys.pop();
                node.children.unshift(sibling0.children.pop());
            } else if (sibling1 && sibling1.keys.length > min_node_keys) {
                node.keys.push(parent.keys[node_idx]);
                parent.keys[node_idx] = sibling1.keys.shift();
                node.children.push(sibling1.children.shift());
            } else if (sibling0) {
                sibling0.keys.push(parent.keys[node_idx - 1]);
                parent.keys.splice(node_idx - 1, 1);
                parent.children.splice(node_idx, 1);

                sibling0.keys.push(...node.keys);
                sibling0.children.push(...node.children);
            } else {
                node.keys.push(parent.keys[node_idx]);
                parent.keys.splice(node_idx, 1);
                parent.children.splice(node_idx + 1, 1);

                node.keys.push(...sibling1.keys);
                node.children.push(...sibling1.children);
            }

            if (parent.keys.length >= min_node_keys)
                return;

            idx--;
        }

        // Empty root, promote only child
        if (!root.keys.length)
            root = root.children[0];
    }

    self.clear();
}

// ------------------------------------------------------------------------
// Date and time
// ------------------------------------------------------------------------

let fmt_crazy = (new Date(2222, 4, 2)).toLocaleDateString().endsWith('2222');
let fmt_sep = (new Date(2222, 4, 2)).toLocaleDateString().includes('/') ? '/' : '-';

// I don't usually use prototype-based classes in JS, but this seems to help reduce
// memory usage when a lot of date objects are generated.
function LocalDate(year = 0, month = 0, day = 0) {
    this.year = year;
    this.month = month;
    this.day = day;

    LocalDate.prototype.clone = function() { return new LocalDate(this.year, this.month, this.day); };

    LocalDate.prototype.isZero = function() { return !this.year && !this.month && !this.day; };
    LocalDate.prototype.isValid = function() {
        if (this.year == null || this.year < -4712)
            return false;
        if (this.month == null || this.month < 1 || this.month > 12)
            return false;
        if (this.day != null && (this.day < 1 || this.day > LocalDate.daysInMonth(this.year, this.month)))
            return false;

        return true;
    };
    LocalDate.prototype.isComplete = function() { return this.isValid() && this.day != null; };

    LocalDate.prototype.equals = function(other) { return +this === +other; };

    LocalDate.prototype.toJulianDays = function() {
        // Straight from the Web:
        // http://www.cs.utsa.edu/~cs1063/projects/Spring2011/Project1/jdn-explanation.html

        let adjust = this.month < 3;
        let year = this.year + 4800 - adjust;
        let month = this.month + 12 * adjust - 3;

        let julian_days = (this.day || 1) + Math.floor((153 * month + 2) / 5) + 365 * year - 32045 +
                          Math.floor(year / 4) - Math.floor(year / 100) + Math.floor(year / 400);

        return julian_days;
    };

    LocalDate.prototype.toJSDate = function(utc = false) {
        if (utc) {
            let unix = Date.UTC(this.year, this.month - 1, this.day);
            return new Date(unix);
        } else {
            return new Date(this.year, this.month - 1, this.day);
        }
    };

    LocalDate.prototype.getWeekDay = function() {
        // Zeller's congruence:
        // https://en.wikipedia.org/wiki/Zeller%27s_congruence

        let year = this.year;
        let month = this.month;
        if (month < 3) {
            year--;
            month += 12;
        }

        let century = Math.floor(year / 100);
        year %= 100;

        let week_day = ((this.day || 1) + Math.floor(13 * (month + 1) / 5) + year + Math.floor(year / 4) +
                        Math.floor(century / 4) + 5 * century + 5) % 7;

        return week_day;
    };

    LocalDate.prototype.diff = function(other) { return this.toJulianDays() - other.toJulianDays(); };

    LocalDate.prototype.plus = function(days) {
        let date = LocalDate.fromJulianDays(this.toJulianDays() + days);
        return date;
    };
    LocalDate.prototype.minus = function(days) {
        let date = LocalDate.fromJulianDays(this.toJulianDays() - days);
        return date;
    };

    LocalDate.prototype.plusMonths = function(months) {
        if (months >= 0) {
            let m = this.month + months - 1;

            let year = this.year + Math.floor(m / 12);
            let month = 1 + (m % 12);
            let day = Math.min(this.day || 1, LocalDate.daysInMonth(year, month));

            return new LocalDate(year, month, day);
        } else {
            let m = 12 - this.month - months;

            let year = this.year - Math.floor(m / 12);
            let month = 12 - (m % 12);
            let day = Math.min(this.day || 1, LocalDate.daysInMonth(year, month));

            return new LocalDate(year, month, day);
        }
    };
    LocalDate.prototype.minusMonths = function(months) { return this.plusMonths(-months); };

    LocalDate.prototype.valueOf = LocalDate.prototype.toJulianDays;

    LocalDate.prototype.toString = function() {
        if (this.day != null) {
            let year_str = ('' + this.year).padStart(4, '0');
            let month_str = ('' + this.month).padStart(2, '0');
            let day_str = ('' + this.day).padStart(2, '0');

            let str = `${year_str}-${month_str}-${day_str}`;
            return str;
        } else {
            let year_str = ('' + this.year).padStart(4, '0');
            let month_str = ('' + this.month).padStart(2, '0');

            let str = `${year_str}-${month_str}`;
            return str;
        }
    };
    LocalDate.prototype.toJSON = LocalDate.prototype.toString;

    LocalDate.prototype.toLocaleString = function() {
        if (this.day != null) {
            let js_date = new Date(this.year, this.month - 1, this.day);
            let str = js_date.toLocaleDateString(undefined, { month: '2-digit', day: '2-digit', year: 'numeric' });

            return str;
        } else {
            let year_str = ('' + this.year).padStart(4, '0');
            let month_str = ('' + this.month).padStart(2, '0');

            let str = fmt_crazy ? `${month_str}${fmt_sep}${year_str}` : `${year_str}${fmt_sep}${month_str}`;
            return str;
        }
    };

    Object.freeze(this);
}

LocalDate.isLeapYear = function(year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
};

LocalDate.daysInMonth = function(year, month) {
    let days_per_month = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
    return days_per_month[month - 1] + (month == 2 && LocalDate.isLeapYear(year));
};

LocalDate.parse = function(str, validate = true) {
    if (str == null)
        return null;
    if (str instanceof LocalDate)
        return str;

    let date;
    {
        let parts = str.split(/[\-\/]/);

        // Support imprecise dates (month and year)
        if (parts.length === 2) {
            if (parts[0].length > parts[1].length) {
                parts.push('00');
            } else {
                parts.unshift('00');
            }
        }

        if (parts.length !== 3) {
            throw new Error(`Date '${str}' is malformed`);
        } else if (parts[0].length < 4 && parts[2].length < 4) {
            throw new Error(`Date '${str}' is ambiguous`);
        }

        if (parts[2].length >= 4)
            parts.reverse();
        parts = parts.map(part => {
            let value = parseInt(part, 10);
            if (Number.isNaN(value))
                throw new Error(`Date '${str}' is malformed`);
            return value;
        });
        if (!parts[2])
            parts[2] = null;

        date = new LocalDate(...parts);
    }

    if (validate && !date.isValid())
        throw new Error(`Date '${str}' is invalid`);

    return date;
};

LocalDate.fromJulianDays = function(days) {
    // Algorithm from Richards, copied from Wikipedia:
    // https://en.wikipedia.org/w/index.php?title=Julian_day&oldid=792497863

    let f = days + 1401 + Math.floor((Math.floor((4 * days + 274277) / 146097) * 3) / 4) - 38;
    let e = 4 * f + 3;
    let g = Math.floor(e % 1461 / 4);
    let h = 5 * g + 2;

    let month = Math.floor(h / 153 + 2) % 12 + 1;
    let year = Math.floor(e / 1461) - 4716 + (month < 3);
    let day = Math.floor(h % 153 / 5) + 1;

    return new LocalDate(year, month, day);
};

LocalDate.today = function() {
    let date = new Date();

    let year = date.getFullYear();
    let month = date.getMonth() + 1;
    let day = date.getDate();

    return new LocalDate(year, month, day);
};

function LocalTime(hour = 0, minute = 0, second = 0) {
    this.hour = hour;
    this.minute = minute;
    this.second = second;

    LocalTime.prototype.clone = function() { return new LocalTime(this.hour, this.minute, this.second); };

    LocalTime.prototype.isZero = function() { return !this.hour && !this.minute && !this.second; };
    LocalTime.prototype.isValid = function() {
        if (this.hour == null || this.hour < 0 || this.hour > 23)
            return false;
        if (this.minute == null || this.minute < 0 || this.minute > 59)
            return false;
        if (this.second == null || this.second < 0 || this.second > 59)
            return false;

        return true;
    };

    LocalTime.prototype.equals = function(other) { return +this === +other; };

    LocalTime.prototype.toDaySeconds = function() { return (this.hour * 3600) + (this.minute * 60) + this.second; };

    LocalTime.prototype.diff = function(other) { return this.toDaySeconds() - other.toDaySeconds(); };

    LocalTime.prototype.valueOf = LocalTime.prototype.toDaySeconds;

    LocalTime.prototype.toString = function() {
        let hour_str = ('' + this.hour).padStart(2, '0');
        let minute_str = ('' + this.minute).padStart(2, '0');
        let second_str = ('' + this.second).padStart(2, '0');

        let str = `${hour_str}:${minute_str}:${second_str}`;
        return str;
    };
    LocalTime.prototype.toJSON = LocalTime.prototype.toString;
    LocalTime.prototype.toLocaleString = LocalTime.prototype.toString;

    Object.freeze(this);
}

LocalTime.parse = function(str, validate = true) {
    if (str == null)
        return null;
    if (str instanceof LocalTime)
        return str;

    let time;
    {
        let m = str.match(/^([0-9]{2})[h:]([0-9]{2})(?:m?|[m:](?:([0-9]{2})s?)?)$/);
        if (m == null)
            throw new Error(`Time '${str}' is malformed`);

        let [, hour, minute, second] = m.map(part => parseInt(part, 10));
        time = new LocalTime(hour, minute, second || 0);
    }

    if (validate && !time.isValid())
        throw new Error(`Time '${str}' is invalid`);

    return time;
};

LocalTime.fromDaySeconds = function(seconds) {
    let hour = Math.floor(seconds / 3600);
    seconds = Math.floor(seconds % 3600);
    let minute = Math.floor(seconds / 60);
    let second = Math.floor(seconds % 60);

    return new LocalTime(hour, minute, second);
};

LocalTime.now = function() {
    let date = new Date();

    let hours = date.getHours();
    let minutes = date.getMinutes();
    let seconds = date.getSeconds();

    return new LocalTime(hours, minutes, seconds);
};

export {
    Util,
    Log,
    Net,
    NetworkError,

    Mutex,

    LruMap,
    BTree,

    LocalDate,
    LocalTime
}
