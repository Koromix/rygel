// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import * as Async from 'lib/web/base/async.js';
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
            } catch (err) {
                console.error(err);
                // Unknown drop, go on and fail hard (with server relay)
            }
        } else if (e.request.method == 'GET') {
            let [response, wait] = createDownloadStream(kid);

            e.waitUntil(wait);
            e.respondWith(response);
        }
    }
});

function createDownloadStream(kid) {
    let [info, key, client] = findDrop(kid);

    let fragments = download(info, key, progress);
    let pending = null;
    let downloaded = 0;
    let complete = false;

    let [wait, resolve] = createPromise();

    let stream = new ReadableStream({
        pull: async (controller) => {
            if (controller.desizedSize <= 0)
                return;

            if (pending != null) {
                push(controller);
                return;
            }

            if (complete) {
                setTimeout(() => {
                    controller.close();
                    resolve();
                }, 1000);

                return;
            }

            try {
                let { value: next, done } = await fragments.next();

                // Reset expiration timer
                findDrop(info.kid);

                if (!done) {
                    downloaded += next.length;
                    progress(downloaded);

                    pending = next;
                    push(controller);
                } else {
                    complete = true;
                }
            } catch (err) {
                controller.error(err);

                console.error(err);
                client.postMessage({ type: 'error', args: [err] });

                resolve();
            }
        }
    });

    function push(controller) {
        let slice = pending.subarray(0, controller.desizedSize);

        controller.enqueue(slice);
        pending = pending.subarray(slice.length);

        if (!pending.length)
            pending = null;
    }

    function progress(value) {
        client.postMessage({ type: 'progress', args: [info.kid, value, info.size] });
    }

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
        case 'drop': { Async.wrap(e.source, msg.id, updateDrop, [e.source, ...msg.args]); } break;
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

function createPromise() {
    let ret = [null, null, null];

    ret[0] = new Promise((resolve, reject) => {
        ret[1] = resolve;
        ret[2] = reject;
    })

    return ret;
}
