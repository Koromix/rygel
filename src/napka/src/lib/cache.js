// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import path from 'path';
import sqlite3 from 'better-sqlite3';

const CACHE = path.resolve('.', process.env.CACHE || 'data/cache.db');
const SCHEMA = 1;

function open(options = {}) {
    let db = sqlite3(CACHE, options);

    db.pragma('journal_mode = WAL');

    let version = db.pragma('user_version', { simple: true });

    if (version != SCHEMA) {
        if (version > SCHEMA)
            throw new Error('Database schema is too recent');

        migrate(db, version);
    }

    return db;
}

function migrate(db, version) {
    console.log(`Migrating cache from ${version} to ${SCHEMA}`);

    db.transaction(() => {
        switch (version) {
            case 0: {
                db.exec(`
                   CREATE TABLE tiles (
                        url TEXT NOT NULL PRIMARY KEY,
                        data BLOB NOT NULL,
                        type TEXT NOT NULL
                    );
                `);
            } // fallthrough
        }

        db.pragma('user_version = ' + SCHEMA);
    })();
}

export { open }
