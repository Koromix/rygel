// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import { xsalsa20poly1305 } from 'vendor/noble/noble.bundle.js';

const DOWNLOAD_QUEUE_SIZE = 2;
const DOWNLOAD_AHEAD_LIMIT = 4;

const EXPIRATION_DELAY = 5 * 60000; // 5 minutes

let drops = new Map;
let clear_timer = null;

onmessage = handleMessage;

function handleMessage(e) {
    let msg = e.data;

    switch (msg.type) {
        case 'drop': { wrapAsync(e.source, msg.id, updateDrop, [e.source, ...msg.args]); } break;
    }
}

// Quick SW activation after update
self.addEventListener('install', e => {
    self.skipWaiting();
})
self.addEventListener('activate', e => {
    e.waitUntil(clients.claim());
});

self.addEventListener('fetch', e => {
    let url = new URL(e.request.url);

    if (e.request.method == 'GET' && url.pathname.startsWith('/api/drop/download/')) {
        let kid = url.pathname.substr(19);
        let [response, wait] = startDownload(kid);

        e.waitUntil(wait);
        e.respondWith(response);

        return;
    }

    e.respondWith(fetch(e.request));
});

function startDownload(kid) {
    let info = findDrop(kid);

    let [wait, resolve, reject] = createPromise();
    let fragments = downloadFragments(info);

    let stream = new ReadableStream({
        pull: async (controller) => {
            try {
                let { done, value } = await fragments.next();

                if (!done) {
                    controller.enqueue(value);
                } else {
                    controller.close();
                    resolve();
                }
            } catch (err) {
                controller.error(err);
                controller.close();

                console.error(err);
                info.client.postMessage({ type: 'error', args: [err] });

                reject(err);
            }
        }
    });

    let options = {
        status: 200,
        headers: {
            'Content-Type': 'application/octet-stream',
            'Content-Disposition': `attachment; filename="${info.name}"`,
            'Content-Length': info.size
        }
    };

    let response = new Response(stream, options);

    return [response, wait];
}

async function *downloadFragments(info) {
    let downloaded = 0;
    let fragment = 0;
    let queue = [];

    let buffers = new Map;
    let yields = 0;

    while (downloaded < info.size || queue.length) {
        if (buffers.size < DOWNLOAD_AHEAD_LIMIT) {
            while (downloaded < info.size && queue.length < DOWNLOAD_QUEUE_SIZE) {
                let expected = Math.min(info.size - fragment * info.chunk, info.chunk) + 16;

                queue.push({
                    idx: fragment,
                    download: downloadFragment(info, fragment, expected)
                });

                downloaded += expected;
                fragment++;
            }
        }

        if (queue.length) {
            let [idx, buf] = await Promise.any(queue.map(it => it.download));

            queue = queue.filter(it => it.idx != idx);
            buffers.set(idx, buf);

            let next = buffers.get(yields);

            if (next != null) {
                buffers.delete(yields);
                yields++;

                yield next;
            }
        }
    }

    while (buffers.size) {
        let next = buffers.get(yields);

        buffers.delete(yields);
        yields++;

        yield next;
    }
}

async function downloadFragment(info, idx, expected) {
    let url = Util.pasteURL('/api/drop/fragment', { kid: info.kid, fragment: idx });
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

        let salsa = xsalsa20poly1305(info.key, nonce);

        buf = salsa.decrypt(cipher);
    }

    return [idx, buf];
}

function updateDrop(client, info, key) {
    drops.delete(info.kid);

    drops.set(info.kid, {
        client: client,

        ...info,

        key: key,
        expire: performance.now() + EXPIRATION_DELAY
    });

    expireDrops();
}

function findDrop(kid) {
    let info = drops.get(kid);

    if (info == null)
        throw new Error('Missing or stale file drop information');

    // Keep at the the end of entries, so expiration times are sorted
    drops.delete(kid);
    drops.set(kid, info);

    expireDrops();

    return info;
}

function expireDrops() {
    let now = performance.now();

    for (let [kid, info] of drops.entries()) {
        if (info.expire > now)
            break;
        drops.delete(kid);
    }

    clearTimeout(clear_timer);

    if (drops.size) {
        let first = drops.values().next().value;
        let timeout = Math.max(0, first.expire - performance.now());

        clear_timer = setTimeout(expireDrops, timeout);
    }
}

async function wrapAsync(client, id, func, args) {
    try {
        let ret = await func(...args);
        client.postMessage({ type: 'return', args: [id, true, ret] });
    } catch (err) {
        client.postMessage({ type: 'return', args: [id, false, err] });
    }
}

function createPromise() {
    let ret = [null, null, null];

    ret[0] = new Promise((resolve, reject) => {
        ret[1] = resolve;
        ret[2] = reject;
    })

    return ret;
}
