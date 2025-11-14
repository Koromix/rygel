// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import crypto from 'crypto';
import fs from 'fs';
import fetch from 'node-fetch';
import xlsx from 'node-xlsx';
import sqlite3 from 'better-sqlite3';
import * as database from '../lib/database.js';
import { Util } from 'src/core/web/base/base.js';
import * as parse from '../lib/parse.js';
import * as imp from '../lib/import.js';

const CATATONIA_XLSX_URL = process.env.CATATONIA_XLSX_URL || '';
const GOOGLE_API_KEY = process.env.GOOGLE_API_KEY || '';

const LIST_KEYS = ["Identites", "Mails"];

const CENTER_MAPPINGS = {};
const PSY_MAPPINGS = {
    orientation: 'markdown'
};

main();

async function main() {
    try {
        await run();
        console.log('Success!');
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function run() {
    // Check prerequisites
    {
        let errors = [];

        if (!CATATONIA_XLSX_URL)
            errors.push('Missing CATATONIA_XLSX_URL (public link to download XLSX file)');
        if (!GOOGLE_API_KEY)
            errors.push('Missing GOOGLE_API_KEY');

        if (errors.length)
            throw new Error(errors.join('\n'));
    }

    let db = database.open();

    // Create map and layers if needed
    db.transaction(() => {
        let map_id = db.prepare(`INSERT INTO maps (name, title, mail)
                                     VALUES ('catatonia', 'Catatonia', 'ali.amad@chu-lille.fr')
                                     ON CONFLICT DO UPDATE SET title = excluded.title,
                                                               mail = excluded.mail
                                     RETURNING id`).pluck().get();

        let stmt = db.prepare(`INSERT INTO layers (map_id, name, title, fields) VALUES (?, ?, ?, ?)
                                   ON CONFLICT DO UPDATE SET title = excluded.title,
                                                             fields = excluded.fields`);

        stmt.run(map_id, 'centres', 'Centres', JSON.stringify(CENTER_MAPPINGS));
        stmt.run(map_id, 'psychologues', 'Psychologues', JSON.stringify(PSY_MAPPINGS));
    })();

    // Load online spreadsheet file
    let wb;
    {
        let response = await fetch(CATATONIA_XLSX_URL);

        if (!response.ok) {
            let text = (await response.text()).trim();
            throw new Error(text);
        }

        let blob = await response.blob();
        let buffer = Buffer.from(await blob.arrayBuffer());

        wb = xlsx.parse(buffer);
    }

    // Transform rows to objects
    let tables = {};
    for (let ws of wb) {
        let keys = ws.data[0].map(key => key.trim());

        let table = [];

        for (let i = 1; i < ws.data.length; i++) {
            let row = {};

            for (let j = 0; j < keys.length; j++) {
                let value = ws.data[i][j];

                if (value == null) {
                    row[keys[j]] = null;
                } else if (typeof value === 'string') {
                    value = value.trim();

                    if (LIST_KEYS.includes(keys[j])) {
                        value = value.split('\n');
                        value = value.map(it => it.trim()).filter(it => it);
                    } else if (!value) {
                        value = null;
                    }

                    row[keys[j]] = value;
                } else {
                    row[keys[j]] = value;
                }
            }

            if (row.ID == null || row.Structure == null)
                break;

            if (row.Adresse != null)
                table.push(row);
        }

        tables[ws.name] = table;
    }

    let centres = tables['Centres'].map(transformCenter);

    db.transaction(() => {
        imp.updateEntries(db, 'catatonia', 'centres', centres);
    })();
    await imp.geomapMissing(db, 'catatonia', GOOGLE_API_KEY);

    db.close();
}

function transformCenter(row) {
    let entry = {
        import: '' + row.ID,
        version: null,
        hide: 0,

        name: row.Structure,
        address: row.Adresse,

        ect: String(row.ECT).trim().toUpperCase() == "OUI",

        mail: parse.cleanMail(row.Mail),
        telephone: parse.cleanPhoneNumber(row.Telephone),
        referents: row.Referents.split(/[,;/\n]/).map(it => it.trim()).filter(it => it)
    };

    entry.version = crypto.createHash('sha256').update(JSON.stringify(entry)).digest('hex');

    return entry;
}
