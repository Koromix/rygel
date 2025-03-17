// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { Util, Log, Net } from '../../../web/core/base.js';
import { Hex } from '../../../web/core/mixer.js';
import * as sqlite3 from '../../../web/core/sqlite3.js';

const DATA_LOCK = 'data';

const DATABASE_VERSION = 6;

let uploads = 0;
let generations = new Map;

let worker = null;
let next_message = 0;
let msg_handlers = new Map;

function initSync() {
    let url = BUNDLES['worker.js'];
    worker = new Worker(url, { type: 'module' });

    worker.onmessage = handleMessage;
}

function isSyncing() {
    let syncing = (uploads > 0);
    return syncing;
}

async function downloadVault(vid) {
    let generation = await callWorker('download', vid);

    generations.set(vid, generation);
}

async function uploadVault(vid) {
    try {
        uploads++;

        let prev = generations.get(vid) ?? 0;
        let generation = await callWorker('upload', vid, prev);

        generations.set(generation);
    } finally {
        uploads--;
    }
}

async function openVault(vid, key) {
    let filename = 'ludivine/' + vid + '.db';

    let url = BUNDLES['sqlite3-worker1-bundler-friendly.mjs'];
    await sqlite3.init(url);

    let db = await sqlite3.open(filename, {
        vfs: 'multipleciphers-opfs',
        lock: DATA_LOCK
    });

    db.changeHandler = () => uploadVault(vid);

    let sql = `
        PRAGMA cipher = 'sqlcipher';
        PRAGMA key = "x'${Hex.toHex(key)}'";
    `;
    await db.exec(sql);

    let version = await db.pluck('PRAGMA user_version');

    if (version == DATABASE_VERSION)
        return db;
    if (version > DATABASE_VERSION)
        throw new Error('Database model is too recent');

    await db.transaction(async t => {
        switch (version) {
            case 0: {
                await t.exec(`
                    CREATE TABLE meta (
                        gender TEXT,
                        picture TEXT,
                        avatar TEXT
                    );

                    CREATE TABLE studies (
                        id INTEGER PRIMARY KEY,
                        key TEXT NOT NULL,
                        start INTEGER NOT NULL
                    );
                    CREATE UNIQUE INDEX studies_k ON studies (key);

                    CREATE TABLE tests (
                        id INTEGER PRIMARY KEY,
                        study INTEGER REFERENCES studies (id) ON DELETE CASCADE,
                        key TEXT NOT NULL,
                        title TEXT NOT NULL,
                        visible INTEGER CHECK (visible IN (0, 1)) NOT NULL,
                        status TEXT CHECK (status IN ('empty', 'draft', 'done')) NOT NULL,
                        schedule TEXT,
                        mtime INTEGER,
                        payload BLOB
                    );
                    CREATE UNIQUE INDEX tests_sk ON tests (study, key);

                    CREATE TABLE events (
                        test INTEGER NOT NULL REFERENCES tests (id) ON DELETE CASCADE,
                        sequence INTEGER NOT NULL,
                        timestamp INTEGER NOT NULL,
                        type TEXT NOT NULL,
                        data TEXT
                    );

                    CREATE TABLE snapshots (
                        test INTEGER NOT NULL REFERENCES tests (id) ON DELETE CASCADE,
                        timestamp NOT NULL,
                        image BLOB NOT NULL
                    );
                `);
            } // fallthrough

            case 1: {
                await t.exec(`
                    DROP TABLE events;
                    DROP TABLE snapshots;

                    PRAGMA foreign_keys = 0;

                    DROP INDEX studies_k;
                    ALTER TABLE studies RENAME TO studies_BAK;

                    CREATE TABLE studies (
                        id INTEGER PRIMARY KEY,
                        key TEXT NOT NULL,
                        start INTEGER NOT NULL,
                        reuse INTEGER CHECK (reuse IN (0, 1)) NOT NULL
                    );
                    CREATE UNIQUE INDEX studies_k ON studies (key);

                    INSERT INTO studies (id, key, start, reuse)
                        SELECT id, key, start, 0 FROM studies_BAK;

                    DROP TABLE studies_BAK;

                    PRAGMA foreign_keys = 1;
                `);
            } // fallthrough

            case 2: {
                await t.exec(`
                    CREATE TABLE diary (
                        id INTEGER PRIMARY KEY,
                        date TEXT NOT NULL,
                        title TEXT,
                        content TEXT NOT NULL
                    );
                `);
            } // fallthrough

            case 3: {
                await t.exec(`
                    ALTER TABLE tests ADD COLUMN notify TEXT;
                `);
            } // fallthrough

            case 4: {
                let tests = await t.fetchAll('SELECT id, payload FROM tests WHERE payload IS NOT NULL');

                for (let test of tests) {
                    let compressed = await deflate(test.payload);
                    await t.exec('UPDATE tests SET payload = ? WHERE id = ?', compressed, test.id);
                }
            } // fallthrough

            case 5: {
                await t.exec(`
                    DROP INDEX studies_k;
                    DROP INDEX tests_sk;

                    ALTER TABLE studies RENAME TO studies_BAK;
                    ALTER TABLE tests RENAME TO tests_BAK;

                    CREATE TABLE studies (
                        id INTEGER PRIMARY KEY,
                        key TEXT NOT NULL,
                        start INTEGER NOT NULL,
                        data TEXT NOT NULL
                    );
                    CREATE UNIQUE INDEX studies_k ON studies (key);

                    CREATE TABLE tests (
                        id INTEGER PRIMARY KEY,
                        study INTEGER REFERENCES studies (id) ON DELETE CASCADE,
                        key TEXT NOT NULL,
                        title TEXT NOT NULL,
                        visible INTEGER CHECK (visible IN (0, 1)) NOT NULL,
                        status TEXT CHECK (status IN ('empty', 'draft', 'done')) NOT NULL,
                        schedule TEXT,
                        mtime INTEGER,
                        payload BLOB,
                        notify TEXT
                    );
                    CREATE UNIQUE INDEX tests_sk ON tests (study, key);

                    INSERT INTO studies (id, key, start, data)
                        SELECT id, key, start, json_object('consentement', 1, 'anciennete', 0, 'reutilisation', reuse) FROM studies_BAK;
                    INSERT INTO tests (id, study, key, title, visible, status, schedule, mtime, payload, notify)
                        SELECT id, study, key, title, visible, status, schedule, mtime, payload, notify FROM tests_BAK;

                    DROP TABLE studies_BAK;
                    DROP TABLE tests_BAK;
                `);
            } // fallthrough
        }

        await t.exec('PRAGMA user_version = ' + DATABASE_VERSION);
    });

    return db;
}

async function callWorker(type, ...args) {
    let p = new Promise((resolve, reject) => {
        let id = ++next_message;

        msg_handlers.set(id, {
            resolve: resolve,
            reject: reject
        });

        let msg = {
            id: id,
            type: type,
            args: args
        };

        worker.postMessage(msg);
    });

    await p;
}

function handleMessage(e) {
    let msg = e.data;
    let handler = msg_handlers.get(msg.id);

    switch (msg.type) {
        case 'success': { handler.resolve(msg.value); } break;
        case 'error': { handler.reject(msg.value); } break;
    }
}

export {
    initSync,

    isSyncing,

    downloadVault,
    uploadVault,
    openVault
}
