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

import crypto from 'crypto';
import fs from 'fs';
import fetch from 'node-fetch';
import xlsx from 'node-xlsx';
import sqlite3 from 'better-sqlite3';
import * as database from '../lib/database.js';
import { Util } from '../../../web/core/common.js';
import * as parse from '../lib/parse.js';
import * as imp from '../lib/import.js';
import secteurs from './secteurs.json';

const PROVENANCE_XLSX_URL = process.env.PROVENANCE_XLSX_URL || '';
const GOOGLE_API_KEY = process.env.GOOGLE_API_KEY || '';

const LIST_KEYS = [];

const DEMHETER_MAPPINGS = {};

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

        if (!PROVENANCE_XLSX_URL)
            errors.push('Missing PROVENANCE_XLSX_URL (public link to download XLSX file)');
        if (!GOOGLE_API_KEY)
            errors.push('Missing GOOGLE_API_KEY');

        if (errors.length)
            throw new Error(errors.join('\n'));
    }

    let db = database.open();

    // Create map and layers if needed
    db.transaction(() => {
        let map_id = db.prepare(`INSERT INTO maps (name, title, mail)
                                     VALUES ('provenance', 'Adressages', 'niels.martignene@protonmail.com')
                                     ON CONFLICT DO UPDATE SET title = excluded.title,
                                                               mail = excluded.mail
                                     RETURNING id`).pluck().get();

        let stmt = db.prepare(`INSERT INTO layers (map_id, name, title, fields) VALUES (?, ?, ?, ?)
                                   ON CONFLICT DO UPDATE SET title = excluded.title,
                                                             fields = excluded.fields`);

        stmt.run(map_id, 'demheter', 'DEMHETER', JSON.stringify(DEMHETER_MAPPINGS));
    })();

    // Load online spreadsheet file
    let wb;
    {
        let response = await fetch(PROVENANCE_XLSX_URL);

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

                if (value == null || !value || value == '?') {
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

            if (row.ID == null)
                break;

            if (row.Origine != null)
                table.push(row);
        }

        tables[ws.name] = table;
    }

    let demheter = tables['DEMHETER'].map(transformDemheter);

    db.transaction(() => {
        imp.updateEntries(db, 'provenance', 'demheter', demheter);
    })();
    await imp.geomapMissing(db, 'provenance', GOOGLE_API_KEY);

    db.close();
}

function transformDemheter(row) {
    let secteur = secteurs[row.Origine];

    let entry = {
        import: '' + row.ID,
        version: null,
        hide: 0,

        name: row.Origine + ' (' + secteur.map(it => it.name).join(', ') + ')',
        address: secteur[0].address,

        profession: row.Fonction
    };

    if (!(['Psychiatre', 'Psychiatre libéral', 'Médecin généraliste']).includes(entry.profession))
        entry.profession = 'Autre';

    entry.version = crypto.createHash('sha256').update(JSON.stringify(entry)).digest('hex');

    return entry;
}
