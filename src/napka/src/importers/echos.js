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
import { Util } from '../../../web/core/base.js';
import * as parse from '../lib/parse.js';
import * as imp from '../lib/import.js';

const ECHOS_XLSX_URL = process.argv[2] || process.env.ECHOS_XLSX_URL || '';
const GOOGLE_API_KEY = process.env.GOOGLE_API_KEY || '';

const LIST_KEYS = [];

const IML_MAPPINGS = {};
const SNCF_MAPPINGS = {};

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

        if (!ECHOS_XLSX_URL)
            errors.push('Missing ECHOS_XLSX_URL or argument path to XLSX file');
        if (!GOOGLE_API_KEY)
            errors.push('Missing GOOGLE_API_KEY');

        if (errors.length)
            throw new Error(errors.join('\n'));
    }

    let db = database.open();

    // Create map and layers if needed
    db.transaction(() => {
        let map_id = db.prepare(`INSERT INTO maps (name, title, mail, style_id)
                                     VALUES ('echos', 'ECHoS', 'niels.martignene@protonmail.com', 'clpaaj4xk004p01pa84497gvr')
                                     ON CONFLICT DO UPDATE SET title = excluded.title,
                                                               mail = excluded.mail,
                                                               style_id = excluded.style_id
                                     RETURNING id`).pluck().get();

        let stmt = db.prepare(`INSERT INTO layers (map_id, name, title, fields) VALUES (?, ?, ?, ?)
                                   ON CONFLICT DO UPDATE SET title = excluded.title,
                                                             fields = excluded.fields`);

        stmt.run(map_id, 'iml', 'IML', JSON.stringify(IML_MAPPINGS));
        stmt.run(map_id, 'sncf', 'SNCF', JSON.stringify(SNCF_MAPPINGS));
    })();

    // Load online spreadsheet file
    let wb;
    if (ECHOS_XLSX_URL.match(/https?:\/\//)) {
        let response = await fetch(ECHOS_XLSX_URL);

        if (!response.ok) {
            let text = (await response.text()).trim();
            throw new Error(text);
        }

        let blob = await response.blob();
        let buffer = Buffer.from(await blob.arrayBuffer());

        wb = xlsx.parse(buffer);
    } else {
        let buffer = fs.readFileSync(ECHOS_XLSX_URL);

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

                if (value == null || value == 'NR') {
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

            if (row.Code == null)
                break;

            table.push(row);
        }

        tables[ws.name] = table;
    }

    let iml = tables['IML'].map(transformIML);
    let sncf = tables['SNCF'].map(transformSNCF);

    // Filter out bad or non-useful data
    iml = iml.filter(row => checkCP(row.CP) && row.Commune != null);
    sncf = sncf.filter(row => row.latitude != null && row.kind == 'Suicide' && row.deces);

    db.transaction(() => {
        imp.updateEntries(db, 'echos', 'iml', iml);
        imp.updateEntries(db, 'echos', 'sncf', sncf);
    })();
    await imp.geomapMissing(db, 'echos', GOOGLE_API_KEY);

    db.close();
}

function transformIML(row) {
    let entry = {
        import: '' + row.Code,
        version: null,
        hide: 0,

        name: row.Hotspot || row.Denomination || row.Type || row.IML,
        address: makeAddress(row.CP, row.Commune),

        lieu: row.Hotspot || row.Denomination || row.Type,
        hotspot: !!row.Hotspot
    };

    entry.version = crypto.createHash('sha256').update(JSON.stringify(entry)).digest('hex');

    return entry;
}

function transformSNCF(row) {
    let location = row.Localisation?.match(/(\-?[0-9]+\.[0-9]+) +(\-?[0-9]+\.[0-9]+)/);

    if (location?.length == 3) {
        location = [parseFloat(location[1]), parseFloat(location[2])];
    } else {
        location = null;
    }

    let entry = {
        import: '' + row.Code,
        version: null,
        hide: 0,

        name: row.Ville,
        address: row.Ville,
        latitude: location?.[1],
        longitude: location?.[0],

        kind: row.Presomption,
        deces: row.Deces
    };

    entry.version = crypto.createHash('sha256').update(JSON.stringify(entry)).digest('hex');

    return entry;
}

function checkCP(cp) {
    if (cp == null)
        return null;

    if (typeof cp == 'number')
        cp = String(cp);
    return cp.length >= 4;
}

function makeAddress(cp, commune) {
    if (cp == null)
        return null;

    if (typeof cp == 'number')
        cp = String(cp);
    cp = cp.padStart(5, '0');

    let address = cp + ' ' + commune;
    return address;
}
