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

const fetch = require('node-fetch');
const querystring = require('querystring');
const { util } = require('../../../web/libjs/common.js');

function FileInfo(filename = null, buffer = null) {
    this.filename = filename;
    this.buffer = buffer;
}

async function geomapMissing(db, map_name, access_token) {
    let rows = db.prepare(`SELECT e.id, e.address FROM entries e
                               INNER JOIN layers l ON (l.id = e.layer_id)
                               INNER JOIN maps m ON (m.id = l.map_id)
                               WHERE m.name = ? AND e.latitude IS NULL`).all(map_name);

    for (let row of rows) {
        let results = await geomapAddress(row.address, access_token);

        if (results.length) {
            let ret = results[0];
            db.prepare('UPDATE entries SET latitude = ?, longitude = ? WHERE id = ?').run(ret.latitude, ret.longitude, row.id);
        }
    }
}

async function geomapAddress(address, access_token) {
    let url = `https://api.mapbox.com/geocoding/v5/mapbox.places/${querystring.escape(address.replace(';', ' '))}.json`;

    url += '?' + new URLSearchParams({
        access_token: access_token,
        fuzzyMatch: true,
        country: 'fr',
        language: 'fr'
    });

    let json = null;
    for (let i = 0; i < 3; i++) {
        let response = await fetch(url);

        if (response.ok) {
            json = await response.json();
            break;
        }

        await util.waitFor(i * 500);
    }
    if (json == null) {
        console.error(`Failed to geocode address '${address}'`);
        return [];
    }

    let features = json.features.slice();

    // Sort by most relevant
    features.sort((feature1, feature2) => feature2.relevance - feature1.relevance);

    if (!features.length || !features[0].relevance) {
        console.error(`No match for '${address}'`);
        return [];
    }

    let results = features.map(feature => ({
        address: feature.place_name,
        latitude: feature.center[1],
        longitude: feature.center[0],
        relevance: feature.relevance
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
            latitude: row.latitude,
            longitude: row.longitude,

            main: {},
            extra: {}
        };

        for (let key in row) {
            if (entry.hasOwnProperty(key))
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

module.exports = {
    FileInfo,
    geomapMissing,
    geomapAddress,
    updateEntries
};
