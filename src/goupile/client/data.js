// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

function MagicData(raw = {}, annotations = null) {
    let self = this;

    let meta_map = new WeakMap;
    let raw_map = MagicData.raw_map;
    let all_changes = new Set;

    let flat_notes = [];
    let next_note_id = 0;
    let clear_markers = {};

    raw = raw_map.get(raw) ?? raw;

    let root = proxyObject(raw);

    if (annotations != null) {
        importAnnotations(raw, annotations);

        flat_notes.sort((note1, note2) => note1.id - note2.id);
        next_note_id = Math.max(-1, ...flat_notes.map(note => note.id)) + 1;

        for (let note of flat_notes)
            clear_markers[note.kind] = 0;
    }

    function importAnnotations(obj, annotations) {
        if (annotations == null)
            return;

        let meta = findMeta(obj);

        meta.notes = annotations.notes.slice();
        flat_notes.push(...annotations.notes);

        for (let key in annotations.children) {
            let target = obj[key];

            if (util.isPodObject(target))
                importAnnotations(target, annotations.children[key]);
        }
    }

    Object.defineProperties(this, {
        raw: { get: () => raw, enumerable: true },
        values: { get: () => root, enumerable: true },

        hasChanged: { get: () => all_changes.size > 0, enumerable: true }
    });

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
                notes: [],
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

    this.annotate = function(obj, kind, value) {
        if (typeof obj == 'string')
            [obj, kind, value] = [root, obj, kind];

        let meta = findMeta(obj);

        let note = {
            id: next_note_id++,
            kind: kind,
            value: value
        };

        if (clear_markers[kind] == null)
            clear_markers[kind] = 0;

        meta.notes.push(note);
        flat_notes.push(note);
    };

    this.getAllNotes = function(kind) {
        let notes = flat_notes;

        if (kind != null)
            notes = notes.filter(note => note.kind == kind);

        return notes;
    };

    this.getObjectNotes = function(obj, kind) {
        if (obj == null || typeof obj == 'string')
            [obj, kind] = [root, obj];

        let meta = findMeta(obj);
        let notes = meta.notes;

        if (kind != null)
            notes = notes.filter(note => note.kind == kind);

        return notes;
    };

    this.clearNotes = function(kind) {
        if (kind != null) {
            clear_markers[kind] = next_note_id;
            flat_notes = flat_notes.filter(note => note.kind != kind);
        } else {
            for (let key in clear_markers)
                clear_markers[kind] = next_note_id;
            flat_notes = [];
        }
    };

    this.exportWithNotes = function() {
        let min_note_id = flat_notes.length ? Math.min(...flat_notes.map(note => note.id)) : 0;
        let [annotations] = exportAnnotations(raw, min_note_id);

        let ret = {
            annotations: annotations,
            values: raw
        };

        return ret;
    };

    function exportAnnotations(obj, min_note_id) {
        let annotations = {
            notes: [],
            children: {}
        };
        let empty = true;

        let meta = findMeta(obj);

        if (meta.notes.length) {
            annotations.notes = meta.notes.slice();
            empty = false;
        }

        for (let key in obj) {
            let value = obj[key];

            if (util.isPodObject(value)) {
                let [ret, valid] = exportAnnotations(value, min_note_id);

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

        // Drop outdated notes
        meta.notes = meta.notes.filter(note => note.id >= clear_markers[note.kind]);

        return meta;
    }
}
MagicData.raw_map = new WeakMap;
