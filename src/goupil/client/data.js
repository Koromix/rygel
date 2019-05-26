// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

window.indexedDB = window.indexedDB || window.webkitIndexedDB || window.mozIndexedDB ||
                   window.OIndexedDB || window.msIndexedDB;

let data = (function () {
    // Beware, the IndexedDB API is asynchronous crap. Not my fault.
    // The whole point of this is to hide most of the async stuff.

    function ObjectStoreWrapper(intf, store_name) {
        function restartWhenReady(func) {
            if (!intf.broken) {
                return new Promise((resolve, reject) => {
                    let call = {
                        func: func,
                        resolve: resolve,
                        reject: reject
                    };
                    intf.pending.push(call);
                });
            } else {
                throw new Error('Database not available');
            }
        }

        this.save = function(value) {
            if (!intf.db)
                return restartWhenReady(() => this.save(key, value));

            let t = intf.db.transaction([store_name], 'readwrite');
            let obj = t.objectStore(store_name);

            obj.put(value);

            return new Promise((resolve, reject) => {
                t.oncomplete = () => resolve();
                t.onerror = e => reject(e.target.error);
            });
        };

        this.load = function(key) {
            if (!intf.db)
                return restartWhenReady(() => this.load(key));

            let t = intf.db.transaction([store_name]);
            let obj = t.objectStore(store_name);
            let req = obj.get(key);

            return new Promise((resolve, reject) => {
                req.onsuccess = e => resolve(e.target.result);
                req.onerror = e => reject(e.target.error);
            });
        };

        this.loadAll = function() {
            if (!intf.db)
                return restartWhenReady(this.loadAll);

            let t = intf.db.transaction([store_name]);
            let obj = t.objectStore(store_name);

            if (obj.getAll) {
                let req = obj.getAll();

                return new Promise((resolve, reject) => {
                    req.onsuccess = e => resolve(e.target.result);
                    req.onerror = e => reject(e.target.error);
                });
            } else {
                let cur = obj.openCursor();
                let values = [];

                return new Promise((resolve, reject) => {
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
                });
            }
        };

        this.delete = function(key) {
            if (!intf.db)
                return restartWhenReady(() => this.delete(key));

            let t = intf.db.transaction([store_name], 'readwrite');
            let obj = t.objectStore(store_name);
            let req = obj.delete(key);

            return new Promise((resolve, reject) => {
                req.onsuccess = e => resolve();
                req.onerror = e => reject(e.target.error);
            });
        };
    }

    function DatabaseInterface(intf) {
        this.openStore = function(store_name) {
            return new ObjectStoreWrapper(intf, store_name);
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
