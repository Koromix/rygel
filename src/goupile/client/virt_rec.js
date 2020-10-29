// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function VirtualRecords(db, zone) {
    let self = this;

    this.create = function(table) {
        let id = util.makeULID();
        let record = {
            _ikey: makeEntryKey(table, id),

            table: table,
            id: id,
            zone: zone,

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
            zone: zone,
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

        await db.transaction('rw', ['rec_entries', 'rec_fragments', 'rec_columns'], async t => {
            let ikey = makeEntryKey(record.table, record.id);
            let entry = await t.load('rec_entries', ikey);

            if (entry) {
                if (!testZone(entry)) {
                    console.log(entry, record);
                    throw new Error(`Zone mismatch for record ${record.id}`);
                }
                if (entry.version !== record.version) {
                    console.log(entry, record);
                    throw new Error(`Cannot save old version of record ${record.id}`);
                }
            } else {
                entry = {
                    _ikey: ikey,

                    table: record.table,
                    id: record.id,
                    zone: zone,

                    version: record.version || -1
                };
            }

            frag.version = ++entry.version;
            frag._ikey = makeFragmentKey(record.table, record.id, frag.version);

            t.save('rec_entries', entry);
            t.save('rec_fragments', frag);
            t.saveAll('rec_columns', columns);
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

        await db.transaction('rw', ['rec_entries', 'rec_fragments'], async t => {
            let entry = await t.load('rec_entries', ikey);

            if (!testZone(entry))
                throw new Error(`Zone mismatch for record ${id}`);

            if (entry != null) {
                entry.version++;

                let frag = {
                    _ikey: makeFragmentKey(table, id, entry.version),

                    table: table,
                    id: id,
                    zone: zone,
                    version: entry.version,
                    page: null, // Delete fragment

                    username: null,
                    mtime: new Date,
                    anchor: -1
                };

                t.save('rec_entries', entry);
                t.save('rec_fragments', frag);
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
            if (!testZone(entry))
                throw new Error(`Zone mismatch for record ${id}`);

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
            let entry = records[j++];

            if (testZone(entry)) {
                while (k < fragments.length && fragments[k]._ikey < entry._ikey)
                    k++;

                let fragments_acc = [];
                while (k < fragments.length && fragments[k].table === table &&
                                               fragments[k].id === entry.id)
                    fragments_acc.push(fragments[k++]);

                if (fragments_acc.length) {
                    records[i] = expandFragments(entry, fragments_acc);
                    i += !!records[i];
                }
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
            zone: entry.zone,

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

        let pages_set = new Set;

        // Deleted record
        if (fragments[fragments.length - 1].page == null)
            return null;

        for (let i = fragments.length - 1; i >= 0; i--) {
            let frag = fragments[i];

            if (frag.version <= version) {
                if (record.version == null) {
                    record.version = frag.version;
                    record.mtime = frag.mtime;
                }

                if (frag.page != null && !pages_set.has(frag.page)) {
                    record.complete[frag.page] = frag.complete;
                    record.values = Object.assign(frag.values, record.values);

                    pages_set.add(frag.page);
                }
            }
        }

        // Keys with undefined value are not actually stored by IndexedDB, so we encode
        // null when saving and restore undefined values on load.
        for (let key in record.values) {
            if (record.values[key] == null)
                record.values[key] = undefined;
        }

        return record;
    }

    function testZone(entry) {
        return zone == null || entry.zone == null || entry.zone === zone;
    }

    this.listColumns = async function(table) {
        return db.loadAll('rec_columns', IDBKeyRange.bound(table + '@', table + '`', false, true));
    };

    this.sync = async function() {
        // Upload new fragments
        {
            let new_fragments = await db.loadAll('rec_fragments/anchor', IDBKeyRange.only(-1));
            new_fragments.sort((frag1, frag2) => util.compareValues(frag1._ikey, frag2._ikey));

            let records = util.mapRLE(new_fragments, frag => frag.id, (id, offset, length) => {
                let fragments = new_fragments.slice(offset, offset + length);
                let frag0 = fragments[0];

                let record = {
                    table: frag0.table,
                    id: frag0.id,
                    zone: frag0.zone,

                    fragments: fragments.map(frag => ({
                        mtime: frag.mtime,
                        version: frag.version,
                        page: frag.page,
                        complete: frag.complete,
                        values: frag.values
                    }))
                };

                return record;
            });
            records = Array.from(records);

            if (zone != null)
                records = records.filter(record => record.zone == null || record.zone === zone);

            if (records.length) {
                let response = await net.fetch(`${env.base_url}api/records/sync`, {
                    method: 'POST',
                    body: JSON.stringify(records)
                });

                if (!response.ok) {
                    if (response.status === 409) {
                        console.log('Some records are in conflict');
                    } else {
                        let err = (await response.text()).trim();
                        throw new Error(err);
                    }
                }
            }
        }

        let anchor = await db.load('rec_sync', (zone != null) ? zone : 0);
        anchor = (anchor != null) ? (anchor + 1) : 0;

        // Update columns
        {
            let url = util.pasteURL(`${env.base_url}api/records/columns`, {anchor: anchor});
            let columns = await net.fetch(url).then(response => response.json());

            if (columns.length) {
                columns = columns.map(col => {
                    let col2 = {
                        key: null,

                        table: col.table,
                        page: col.page,
                        variable: col.variable,
                        type: col.type,
                        prop: col.prop,

                        before: col.before,
                        after: col.after
                    }

                    if (col.hasOwnProperty('prop')) {
                        col2.key = makeColumnKeyMulti(col.table, col.page, col.variable, col.prop);
                    } else {
                        col2.key = makeColumnKey(col.table, col.page, col.variable);
                        delete col2.prop;
                    }

                    return col2;
                });

                await db.saveAll('rec_columns', columns);
            }
        }

        // Get new fragments
        let changes = [];
        {
            let url = util.pasteURL(`${env.base_url}api/records/load`, {anchor: anchor});
            let records = await net.fetch(url).then(response => response.json());

            if (records.length) {
                let entries = records.map(record => ({
                    _ikey: makeEntryKey(record.table, record.id),

                    id: record.id,
                    table: record.table,
                    zone: record.zone,
                    sequence: record.sequence,

                    version: record.fragments[record.fragments.length - 1].version,
                }));

                let fragments = records.flatMap(record => record.fragments.map(frag => ({
                    _ikey: makeFragmentKey(record.table, record.id, frag.version),

                    table: record.table,
                    id: record.id,
                    zone: record.zone,
                    version: frag.version,
                    page: frag.page,

                    username: frag.username,
                    mtime: new Date(frag.mtime),
                    anchor: frag.anchor,

                    complete: frag.complete,
                    values: frag.values
                })));

                await db.saveAll('rec_entries', entries);
                await db.saveAll('rec_fragments', fragments);

                let new_anchor = Math.max(...fragments.map(frag => frag.anchor));
                await db.saveWithKey('rec_sync', (zone != null) ? zone : 0, new_anchor);
            }

            changes = records.map(record => record.id);
        }

        return changes;
    };

    function makeEntryKey(table, id) {
        return `${table}:${id}`;
    }

    function makeFragmentKey(table, id, version) {
        return `${table}:${id}@${version.toString().padStart(9, '0')}`;
    }

    // XXX: Duplicated in server/js.js, keep in sync
    function makeColumnKey(table, page, variable) {
        return `${table}@${page}.${variable}`;
    }
    function makeColumnKeyMulti(table, page, variable, prop) {
        return `${table}@${page}.${variable}@${prop}`;
    }
}
