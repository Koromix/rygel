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

const sqlite3 = new function() {
    let self = this;

    let promiser = null;
    let id = 0;

    this.init = async function(worker) {
        if (promiser != null)
            return;

        promiser = await new Promise((resolve, reject) => {
            let sqlite3 = null;

            if (typeof worker === 'string') {
                worker = new Worker(worker);
            } else if (typeof worker === 'function') {
                worker = worker();
            } else if (!(worker instanceof Worker)) {
                reject(new Error('Missing URL for SQLite3 worker script'));
            }
            worker.onerror = e => reject(new Error('Failed to load SQLite3 worker script'));

            let config = {
                worker: worker,
                onready: () => resolve(sqlite3)
            };

            sqlite3 = sqlite3Worker1Promiser(config);
        });
    }

    this.open = async function(filename) {
        if (promiser == null)
            throw new Error('Call sqlite3.init() first');

        if (filename == null)
            filename = ':memory:';
        if (filename != ':memory:') {
            if (!self.hasPersistence())
                throw new Error('This browser does not support persistent SQLite3 databases');

            filename = `file:${filename}?vfs=opfs`;
        }

        try {
            await promiser({ type: 'open', dbId: ++id, args: { filename: filename }} );
        } catch (msg) {
            throw msg.result;
        }

        let db = new DatabaseWrapper(promiser, id);
        return db;
    };

    this.hasPersistence = function() {
        if (!window.crossOriginIsolated)
            return false;
        if (!window.SharedArrayBuffer)
            return false;
        if (!window.Atomics)
            return false;
        if (!window.FileSystemHandle)
            return false;
        if (!window.FileSystemDirectoryHandle)
            return false;
        if (!window.FileSystemFileHandle)
            return false;
        // if (!window.FileSystemFileHandle.prototype.createSyncAccessHandle)
        //    return false;
        if (!navigator.storage.getDirectory)
            return false;

        return true;
    };

    function DatabaseWrapper(promiser, db) {
        let self = this;

        this.exec = async function(sql, ...args) {
            try {
                await promiser({ type: 'exec', dbId: db, args: {
                    sql: sql,
                    bind: args
                }});
            } catch (msg) {
                throw msg.result;
            }
        };

        this.transaction = async function(func) {
            try {
                await self.exec('BEGIN');
                await func(self);
                await self.exec('COMMIT');
            } catch (err) {
                await self.exec('ROLLBACK');
                throw err;
            }
        };

        this.fetch1 = async function(sql, ...args) {
            let result = await new Promise((resolve, reject) => {
                let p = promiser({ type: 'exec', dbId: db, args: {
                    sql: sql,
                    bind: args,
                    rowMode: 'object',
                    callback: msg => {
                        if (msg.rowNumber != null) {
                            resolve(msg.row);
                        } else {
                            resolve(null);
                        }

                        return false;
                    }
                }});

                p.catch(msg => reject(msg.result));
            });

            return result;
        };

        this.fetchAll = async function(sql, ...args) {
            let results = await new Promise((resolve, reject) => {
                let results = [];

                let p = promiser({ type: 'exec', dbId: db, args: {
                    sql: sql,
                    bind: args,
                    rowMode: 'object',
                    callback: msg => {
                        if (msg.rowNumber == null) {
                            resolve(results);
                            return false;
                        }

                        results.push(msg.row);
                        return true;
                    }
                }});

                p.catch(msg => reject(msg.result));
            });

            return results;
        };

        this.close = async function() {
            try {
                await promiser('close', { dbId: db });
            } catch (msg) {
                throw msg.result;
            }
        };
    }
};
