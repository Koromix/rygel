// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import { xsalsa20poly1305 } from 'vendor/noble/noble.bundle.js';

const EXPIRATION_DELAY = 5 * 60000; // 5 minutes

let keys = new Map;
let clear_timer = null;

onmessage = handleMessage;

function handleMessage(e) {
    let msg = e.data;

    switch (msg.type) {
        case 'key': { wrapAsync(e.source, msg.id, updateKey, msg.args); } break;
    }
}

async function wrapAsync(client, id, func, args) {
    try {
        let ret = await func(...args);
        client.postMessage({ id: id, type: 'success', value: ret });
    } catch (err) {
        client.postMessage({ id: id, type: 'error', value: err });
    }
}

self.addEventListener('fetch', e => {
    e.respondWith(async function() {
        let url = new URL(e.request.url);

        if (e.request.method == 'GET' && url.pathname.startsWith('/api/drop/download/')) {
            let kid = url.pathname.substr(19);
            let response = await downloadDrop(kid);
            return response;
        }

        return await fetch(e.request);
    }());
});

async function downloadDrop(kid) {
    let info;
    {
        let url = Util.pasteURL('/api/drop/info', { kid: kid });
        let response = await fetch(url);

        if (!response.ok)
            return response;

        info = await response.json();
    }

    let fragment = 0;
    let downloaded = 0;

    let options = {
        status: 200,
        headers: {
            'Content-Type': 'application/octet-stream',
            'Content-Disposition': `attachment; filename="${info.name}"`,
            'Content-Length': info.size
        }
    };

    let key = findKey(kid);
    if (key == null)
        throw new Error('Missing decryption key');
    updateKey(kid, key);

    let stream = new ReadableStream({
        pull: async (controller) => {
            if (fragment == null)
                return;

            let url = Util.pasteURL('/api/drop/fragment', { kid: kid, fragment: fragment });
            let expected = Math.min(info.size - fragment * info.chunk, info.chunk) + 16;

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

            if (cipher?.length != expected) {
                let err = new Error('Failed to download file fragment');
                console.error(err);

                controller.error(err);
                controller.close();
                fragment = null;

                return;
            }

            // Reset timer
            updateKey(kid, key);

            let buf = null;

            // Decrypt fragment
            try {
                let nonce = new Uint8Array(24);
                (new DataView(nonce.buffer)).setUint32(0, fragment, true);

                let salsa = xsalsa20poly1305(key, nonce);

                buf = salsa.decrypt(cipher);
            } catch (err) {
                console.error(err);

                controller.error(new Error('Failed to decrypt fragment'));
                controller.close();
                fragmentr = null;

                return;
            }

            downloaded += buf.length;
            fragment++;

            controller.enqueue(buf);

            if (downloaded == info.size) {
                controller.close();
                fragment = null;
            }
        }
    });

    return new Response(stream, options);
}

function findKey(kid) {
    let info = keys.get(kid);
    return info?.key;
}

function updateKey(kid, key) {
    // Update expiration time and keep at the end of entries, so expiration times are sorted

    keys.delete(kid);
    keys.set(kid, {
        key: key,
        expire: performance.now() + EXPIRATION_DELAY
    });

    expireKeys();
}

function expireKeys() {
    let now = performance.now();

    for (let [kid, info] of keys.entries()) {
        if (info.expire > now)
            break;
        keys.delete(kid);
    }

    clearTimeout(clear_timer);

    if (keys.size) {
        let first = keys.values().next().value;
        let timeout = Math.max(0, first.expire - performance.now());

        clear_timer = setTimeout(expireKeys, timeout);
    }
}
