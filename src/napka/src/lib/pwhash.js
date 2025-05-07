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

import crypto from 'crypto';

function pbkdf2(str) {
    let digest = 'sha256';
    let salt = crypto.randomBytes(20);
    let iterations = 100000;

    let hash = crypto.pbkdf2Sync(str, salt, iterations, 32, digest);
    let pwhash = `${digest}$${iterations}$${salt.toString('base64')}$${hash.toString('base64')}`;

    return pwhash;
}

function verify(str, pwhash) {
    let [digest, iterations, salt, hash1] = pwhash.split('$');

    iterations = parseInt(iterations);
    salt = Buffer.from(salt, 'base64');
    hash1 = Buffer.from(hash1, 'base64');

    let hash2 = crypto.pbkdf2Sync(str, salt, iterations, 32, digest);

    let match = crypto.timingSafeEqual(hash1, hash2);
    return match;
}

export {
    pbkdf2,
    verify
}
