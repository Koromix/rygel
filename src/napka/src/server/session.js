// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import session from 'express-session';
import sqlite3_session_store from 'better-sqlite3-session-store';
import * as pwhash from '../lib/pwhash.js';

const SqliteStore = sqlite3_session_store(session);

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

    let profile = rowToProfile(row);
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

    let profile = rowToProfile(row);
    res.json(profile);
}

function rowToProfile(row) {
    let profile = {
        userid: row.userid,
        username: row.username
    };

    return profile;
}

export {
    init,
    login,
    logout,
    profile
}
