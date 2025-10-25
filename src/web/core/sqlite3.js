// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import sqlite3Worker1Promiser from '../../../vendor/sqlite3mc/wasm/jswasm/sqlite3-worker1-promiser.mjs';

let promiser = null;
let id = 0;

async function init(worker) {
    if (promiser != null)
        return;

    promiser = await new Promise((resolve, reject) => {
        let sqlite3 = null;

        if (typeof worker === 'string') {
            worker = new Worker(worker, { type: 'module' });
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

async function open(filename, options = {}) {
    if (promiser == null)
        throw new Error('Call init() first');

    options = Object.assign({
        vfs: 'opfs',
        lock: 'sqlite3'
    }, options);

    if (filename == null)
        filename = ':memory:';
    if (filename != ':memory:') {
        if (!hasPersistence())
            throw new Error('This browser does not support persistent SQLite3 databases');

        filename = `file:${filename}?vfs=${options.vfs}`;
    }

    try {
        await promiser({ type: 'open', dbId: ++id, args: { filename: filename }} );
    } catch (msg) {
        throw msg.result;
    }

    let db = new DatabaseWrapper(promiser, id, options.lock);
    return db;
}

function hasPersistence() {
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
}

function DatabaseWrapper(promiser, db, lock) {
    let self = this;

    let change_handler = () => {};

    let has_changed = false;

    Object.defineProperties(this, {
        changeHandler: { get: () => change_handler, set: handler => { change_handler = handler; }, enumerable: true }
    });

    this.transaction = locked(async function(func) {
        let prev_changed = has_changed;

        try {
            let t = new TransactionWrapper(promiser, db);

            await t.exec('BEGIN');
            await func(t);
            await t.exec('COMMIT');

            await handleChanges(t);
        } catch (err) {
            await self.exec('ROLLBACK');
            has_changed = prev_changed;

            throw err;
        }
    });

    this.exec = locked(async function(sql, ...args) {
        let t = new TransactionWrapper(promiser, db);
        let ret = await t.exec(sql, ...args);

        await handleChanges(t);

        return ret;
    });

    this.fetch1 = locked(async function(sql, ...args) {
        let t = new TransactionWrapper(promiser, db);
        let ret = await t.fetch1(sql, ...args);

        await handleChanges(t);

        return ret;
    });

    this.fetchAll = locked(async function(sql, ...args) {
        let t = new TransactionWrapper(promiser, db);
        let ret = await t.fetchAll(sql, ...args);

        await handleChanges(t);

        return ret;
    });

    this.pluck = locked(async function(sql, ...args) {
        let t = new TransactionWrapper(promiser, db);
        let ret = await t.pluck(sql, ...args);

        await handleChanges(t);

        return ret;
    });

    this.close = locked(async function() {
        try {
            await promiser({ type: 'close', dbId: db });
        } catch (msg) {
            throw msg.result;
        }
    });

    function locked(func) {
        return async (...args) => {
            let ret = await navigator.locks.request(lock, () => func(...args));
            return ret;
        };
    }

    async function handleChanges(t) {
        has_changed ||= (t.changeCount > 0);

        if (!has_changed)
            return;

        await change_handler();
        has_changed = false;
    }
}

function TransactionWrapper(promiser, db) {
    let self = this;

    let changes = 0;

    Object.defineProperties(this, {
        changeCount: { get: () => changes, enumerable: true }
    });

    this.exec = async function(sql, ...args) {
        try {
            let ret = await promiser({ type: 'exec', dbId: db, args: {
                sql: sql,
                bind: args,
                countChanges: true
            }});

            handleResult(ret);
        } catch (msg) {
            throw msg.result;
        }
    };

    this.fetch1 = async function(sql, ...args) {
        let result = await new Promise((resolve, reject) => {
            let row = null;

            let p = promiser({ type: 'exec', dbId: db, args: {
                sql: sql,
                bind: args,
                countChanges: true,
                rowMode: 'object',
                callback: msg => {
                    if (msg.rowNumber != null)
                        row = msg.row;

                    return false;
                }
            }});

            p.then(handleResult)
             .then(() => resolve(row))
             .catch(msg => reject(msg.result));
        });

        return result;
    };

    this.fetchAll = async function(sql, ...args) {
        let results = await new Promise((resolve, reject) => {
            let results = [];

            let p = promiser({ type: 'exec', dbId: db, args: {
                sql: sql,
                bind: args,
                countChanges: true,
                rowMode: 'object',
                callback: msg => {
                    if (msg.rowNumber == null)
                        return false;

                    results.push(msg.row);
                    return true;
                }
            }});

            p.then(handleResult)
             .then(() => resolve(results))
             .catch(msg => reject(msg.result));
        });

        return results;
    };

    this.pluck = async function(sql, ...args) {
        let result = await new Promise((resolve, reject) => {
            let value = null;

            let p = promiser({ type: 'exec', dbId: id, args: {
                sql: sql,
                bind: args,
                countChanges: true,
                rowMode: 'array',
                callback: msg => {
                    if (msg.rowNumber != null)
                        value = msg.row[0];

                    return false;
                }
            }});

            p.then(handleResult)
             .then(() => resolve(value))
             .catch(msg => reject(msg.result));
        });

        return result;
    };

    function handleResult(ret) {
        changes += ret.result.changeCount;
    }
}

export {
    init,
    open,
    hasPersistence
}
