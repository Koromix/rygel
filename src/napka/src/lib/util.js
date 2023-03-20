if (String.prototype.format == null) {
    String.prototype.format = function(...args) {
        let str = this.replace(/{(\d+)}/g, (match, idx) => {
            idx = parseInt(idx, 10) - 1;

            let arg = args[idx];
            return (arg !== undefined) ? arg : match;
        });
        return str;
    }
}

const util = new function() {
    let self = this;

    this.load_script = function(url) {
        return new Promise((resolve, reject) => {
            let script = document.createElement('script');
            script.type = 'text/javascript';
            script.src = url;

            script.onload = e => resolve(script);
            script.onerror = e => reject(new Error(`Failed to load '${url}' script`));

            document.head.appendChild(script);
        });
    };

    this.wait_for = function(ms) {
        return new Promise((resolve, reject) => {
            setTimeout(resolve, ms);
        });
    };

    this.isPodObject = function(value) {
        return !!value && value.constructor === Object;
    }

    this.isNumeric = function(n) {
        return !isNaN(parseFloat(n)) && isFinite(n);
    }

    this.waitFor = function(ms) {
        return new Promise((resolve, reject) => {
            setTimeout(resolve, ms);
        });
    }

    this.deepAssign = function(obj, ...sources) {
        for (let src of sources)
            recurse(obj, src);

        function recurse(obj, src) {
            for (let key in src) {
                let from = src[key];

                if (self.isPodObject(from) && !Object.isFrozen(from)) {
                    let to = obj[key];

                    if (!self.isPodObject(to)) {
                        to = {};
                        obj[key] = to;
                    }

                    recurse(to, from);
                } else {
                    obj[key] = from;
                }
            }
        }

        return obj;
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

    this.mapRange = function(start, end, func) {
        let arr = [];
        for (let i = start; i < end; i++) {
            let value = func(i);
            arr.push(value);
        }
        return arr;
    };

    this.capitalize = function(str) {
        if (str.length) {
            return str[0].toUpperCase() + str.substr(1);
        } else {
            return str;
        }
    };
};

const log = new function() {
    let self = this;

    let handlers = [];

    // Log to console
    this.default_handler = function(action, entry) {
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
    handlers.push(self.default_handler);

    this.default_timeout = 3000;

    this.push_handler = function(func) { handlers.push(func); };
    this.pop_handler = function() { handlers.pop(); };

    function update_entry(entry, type, msg, timeout) {
        let func = handlers[handlers.length - 1];
        let is_new = (entry.type == null);

        entry.type = type;
        entry.msg = msg;

        if (entry.timer_id != null) {
            clearTimeout(entry.timer_id);
            entry.timer_id = null;
        }
        if (timeout >= 0)
            entry.timer_id = setTimeout(() => close_entry(entry), timeout);

        func(is_new ? 'open' : 'edit', entry);

        return entry;
    }

    function close_entry(entry) {
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

        this.debug = function(msg, timeout = log.default_timeout) { return update_entry(self, 'debug', msg, timeout); };
        this.info = function(msg, timeout = log.default_timeout) { return update_entry(self, 'info', msg, timeout); };
        this.success = function(msg, timeout = log.default_timeout) { return update_entry(self, 'success', msg, timeout); };
        this.error = function(msg, timeout = log.default_timeout) { return update_entry(self, 'error', msg, timeout); };

        this.progress = function(action, value = null, max = null) {
            if (value != null) {
                let msg = `${action}: ${value}${max != null ? ('/' + max) : ''}`;
                return update_entry(self, 'progress', msg, -1);
            } else {
                return update_entry(self, 'progress', action, -1);
            }
        };

        this.close = function() { close_entry(self); };
    };

    this.debug = function(msg, timeout = log.default_timeout) { return (new self.Entry).debug(msg, timeout); };
    this.info = function(msg, timeout = log.default_timeout) { return (new self.Entry).info(msg, timeout); };
    this.success = function(msg, timeout = log.default_timeout) { return (new self.Entry).success(msg, timeout); };
    this.error = function(msg, timeout = log.default_timeout) { return (new self.Entry).error(msg, timeout); };
    this.progress = function(action, value = null, max = null) { return (new self.Entry).progress(action, value, max); };
};

const cookies = new (function() {
    let self = this;

    this.get = function(key, default_value = undefined) {
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

    this.set = function(name, value, options = {}) {
        let cookie = `${name}=${encodeURIComponent(value)}; SameSite=Lax;`;

        if (options.path != null)
            cookie += ` Path=${options.path}`;
        if (options.max_age != null)
            cookie += ` Max-Age=${options.max_age};`;

        document.cookie = cookie;
    };

    this.delete = function(name, options = {}) {
        options = Object.assign({}, options);
        options.max_age = 0;

        self.set(name, '', options);
    };
})();

const net = new (function() {
    let self = this;

    let caches = {};

    this.cache_duration = 300000; // 5 minutes
    this.error_handler = (status) => false;

    this.fetch = async function(url, options = {}) {
        options = Object.assign({}, options);
        options.headers = Object.assign({ 'X-Requested-With': 'XMLHTTPRequest' }, options.headers);

        for (;;) {
            let response = await fetch(url, options);

            if (!response.ok) {
                let text = (await response.text()).trim();

                throw new Error(text);
            }

            let json = await response.json();
            return json;
        }
    };

    this.get = async function(url) {
        return self.fetch(url);
    };

    this.post = function(url, obj = {}) {
        return self.fetch(url, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'X-Requested-With': 'XMLHTTPRequest'
            },
            body: JSON.stringify(obj)
        });
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

    this.invalidate = function(key) {
        delete caches[key];
    };
})();

function LruMap(limit) {
    if (limit == null || limit < 2)
        throw new Error('LruMap limit must be >= 2');

    let self = this;

    let map = {};

    this.size = 0;

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
            if (self.size >= limit)
                deleteBucket(root_bucket.next);

            bucket = {
                key: key,
                value: value,
                prev: null,
                next: null
            };

            map[key] = bucket;
            link(bucket);
            self.size++;
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
        self.size--;
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
        self.size = 0;
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

module.exports = {
    util,
    log,
    cookies,
    net,
    LruMap
};
