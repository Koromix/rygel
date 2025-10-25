#!/usr/bin/env node
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const koffi = require('../../koffi');
const crypto = require('crypto');
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { cnoke } = require('./package.json');

const sqlite3 = koffi.opaque('sqlite3');
const sqlite3_stmt = koffi.opaque('sqlite3_stmt');

main();

async function main() {
    try {
        await test();
        console.log('Success!');
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function test() {
    let lib_filename = path.join(__dirname, cnoke.output, 'sqlite3' + koffi.extension);
    let lib = koffi.load(lib_filename);

    const sqlite3_open_v2 = lib.func('sqlite3_open_v2', 'int', ['str', koffi.out(koffi.pointer(sqlite3, 2)), 'int', 'str']);
    const sqlite3_exec = lib.func('sqlite3_exec', 'int', [koffi.pointer(sqlite3), 'str', 'void *', 'void *', 'void *']);
    const sqlite3_prepare_v2 = lib.func('sqlite3_prepare_v2', 'int', [koffi.pointer(sqlite3), 'str', 'int', koffi.out(koffi.pointer(sqlite3_stmt, 2)), 'string']);
    const sqlite3_reset = lib.func('sqlite3_reset', 'int', [koffi.pointer(sqlite3_stmt)]);
    const sqlite3_bind_text = lib.func('sqlite3_bind_text', 'int', [koffi.pointer(sqlite3_stmt), 'int', 'str', 'int', 'void *']);
    const sqlite3_bind_int = lib.func('sqlite3_bind_int', 'int', [koffi.pointer(sqlite3_stmt), 'int', 'int']);
    const sqlite3_column_text = lib.func('sqlite3_column_text', 'str', [koffi.pointer(sqlite3_stmt), 'int']);
    const sqlite3_column_int = lib.func('sqlite3_column_int', 'int', [koffi.pointer(sqlite3_stmt), 'int']);
    const sqlite3_step = lib.func('sqlite3_step', 'int', [koffi.pointer(sqlite3_stmt)]);
    const sqlite3_finalize = lib.func('sqlite3_finalize', 'int', [koffi.pointer(sqlite3_stmt)]);
    const sqlite3_close_v2 = lib.func('sqlite3_close_v2', 'int', [koffi.pointer(sqlite3)]);

    const SQLITE_OPEN_READWRITE = 0x2;
    const SQLITE_OPEN_CREATE = 0x4;
    const SQLITE_ROW = 100;
    const SQLITE_DONE = 101;

    const SQLITE_STATIC = 0;
    const SQLITE_TRANSIENT = -1;

    let filename = await create_temporary_file(path.join(os.tmpdir(), 'test_sqlite'));
    let db = null;

    let expected = Array.from(Array(200).keys()).map(i => [`TXT ${i}`, i % 7, `abc ${i % 7}`]);

    try {
        let ptr = [null];

        // Open database
        assert.equal(sqlite3_open_v2(filename, ptr, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, null), 0);
        db = ptr[0];

        assert.equal(sqlite3_exec(db, 'CREATE TABLE foo (id INTEGER PRIMARY KEY, bar TEXT, value INT, bar2 TEXT);', null, null, null), 0);

        let stmt = null;

        assert.equal(sqlite3_prepare_v2(db, "INSERT INTO foo (bar, value, bar2) VALUES (?1, ?2, ?3)", -1, ptr, null), 0);
        stmt = ptr[0];

        for (let it of expected) {
            sqlite3_reset(stmt);

            sqlite3_bind_text(stmt, 1, it[0], -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 2, it[1]);
            sqlite3_bind_text(stmt, 3, it[2], -1, SQLITE_TRANSIENT);

            assert.equal(sqlite3_step(stmt), SQLITE_DONE);
        }

        assert.equal(sqlite3_finalize(stmt), 0);

        assert.equal(sqlite3_prepare_v2(db, "SELECT id, bar, value, bar2 FROM foo ORDER BY id", -1, ptr, null), 0);
        stmt = ptr[0];

        for (let i = 0; i < expected.length; i++) {
            let it = expected[i];

            assert.equal(sqlite3_step(stmt), SQLITE_ROW);

            assert.equal(sqlite3_column_int(stmt, 0), i + 1);
            assert.equal(sqlite3_column_text(stmt, 1), it[0]);
            assert.equal(sqlite3_column_int(stmt, 2), it[1]);
            assert.equal(sqlite3_column_text(stmt, 3), it[2]);
        }
        assert.equal(sqlite3_step(stmt), SQLITE_DONE);

        let stmt_raw = koffi.address(stmt);

        assert.equal(typeof stmt_raw, 'bigint');
        assert.equal(sqlite3_finalize(stmt_raw), 0);
    } finally {
        sqlite3_close_v2(db);
        fs.unlinkSync(filename);
    }
}

async function create_temporary_file(prefix) {
    let buf = Buffer.allocUnsafe(4);

    for (;;) {
        try {
            crypto.randomFillSync(buf);

            let suffix = buf.toString('hex').padStart(8, '0');
            let filename = `${prefix}.${suffix}`;

            let file = await new Promise((resolve, reject) => {
                let file = fs.createWriteStream(filename, { flags: 'wx', mode: 0o644 });

                file.on('open', () => resolve(file));
                file.on('error', reject);
            });
            file.close();

            return filename;
        } catch (err) {
            if (err.code != 'EEXIST')
                throw err;
        }
    }
}
