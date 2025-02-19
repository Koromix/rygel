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

import { Util, Log } from '../../../web/core/base.js';

let map = new WeakMap;

function wrap(data) {
    let raw = expand(data).$v;
    let obj = compact(raw);

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

        ptr.obj[key] = compact(value.$v);

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

function compact(value) {
    if (!Util.isPodObject(value))
        return value;

    let obj = {};
    for (let key in value)
        obj[key] = compact(value[key].$v);
    let proxy = new Proxy(obj, handler);

    let ptr = {
        raw: value,
        obj: obj
    };

    map.set(proxy, ptr);
    map.set(obj, ptr);

    return proxy;
}

function annotate(obj, key) {
    let ptr = map.get(obj);
    let raw = ptr?.raw?.[key];

    if (raw == null)
        return null;

    return raw.$n;
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
    annotate
}
