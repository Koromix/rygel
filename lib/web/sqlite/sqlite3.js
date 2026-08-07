// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import sqlite3Worker1Promiser from 'vendor/sqlite3mc/wasm/jswasm/sqlite3-worker1-promiser.mjs';

let worker_init = null;

let prev_worker = null;
let prev_promiser = null;
let shutdown_timer = null;

let next_id = 0;

async function init(init) {
    if (typeof init == 'function') {
        worker_init = init;
    } else {
        worker_init = () => new Worker(init, { type: 'module' });
    }
}

async function create(lock = null) {
    if (worker_init == null)
        throw new Error('Call init() first');

    if (lock == null)
        lock = 'sqlite3:' + crypto.randomUUID();

    let worker = prev_worker;
    let promiser = prev_promiser;

    if (promiser == null) {
        worker = await worker_init();
        promiser = await sqlite3Worker1Promiser({ worker: worker });
    }

    prev_worker = null;
    prev_promiser = null;
    clearTimeout(shutdown_timer);

    let id = ++next_id;
    let db = new DatabaseWrapper(worker, promiser, id, lock);

    return db;
}

function hasOPFS() {
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

    let is_safari = !!navigator.userAgent.match(/Version\/[\d\.]+.*Safari/);
    let is_safari17 = is_safari && CSS.supports('contain-intrinsic-size', '100px');

    if (is_safari && !is_safari17) {
        console.warn('Refusing to use OPFS in Safari < 17');
        return false;
    }

    return true;
}

function DatabaseWrapper(worker, promiser, id, lock) {
    let self = this;

    let change_handler = () => {};

    let is_open = false;
    let has_changed = false;

    Object.defineProperties(this, {
        changeHandler: { get: () => change_handler, set: handler => { change_handler = handler; }, enumerable: true }
    });

    this.open = locked(async function(filename, vfs = 'opfs') {
        if (is_open !== false)
            throw new Error('Cannot open database multiple times');

        if (filename == null)
            filename = ':memory:';
        if (filename != ':memory:')
            filename = `file:${filename}?vfs=${vfs}`;

        try {
            await promiser({ type: 'open', dbId: id, args: { filename: filename }} );
        } catch (msg) {
            throw new Error(msg.result?.message ?? msg.result);
        }

        is_open = true;
    });

    this.transaction = locked(async function(func) {
        let prev_changed = has_changed;

        try {
            let t = new TransactionWrapper(promiser, id);

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
        let t = new TransactionWrapper(promiser, id);
        let ret = await t.exec(sql, ...args);

        await handleChanges(t);

        return ret;
    });

    this.fetch1 = locked(async function(sql, ...args) {
        let t = new TransactionWrapper(promiser, id);
        let ret = await t.fetch1(sql, ...args);

        await handleChanges(t);

        return ret;
    });

    this.fetchAll = locked(async function(sql, ...args) {
        let t = new TransactionWrapper(promiser, id);
        let ret = await t.fetchAll(sql, ...args);

        await handleChanges(t);

        return ret;
    });

    this.pluck = locked(async function(sql, ...args) {
        let t = new TransactionWrapper(promiser, id);
        let ret = await t.pluck(sql, ...args);

        await handleChanges(t);

        return ret;
    });

    this.close = locked(async function() {
        try {
            await promiser({ type: 'close', dbId: id });

            if (prev_worker != null)
                prev_worker.terminate();

            prev_worker = worker;
            prev_promiser = promiser;

            clearTimeout(shutdown_timer);

            shutdown_timer = setTimeout(() => {
                prev_worker = null;
                prev_promiser = null;

                worker.terminate();
            }, 10000);
        } catch (msg) {
            throw new Error(msg.result?.message ?? msg.result);
        }
    });

    this.send = async function (obj) {
        let ret = await promiser(obj);
        return ret;
    };

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

function TransactionWrapper(promiser, id) {
    let self = this;

    let changes = 0;

    Object.defineProperties(this, {
        changeCount: { get: () => changes, enumerable: true }
    });

    this.exec = async function (sql, ...args) {
        try {
            let ret = await promiser({ type: 'exec', dbId: id, args: {
                sql: sql,
                bind: args,
                countChanges: true
            }});

            handleResult(ret);
        } catch (msg) {
            throw new Error(msg.result?.message ?? msg.result);
        }
    };

    this.fetch1 = async function (sql, ...args) {
        let result = await new Promise((resolve, reject) => {
            let row = null;

            let p = promiser({ type: 'exec', dbId: id, args: {
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

    this.fetchAll = async function (sql, ...args) {
        let results = await new Promise((resolve, reject) => {
            let results = [];

            let p = promiser({ type: 'exec', dbId: id, args: {
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

    this.pluck = async function (sql, ...args) {
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

async function initSabFS(db) {
    try {
        await db.send({ type: 'sabfs.init', args: [] });
    } catch (e) {
        throw e.value;
    }

    return new SabFSWrapper(db);
}

function SabFSWrapper(db) {
    this.write = async function (name, sab) {
        try {
            await db.send({ type: 'sabfs.write', args: [name, sab] });
        } catch (e) {
            throw e.value;
        }
    };

    this.shrink = async function (name) {
        try {
            let ret = await db.send({ type: 'sabfs.shrink', args: [name] });
            return ret.value;
        } catch (e) {
            throw e.value;
        }
    };
};

export {
    init,
    create,

    hasOPFS,

    initSabFS
}
