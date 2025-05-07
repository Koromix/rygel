#!/usr/bin/env node

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

'use strict';

const crypto = require('crypto');
const cache = require('../lib/cache.js');
const database = require('../lib/database.js');
const pwhash = require('../lib/pwhash.js');

main();

async function main() {
    let command = null;
    let values = [];

    // Parse options
    {
        if (process.argv[2] == '--help') {
            print_usage();
            return;
        }
        if (process.argv.length < 3 || process.argv[2][0] == '-')
            throw new Error(`Missing command, use --help`);

        switch (process.argv[2]) {
            case 'list_users': { command = listUsers; } break;
            case 'allow_users': { command = allowUsers; } break;
            case 'revoke_users': { command = revokeUsers; } break;
            case 'clear_tiles': { command = clearTiles; } break;

            default: { throw new Error(`Unknown command '${process.argv[2]}'`); } break;
        }

        for (let i = 3; i < process.argv.length; i++) {
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
                print_usage();
                return;
            } else if (arg[0] == '-') {
                throw new Error(`Unexpected argument '${arg}'`);
            } else {
                values.push(arg);
            }
        }
    }

    try {
        let success = !!command(values);
        process.exit(!success);
    } catch (err) {
        console.error(err);
        process.exit(1);
    } finally {
        db.close();
    }
}

function allowUsers(usernames) {
    let db = database.open({ fileMustExist: true });

    if (!usernames.length)
        throw new Error('Missing user names');

    console.log('ID     | Username   | Password');
    console.log('------------------------------');

    db.transaction(() => {
        for (let username of usernames) {
            let password = generatePassword(24);
            let password_hash = pwhash.pbkdf2(password);

            let userid = db.prepare(`INSERT INTO users (username, password_hash)
                                         VALUES (?, ?)
                                         ON CONFLICT DO UPDATE SET password_hash = excluded.password_hash
                                         RETURNING userid`).pluck().get(username, password_hash);

            console.log(`${String(userid).padEnd(6, ' ')} | ${username.padEnd(10, ' ')} | ${password}`);
        }
    })();
}

function revokeUsers(usernames) {
    let db = database.open({ fileMustExist: true });

    if (!usernames.length)
        throw new Error('Missing user names');

    let removed = 0;

    db.transaction(() => {
        for (let username of usernames) {
            let info = db.prepare('DELETE FROM users WHERE username = ?').run(username);

            if (info.changes) {
                removed++;
            } else {
                console.error(`Ignoring missing user '${username}'`)
            }
        }
    })();

    if (!removed)
        throw new Error('Failed to remove any user');

    console.log(`Removed ${removed} ${removed == 1 ? 'user' : 'users'}`);
}

function listUsers() {
    let db = database.open({ fileMustExist: true });

    let users = db.prepare('SELECT userid, username FROM users').all();

    console.log('ID     | Username');
    console.log('-----------------');

    for (let user of users)
        console.log(`${String(user.userid).padEnd(6, ' ')} | ${user.username}`);
}

function generatePassword(len = 24) {
    const CHARS = "ABCDEFGHJKMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789!_-%$";

    let password = '';

    for (let i = 0; i < len; i++) {
        let rnd = crypto.randomInt(0, CHARS.length);
        password += CHARS[rnd];
    }

    return password;
}

function clearTiles() {
    let db = cache.open({ fileMustExist: true });

    db.prepare('DELETE FROM tiles').run();
    console.log('Tile cache cleared');
}

function printUsage() {
    let help = `Usage: node users.js <command> [users...]

User commands:

    list_users                   List allowed users

    allow_users                  Allow one or more users
                                 (hint: reset password is user exists)
    revoke_users                 Revoke one or more users

Misc commands:

    clear_tiles                  Clear cache of tiles
`;

    console.log(help);
}
