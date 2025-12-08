// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import sqlite3InitModule from 'vendor/sqlite3mc/wasm/jswasm/sqlite3.js';
import * as SABFS from './sabfs.js';

sqlite3InitModule().then(sqlite3 => {
    sqlite3.initWorker1API();
    SABFS.install(sqlite3);

    let passthrough = onmessage;

    onmessage = function (e) {
        let msg = e.data;

        switch (msg.type) {
            case 'sabfs.write': return wrapAsync(msg.messageId, SABFS.write, msg.args);
            case 'sabfs.shrink': return wrapAsync(msg.messageId, SABFS.shrink, msg.args);

            default: { passthrough(e); } break;
        }
    };
});

async function wrapAsync(id, func, args) {
    try {
        let ret = await func(...args);
        self.postMessage({ messageId: id, type: 'success', value: ret });
    } catch (err) {
        self.postMessage({ messageId: id, type: 'error', value: err });
    }
}
