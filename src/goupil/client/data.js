// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

window.indexedDB = window.indexedDB || window.webkitIndexedDB || window.mozIndexedDB ||
                   window.OIndexedDB || window.msIndexedDB;

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

                    if (t_status === 'valid') {
                        let complete_funcs = [];
                        let error_funcs = []

                        let t = intf.db.transaction(Array.from(t_stores),
                                                    t_readwrite ? 'readwrite' : 'readonly');
                        for (let query of t_queries) {
                            query.func(t, query.resolve, query.reject);

                            if (t.oncomplete)
                                complete_funcs.push(t.oncomplete);
                            if (t.onerror)
                                error_funcs.push(t.onerror);
                        }

                        return new Promise((resolve, reject) => {
                            t.oncomplete = e => {
                                for (let func of complete_funcs)
                                    func();
                                resolve();
                            };
                            t.onerror = e => {
                                for (let func of error_funcs)
                                    func(e.target.error);
                                reject(e.target.error);
                            };
                        });
                    }
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
            resetTransaction();
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

                t.oncomplete = () => resolve();
                t.onerror = e => reject(e.target.error);
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

                t.oncomplete = e => resolve();
                t.onerror = e => reject(e.target.error);
            });
        };

        this.clear = function(store) {
            return executeQuery(store, true, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.clear();

                t.onsuccess = e => resolve();
                t.onerror = e => reject(e.target.error);
            });
        };
    }

    function resolvePending(pending) {
        for (let call of pending)
            call.func().then(call.resolve).catch(call.reject);
    }

    this.open = function(db_name, update_func = () => {}) {
        let req = window.indexedDB.open(db_name, 1);

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
            update_func(db);
        };

        return new DatabaseInterface(intf);
    };

    return this;
}).call({});
