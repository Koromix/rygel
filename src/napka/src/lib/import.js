// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import fetch from 'node-fetch';
import querystring from 'querystring';
import { Util } from '../../../core/web/base/base.js';

function FileInfo(filename = null, buffer = null) {
    this.filename = filename;
    this.buffer = buffer;
}

async function geomapMissing(db, map_name, api_key) {
    let rows = db.prepare(`SELECT e.id, e.address FROM entries e
                               INNER JOIN layers l ON (l.id = e.layer_id)
                               INNER JOIN maps m ON (m.id = l.map_id)
                               WHERE m.name = ? AND e.invalid_address = 0
                                                AND e.latitude IS NULL`).all(map_name);

    for (let row of rows) {
        let results = await geomapAddress(row.address, api_key);

        if (results.length) {
            let ret = results[0];
            db.prepare('UPDATE entries SET latitude = ?, longitude = ? WHERE id = ?').run(ret.latitude, ret.longitude, row.id);
        } else {
            db.prepare('UPDATE entries SET invalid_address = 1 WHERE id = ?').run(row.id);
        }
    }
}

async function geomapAddress(address, api_key) {
    let url = 'https://maps.googleapis.com/maps/api/geocode/json';

    url += '?' + new URLSearchParams({
        key: api_key,
        address: address,
        language: 'fr'
    });

    let json = null;
    for (let i = 0; i < 3; i++) {
        let response = await fetch(url);

        if (response.ok) {
            json = await response.json();
            break;
        }

        await Util.waitFor(i * 500);
    }
    if (json == null) {
        console.error(`Failed to geocode address '${address}'`);
        return [];
    }

    if (!json.results.length) {
        console.error(`No match for '${address}'`);
        return [];
    }

    let results = json.results.map(result => ({
        address: result.formatted_address,
        latitude: result.geometry.location.lat,
        longitude: result.geometry.location.lng
    }));

    return results;
}

function updateEntries(db, map_name, layer_name, rows, links = {}) {
    let layer = db.prepare(`SELECT l.id FROM layers l
                                INNER JOIN maps m ON (m.id = l.map_id)
                                WHERE m.name = ? AND l.name = ?`).get(map_name, layer_name);

    if (layer == null)
        throw new Error(`Unknown map or layer '${map_name}/${layer_name}'`);

    for (let row of rows) {
        let entry = {
            layer_id: layer.id,

            import: row.import,
            version: row.version,
            hide: row.hide || 0,

            name: row.name,
            address: row.address,
            invalid_address: 0,
            latitude: row.latitude,
            longitude: row.longitude,

            main: {},
            extra: {}
        };

        for (let key in row) {
            if (Object.hasOwn(entry, key))
                continue;

            let ptr = entry.main;
            let link = links[key];
            let value = row[key];

            if (link != null) {
                if (entry.extra[link] == null)
                    entry.extra[link] = [];

                ptr = entry.extra;
                key = link;
            }

            if (value instanceof FileInfo) {
                // keys.push(key + '_name');
                // keys.push(key + '_file');
                // values.push(value.filename);
                // values.push(value.buffer);
            } else if (value == null) {
                ptr[key] = null;
            } else {
                ptr[key] = value;
            }
        }

        entry.main = JSON.stringify(entry.main);
        entry.extra = JSON.stringify(entry.extra);

        db.prepare(`DELETE FROM entries
                        WHERE layer_id = ? AND import = ? AND version <> ?;`).run(layer.id, row.import, row.version);
        db.prepare(`INSERT INTO entries (${Object.keys(entry).join(',')}) VALUES (${Object.keys(entry).fill('?').join(',')})
                        ON CONFLICT DO NOTHING;`).run(...Object.values(entry));
    }
}

export {
    FileInfo,
    geomapMissing,
    geomapAddress,
    updateEntries
}
