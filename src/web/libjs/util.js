// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

if (typeof lithtml !== 'undefined') {
    window.render = lithtml.render;
    window.html = lithtml.html;
    window.svg = lithtml.svg;
}

// ------------------------------------------------------------------------
// Compatibility
// ------------------------------------------------------------------------

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

let util = new function() {
    let self = this;

    this.clamp = function(value, min, max) {
        return Math.min(Math.max(value, min), max);
    };

    this.mapRange = function(start, end, func) {
        let len = end - start;
        let arr = Array.from({length: len});

        for (let i = 0; i < len; i++)
            arr[i] = func(start + i);

        return arr;
    };

    this.mapRLE = function(arr, key_func, func) {
        let arr2 = Array.from({length: arr.length});

        let arr2_len = 0;
        for (let i = 0; i < arr.length;) {
            let current_key = key_func(arr[i]);

            let j = i + 1;
            while (j < arr.length && key_func(arr[j]) === current_key) {
                j++;
            }

            arr2[arr2_len++] = func(current_key, i, j - i);
            i = j;
        }
        arr2.length = arr2_len;

        return arr2;
    };

    this.mapArray = function(arr, key_func, value_func = it => it) {
        let obj = {};
        for (let it of arr) {
            let key = key_func(it);
            obj[key] = value_func(it);
        }

        return obj;
    };

    this.map = function*(obj, func) {
        for (let it of obj)
            yield func(it);
    };

    this.assignDeep = function(obj, ...sources) {
        for (let src of sources)
            assignDeep1(obj, src);

        return obj;
    };

    function assignDeep1(obj, src) {
        for (let key in src) {
            let from = src[key];

            if (isPodObject(from) && !Object.isFrozen(from)) {
                let to = obj[key];

                if (!isPodObject(to)) {
                    to = {};
                    obj[key] = to;
                }

                assignDeep1(to, from);
            } else {
                obj[key] = from;
            }
        }
    }

    function isPodObject(value) {
        return !!value && value.constructor === Object;
    }

    this.deepFreeze = function(obj, ignore = []) {
        Object.freeze(obj);

        for (let key in obj) {
            let value = obj[key];
            if (!Object.isFrozen(value) && !ignore.includes(key) && (typeof value === 'object' ||
                                                                     typeof value === 'function'))
                self.deepFreeze(value);
        }

        return obj;
    };

    this.pasteURL = function(url, params = {}, hash = null) {
        for (let key in params) {
            if (params[key] != null) {
                params[key] = params[key].toString();
            } else {
                delete params[key];
            }
        }

        let query_str = new URLSearchParams(params).toString();
        return `${url}${query_str ? '?' : ''}${query_str || ''}${hash ? '#' : ''}${hash || ''}`;
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

    this.roundTo = function(n, digits) {
        if (digits === undefined)
            digits = 0;

        let multiplicator = Math.pow(10, digits);
        n = parseFloat((n * multiplicator).toFixed(11));
        return Math.round(n) / multiplicator;
    };

    this.compareValues = function(value1, value2) {
        value1 = value1 || '';
        value2 = value2 || '';

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

    function encode30bits(value, buf, offset, len) {
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

    this.saveBlob = function(blob, filename) {
        let url = URL.createObjectURL(blob);

        let a = document.createElement('a');
        a.download = filename;
        a.href = url;

        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);

        if(URL.revokeObjectURL)
            setTimeout(() => URL.revokeObjectURL(url), 60000);
    };

    this.parseEvalErrorLine = function(err) {
        if (err instanceof SyntaxError) {
            // At least Firefox seems to do well in this case, it's better than nothing
            return err.lineNumber - 2;
        } else if (err.stack) {
            let m;
            if (m = err.stack.match(/ > Function:([0-9]+):[0-9]+/) ||
                    err.stack.match(/, <anonymous>:([0-9]+):[0-9]+/)) {
                // Can someone explain to me why do I have to offset by -2?
                let line = parseInt(m[1], 10) - 2;
                return line;
            } else if (m = err.stack.match(/Function code:([0-9]+):[0-9]+/)) {
                let line = parseInt(m[1], 10);
                return line;
            }
        } else {
            return null;
        }
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

                if (target && (target.tagName === 'A' || target.tagName === 'a') &&
                        !target.getAttribute('download')) {
                    let href = target.getAttribute('href');
                    if (href && !href.match(/^[a-z]+:/) && href[0] != '#') {
                        func(e, href);
                        e.preventDefault();
                    }
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

    this.getRandomInt = function(min, max) {
        min = Math.ceil(min);
        max = Math.floor(max);

        let rnd = Math.floor(Math.random() * (max - min)) + min;
        return rnd;
    };
};

// ------------------------------------------------------------------------
// Log
// ------------------------------------------------------------------------

let log = new function() {
    let self = this;

    let handlers = [];
    let entries = [];

    // Log to console
    this.defaultHandler = function(action, entry) {
        if (action !== 'close') {
            switch (entry.type) {
                case 'debug':
                case 'info':
                case 'success':
                case 'progress': { console.log(entry.msg); } break;
                case 'error': { console.error(entry.msg); } break;
            }
        }
    };
    handlers.push(self.defaultHandler);

    // Show to user
    this.notifyHandler = function(action, entry) {
        if (typeof lithtml !== 'undefined' && entry.type !== 'debug') {
            switch (action) {
                case 'open': {
                    entries.unshift(entry);

                    if (entry.type === 'progress') {
                        // Wait a bit to show progress entries to prevent quick actions from showing up
                        setTimeout(renderLog, 200);
                    } else {
                        renderLog();
                    }
                } break;
                case 'edit': {
                    renderLog();
                } break;
                case 'close': {
                    entries = entries.filter(it => it !== entry);
                    renderLog();
                } break;
            }
        }

        self.defaultHandler(action, entry);
    };

    function closeLogEntry(idx) {
        entries.splice(idx, 1);
        renderLog();
    }

    function renderLog() {
        let log_el = document.querySelector('#ut_log');
        if (!log_el) {
            log_el = document.createElement('div');
            log_el.id = 'ut_log';
            document.body.appendChild(log_el);
        }

        render(entries.map((entry, idx) => {
            let msg = (entry.msg instanceof Error) ? entry.msg.message : entry.msg;

            if (entry.type === 'progress') {
                return html`<div class="ut_log_entry progress">
                    <div class="ut_log_spin"></div>
                    ${msg.split('\n').map(line => [line, html`<br/>`])}
                </div>`;
            } else {
                return html`<div class=${'ut_log_entry ' + entry.type} @click=${e => closeLogEntry(idx)}>
                    <button class="ut_log_close">X</button>
                    ${msg.split('\n').map(line => [line, html`<br/>`])}
                </div>`;
            }
        }), log_el);
    }

    this.pushHandler = function(func) { handlers.push(func); };
    this.popHandler = function() { handlers.pop(); };

    function updateEntry(entry, type, msg, timeout) {
        let func = handlers[handlers.length - 1];
        let is_new = (entry.type == null);

        entry.type = type;
        entry.msg = msg;

        if (entry.timer_id != null) {
            clearTimeout(entry.timer_id);
            entry.timer_id = null;
        }
        if (timeout >= 0)
            entry.timer_id = setTimeout(() => closeEntry(entry), timeout);

        func(is_new ? 'open' : 'edit', entry);
    }

    function closeEntry(entry) {
        let func = handlers[handlers.length - 1];
        func('close', entry);
    }

    this.Entry = function() {
        let self = this;

        this.type = null;
        this.msg = null;
        this.timer_id = null;

        this.debug = function(msg, timeout = 6000) { updateEntry(self, 'debug', msg, timeout); };
        this.info = function(msg, timeout = 6000) { updateEntry(self, 'info', msg, timeout); };
        this.success = function(msg, timeout = 6000) { updateEntry(self, 'success', msg, timeout); };
        this.error = function(msg, timeout = 6000) { updateEntry(self, 'error', msg, timeout); };

        this.progress = function(action, value = null, max = null) {
            if (value != null) {
                let msg = `${action}: ${value}${max != null ? ('/' + max) : ''}`;
                updateEntry(self, 'progress', msg, -1);
            } else {
                updateEntry(self, 'progress', action, -1);
            }
        };

        this.close = function() { closeEntry(self); };
    };

    this.debug = function(msg, timeout = 6000) { new self.Entry().debug(msg, timeout); };
    this.info = function(msg, timeout = 6000) { new self.Entry().info(msg, timeout); };
    this.success = function(msg, timeout = 6000) { new self.Entry().success(msg, timeout); };
    this.error = function(msg, timeout = 6000) { new self.Entry().error(msg, timeout); };
};

// ------------------------------------------------------------------------
// Network
// ------------------------------------------------------------------------

let net = new function() {
    let self = this;

    let plugged = true;
    let online = true;

    this.changeHandler = online => {};

    this.fetch = async function(request, init) {
        if (!plugged)
            throw new Error('Cannot perform request in offline mode');

        try {
            let response = await fetch(request, init);

            setOnline(true);
            return response;
        } catch (err) {
            setOnline(false);
            throw new Error('Request failure: network error');
        }
    };

    this.loadScript = function(url) {
        return new Promise((resolve, reject) => {
            let script = document.createElement('script');
            script.type = 'text/javascript';
            script.src = url;

            script.onload = e => resolve(script);
            script.onerror = e => {
                setOnline(false);
                reject(new Error(`Failed to load '${url}' script`));
            }

            document.head.appendChild(script);
        });
    };

    function setOnline(online2) {
        if (online2 !== online) {
            online = online2;
            self.changeHandler(online);
        }
    }

    this.isPlugged = function() { return plugged; };
    this.isOnline = function() { return plugged && online; };

    this.setPlugged = function(plug) { plugged = plug; };
};

// ------------------------------------------------------------------------
// Containers
// ------------------------------------------------------------------------

function LruMap(limit) {
    if (limit == null || limit < 2)
        throw new Error('LruMap limit must be >= 2');

    let self = this;

    let map = {};
    let count = 0;

    let root_bucket = {}
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
            if (count >= limit)
                deleteBucket(root_bucket.next);

            bucket = {
                key: key,
                value: value,
                prev: null,
                next: null
            };

            map[key] = bucket;
            link(bucket);
            count++;
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
        count--;
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

    this.clear = function() {
        root_bucket.prev = root_bucket;
        root_bucket.next = root_bucket;

        map = {};
        count = 0;
    };

    this[Symbol.iterator] = function*() {
        let it = root_bucket.next;
        while (it !== root_bucket) {
            yield [it.key, it.value];
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

    this.first = function() { return leaf0 ? leaf0.values[0] : undefined; };

    this.forEach = function(func, this_arg) {
        let leaf = leaf0;

        while (leaf) {
            for (let i = 0; i < leaf.keys.length; i++)
                func.call(this_arg, leaf.values[i], leaf.keys[i], self);
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

let dates = new function() {
    let self = this;

    this.isLeapYear = function(year) {
        return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    };

    this.daysInMonth = function(year, month) {
        let days_per_month = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
        return days_per_month[month - 1] + (month == 2 && self.isLeapYear(year));
    };

    // I don't usually use prototype-based classes in JS, but this seems to help reduce
    // memory usage when a lot of date objects are generated.
    function LocalDate(year = 0, month = 0, day = 0) {
        this.year = year;
        this.month = month;
        this.day = day;

        LocalDate.prototype.clone = function() { return dates.create(this.year, this.month, this.day); };

        LocalDate.prototype.isZero = function() { return !this.year && !this.month && !this.day; };
        LocalDate.prototype.isValid = function() {
            if (this.year == null || this.year < -4712)
                return false;
            if (this.month == null || this.month < 1 || this.month > 12)
                return false;
            if (this.day == null || this.day < 1 || this.day > dates.daysInMonth(this.year, this.month))
                return false;

            return true;
        };

        LocalDate.prototype.equals = function(other) { return +this === +other; };

        LocalDate.prototype.toJulianDays = function() {
            // Straight from the Web:
            // http://www.cs.utsa.edu/~cs1063/projects/Spring2011/Project1/jdn-explanation.html

            let adjust = this.month < 3;
            let year = this.year + 4800 - adjust;
            let month = this.month + 12 * adjust - 3;

            let julian_days = this.day + Math.floor((153 * month + 2) / 5) + 365 * year - 32045 +
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

            let week_day = (this.day + Math.floor(13 * (month + 1) / 5) + year + Math.floor(year / 4) +
                            Math.floor(century / 4) + 5 * century + 5) % 7;

            return week_day;
        };

        LocalDate.prototype.diff = function(other) { return this.toJulianDays() - other.toJulianDays(); };

        LocalDate.prototype.plus = function(days) {
            let date = dates.fromJulianDays(this.toJulianDays() + days);
            return date;
        };
        LocalDate.prototype.minus = function(days) {
            let date = dates.fromJulianDays(this.toJulianDays() - days);
            return date;
        };

        LocalDate.prototype.plusMonths = function(months) {
            if (months >= 0) {
                let m = this.month + months - 1;

                let year = this.year + Math.floor(m / 12);
                let month = 1 + (m % 12);
                let day = Math.min(this.day, dates.daysInMonth(year, month));

                return dates.create(year, month, day);
            } else {
                let m = 12 - this.month - months;

                let year = this.year - Math.floor(m / 12);
                let month = 12 - (m % 12);
                let day = Math.min(this.day, dates.daysInMonth(year, month));

                return dates.create(year, month, day);
            }
        };
        LocalDate.prototype.minusMonths = function(months) { return this.plusMonths(-months); };

        LocalDate.prototype.valueOf = function() {
            let value = (this.year << 16) | (this.month << 8) | (this.day);
            return value;
        };

        LocalDate.prototype.toString = function() {
            let year_str = ('' + this.year).padStart(4, '0');
            let month_str = ('' + this.month).padStart(2, '0');
            let day_str = ('' + this.day).padStart(2, '0');

            let str = `${year_str}-${month_str}-${day_str}`;
            return str;
        };

        LocalDate.prototype.toLocaleString = function() {
            let js_date = new Date(this.year, this.month - 1, this.day);
            return js_date.toLocaleDateString(undefined, {month: '2-digit', day: '2-digit', year: 'numeric'});
        };
    }

    this.create = function(year = 0, month = 0, day = 0) {
        let date = new LocalDate(year, month, day);
        return Object.freeze(date);
    };

    this.parse = function(str, validate = true) {
        if (str == null)
            return null;

        let date;
        try {
            let [year, month, day] = str.split('-').map(x => parseInt(x, 10));
            date = dates.create(year, month, day);
        } catch (err) {
            throw new Error(`Date '${str}' is malformed`);
        }

        if (validate && !date.isValid())
            throw new Error(`Date '${str}' is invalid`);

        return date;
    };

    this.parseSafe = function(str, validate = true) {
        try {
            return self.parse(str, validate);
        } catch (err) {
            return null;
        }
    };

    this.parseLog = function(str, validate = true) {
        try {
            return self.parse(str, validate);
        } catch (err) {
            log.error(err);
            return null;
        }
    };

    this.fromJulianDays = function(days) {
        // Algorithm from Richards, copied from Wikipedia:
        // https://en.wikipedia.org/w/index.php?title=Julian_day&oldid=792497863

        let f = days + 1401 + Math.floor((Math.floor((4 * days + 274277) / 146097) * 3) / 4) - 38;
        let e = 4 * f + 3;
        let g = Math.floor(e % 1461 / 4);
        let h = 5 * g + 2;

        let month = Math.floor(h / 153 + 2) % 12 + 1;
        let year = Math.floor(e / 1461) - 4716 + (month < 3);
        let day = Math.floor(h % 153 / 5) + 1;

        return dates.create(year, month, day);
    };

    this.today = function() {
        let date = new Date();

        let year = date.getFullYear();
        let month = date.getMonth() + 1;
        let day = date.getDate();

        return dates.create(year, month, day);
    };
};

let times = new function() {
    let self = this;

    function LocalTime(hour = 0, minute = 0, second = 0) {
        this.hour = hour;
        this.minute = minute;
        this.second = second;

        LocalTime.prototype.clone = function() { return dates.create(this.hour, this.minute, this.second); };

        LocalTime.prototype.isZero = function() { return !this.hour && !this.minute && !this.second; };
        LocalTime.prototype.isValid = function() {
            if (this.hour == null || this.hour < 1 || this.hour > 23)
                return false;
            if (this.minute == null || this.minute < 0 || this.minute > 59)
                return false;
            if (this.second == null || this.second < 0 || this.second > 59)
                return false;

            return true;
        };

        LocalTime.prototype.equals = function(other) { return +this === +other; };

        LocalTime.prototype.toDaySeconds = function()
            { return (this.hour * 3600) + (this.minute * 60) + this.second; };

        LocalTime.prototype.diff = function(other) { return this.toDaySeconds() - other.toDaySeconds(); };

        LocalTime.prototype.valueOf = LocalTime.prototype.toDaySeconds;

        LocalTime.prototype.toString = function() {
            let hour_str = ('' + this.hour).padStart(2, '0');
            let minute_str = ('' + this.minute).padStart(2, '0');
            let second_str = ('' + this.second).padStart(2, '0');

            let str = `${hour_str}:${minute_str}:${second_str}`;
            return str;
        };
        LocalTime.prototype.toLocaleString = LocalTime.prototype.toString;
    }

    this.create = function(hour = 0, minute = 0, second = 0) {
        let time = new LocalTime(hour, minute, second);
        return Object.freeze(time);
    };

    this.fromDaySeconds = function(seconds) {
        let hour = Math.floor(seconds / 3600);
        seconds = Math.floor(seconds % 3600);
        let minute = Math.floor(seconds / 60);
        let second = Math.floor(seconds % 60);

        return times.create(hour, minute, second);
    };

    this.parse = function(str, validate = true) {
        if (str == null)
            return null;

        let time;
        try {
            let [hour, minute, second] = str.split(':').map(x => parseInt(x, 10));
            time = times.create(hour, minute, second || 0);
        } catch (err) {
            throw new Error(`Time '${str}' is malformed`);
        }

        if (validate && !time.isValid())
            throw new Error(`Time '${str}' is invalid`);

        return time;
    };

    this.parseSafe = function(str, validate = true) {
        try {
            return self.parse(str, validate);
        } catch (err) {
            return null;
        }
    };

    this.parseLog = function(str, validate = true) {
        try {
            return self.parse(str, validate);
        } catch (err) {
            log.error(err);
            return null;
        }
    };
};
