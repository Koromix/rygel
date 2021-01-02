// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// XXX: Support degraded mode when IDB is not available (e.g. private browsing)
const indexeddb = new function() {
    this.open = function(db_name, version = undefined, version_func = undefined) {
        let req;
        if (version != null) {
            req = indexedDB.open(db_name, version);

            req.onupgradeneeded = e => {
                let db = e.target.result;
                let t = e.target.transaction;
                let intf = new DatabaseInterface(e.target.result, t);

                intf.createStore = function(store, params = {}) { return db.createObjectStore(store, params); };
                intf.deleteStore = function(store) { db.deleteObjectStore(store); };

                intf.createIndex = function(store, index, path, params = {}) {
                    let obj = t.objectStore(store);
                    obj.createIndex(index, path, params);
                };
                intf.deleteIndex = function(store, index) {
                    let obj = t.objectStore(store);
                    obj.deleteIndex(index);
                };

                version_func(intf, e.oldVersion || null);
            };
        } else {
            req = indexedDB.open(db_name);
        }

        return new Promise((resolve, reject) => {
            req.onsuccess = e => {
                let intf = new DatabaseInterface(e.target.result, null);
                resolve(intf);
            };
            req.onerror = e => logAndReject(reject, 'Failed to open database');
        });
    };

    function DatabaseInterface(db, transaction) {
        let self = this;

        let aborted = false;

        this.transaction = async function(mode, stores, func) {
            switch (mode) {
                case 'r': { mode = 'readonly'; } break;
                case 'rw': { mode = 'readwrite'; } break;
            }
            if (Array.isArray(stores) && stores.length === 1)
                stores = stores[0];

            if (transaction)
                throw new Error('Nested transactions are not supported');

            let t = db.transaction(stores, mode);
            let intf = new DatabaseInterface(db, t);

            let p = new Promise((resolve, reject) => {
                t.oncomplete = e => resolve();
                t.onabort = e => reject(new Error('Database transaction aborted'));
                t.onerror = e => {
                    e.preventDefault();
                    reject(new Error(t.error));
                };
            });

            try {
                await func(intf);
                await p;
            } catch (err) {
                if (t.error)
                    intf.abort();

                throw err;
            }
        };

        this.abort = function() {
            if (transaction == null)
                throw new Error('Cannot abort from outside transaction');

            if (!aborted) {
                transaction.abort();
                aborted = true;
            }
        };

        this.save = function(store, value) {
            return executeQuery('readwrite', store, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                let req = obj.put(value);

                req.onsuccess = e => resolve();
                req.onerror = e => logAndReject(reject, 'Failed to save record');
            });
        };

        this.saveWithKey = function(store, key, value) {
            return executeQuery('readwrite', store, (t, resolve, reject) => {
                let obj = t.objectStore(store);
                let req = obj.put(value, key);

                req.onsuccess = e => resolve();
                req.onerror = e => logAndReject(reject, 'Failed to save record');
            });
        };

        this.saveAll = async function(store, values) {
            for (let value of values)
                await self.save(store, value);
        };

        this.load = function(where, key) {
            let [store, index] = where.split('/');

            return executeQuery('readonly', store, (t, resolve, reject) => {
                let obj = openStoreOrIndex(t, store, index);
                let req = obj.get(key);

                req.onsuccess = e => resolve(e.target.result);
                req.onerror = e => logAndReject(reject, e.target.error);
            });
        };

        this.loadAll = function(where, range = undefined) {
            let [store, index] = where.split('/');

            return executeQuery('readonly', store, (t, resolve, reject) => {
                let obj = openStoreOrIndex(t, store, index);

                if (obj.getAll) {
                    let req = obj.getAll(range);

                    req.onsuccess = e => resolve(e.target.result);
                    req.onerror = e => logAndReject(reject, e.target.error);
                } else {
                    let cur = obj.openCursor(range);
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
                    cur.onerror = e => reject(new Error(e.target.error));
                }
            });
        };

        this.list = function(where, range = undefined) {
            let [store, index] = where.split('/');

            return executeQuery('readonly', store, (t, resolve, reject) => {
                let obj = openStoreOrIndex(t, store, index);

                if (obj.getAllKeys) {
                    let req = obj.getAllKeys(range);

                    req.onsuccess = e => resolve(e.target.result);
                    req.onerror = e => logAndReject(reject, e.target.error);
                } else {
                    let cur = obj.openKeyCursor(range);
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
                    cur.onerror = e => reject(new Error(e.target.error));
                }
            });
        };

        this.getMinMax = function(where) {
            let [store, index] = where.split('/');

            return executeQuery('readonly', store, (t, resolve, reject) => {
                let obj = openStoreOrIndex(t, store, index);
                let cur1 = obj.openKeyCursor(null, 'next');
                let cur2 = obj.openKeyCursor(null, 'prev');

                let min, max;
                cur1.onsuccess = e => {
                    let cursor = e.target.result;
                    if (cursor)
                        min = cursor.key;
                };
                cur2.onsuccess = e => {
                    let cursor = e.target.result;
                    if (cursor)
                        max = cursor.key;
                };
                cur1.onerror = e => reject(new Error(e.target.error));
                cur2.onerror = e => reject(new Error(e.target.error));

                t.addEventListener('complete', e => resolve([min, max]));
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

        this.deleteAll = function(store, range = undefined) {
            if (range != null) {
                return executeQuery('readwrite', store, (t, resolve, reject) => {
                    let obj = t.objectStore(store);
                    obj.delete(range);

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

        function openStoreOrIndex(t, store, index = null) {
            let obj = t.objectStore(store);

            if (index != null) {
                return obj.index(index);
            } else {
                return obj;
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

    function logAndReject(reject, msg) {
        log.error(msg);
        reject(new Error(msg));
    }
};
