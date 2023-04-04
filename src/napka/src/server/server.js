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
const fs = require('fs');
const crypto = require('crypto');
const express = require('express');
const compression = require('compression');
const mime = require('mime');
const esbuild = require('esbuild');
const nunjucks = require('nunjucks');
const fetch = require('node-fetch');

const cache = require('../lib/cache.js');
const database = require('../lib/database.js');
const map = require('./map.js');
const session = require('./session.js');

const PORT = process.env.PORT || 9977;
const SESSION_SECRET = process.env.SESSION_SECRET || '';

const MAPBOX_USERNAME = process.env.MAPBOX_USERNAME || '';
const MAPBOX_STYLE_ID = process.env.MAPBOX_STYLE_ID || '';
const MAPBOX_ACCESS_TOKEN = process.env.MAPBOX_ACCESS_TOKEN || '';
const MAPBOX_TILESIZE = process.env.MAPBOX_TILESIZE || 512;

let FILES = {};

function main() {
    // Check prerequisites
    {
        let errors = [];

        if (!MAPBOX_USERNAME)
            errors.push('Missing MAPBOX_USERNAME');
        if (!MAPBOX_STYLE_ID)
            errors.push('Missing MAPBOX_STYLE_ID');
        if (!MAPBOX_ACCESS_TOKEN)
            errors.push('Missing MAPBOX_ACCESS_TOKEN');
        if (!SESSION_SECRET)
            errors.push('Missing SESSION_SECRET (for server-side sessions)');

        if (errors.length)
            throw new Error(errors.join('\n'));
    }

    let live = false;

    // Parse options
    for (let i = 2; i < process.argv.length; i++) {
        let arg = process.argv[i];
        let value = null;

        if (arg[0] == '-') {
            if (arg.length > 2 && arg[1] != '-') {
                value = arg.substr(2);
                arg = arg.substr(0, 2);
            } else if (arg[1] == '-') {
                let offset = arg.indexOf('=');

                if (offset > 2 && arg.length > offset + 1) {
                    value = arg.substr(offset + 1);
                    arg = arg.substr(0, offset);
                }
            }
            if (value == null && process.argv[i + 1] != null && process.argv[i + 1][0] != '-') {
                value = process.argv[i + 1];
                i++; // Skip this value next iteration
            }
        }

        if (arg == '--help') {
            printUsage();
            return;
        } else if (arg == '--live') {
            live = true;
        } else if (arg[0] == '-') {
            throw new Error(`Unexpected argument '${arg}'`);
        }
    }

    let app = express();

    let db = database.open();
    let cache_db = cache.open();

    app.enable('strict routing');

    // Init Express stuff
    app.use(compression())
    app.use(express.json());
    session.init(app, db, SESSION_SECRET);
    if (live)
        nunjucks.configure(null, { noCache: true });

    let maps = db.prepare('SELECT * FROM maps').all();

    // Static endpoints
    for (let map of maps)
        buildClient(app, map, live);

    // Expose API endpoints for all maps
    for (let it of maps) {
        let layers = db.prepare('SELECT * FROM layers WHERE map_id = ?').all(it.id);

        for (let layer of layers)
            layer.fields = JSON.parse(layer.fields);

        // Public map and layer endppoints
        app.route('/' + it.name + '/api/entries').get((req, res) => {
            let entries = map.fetchMap(db, layers);
            res.json(entries);
        });
        for (let layer of layers) {
            app.route('/' + it.name + '/api/entries/' + layer.name).get((req, res) => {
                let entries = map.fetchLayer(db, layer.id, layer.fields);
                res.json(entries);
            });
        }

        // Session API
        app.route('/' + it.name + '/api/admin/login').post((req, res) => session.login(db, req, res));
        app.route('/' + it.name + '/api/admin/logout').post((req, res) => session.logout(db, req, res));
        app.route('/' + it.name + '/api/admin/profile').get((req, res) => session.profile(db, req, res));

        // Administration API
        app.route('/' + it.name + '/api/admin/geocode').post((req, res) => map.geocode(db, req, res, MAPBOX_ACCESS_TOKEN));
        app.route('/' + it.name + '/api/admin/edit').post((req, res) => map.updateEntry(db, req, res));
        app.route('/' + it.name + '/api/admin/delete').post((req, res) => map.deleteEntry(db, req, res));

        // Tiles API
        app.route('/' + it.name + '/tiles/:z/:x/:y').get((req, res) => relayTile(cache_db, req, res));
    }

    app.listen(PORT, () => {
        console.log(`Server started on port: ${PORT}`);
    });
}

main();

function printUsage() {
    let help = `Usage: node server.js [options...]

Options:
        --live                   Rebuild client code for each load
`;

    console.log(help);
}

function buildClient(app, map, live) {
    let files = buildFiles(map);

    for (let filename in files) {
        let type = mime.getType(filename);

        if (filename == 'map.html') {
            app.route('/' + map.name + '/').get((req, res) => {
                if (live) {
                    try {
                        Object.assign(files, buildFiles(map));
                    } catch (err) {
                        console.error(err);
                    }
                }

                res.set('Content-Type', type);
                res.send(files[filename]);
            });
            app.route('/' + map.name).get((req, res) => res.redirect(301, '/' + map.name + '/'));
        } else {
            app.route('/' + map.name + '/static/' + filename).get((req, res) => {
                res.set('Content-Type', type);
                if (live) {
                    res.set('Cache-Control', 'no-store');
                } else {
                    res.set('Cache-Control', 'public, max-age=31536000');
                }
                res.send(files[filename]);
            });
        }
    }
}

function buildFiles(map) {
    let files = {};
    let timer = null;

    let uuid = crypto.randomUUID();
    let prefix = __dirname + `/../client/${map.name}`;

    files['map.html'] = nunjucks.render(prefix + '/../map.html', {
        title: map.title,
        mail: map.mail,
        buster: uuid,
        env: {
            map: {
                url: 'tiles/{z}/{x}/{y}',
                tilesize: MAPBOX_TILESIZE,
                min_zoom: 2,
                max_zoom: 20,
                attribution: "&copy; <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a> contributors"
            }
        }
    });

    let css = esbuild.buildSync({
        entryPoints: [prefix + `/${map.name}.css`],
        bundle: true,
        minify: true,
        write: false,
        loader: {
            '.png': 'dataurl'
        },
        outfile: 'map.css'
    });
    files['map.css'] = Buffer.from(css.outputFiles[0].contents);

    let js = esbuild.buildSync({
        entryPoints: [prefix + `/${map.name}.js`],
        bundle: true,
        minify: false,
        write: false,
        format: 'iife',
        globalName: 'napka',
        target: [
            'chrome52',
            'edge18',
            'firefox60',
            'safari11'
        ],
        outfile: 'map.js'
    });
    files['map.js'] = Buffer.from(js.outputFiles[0].contents);

    if (fs.existsSync(prefix + '/icons')) {
        for (let basename of fs.readdirSync(prefix + '/icons')) {
            if (basename.endsWith('.png')) {
                let filename = prefix + `/icons/${basename}`;
                files['icons/' + basename] = fs.readFileSync(filename);
            }
        }
    }

    return files;
}

async function relayTile(db, req, res) {
    let template = `https://api.mapbox.com/styles/v1/${MAPBOX_USERNAME}/${MAPBOX_STYLE_ID}/tiles/{tilesize}/{z}/{x}/{y}`;
    let url = parseURL(template, req.params.z, req.params.x, req.params.y);

    // Try cached version first
    {
        let row = db.prepare('SELECT data, type FROM tiles WHERE url = ?').get(url);

        if (row != null) {
            res.set('Content-Type', row.type);
            res.send(row.data);

            return;
        }
    }

    // Now, ask Mapbox
    {
        let url_with_token = url + '?access_token=' + MAPBOX_ACCESS_TOKEN;
        let response = await fetch(url_with_token);

        if (!response.ok) {
            res.status(response.status).send('Failed to fetch tile');
            return;
        }

        let buf = Buffer.from(await response.arrayBuffer());
        let type = response.headers.get('content-type');

        // Cache tile
        db.prepare(`INSERT INTO tiles (url, data, type) VALUES (?, ?, ?)
                    ON CONFLICT DO NOTHING`).run(url, buf, type);

        res.set('Content-Type', type);
        res.send(buf);
    }
}

function parseURL(url, z, x, y) {
    let ret = url.replace(/{[a-z]+}/g, m => {
        switch (m) {
            case '{s}': return 'a';
            case '{z}': return z;
            case '{x}': return x;
            case '{y}': return y;
            case '{r}': return '';
            case '{tilesize}': return MAPBOX_TILESIZE;
            case '{ext}': return 'png';
        }
    });

    return ret;
}
