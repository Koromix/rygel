// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// TODO: Support degraded mode when IDB is not available (e.g. private browsing)
let idb = new function () {
    function DatabaseInterface(db) {
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
            if (t_status !== 'none')
                throw new Error('Ongoing database transaction');

            try {
                t_status = 'valid';
                func(self);

                let t = db.transaction(Array.from(t_stores),
                                       t_readwrite ? 'readwrite' : 'readonly');

                for (let query of t_queries)
                    query.func(t, query.resolve, query.reject);
                if (t_status === 'abort')
                    t.abort();

                return new Promise((resolve, reject) => {
                    t.addEventListener('complete', e => resolve());
                    t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
                });
            } finally {
                resetTransaction();
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
                t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
            });
        };

        this.saveWithKey = function(store, key, value) {
            return executeQuery(store, true, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.put(value, key);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
            });
        };

        this.saveAll = function(store, values) {
            return executeQuery(store, true, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                for (let value of values)
                    obj.put(value);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
            });
        };

        this.load = function(store, key) {
            return executeQuery(store, false, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                let req = obj.get(key);

                req.onsuccess = e => resolve(e.target.result);
                req.onerror = e => logAndReject(reject, e.target.error);
            });
        };

        this.loadAll = function(store, start = null, end = null) {
            let query = makeKeyRange(start, end);

            return executeQuery(store, false, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                let req = obj.getAll(query);

                req.onsuccess = e => resolve(e.target.result);
                req.onerror = e => logAndReject(reject, e.target.error);
            });
        };

        this.list = function(store, start = null, end = null) {
            let query = makeKeyRange(start, end);

            return executeQuery(store, false, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                let req = obj.getAllKeys(query);

                req.onsuccess = e => resolve(e.target.result);
                req.onerror = e => logAndReject(reject, e.target.error);
            });
        };

        this.delete = function(store, key) {
            return executeQuery(store, true, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.delete(key);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
            });
        };

        this.deleteAll = function(store, start = null, end = null) {
            let query = makeKeyRange(start, end);

            if (query) {
                return executeQuery(store, true, (t, resolve, reject) => {
                    let obj = t.objectStore(store);
                    obj.delete(query);

                    t.addEventListener('complete', e => resolve());
                    t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
                });
            } else {
                return self.clear();
            }
        };

        this.clear = function(store) {
            return executeQuery(store, true, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.clear();

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
            });
        };

        function makeKeyRange(start, end) {
            if (start != null && end != null) {
                return IDBKeyRange.bound(start, end, false, true);
            } else if (start != null) {
                return IDBKeyRange.lowerBound(start);
            } else if (end != null) {
                return IDBKeyRange.upperBound(end, true);
            } else {
                return null;
            }
        }
    }

    this.open = function(db_name, version = null, update_func = () => {}) {
        let req;
        if (version != null) {
            req = indexedDB.open(db_name, version);

            req.onupgradeneeded = e => {
                let db = e.target.result;
                update_func(db, e.oldVersion || null);
            };
        } else {
            req = indexedDB.open(db_name);
        }

        return new Promise((resolve, reject) => {
            req.onsuccess = e => {
                let intf = new DatabaseInterface(e.target.result);
                resolve(intf);
            };
            req.onerror = e => logAndReject(reject, 'Failed to open database');
        });
    };

    function logAndReject(reject, err) {
        log.error(err);
        reject(err);
    }
};
