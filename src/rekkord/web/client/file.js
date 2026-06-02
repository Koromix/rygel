// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { scrypt, hkdf, sha256, chacha20poly1305 } from 'vendor/noble/noble.bundle.js';
import { Util, Log, Net, HttpError } from 'lib/web/base/base.js';
import { Base64 } from 'lib/web/base/mixer.js';

// Use scrypt for age compatibility
const WORK_FACTOR_LOG2 = 18;
const BODY_CHUNK_SIZE = 65536;
const SALT_PREFIX = 'age-encryption.org/v1/scrypt';

const UPLOAD_QUEUE_SIZE = 2;
const UPLOAD_AHEAD_LIMIT = 3;
const UPLOAD_MARK_DELAY = 5000;

const DOWNLOAD_QUEUE_SIZE = 2;
const DOWNLOAD_AHEAD_LIMIT = 4;

function createKey(password = null) {
    let master = new Uint8Array(16);
    crypto.getRandomValues(master);

    let passphrase;
    {
        let rnd = new Uint8Array(32);
        crypto.getRandomValues(rnd);

        passphrase = Base64.toBase64Url(rnd);
    }

    let salt;
    {
        let rnd = new Uint8Array(16);
        crypto.getRandomValues(rnd);

        salt = Base64.toBase64(rnd);
    }

    if (password == null)
        password = '';
    if (password)
        password = '/' + password;

    let n = 2 ** WORK_FACTOR_LOG2;
    let wrap = scrypt(passphrase + password, SALT_PREFIX + salt, { N: n, r: 8, p: 1, dkLen: 32 });
    let chacha = chacha20poly1305(wrap, new Uint8Array(12));
    let body = chacha.encrypt(master);

    let nonce = new Uint8Array(16);
    crypto.getRandomValues(nonce);
    let info = (new TextEncoder).encode('payload');
    let key = hkdf(sha256, master, nonce, info, 32);

    return {
        key: key,
        passphrase: passphrase,
        salt: salt,
        body: Base64.toBase64(body),
        nonce: Base64.toBase64(nonce)
    };
}

function deriveKey(salt, body, nonce, passphrase, password = null) {
    if (password == null)
        password = '';
    if (password)
        password = '/' + password;

    body = Base64.toBytes(body);
    nonce = Base64.toBytes(nonce);

    let n = 2 ** WORK_FACTOR_LOG2;
    let wrap = scrypt(passphrase + password, SALT_PREFIX + salt, { N: n, r: 8, p: 1, dkLen: 32 });
    let chacha = chacha20poly1305(wrap, new Uint8Array(12));
    let master = chacha.decrypt(body);

    let info = (new TextEncoder).encode('payload');
    let key = hkdf(sha256, master, nonce, info, 32);

    return key;
}

async function* upload(info, key, iter) {
    let timer = null;
    let uploaded = 0;

    let fragments = parallelize(uploadFragments(info, key, iter), UPLOAD_QUEUE_SIZE, UPLOAD_AHEAD_LIMIT);

    for await (let frag of fragments) {
        uploaded += frag.length;
        yield frag;

        if (timer == null) {
            timer = setTimeout(async () => {
                await markUpload(info.kid, uploaded);
                timer = null;
            }, UPLOAD_MARK_DELAY);
        }
    }

    clearTimeout(timer);
    await markUpload(info.kid, uploaded);
}

async function* uploadFragments(info, key, iter) {
    let uploaded = 0;
    let idx = 0;
    let counter = 0n;

    let fragments = pump(iter, info.split);
    let { value: next, done } = await fragments.next();

    while (!done) {
        let frag = next;

        // We need to know when the last fragment comes
        {
            let it = await fragments.next();

            next = it.value;
            done = it.done;
        }

        let url = Util.pasteURL('/api/drop/upload', { kid: info.kid, fragment: idx });

        let extra = Math.floor((frag.length + BODY_CHUNK_SIZE - 1) / BODY_CHUNK_SIZE) * 16;
        let cipher = new Uint8Array(frag.length + extra);

        // Encrypt chunks
        for (let i = 0, j = 0; i < frag.length; i += BODY_CHUNK_SIZE, j += BODY_CHUNK_SIZE + 16) {
            let nonce = new Uint8Array(12);
            let last = done && (i + BODY_CHUNK_SIZE >= frag.length);

            (new DataView(nonce.buffer)).setBigUint64(3, counter++, false);
            nonce[11] = last ? 1 : 0;

            let chacha = chacha20poly1305(key, nonce);
            let sub = frag.subarray(i, Math.min(i + BODY_CHUNK_SIZE, frag.length));
            let buf = chacha.encrypt(sub);

            cipher.set(buf, j);
        }

        uploaded += frag.length;
        idx++;

        // Upload fragment
        {
            let response = await Net.fetch(url, {
                method: 'PUT',
                body: cipher,
                timeout: 30000
            });

            if (!response.ok) {
                let msg = await Net.readError(response);
                throw new HttpError(response.status, msg);
            }
        }

        yield frag;
    }
}

async function markUpload(kid, uploaded) {
    let url = Util.pasteURL('/api/drop/mark', { kid: kid });
    await Net.post(url, { uploaded: uploaded });
}

function download(info, key) {
    return parallelize(downloadFragments(info, key), DOWNLOAD_QUEUE_SIZE, DOWNLOAD_AHEAD_LIMIT);
}

async function* downloadFragments(info, key) {
    let downloaded = 0;
    let idx = 0;
    let counter = 0n;

    while (downloaded < info.size) {
        let url = Util.pasteURL('/api/drop/fragment', { kid: info.kid, fragment: idx });

        let expected = Math.min(info.size - idx * info.split, info.split);
        let extra = Math.floor((expected + BODY_CHUNK_SIZE - 1) / BODY_CHUNK_SIZE) * 16;

        let cipher = null;

        for (let i = 0; i < 4; i++) {
            try {
                let response = await fetch(url);

                if (response.ok)
                    cipher = await response.bytes();
            } catch (err) {
                console.error(err);
            }

            if (cipher?.length == expected + extra)
                break;

            // Transient S3 errors will result in truncated output, but a 200 status because the server
            // streams the response. Retry if buffer is shorter than expected!
            await Util.waitFor(1000 + i * 2000);
        }

        if (cipher?.length != expected + extra)
            throw new Error('Failed to download file fragment');

        downloaded += expected;
        idx++;

        let frag = new Uint8Array(expected);

        // Decrypt chunks
        for (let i = 0, j = 0; i < cipher.length; i += BODY_CHUNK_SIZE + 16, j += BODY_CHUNK_SIZE) {
            let nonce = new Uint8Array(12);
            let last = (downloaded == info.size) && (i + BODY_CHUNK_SIZE + 16 >= cipher.length);

            (new DataView(nonce.buffer)).setBigUint64(3, counter++, false);
            nonce[11] = last ? 1 : 0;

            let chacha = chacha20poly1305(key, nonce);
            let sub = cipher.subarray(i, Math.min(i + BODY_CHUNK_SIZE + 16, cipher.length));
            let buf = chacha.decrypt(sub);

            frag.set(buf, j);
        }

        yield frag;
    }
}

async function* pump(iter, split) {
    let buf = new Uint8Array(split);

    for (;;) {
        let offset = 0;

        while (offset < split) {
            let { value: chunk, done } = await iter.next();

            if (done) {
                if (offset) {
                    yield buf.subarray(0, offset);
                    buf = new Uint8Array(split);
                }
                return;
            }

            while (chunk.length > split - offset) {
                buf.set(chunk.subarray(0, split - offset), offset);

                yield buf;
                buf = new Uint8Array(split);

                chunk = chunk.subarray(split - offset);
                offset = 0;
            }

            buf.set(chunk, offset);
            offset += chunk.length;
        }

        yield buf;
        buf = new Uint8Array(split);
    }
}

async function* parallelize(iter, parallel, ahead) {
    let count = 0;
    let queue = new Map;
    let complete = false;

    let values = new Map;
    let yields = 0;

    while (!complete || queue.size) {
        if (values.size < ahead) {
            while (!complete && queue.size < parallel) {
                let idx = count++;
                let p = iter.next().then(next => ({ idx: idx, ...next }));

                queue.set(idx, p);
            }
        }

        if (queue.size) {
            let { idx, done, value } = await Promise.race(queue.values());

            queue.delete(idx);

            if (done) {
                complete = true;
            } else {
                values.set(idx, value);
            }

            let next = values.get(yields);

            if (next != null) {
                values.delete(yields);
                yields++;

                yield next;
            }
        }
    }

    while (values.size) {
        let next = values.get(yields);

        values.delete(yields);
        yields++;

        yield next;
    }
}

export {
    createKey,
    deriveKey,

    upload,
    download
}
