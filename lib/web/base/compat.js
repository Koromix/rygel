// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

if (Object.hasOwn == null) {
    // Not standard compliant but this will do for my code

    Object.hasOwn = function (obj, key) {
        return Object.prototype.hasOwnProperty.call(obj, key);
    };
}

if (Map.prototype.getOrInsert == null) {
    // Not standard compliant but these will do for my code

    Map.prototype.getOrInsert = function (key, value) {
        if (this.has(key)) {
            return this.get(key);
        } else {
            this.set(key, value);
            return value;
        }
    };
    Map.prototype.getOrInsertComputed = function (key, compute) {
        if (this.has(key)) {
            return this.get(key);
        } else {
            let value = compute(key);
            this.set(key, value);
            return value;
        }
    };
}

if (WeakMap.prototype.getOrInsert == null) {
    // Not standard compliant but these will do for my code

    WeakMap.prototype.getOrInsert = function (key, value) {
        if (this.has(key)) {
            return this.get(key);
        } else {
            this.set(key, value);
            return value;
        }
    };
    WeakMap.prototype.getOrInsertComputed = function (key, compute) {
        if (this.has(key)) {
            return this.get(key);
        } else {
            let value = compute(key);
            this.set(key, value);
            return value;
        }
    };
}

// Make sure iterator helpers are available for common types
{
    let prototypes = null;

    if (typeof Iterator != 'undefined') {
        prototypes = [Iterator.prototype];
    } else {
        prototypes = Array.from(new Set([
            Object.getPrototypeOf(new Map([]).entries()),
            Object.getPrototypeOf(new Set([]).entries())
        ]));
    }

    for (let proto of prototypes) {
        if (proto.map == null) {
            proto.map = function* (func) {
                let idx = 0;
                for (let it of this)
                    yield func(it, idx++);
            };
        }

        if (proto.filter == null) {
            proto.filter = function* (func) {
                let idx = 0;
                for (let it of this) {
                    if (func(it, idx++))
                        yield it;
                }
            };
        }

        if (proto.find == null) {
            proto.find = function (func) {
                let idx = 0;
                for (let it of this) {
                    if (func(it, idx++))
                        return it;
                }
            };
        }
    }
}
