// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

window.indexedDB = window.indexedDB || window.webkitIndexedDB || window.mozIndexedDB ||
                   window.OIndexedDB || window.msIndexedDB;

// TODO: Support degraded mode when IDB is not available (e.g. private browsing)
let data = (function () {
    // Beware, the IndexedDB API is asynchronous crap. Not my fault.
    // The whole point of this is to hide most of the async stuff.

    function DatabaseInterface(intf) {
        let self = this;

        let t_status = 'none';
        let t_readwrite = false;
        let t_stores = new Set;
        let t_queries = [];

        function resetTransaction() {
            t_status = 'none';
            t_readwrite = false;
            t_stores.clear();
            t_queries.length = 0;
        }

        this.transaction = function(func) {
            if (intf.db) {
                if (t_status !== 'none')
                    throw new Error('Ongoing database transaction');

                try {
                    t_status = 'valid';
                    func(self);

                    let t = intf.db.transaction(Array.from(t_stores),
                                                t_readwrite ? 'readwrite' : 'readonly');

                    for (let query of t_queries)
                        query.func(t, query.resolve, query.reject);
                    if (t_status === 'abort')
                        t.abort();

                    return new Promise((resolve, reject) => {
                        t.addEventListener('complete', e => resolve());
                        t.addEventListener('abort', e => {
                            log.error('Database transaction failure');
                            reject('Database transaction failure');
                        });
                    });
                } finally {
                    resetTransaction();
                }
            } else if (!intf.broken) {
                return new Promise((resolve, reject) => {
                    let call = {
                        func: () => self.transaction(func),
                        resolve: resolve,
                        reject: reject
                    };
                    intf.pending.push(call);
                });
            } else {
                throw new Error('Database not available');
            }
        };

        this.abort = function() {
            t_status = 'abort';
        };

        function executeQuery(store, readwrite, func) {
            if (t_status !== 'none') {
                t_readwrite |= readwrite;
                t_stores.add(store);

                return new Promise((resolve, reject) => {
                    let query = {
                        func: func,
                        resolve: resolve,
                        reject: reject
                    };
                    t_queries.push(query);
                });
            } else {
                let ret;
                self.transaction(() => { ret = executeQuery(store, readwrite, func); });
                return ret;
            }
        }

        this.save = function(store, value) {
            return executeQuery(store, true, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.put(value);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => reject('Database transaction failure'));
            });
        };

        this.saveWithKey = function(store, key, value) {
            return executeQuery(store, true, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.put(value, key);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => reject('Database transaction failure'));
            });
        };

        this.saveAll = function(store, values) {
            return executeQuery(store, true, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                for (let value of values)
                    obj.put(value);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => reject('Database transaction failure'));
            });
        };

        this.load = function(store, key) {
            return executeQuery(store, false, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                let req = obj.get(key);

                req.onsuccess = e => resolve(e.target.result);
                req.onerror = e => reject(e.target.error);
            });
        };

        this.loadAll = function(store) {
            return executeQuery(store, false, (t, resolve, reject) => {
                let obj = t.objectStore(store);

                if (obj.getAll) {
                    let req = obj.getAll();

                    req.onsuccess = e => resolve(e.target.result);
                    req.onerror = e => reject(e.target.error);
                } else {
                    let cur = obj.openCursor();
                    let values = [];

                    cur.onsuccess = e => {
                        let cursor = e.target.result;
                        if (cursor) {
                            values.push(cursor.value);
                            cursor.continue();
                        } else {
                            resolve(values);
                        }
                    };
                    cur.onerror = e => reject(e.target.error);
                }
            });
        };

        this.delete = function(store, key) {
            return executeQuery(store, true, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.delete(key);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => reject('Database transaction failure'));
            });
        };

        this.clear = function(store) {
            return executeQuery(store, true, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.clear();

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => reject('Database transaction failure'));
            });
        };
    }

    function resolvePending(pending) {
        for (let call of pending)
            call.func().then(call.resolve).catch(call.reject);
    }

    this.open = function(db_name, version, update_func = () => {}) {
        let req = window.indexedDB.open(db_name, version);

        let intf = {
            db: null,
            broken: false,
            pending: []
        };

        req.onsuccess = e => {
            intf.db = e.target.result;
            resolvePending(intf.pending);
        };
        req.onerror = e => {
            console.log('Failed to open database');
            intf.broken = true;
            resolvePending(intf.pending);
        };
        req.onupgradeneeded = e => {
            let db = e.target.result;
            update_func(db, e.oldVersion || null);
        };

        return new DatabaseInterface(intf);
    };

    return this;
}).call({});
