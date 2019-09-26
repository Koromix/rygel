// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const {render, html, svg} = lithtml;

// ------------------------------------------------------------------------
// Log
// ------------------------------------------------------------------------

let log = (function() {
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
        if (entry.type !== 'debug') {
            switch (action) {
                case 'open': {
                    entries.unshift(entry);

                    if (entry.type === 'progress') {
                        // Wait a bit to show progress entries to prevent quick actions from showing up
                        setTimeout(renderLog, 100);
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
            return html`<div class=${'ut_log_entry ' + entry.type}>
                ${entry.type === 'progress' ?
                    html`<div class="ut_log_spin"></div>` :
                    html`<button class="ut_log_close" @click=${e => closeLogEntry(idx)}>X</button>`}
                ${entry.msg}
             </div>`;
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
            entry.timer_id = setTimeout(() => func('close', entry), timeout);

        func(is_new ? 'open' : 'edit', entry);
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
    let self = this;

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

    this.assignDeep = function(obj, ...sources) {
        for (let src of sources)
            assignDeep1(obj, src);

        return obj;
    };

    function assignDeep1(obj, src) {
        for (key in src) {
            let to = obj[key];
            let from = src[key];

            if (isRealObject(to) && isRealObject(from) && !Object.isFrozen(to)) {
                assignDeep1(to, from);
            } else {
                obj[key] = from;
            }
        }
    }

    function isRealObject(value) {
        return (typeof value === 'object') && value !== null && !Array.isArray(value);
    }

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

    this.findParent = function(el, func) {
        while (el && !func(el))
            el = el.parentNode;
        return el;
    }

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

        this.toLocaleString = function() {
            let js_date = new Date(self.year, self.month - 1, self.day);
            return js_date.toLocaleDateString();
        };
    }

    this.create = function(year = 0, month = 0, day = 0) {
        let date = new LocalDate(year, month, day);
        return Object.freeze(date);
    };

    this.fromString = function(str, validate = true) {
        if (str == null)
            return null;

        let date;
        try {
            let [year, month, day] = str.split('-').map(x => parseInt(x, 10));
            date = dates.create(year, month, day);
        } catch (err) {
            log.error(`Date '${str}' is malformed`);
            return null;
        }

        if (validate && !date.isValid()) {
            log.error(`Date '${str}' is invalid`);
            return null;
        }

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
