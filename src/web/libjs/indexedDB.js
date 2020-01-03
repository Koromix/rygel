// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// XXX: Support degraded mode when IDB is not available (e.g. private browsing)
let idb = new function () {
    this.open = function(db_name, version = null, version_func = null) {
        let req;
        if (version != null) {
            req = indexedDB.open(db_name, version);

            req.onupgradeneeded = e => {
                let db = e.target.result;
                let intf = new DatabaseInterface(e.target.result, e.target.transaction);

                intf.createStore = function(store, params = {}) { db.createObjectStore(store, params); };
                intf.deleteStore = function(store) { db.deleteObjectStore(store); };

                version_func(intf, e.oldVersion || null);
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

    function DatabaseInterface(db, transaction = null) {
        let self = this;

        let aborted = false;

        this.transaction = async function(mode, stores, func) {
            switch (mode) {
                case 'r': { mode = 'readonly'; } break;
                case 'rw': { mode = 'readwrite'; } break;
            }
            if (!Array.isArray(stores))
                stores = [stores];

            if (transaction)
                throw new Error('Nested transactions are not supported');

            transaction = db.transaction(stores, mode);
            try {
                await func();
            } catch (err) {
                if (!aborted)
                    transaction.abort();

                throw err;
            } finally {
                transaction = null;
                aborted = false;
            }
        };

        this.abort = function() {
            transaction.abort();
            aborted = true;
        };

        this.save = function(store, value) {
            return executeQuery('readwrite', store, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.put(value);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
            });
        };

        this.saveWithKey = function(store, key, value) {
            return executeQuery('readwrite', store, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.put(value, key);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
            });
        };

        this.saveAll = function(store, values) {
            return executeQuery('readwrite', store, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                for (let value of values)
                    obj.put(value);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
            });
        };

        this.load = function(store, key) {
            return executeQuery('readonly', store, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                let req = obj.get(key);

                req.onsuccess = e => resolve(e.target.result);
                req.onerror = e => logAndReject(reject, e.target.error);
            });
        };

        this.loadAll = function(store, start = null, end = null) {
            let query = makeKeyRange(start, end);

            return executeQuery('readonly', store, (t, resolve, reject) => {
                let obj = t.objectStore(store);

                if (obj.getAll) {
                    let req = obj.getAll(query);

                    req.onsuccess = e => resolve(e.target.result);
                    req.onerror = e => logAndReject(reject, e.target.error);
                } else {
                    let cur = obj.openCursor(query);
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

        this.list = function(store, start = null, end = null) {
            let query = makeKeyRange(start, end);

            return executeQuery('readonly', store, (t, resolve, reject) => {
                let obj = t.objectStore(store);

                if (obj.getAllKeys) {
                    let req = obj.getAllKeys(query);

                    req.onsuccess = e => resolve(e.target.result);
                    req.onerror = e => logAndReject(reject, e.target.error);
                } else {
                    let cur = obj.openKeyCursor(query);
                    let keys = [];

                    cur.onsuccess = e => {
                        let cursor = e.target.result;
                        if (cursor) {
                            keys.push(cursor.key);
                            cursor.continue();
                        } else {
                            resolve(keys);
                        }
                    };
                    cur.onerror = e => reject(e.target.error);
                }
            });
        };

        this.delete = function(store, key) {
            return executeQuery('readwrite', store, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                obj.delete(key);

                t.addEventListener('complete', e => resolve());
                t.addEventListener('abort', e => logAndReject(reject, 'Database transaction failure'));
            });
        };

        this.deleteAll = function(store, start = null, end = null) {
            let query = makeKeyRange(start, end);

            if (query) {
                return executeQuery('readwrite', store, (t, resolve, reject) => {
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
            return executeQuery('readwrite', store, (t, resolve, reject) => {
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

        function executeQuery(mode, store, func) {
            if (transaction) {
                return new Promise((resolve, reject) => func(transaction, resolve, reject));
            } else {
                let t = db.transaction([store], mode);
                return new Promise((resolve, reject) => func(t, resolve, reject));
            }
        }
    }

    function logAndReject(reject, err) {
        log.error(err);
        reject(err);
    }
};
