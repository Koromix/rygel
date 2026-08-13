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
