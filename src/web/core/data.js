// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

import { Util, Log } from '../../web/core/base.js';

let map = new WeakMap;

function wrap(data) {
    let raw = expand(data).$v;
    let obj = proxify(raw);

    return [raw, obj];
}

const handler = {
    set: function(obj, key, value) {
        if (!checkKey(key))
            return false;

        value = expand(value);

        let ptr = map.get(obj);
        let raw = ptr.raw[key];

        if (raw != null) {
            raw.$v = value.$v;
        } else {
            ptr.raw[key] = value;
        }

        let v = proxify(value.$v);
        ptr.obj[key] = v;

        if (!equals(v, ptr.copy[key])) {
            ptr.changes.add(key);
        } else {
            ptr.changes.delete(key);
        }
        if (Util.isPodObject(v)) {
            ptr.children.set(key, map.get(v));
        } else {
            ptr.children.delete(key);
        }

        return true;
    },

    deleteProperty: function(obj, key) {
        if (!checkKey(key))
            return false;
        if (!obj.hasOwnProperty(key))
            return false;

        let ptr = map.get(obj);

        delete ptr.raw[key];
        delete ptr.obj[key];

        if (ptr.copy.hasOwnProperty(key)) {
            ptr.changes.add(key);
        } else {
            ptr.changes.delete(key);
        }
        ptr.children.delete(key);

        return true;
    }
};

function expand(value) {
    if (Util.isPodObject(value)) {
        if (value.hasOwnProperty('$n')) {
            value.$v = expand(value.$v).$v;
            return value;
        }

        let obj = {};

        for (let key in value) {
            if (!checkKey(key))
                continue;

            obj[key] = expand(value[key]);
        }

        value = obj;
    }

    value = {
        '$n': {},
        '$v': value
    };

    return value;
}

function proxify(value) {
    if (!Util.isPodObject(value))
        return value;

    let children = new Map;
    let obj = {};

    for (let key in value) {
        let v = proxify(value[key].$v);
        if (Util.isPodObject(v))
            children.set(key, map.get(v));
        obj[key] = v;
    }
    let proxy = new Proxy(obj, handler);

    let ptr = {
        raw: value,
        obj: obj,
        children: children,
        copy: Object.assign({}, obj),
        changes: new Set
    };

    map.set(proxy, ptr);
    map.set(obj, ptr);

    return proxy;
}

function unwrap(obj) {
    let ptr = map.get(obj);
    return ptr.obj;
}

function annotate(obj, key) {
    let ptr = map.get(obj);

    let raw = ptr?.raw?.[key];

    if (raw == null)
        return null;

    return raw.$n;
}

function hasChanged(obj) {
    let ptr = map.get(obj);

    if (ptr.changes.size)
        return true;
    for (let sub of ptr.children.values()) {
        if (sub?.changes.size)
            return true;
    }

    return false;
}

function equals(value1, value2) {
    if (value1 == null) {
        return value2 == null;
    } else if (typeof value1 == 'string' || typeof value1 == 'number' || typeof value1 == 'boolean') {
        return value2 === value1;
    } else if (Array.isArray(value1)) {
        if (!Array.isArray(value2))
            return false;
        if (value2.length != value1.length)
            return false;

        for (let i = 0; i < value1.length; i++) {
            if (!equals(value1[i], value2[i]))
                return false;
        }

        return true;
    } else if (Util.isPodObject(value1)) {
        if (!Util.isPodObject(value2))
            return false;

        let keys1 = Object.keys(value1);
        let keys2 = Object.keys(value2);

        if (keys1.length != keys2.length)
            return false;

        for (let i = 0; i < keys1.length; i++) {
            let key = keys1[i];

            if (key != keys2[i])
                return false;
            if (!equals(value1[key], value2[key]))
                return false;
        }

        return true;
    } else {
        let json1 = JSON.stringify(value1);
        let json2 = JSON.stringify(value2);

        return json1 == json2;
    }
}

function checkKey(key) {
    if (key.startsWith('$'))
        return false;
    if (key.startsWith('__'))
        return false;

    return true;
}

export {
    wrap,
    unwrap,
    annotate,
    hasChanged
}
