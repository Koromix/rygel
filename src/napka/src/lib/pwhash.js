// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

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
