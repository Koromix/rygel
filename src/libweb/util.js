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

    this.defaultHandler = function(entry) {
        switch (entry.type) {
            case 'debug':
            case 'info':
            case 'success':
            case 'progress': { console.log(entry.msg); } break;
            case 'error': { console.error(entry.msg); } break;
        }
    };

    let handlers = [self.defaultHandler];

    this.pushHandler = function(func) { handlers.push(func); };
    this.popHandler = function() { handlers.pop(); };

    function updateEntry(entry, type, msg, timeout) {
        entry.new = !entry.type;

        entry.type = type;
        entry.msg = msg;

        if (entry.timer_id != null) {
            clearTimeout(entry.timer_id);
            entry.timer_id = null;
        }
        if (timeout >= 0)
            entry.timer_id = setTimeout(() => updateEntry(entry, null, null), timeout);

        let func = handlers[handlers.length - 1];
        func(entry);
    }

    this.Entry = function() {
        let self = this;

        this.type = null;
        this.msg = null;
        this.timer_id = null;
        this.new = true;

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
    };

    this.debug = function(msg, timeout = 6000) { new self.Entry().debug(msg, timeout); };
    this.info = function(msg, timeout = 6000) { new self.Entry().info(msg, timeout); };
    this.success = function(msg, timeout = 6000) { new self.Entry().success(msg, timeout); };
    this.error = function(msg, timeout = 6000) { new self.Entry().error(msg, timeout); };

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
            unlink(bucket);
            link(bucket);
        } else if (count < limit) {
            bucket = {
                key: key,
                value: value,
                prev: null,
                next: null
            };

            map[key] = bucket;
            link(bucket);
            count++;
        } else {
            bucket = root_bucket.next;

            bucket.value = value;
            unlink(bucket);
            link(bucket);
        }
    };

    this.delete = function(key) {
        let bucket = map[key];

        if (bucket) {
            unlink(bucket);
            delete map[key];
            count--;
        }
    };

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
        // First, break cycle to help GC (maybe)
        root_bucket.prev.next = null;
        root_bucket.next.prev = null;
        root_bucket.prev = root_bucket;
        root_bucket.next = root_bucket;

        map = {};
        count = 0;
    };
}

// ------------------------------------------------------------------------
// Date
// ------------------------------------------------------------------------

let dates = (function() {
    let self = this;

    this.isLeapYear = function(year) {
        return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    };

    this.daysInMonth = function(year, month) {
        let days_per_month = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
        return days_per_month[month - 1] + (month == 2 && self.isLeapYear(year));
    };

    function LocalDate(year = 0, month = 0, day = 0) {
        let self = this;

        this.year = year;
        this.month = month;
        this.day = day;

        this.clone = function() { return dates.create(self.year, self.month, self.day); };

        this.isZero = function() { return !year && !month && !day; };
        this.isValid = function() {
            if (self.year < -4712)
                return false;
            if (self.month < 1 || self.month > 12)
                return false;
            if (self.day < 1 || self.day > dates.daysInMonth(self.year, self.month))
                return false;

            return true;
        };

        this.equals = function(other) { return +self === +other; };

        this.toJulianDays = function() {
            // Straight from the Web:
            // http://www.cs.utsa.edu/~cs1063/projects/Spring2011/Project1/jdn-explanation.html

            let adjust = self.month < 3;
            let year = self.year + 4800 - adjust;
            let month = self.month + 12 * adjust - 3;

            let julian_days = self.day + Math.floor((153 * month + 2) / 5) + 365 * year - 32045 +
                              Math.floor(year / 4) - Math.floor(year / 100) + Math.floor(year / 400);

            return julian_days;
        };

        this.getWeekDay = function() {
            // Zeller's congruence:
            // https://en.wikipedia.org/wiki/Zeller%27s_congruence

            let year = self.year;
            let month = self.month;
            if (month < 3) {
                year--;
                month += 12;
            }

            let century = Math.floor(year / 100);
            year %= 100;

            let week_day = (self.day + Math.floor(13 * (month + 1) / 5) + year + Math.floor(year / 4) +
                            Math.floor(century / 4) + 5 * century + 5) % 7;

            return week_day;
        };

        this.diff = function(other) { return self.toJulianDays() - other.toJulianDays(); };

        this.plus = function(days) {
            let date = dates.fromJulianDays(self.toJulianDays() + days);
            return date;
        };
        this.minus = function(days) {
            let date = dates.fromJulianDays(self.toJulianDays() - days);
            return date;
        };

        this.plusMonths = function(months) {
            if (months >= 0) {
                let m = self.month + months - 1;

                let year = self.year + Math.floor(m / 12);
                let month = 1 + (m % 12);
                let day = Math.min(self.day, dates.daysInMonth(year, month));

                return dates.create(year, month, day);
            } else {
                let m = 12 - self.month - months;

                let year = self.year - Math.floor(m / 12);
                let month = 12 - (m % 12);
                let day = Math.min(self.day, dates.daysInMonth(year, month));

                return dates.create(year, month, day);
            }
        };
        this.minusMonths = function(months) { return self.plusMonths(-months); };

        this.valueOf = function() {
            let value = (self.year << 16) | (self.month << 8) | (self.day);
            return value;
        };

        this.toString = function() {
            let year_str = ('' + self.year).padStart(4, '0');
            let month_str = ('' + self.month).padStart(2, '0');
            let day_str = ('' + self.day).padStart(2, '0');

            let str = `${year_str}-${month_str}-${day_str}`;
            return str;
        };

        // FIXME: Actually use locale string format
        this.toLocaleString = function() {
            let year_str = ('' + self.year).padStart(4, '0');
            let month_str = ('' + self.month).padStart(2, '0');
            let day_str = ('' + self.day).padStart(2, '0');

            let str = `${day_str}/${month_str}/${year_str}`;
            return str;
        };
    }

    this.create = function(year = 0, month = 0, day = 0) {
        let date = new LocalDate(year, month, day);
        return Object.freeze(date);
    };

    this.fromString = function(str, validate = true) {
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

    return this;
}).call({});
