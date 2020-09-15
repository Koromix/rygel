// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function VirtualRecords(db) {
    let self = this;

    this.create = function(table) {
        let id = util.makeULID();
        let record = {
            _ikey: makeEntryKey(table, id),

            table: table,
            id: id,

            versions: [],
            version: null,
            mtime: null,
            sequence: null,
            complete: {},

            values: {}
        };

        return record;
    };

    this.save = function(record, page, variables) { return saveFragment(record, page, variables, false); };
    this.validate = function(record, page) { return saveFragment(record, page, [], true); };

    async function saveFragment(record, page, variables, complete) {
        let frag = {
            _ikey: null,

            table: record.table,
            id: record.id,
            version: 0,
            page: page,

            username: null,
            mtime: new Date,
            anchor: -1, // null/undefined keys are not indexed in IndexedDB

            complete: complete,
            values: {}
        };
        for (let variable of variables)
            frag.values[variable.key] = !variable.missing ? record.values[variable.key] : null;

        let columns = variables.flatMap((variable, idx) => {
            if (variable.multi) {
                let ret = variable.props.map(prop => ({
                    key: makeColumnKeyMulti(record.table, page, variable.key, prop.value),

                    table: record.table,
                    page: page,
                    variable: variable.key.toString(),
                    type: variable.type,
                    prop: prop.value,

                    before: null,
                    after: null
                }));

                return ret;
            } else {
                let ret = {
                    key: makeColumnKey(record.table, page, variable.key),

                    table: record.table,
                    page: page,
                    variable: variable.key.toString(),
                    type: variable.type,

                    before: null,
                    after: null
                };

                return ret;
            }
        });
        for (let i = 0; i < columns.length; i++) {
            let col = columns[i];

            col.before = i ? columns[i - 1].key : null;
            col.after = i < (columns.length - 1) ? columns[i + 1].key : null;
        }

        await db.transaction('rw', ['rec_entries', 'rec_fragments', 'rec_columns'], async () => {
            let ikey = makeEntryKey(record.table, record.id);
            let entry = await db.load('rec_entries', ikey);

            if (entry) {
                if (entry.version !== record.version) {
                    console.log(entry, record);
                    throw new Error(`Cannot save old version of record ${record.id}`);
                }
            } else {
                entry = {
                    _ikey: ikey,

                    table: record.table,
                    id: record.id,

                    pages: [],
                    version: record.version || -1
                };
            }

            entry.pages = entry.pages.filter(key => key !== page);
            entry.pages.push(page);
            frag.version = ++entry.version;
            frag._ikey = makeFragmentKey(record.table, record.id, frag.version);

            db.save('rec_entries', entry);
            db.save('rec_fragments', frag);
            db.saveAll('rec_columns', columns);
        });

        let record2 = Object.assign({}, record);
        record2.versions = record.versions.slice();
        record2.versions.push({
            version: frag.version,
            username: frag.username,
            mtime: frag.mtime
        });
        record2.version = frag.version;
        record2.mtime = frag.mtime;
        record2.complete = Object.assign({}, record.complete);
        record2.complete[page] = complete;
        record2.values = Object.assign({}, record.values);

        return record2;
    }

    this.delete = async function(table, id) {
        let ikey = makeEntryKey(table, id);

        await db.transaction('rw', ['rec_entries', 'rec_fragments'], async () => {
            let entry = await db.load('rec_entries', ikey);

            if (entry != null) {
                entry.version++;

                let frag = {
                    _ikey: makeFragmentKey(table, id, entry.version),

                    table: table,
                    id: id,
                    version: entry.version,
                    page: null, // Delete fragment

                    username: null,
                    mtime: new Date,
                    anchor: -1
                };

                db.save('rec_entries', entry);
                db.save('rec_fragments', frag);
            }
        });
    };

    this.load = async function(table, id, version = undefined) {
        let ikey = makeEntryKey(table, id);

        let [entry, fragments] = await Promise.all([
            db.load('rec_entries', ikey),
            db.loadAll('rec_fragments', IDBKeyRange.bound(ikey + '@', ikey + '`', false, true))
        ]);

        if (entry && fragments.length) {
            let record = expandFragments(entry, fragments, version);
            return record;
        } else {
            return null;
        }
    };

    this.loadAll = async function(table) {
        let [records, fragments] = await Promise.all([
            db.loadAll('rec_entries', IDBKeyRange.bound(table + ':', table + '`', false, true)),
            db.loadAll('rec_fragments', IDBKeyRange.bound(table + ':', table + '`', false, true))
        ]);

        let i = 0, j = 0, k = 0;
        while (j < records.length && k < fragments.length) {
            records[i] = records[j++];

            // Find matching data row
            while (k < fragments.length && fragments[k]._ikey < records[i]._ikey)
                k++;

            let fragments_acc = [];
            while (k < fragments.length && fragments[k].table === table &&
                                           fragments[k].id === records[i].id)
                fragments_acc.push(fragments[k++]);

            if (fragments_acc.length) {
                records[i] = expandFragments(records[i], fragments_acc);
                i += !!records[i];
            }
        }
        records.length = i;

        return records;
    };

    function expandFragments(entry, fragments, version = undefined) {
        if (version == null)
            version = entry.version;

        let record = {
            _ikey: entry._ikey,

            table: entry.table,
            id: entry.id,

            versions: fragments.map(frag => ({
                version: frag.version,
                username: frag.username,
                mtime: frag.mtime
            })),
            version: null,
            mtime: null,
            sequence: entry.sequence,

            complete: {},
            values: {}
        };

        let pages_set = new Set(entry.pages);

        // Deleted record
        if (fragments[fragments.length - 1].page == null)
            return null;

        for (let i = fragments.length - 1; i >= 0 && pages_set.size; i--) {
            let frag = fragments[i];

            if (frag.version <= version) {
                if (record.version == null) {
                    record.version = frag.version;
                    record.mtime = frag.mtime;
                }

                if (frag.page != null && pages_set.delete(frag.page)) {
                    record.complete[frag.page] = frag.complete;
                    record.values = Object.assign(frag.values, record.values);
                }
            }
        }

        // Keys with undefined value are not actually stored by IndexedDB, so we encode
        // null when saving and restore undefined values on load.
        for (let key in record.values) {
            if (record.values[key] == null)
                record.values[key] = undefined;
        }

        if (record.version === record.versions.length - 1 && pages_set.size) {
            let list = Array.from(pages_set);
            console.error(`Damaged record ${record.id} (missing pages: ${list.join(', ')})`);
        }

        return record;
    }

    this.listColumns = async function(table) {
        return db.loadAll('rec_columns', IDBKeyRange.bound(table + '@', table + '`', false, true));
    };

    this.sync = async function() {
        let fragments = await db.loadAll('rec_fragments');
        // fragments = fragments.filter(frag => frag.anchor < 0);

        // Upload fragments
        let uploads = util.mapRLE(fragments, frag => frag.id, (id, offset, length) => {
            let record_fragments = fragments.slice(offset, offset + length);

            if (record_fragments.some(frag => frag.anchor < 0)) {
                let frag0 = record_fragments[0];

                record_fragments = record_fragments.map(frag => ({
                    mtime: frag.mtime,
                    version: frag.version,
                    page: frag.page,
                    values: frag.values
                }));

                let url = `${env.base_url}api/records/${frag0.table}/${frag0.id}`;
                return net.fetch(url, {
                    method: 'PUT',
                    body: JSON.stringify(record_fragments)
                });
            } else {
                return null;
            }
        });
        for (let p of uploads) {
            if (p != null) {
                let response = await p;

                if (!response.ok) {
                    let err = (await response.text()).trim();

                    if (response.status === 409) {
                        // XXX: Avoid this kind of silent data loss...
                        console.log(err.message);
                    } else {
                        throw new Error(err);
                    }
                }
            }
        }

        // Get new fragments
        for (let form of app.forms) {
            let url = `${env.base_url}api/records/${form.key}`;
            let records = await net.fetch(url).then(response => response.json());

            let entries = records.map(record => ({
                _ikey: makeEntryKey(record.table, record.id),
                id: record.id,
                pages: record.fragments.filter(frag => frag.page != null).map(frag => frag.page),
                table: record.table,
                version: record.fragments.length - 1,
                sequence: record.sequence
            }));
            let fragments = records.flatMap(record => record.fragments.map((frag, version) => ({
                _ikey: makeFragmentKey(record.table, record.id, version),
                anchor: frag.anchor,
                complete: frag.complete,
                id: record.id,
                mtime: frag.mtime,
                page: frag.page,
                table: record.table,
                username: frag.username,
                values: frag.values,
                version: version
            })));

            await db.saveAll('rec_entries', entries);
            await db.saveAll('rec_fragments', fragments);
        }

        // Update columns
        for (let form of app.forms) {
            let url = util.pasteURL(`${env.base_url}api/records/columns`, {table: form.key});
            let columns = await net.fetch(url).then(response => response.json());

            columns = columns.map(col => {
                let col2 = {
                    key: null,

                    // XXX: Fix missing variable type
                    table: form.key,
                    page: col.page,
                    variable: col.variable,
                    prop: null,

                    before: col.before,
                    after: col.after
                }

                if (col.hasOwnProperty('prop')) {
                    col2.key = makeColumnKeyMulti(form.key, col.page, col.variable, col.prop);
                    col2.prop = col.prop;
                } else {
                    col2.key = makeColumnKey(form.key, col.page, col.variable);
                    delete col2.prop;
                }

                return col2;
            });

            await db.saveAll('rec_columns', columns);
        }
    };

    function makeEntryKey(table, id) {
        return `${table}:${id}`;
    }

    function makeFragmentKey(table, id, version) {
        return `${table}:${id}@${version.toString().padStart(9, '0')}`;
    }

    function makeColumnKey(table, page, variable) {
        return `${table}@${page}.${variable}`;
    }
    function makeColumnKeyMulti(table, page, variable, prop) {
        return `${table}@${page}.${variable}@${prop}`;
    }
}
