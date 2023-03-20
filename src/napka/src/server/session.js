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
