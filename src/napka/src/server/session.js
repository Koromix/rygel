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

const session = require('express-session');
const SqliteStore = require('better-sqlite3-session-store')(session);
const pwhash = require('../lib/pwhash.js');

function init(app, db, secret) {
    app.use(
        session({
            store: new SqliteStore({
                client: db,
                expired: {
                    clear: true,
                    intervalMs: 900000
                }
            }),
            secret: secret,
            resave: false,
            saveUninitialized: true
        })
    );
}

function login(db, req, res) {
    if (req.body == null || typeof req.body != 'object' || typeof req.body.username != 'string' ||
                                                           typeof req.body.password != 'string') {
        res.status(422).send('Malformed input');
        return;
    }

    let row = db.prepare('SELECT userid, username, password_hash FROM users WHERE username = ?').get(req.body.username);

    if (row == null) {
        res.status(404).send('Username does not exist');
        return;
    }
    if (!pwhash.verify(req.body.password, row.password_hash)) {
        res.status(403).send('Wrong password');
        return;
    }

    req.session.userid = row.userid;

    let profile = row_to_profile(row);
    res.json(profile);
}

function logout(db, req, res) {
    delete req.session.userid;
    req.session.destroy(() => {});

    res.json(null);
}

function profile(db, req, res) {
    let row = db.prepare('SELECT userid, username FROM users WHERE userid = ?').get(req.session.userid);

    if (row == null) {
        res.json(null);
        return;
    }

    let profile = row_to_profile(row);
    res.json(profile);
}

function row_to_profile(row) {
    let profile = {
        userid: row.userid,
        username: row.username
    };

    return profile;
}

module.exports = {
    init,
    login,
    logout,
    profile
};
