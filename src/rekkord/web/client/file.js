// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { xsalsa20poly1305 } from 'vendor/noble/noble.bundle.js';
import { Util, Log, Net, HttpError } from 'lib/web/base/base.js';

const UPLOAD_QUEUE_SIZE = 2;
const UPLOAD_AHEAD_LIMIT = 3;
const UPLOAD_MARK_DELAY = 5000;

const DOWNLOAD_QUEUE_SIZE = 2;
const DOWNLOAD_AHEAD_LIMIT = 4;

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

    let fragments = pumpChunks(iter, info.chunk);

    for await (let frag of fragments) {
        let url = Util.pasteURL('/api/drop/upload', { kid: info.kid, fragment: idx });

        let cipher = null;

        // Encrypt fragment
        {
            let nonce = new Uint8Array(24);
            (new DataView(nonce.buffer)).setUint32(0, idx, true);

            let salsa = xsalsa20poly1305(key, nonce);

            cipher = salsa.encrypt(frag);
        }

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

        uploaded += frag.length;
        idx++;

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

    while (downloaded < info.size) {
        let url = Util.pasteURL('/api/drop/fragment', { kid: info.kid, fragment: idx });
        let expected = Math.min(info.size - idx * info.chunk, info.chunk) + 16;

        let cipher = null;

        for (let i = 0; i < 4; i++) {
            try {
                let response = await fetch(url);

                if (response.ok)
                    cipher = await response.bytes();
            } catch (err) {
                console.error(err);
            }

            if (cipher?.length == expected)
                break;

            // Transient S3 errors will result in truncated output, but a 200 status because the server
            // streams the response. Retry if buffer is shorter than expected!
            await Util.waitFor(1000 + i * 2000);
        }

        if (cipher?.length != expected)
            throw new Error('Failed to download file fragment');

        let buf = null;

        // Decrypt fragment
        {
            let nonce = new Uint8Array(24);
            (new DataView(nonce.buffer)).setUint32(0, idx, true);

            let salsa = xsalsa20poly1305(key, nonce);

            buf = salsa.decrypt(cipher);
        }

        downloaded += expected;
        idx++;

        yield buf;
    }
}

async function* pumpChunks(iter, split) {
    for (;;) {
        let buf = new Uint8Array(split);
        let offset = 0;

        while (offset < split) {
            let { value: chunk, done } = await iter.next();

            if (done) {
                if (offset)
                    yield buf.subarray(0, offset);
                return;
            }

            while (chunk.length > split - offset) {
                buf.set(chunk.subarray(0, split - offset), offset);

                yield buf;

                chunk = chunk.subarray(split - offset);
                offset = 0;
            }

            buf.set(chunk, offset);
            offset += chunk.length;
        }

        yield buf;
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
    upload,
    download
}
