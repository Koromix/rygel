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

import { Util, Log } from '../../web/core/common.js';

// Globally shared between all MagicData instances
let raw_map = new WeakMap;

function MagicData(raw = {}, annotations = null) {
    let self = this;

    let meta_map = new WeakMap;

    let all_changes = new Set;
    let clear_markers = {};

    raw = raw_map.get(raw) ?? raw;

    let root = proxyObject(raw);

    if (annotations != null)
        importAnnotations(raw, annotations);

    function importAnnotations(obj, annotations) {
        if (annotations == null)
            return;

        let meta = findMeta(obj);

        for (let kind in annotations.notes) {
            meta.notes[kind] = {
                clear: 0,
                data: annotations.notes[kind]
            };

            clear_markers[kind] = 0;
        }

        for (let key in annotations.children) {
            let target = obj[key];

            if (Util.isPodObject(target))
                importAnnotations(target, annotations.children[key]);
        }
    }

    Object.defineProperties(this, {
        proxy: { get: () => root, enumerable: true },
        raw: { get: () => raw, enumerable: true },
    });

    this.hasChanged = function() { return all_changes.size > 0; };

    function proxyObject(obj) {
        if (obj == null)
            return obj;
        if (typeof obj != 'object')
            return obj;
        if (Object.prototype.toString.call(obj) != '[object Object]') // Don't mess with "special" objects
            return obj;

        let meta = meta_map.get(obj);

        if (meta == null) {
            meta = {
                notes: {},
                proxy: null
            };

            let copy = {};
            let changes = new Set;

            meta.proxy = new Proxy(obj, {
                set: (obj, key, value) => {
                    value = proxyObject(value);
                    obj[key] = raw_map.get(value) ?? value;

                    let json1 = JSON.stringify(copy[key]) || '{}';
                    let json2 = JSON.stringify(value) || '{}';

                    if (json2 !== json1) {
                        changes.add(key);
                    } else {
                        changes.delete(key);
                    }

                    if (changes.size) {
                        all_changes.add(obj);
                    } else {
                        all_changes.delete(obj);
                    }

                    return true;
                },

                get: (obj, key) => proxyObject(obj[key])
            });

            for (let key in obj) {
                let value = proxyObject(obj[key]);
                copy[key] = value;
            }

            meta_map.set(obj, meta);
            raw_map.set(meta.proxy, obj);
        }

        return meta.proxy;
    }

    this.openNote = function(obj, kind, default_value) {
        if (obj == null)
            [obj, kind] = [root, obj];

        let meta = findMeta(obj);
        let note = meta.notes[kind];

        if (note == null) {
            if (clear_markers[kind] == null)
                clear_markers[kind] = 0;

            note = {
                clear: clear_markers[kind],
                data: default_value
            };

            meta.notes[kind] = note;
        }

        return note.data;
    };

    this.clearNotes = function(kind) {
        if (clear_markers[kind] != null)
            clear_markers[kind]++;
    };

    this.exportNotes = function() {
        let [annotations] = exportAnnotations(raw);
        return annotations;
    };

    function exportAnnotations(obj) {
        let annotations = {
            notes: {},
            children: {}
        };
        let empty = true;

        let meta = findMeta(obj);

        for (let kind in meta.notes) {
            let note = meta.notes[kind];

            if (!isEmpty(note.data)) {
                annotations.notes[kind] = note.data;
                empty = false;
            }
        }

        for (let key in obj) {
            let value = obj[key];

            if (Util.isPodObject(value)) {
                let [ret, valid] = exportAnnotations(value);

                if (valid) {
                    annotations.children[key] = ret;
                    empty = false;
                }
            }
        }

        return [annotations, !empty];
    }

    function findMeta(obj) {
        obj = raw_map.get(obj) ?? obj;

        let meta = meta_map.get(obj);
        if (meta == null)
            throw new Error('Cannot annotate object outside of managed values');

        // Clean up cleared notes
        for (let kind in meta.notes) {
            let note = meta.notes[kind];

            if (note.clear < clear_markers[kind]) {
                note.clear = clear_markers[kind];
                note.data = Array.isArray(note.data) ? [] : {};
            }
        }

        return meta;
    }

    function isEmpty(notes) {
        return !Object.keys(notes).length;
    }
}

export { MagicData }
