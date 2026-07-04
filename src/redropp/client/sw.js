// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import { download } from './file.js';

const EXPIRATION_DELAY = 5 * 60000; // 5 minutes

let drops = new Map;
let clear_timer = null;

onmessage = handleMessage;

self.addEventListener('install', e => e.waitUntil(self.skipWaiting()));
self.addEventListener('activate', e => e.waitUntil(self.clients.claim()));

self.addEventListener('fetch', e => {
    let url = new URL(e.request.url);

    if (url.pathname.startsWith('/drop/decrypt/')) {
        let kid = url.pathname.substr(14);

        if (e.request.method == 'HEAD') {
            try {
                let [info] = findDrop(kid);

                let response = new Response('', {
                    status: 200,
                    headers: prepareHeaders(info)
                });
                e.respondWith(response);

                return;
            } catch (err) {
                console.error(err);
                // Unknown drop, go on and fail hard (with server relay)
            }
        } else if (e.request.method == 'GET') {
            let [response, wait] = createDownloadStream(kid);

            e.waitUntil(wait);
            e.respondWith(response);

            return;
        }
    }

    e.respondWith(fetch(e.request));
});

function createDownloadStream(kid) {
    let [info, key, client] = findDrop(kid);

    let [wait, resolve, reject] = createPromise();

    let fragments = download(info, key);
    let downloaded = 0;

    let stream = new ReadableStream({
        pull: async (controller) => {
            if (controller.desizedSize < 0)
                return;

            try {
                let { value: frag, done } = await fragments.next();

                // Reset expiration timer
                findDrop(info.kid);

                if (!done) {
                    downloaded += frag.length;
                    client.postMessage({ type: 'progress', args: [info.kid, downloaded, info.size] });

                    controller.enqueue(frag);
                } else {
                    setTimeout(() => controller.close(), 1000);
                    resolve();
                }
            } catch (err) {
                controller.error(err);

                console.error(err);
                client.postMessage({ type: 'error', args: [err] });

                reject(err);
            }
        }
    });

    let options = {
        status: 200,
        headers: prepareHeaders(info)
    };

    let response = new Response(stream, options);

    return [response, wait];
}

function prepareHeaders(info) {
    let headers = {
        'Content-Type': 'application/octet-stream',
        'Content-Disposition': `attachment; filename="${info.name.replaceAll('"', '\"')}"`,
        'Content-Length': info.size,

        'X-Content-Type-Options': 'nosniff'
    };

    return headers;
}

function handleMessage(e) {
    let msg = e.data;

    switch (msg.type) {
        case 'drop': { wrapAsync(e.source, msg.id, updateDrop, [e.source, ...msg.args]); } break;
    }
}

function updateDrop(client, info, key) {
    drops.delete(info.kid);

    drops.set(info.kid, {
        client: client,

        info: info,
        key: key,

        expire: performance.now() + EXPIRATION_DELAY
    });

    expireDrops();
}

function findDrop(kid) {
    let drop = drops.get(kid);

    if (drop == null)
        throw new Error('Missing or stale file drop information');

    // Keep at the the end of entries, so expiration times are sorted
    drops.delete(kid);
    drops.set(kid, drop);

    expireDrops();

    return [drop.info, drop.key, drop.client];
}

function expireDrops() {
    let now = performance.now();

    for (let [kid, drop] of drops.entries()) {
        if (drop.expire > now)
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
