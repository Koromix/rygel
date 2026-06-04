// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import { download } from './d_file.js';

const EXPIRATION_DELAY = 5 * 60000; // 5 minutes

let drops = new Map;
let clear_timer = null;

onmessage = handleMessage;

self.addEventListener('install', e => e.waitUntil(self.skipWaiting()));
self.addEventListener('activate', e => e.waitUntil(self.clients.claim()));

self.addEventListener('fetch', e => {
    let url = new URL(e.request.url);

    if (e.request.method == 'GET' && url.pathname.startsWith('/api/drop/download/')) {
        let kid = url.pathname.substr(19);
        let [response, wait] = createDownloadStream(kid);

        e.waitUntil(wait);
        e.respondWith(response);

        return;
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
            try {
                let { value: frag, done } = await fragments.next();

                // Reset expiration timer
                findDrop(info.kid);

                if (!done) {
                    downloaded += frag.length;
                    client.postMessage({ type: 'progress', args: [info.kid, downloaded, info.size] });

                    controller.enqueue(frag);
                } else {
                    controller.close();
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
        headers: {
            'Content-Type': 'application/octet-stream',
            'Content-Disposition': `attachment; filename="${info.name}"`,
            'Content-Length': info.size
        }
    };

    let response = new Response(stream, options);

    return [response, wait];
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
