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

async function sqlite3(filename) {
    if (sqlite3.promiser == null) {
        self.id = 0;

        sqlite3.promiser = await new Promise((resolve, reject) => {
            let sqlite3 = null;

            let config = {
                onready: () => resolve(sqlite3),
                onerror: err => reject(err)
            };

            sqlite3 = sqlite3Worker1Promiser(config);
        });
    }

    let promiser = sqlite3.promiser;
    let id = ++sqlite3.id;

    if (filename != null && filename != ':memory:')
        filename = `file:${filename}?vfs=opfs`;

    try {
        await sqlite3.promiser({ type: 'open', dbId: id, args: { filename: filename }} );
    } catch (msg) {
        throw msg.result;
    }

    let db = new SQLiteDatabase(sqlite3.promiser, id);
    return db;
}

function SQLiteDatabase(promiser, db) {
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
