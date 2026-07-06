// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { scryptAsync, hkdf, hmac, sha256, chacha20poly1305, randomBytes } from 'vendor/noble/noble.bundle.js';
import { Util, Log, Net, NetworkError, HttpError } from 'lib/web/base/base.js';
import { Base64 } from 'lib/web/base/mixer.js';

// Use scrypt for age compatibility
const WORK_FACTOR_LOG2 = 18;
const BODY_CHUNK_SIZE = 65536;
const HEADER_SIGNATURE = 'age-encryption.org/v1';
const HEADER_LENGTH = 149;
const SALT_PREFIX = 'age-encryption.org/v1/scrypt';

const UPLOAD_QUEUE_SIZE = 2;
const UPLOAD_AHEAD_LIMIT = 3;
const UPLOAD_MARK_DELAY = 5000;

const DOWNLOAD_QUEUE_SIZE = 2;
const DOWNLOAD_AHEAD_LIMIT = 4;

async function createHeader(password = null) {
    if (password == null)
        password = '';
    if (password)
        password = '/' + password;

    let master = randomBytes(16);
    let passphrase = Base64.toBase64Url(randomBytes(32));
    let salt = randomBytes(16);
    let nonce = randomBytes(16);

    let n = 2 ** WORK_FACTOR_LOG2;
    let wrap = await scryptAsync(passphrase + password, encodeSalt(salt), { N: 2 ** WORK_FACTOR_LOG2, r: 8, p: 1, dkLen: 32 });
    let chacha = chacha20poly1305(wrap, new Uint8Array(12));
    let body = chacha.encrypt(master);

    let info = (new TextEncoder).encode('payload');
    let key = hkdf(sha256, master, nonce, info, 32);

    let headers = [
        HEADER_SIGNATURE,
        `-> scrypt ${Base64.toBase64(salt, false)} 18`,
        Base64.toBase64(body, false),
        '---'
    ];

    // Append header MAC
    {
        let mkey = hkdf(sha256, master, undefined, (new TextEncoder).encode('header'), 32);
        let mac = hmac(sha256, mkey, (new TextEncoder).encode(headers.join('\n')));

        headers[headers.length - 1] = '--- ' + Base64.toBase64(mac, false);
    }

    return {
        passphrase: passphrase,
        header: headers.join('\n'),
        nonce: Base64.toBase64(nonce),
        key: key
    };
}

async function decodeHeader(header, nonce, passphrase, password = null) {
    if (password == null)
        password = '';
    if (password)
        password = '/' + password;

    if (header.length != HEADER_LENGTH)
        throw new Error('Invalid header length');

    let headers = header.split('\n');

    try {
        header = parseHeaders(headers);
        nonce = Base64.toBytes(nonce);
    } catch (err) {
        console.error(err);
        throw new Error('Invalid age-v1 header or nonce');
    }

    let n = 2 ** header.factor;
    let wrap = await scryptAsync(passphrase + password, encodeSalt(header.salt), { N: n, r: 8, p: 1, dkLen: 32 });
    let chacha = chacha20poly1305(wrap, new Uint8Array(12));
    let master = chacha.decrypt(header.body);

    // Check header MAC
    {
        headers[3] = '---';

        let mkey = hkdf(sha256, master, undefined, (new TextEncoder).encode('header'), 32);
        let mac = hmac(sha256, mkey, (new TextEncoder).encode(headers.join('\n')));

        if (!compareBuffers(mac, header.mac))
            throw new Error('Corrupt age-v1 header MAC');
    }

    let key = hkdf(sha256, master, nonce, (new TextEncoder).encode('payload'), 32);

    return key;
}

function compareBuffers(a, b) {
    if (a.length != b.length)
        return false;

    for (let i = 0; i < a.length; i++) {
        if (a[i] != b[i])
            return false;
    }

    return true;
}

function parseHeaders(headers) {
    if (headers.length != 4)
        throw new Error('Invalid encryption header format');
    if (headers[0] != HEADER_SIGNATURE)
        throw new Error('Invalid encryption header signature');
    if (!headers[1].startsWith('-> scrypt '))
        throw new Error('Invalid or unsupported recipient');
    if (!headers[3].startsWith('--- '))
        throw new Error('Invalid header MAC prefix');

    let salt = Base64.toBytes(headers[1].substr(10, 22), false);
    let factor = parseInt(headers[1].substr(33), 10);
    let body = Base64.toBytes(headers[2], false);
    let mac = Base64.toBytes(headers[3].substr(4), false);

    if (Number.isNaN(factor))
        throw new Error('Invalid work factor value');

    return {
        salt: salt,
        factor: factor,
        body: body,
        mac: mac
    };
}

function encodeSalt(salt) {
    let buf = new Uint8Array(SALT_PREFIX.length + salt.length);

    buf.set((new TextEncoder).encode(SALT_PREFIX), 0);
    buf.set(salt, SALT_PREFIX.length);

    return buf;
}

async function upload(info, key, iter, progress = () => {}) {
    let timer = null;
    let uploaded = 0;

    let fragments = parallelize(uploadFragments(info, key, iter, progress), UPLOAD_QUEUE_SIZE, UPLOAD_AHEAD_LIMIT);

    for await (let frag of fragments) {
        uploaded += frag.length;

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

async function* uploadFragments(info, key, iter, progress) {
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

        let url = Util.pasteURL('/api/drop/fragment', { kid: info.kid, fragment: idx });

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

        idx++;

        // Upload fragment with retry logic
        {
            let success = false;

            for (let i = 0; i < 4; i++) {
                try {
                    await new Promise((resolve, reject) => {
                        let xhr = new XMLHttpRequest;

                        xhr.responseType = 'text';
                        xhr.timeout = 60000;

                        xhr.onload = async () => {
                            if (xhr.status == 200) {
                                resolve();
                            } else {
                                let text = (new TextDecoder).decode(body);
                                let msg = await Net.readError(xhr.responseText);

                                reject(new HttpError(xhr.status, msg));
                            }
                        };
                        xhr.ontimeout = e => reject(new NetworkError('timeout'));
                        xhr.onerror = e => reject(new NetworkError(e.type ?? 'network error'));

                        xhr.upload.onprogress = e => {
                            let normalized = Math.round(e.loaded / cipher.length * frag.length);
                            progress(uploaded + normalized);
                        };

                        xhr.open('PUT', url, true);
                        xhr.send(cipher);
                    });

                    success = true;
                    break;
                } catch (err) {
                    console.error(err);
                }

                await Util.waitFor(1000 + i * 2000);
            }

            if (!success)
                throw new Error('Failed to upload file fragment');
        }

        uploaded += frag.length;
        progress(uploaded);

        yield frag;
    }
}

async function markUpload(kid, uploaded) {
    let url = Util.pasteURL('/api/drop/mark', { kid: kid });
    await Net.post(url, { uploaded: uploaded });
}

function download(info, key, progress = () => {}) {
    return parallelize(downloadFragments(info, key, progress), DOWNLOAD_QUEUE_SIZE, DOWNLOAD_AHEAD_LIMIT);
}

async function* downloadFragments(info, key, progress) {
    let downloaded = 0;
    let idx = 0;
    let counter = 0n;

    while (downloaded < info.size) {
        let url = Util.pasteURL('/api/drop/fragment', { kid: info.kid, fragment: idx });

        let expected = Math.min(info.size - idx * info.split, info.split);
        let extra = Math.floor((expected + BODY_CHUNK_SIZE - 1) / BODY_CHUNK_SIZE) * 16;

        let cipher = new Uint8Array(expected + extra);

        // Download fragment with retry logic
        {
            let success = false;

            for (let i = 0; i < 4; i++) {
                try {
                    let response = await fetch(url);

                    if (!response.ok) {
                        let msg = await Net.readError(response);
                        throw new HttpError(response.status, msg);
                    }

                    let reader = response.body.getReader();
                    let loaded = 0;

                    try {
                        for (;;) {
                            let { value: chunk, done } = await reader.read();

                            if (done)
                                break;

                            if (loaded + chunk.length <= cipher.length)
                                cipher.set(chunk, loaded);
                            loaded += chunk.length;

                            let normalized = Math.round(loaded / cipher.length * expected);
                            progress(downloaded + normalized);
                        }
                    } finally {
                        reader.releaseLock();
                    }

                    if (loaded == cipher.length) {
                        success = true;
                        break;
                    }
                } catch (err) {
                    console.error(err);
                }

                // Transient S3 errors will result in truncated output, but a 200 status because the server
                // streams the response. Retry if buffer is shorter than expected!
                await Util.waitFor(1000 + i * 2000);
            }

            if (!success)
                throw new Error('Failed to download file fragment');
        }

        idx++;

        downloaded += expected;
        progress(downloaded);

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
    createHeader,
    decodeHeader,

    upload,
    download
}
