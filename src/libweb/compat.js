// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Most of these implementations are not standard-compliant, but it's enough for the kind
// of code I write.

if (!Array.from) {
    Array.from = function(obj) {
        let arr = [];
        if (obj.values && typeof obj.values === 'function') {
            for (const value of obj.values())
                arr.push(value);
        } else {
            for (var i = 0; i < obj.length; i++)
                arr.push(obj[i]);
        }

        return arr;
    };
}

if (!Array.findIndex) {
    Array.prototype.findIndex = function(callback, thisArg) {
        for (let i = 0; i < this.length; i++) {
            if (callback.call(thisArg, this[i], i, this))
                return i;
        }
        return -1;
    };

    Array.prototype.find = function(callback, thisArg) {
        let idx = this.findIndex(callback, thisArg);
        return idx >= 0 ? this[idx] : undefined;
    };
}

if (!Array.prototype.includes) {
    Array.prototype.includes = function(needle) {
        function sameValueZero(x, y) {
            return x === y ||
                   (typeof x === 'number' && typeof y === 'number' && isNaN(x) && isNaN(y));
        }

        for (const value of this) {
            if (sameValueZero(value, needle))
                return true;
        }
        return false;
    };
}

if (!Object.assign) {
    Object.assign = function(target, source) {
        const hasOwnProperty = Object.prototype.hasOwnProperty;

        for (let i = 1; i < arguments.length; i++) {
            const from = arguments[i];

            if (from) {
                for (const key in from) {
                    if (hasOwnProperty.call(from, key))
                        target[key] = from[key];
                }
            }
        }

        return target;
    };
}

if (!Object.values) {
    Object.values = function(obj) {
        const isEnumerable = Object.prototype.propertyIsEnumerable;

        let values = [];
        for (let key in obj) {
            if (typeof key === 'string' && isEnumerable.call(obj, key))
                values.push(obj[key]);
        }

        return values;
    }
}

if (!String.prototype.padStart) {
    String.prototype.padStart = function(target_len, pad) {
        if (pad === undefined)
            pad = ' ';

        if (this.length >= target_len) {
            return String(this);
        } else {
            target_len = target_len - this.length;
            if (target_len > pad.length) {
                pad += pad.repeat(target_len / pad.length);
            }
            return pad.slice(0, target_len) + String(this);
        }
    };
}

if (window.NodeList && !NodeList.prototype.forEach) {
    NodeList.prototype.forEach = function(callback, thisArg) {
        thisArg = thisArg || window;

        for (let i = 0; i < this.length; i++)
            callback.call(thisArg, this[i], i, this);
    };
}
