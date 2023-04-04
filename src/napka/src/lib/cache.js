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

const path = require('path');
const sqlite3 = require('better-sqlite3');

const CACHE = path.resolve(__dirname + '/../..', process.env.CACHE || 'data/cache.db');
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

module.exports = {
    open: open
};
