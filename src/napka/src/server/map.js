// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

import * as imp from '../lib/import.js';

function fetchMap(db, layers) {
    let json = {};

    for (let layer of layers)
        json[layer.name] = fetchLayer(db, layer.id, layer.fields);

    return json;
}

function fetchLayer(db, layer_id, fields) {
    let rows = db.prepare('SELECT * FROM entries WHERE layer_id = ? AND hide = 0').all(layer_id);

    // Transform special values
    for (let row of rows) {
        let main = JSON.parse(row.main);

        row.address = {
            address: row.address,
            latitude: row.latitude,
            longitude: row.longitude
        };
        delete row.latitude;
        delete row.longitude;

        delete row.main;
        delete row.extra;

        Object.assign(row, main);

        for (let key in fields) {
            let type = fields[key];

            if (Array.isArray(type)) {
                // Nothing to do
            } else if (type == 'boolean') {
                row[key] = row[key] && (row[key] != '0');
            } else if (type == 'telephone' && row[key]) {
                row[key] = row[key].match(/(.{1,2})/g).join('.');
            }
        }
    }

    return {
        fields: fields,
        rows: rows
    };
}

async function geocode(db, req, res, access_token) {
    if (req.body == null || typeof req.body != 'object' || typeof req.body.address != 'string') {
        res.status(422).send('Malformed input');
        return;
    }

    let results = await imp.geomapAddress(req.body.address, access_token);
    res.json(results);
}

async function updateEntry(db, req, res) {
    if (!req.session.userid) {
        res.status(403).send('Unauthorized');
        return;
    }
    if (req.body == null || typeof req.body != 'object' || typeof req.body.id != 'number') {
        res.status(422).send('Malformed input');
        return;
    }

    const COLUMNS = {
        'id': false,
        'layer_id': false,
        'import': false,
        'version': false,
        'hide': false,
        'name': true,
        'address': true,
        'main': false,
        'extra': false
    };

    let db_changes = {};
    let main_changes = {};

    for (let key in req.body) {
        if (key == 'id')
            continue;

        let col = COLUMNS[key];

        if (col === false) {
            res.status(422).send('Forbidden column name');
            return;
        } else if (col === true) {
            if (key == 'address') {
                db_changes.address = req.body.address.address;
                db_changes.latitude = req.body.address.latitude;
                db_changes.longitude = req.body.address.longitude;
            } else {
                db_changes[key] = req.body[key];
            }
        } else {
            main_changes[key] = req.body[key];
        }
    }

    db.transaction(() => {
        for (let key in db_changes)
            db.prepare(`UPDATE entries SET ${key} = ? WHERE id = ?`).run(db_changes[key], req.body.id);
        db.prepare(`UPDATE entries SET main = json_patch(main, ?) WHERE id = ?`).run(JSON.stringify(main_changes), req.body.id);
    })();

    res.json(null);
}

async function deleteEntry(db, req, res) {
    if (!req.session.userid) {
        res.status(403).send('Unauthorized');
        return;
    }
    if (req.body == null || typeof req.body != 'object' || typeof req.body.id != 'number') {
        res.status(422).send('Malformed input');
        return;
    }

    db.prepare(`UPDATE entries SET hide = 1 WHERE id = ?`).run(req.body.id);

    res.json(null);
}

export {
    fetchMap,
    fetchLayer,
    geocode,
    updateEntry,
    deleteEntry
}
