// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import * as Async from 'lib/web/base/async.js';
import { download } from './file.js';

const EXPIRATION_DELAY = 5 * 60000; // 5 minutes
const STALL_DELAY = 20000; // 20 seconds

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
                let drop = findDrop(kid);

                let response = new Response('', {
                    status: 200,
                    headers: prepareHeaders(drop.info)
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
    let drop = findDrop(kid);
    let client = drop.client;

    let fragments = download(drop.info, drop.key, progress);

    let pending_frag = null;
    let downloaded = 0;
    let download_complete = false;
    let stall_timer = null;

    let [wait, resolve] = createPromise();

    let stream = new ReadableStream({
        pull: async (controller) => {
            if (controller.desizedSize <= 0)
                return;

            if (pending_frag != null) {
                push(controller);
                return;
            }

            if (download_complete) {
                setTimeout(() => {
                    controller.close();
                    end();
                }, 1000);

                return;
            }

            try {
                let { value: next, done } = await fragments.next();

                // Reset expiration timer
                findDrop(kid);

                if (!done) {
                    downloaded += next.length;
                    progress(downloaded);

                    pending_frag = next;
                    push(controller);
                } else {
                    download_complete = true;
                }
            } catch (err) {
                controller.error(err);

                console.error(err);
                client.postMessage({ kind: 'failed', args: [drop.id, err] });

                end();
            }
        }
    });

    function push(controller) {
        let slice = pending_frag.subarray(0, controller.desizedSize);

        try {
            controller.enqueue(slice);
        } catch (err) {
            console.error(err);
            client.postMessage({ kind: 'failed', args: [drop.id, null] });
        }

        pending_frag = pending_frag.subarray(slice.length);

        if (!pending_frag.length)
            pending_frag = null;
    }

    function progress(value) {
        client.postMessage({ kind: 'progress', args: [drop.id, value, drop.info.size] });

        clearTimeout(stall_timer);
        stall_timer = setTimeout(() => progress(value), STALL_DELAY);
    }

    function end() {
        clearTimeout(stall_timer);
        resolve();
    }

    let response = new Response(stream, {
        status: 200,
        headers: prepareHeaders(drop.info)
    });

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

    switch (msg.kind) {
        case 'drop': { Async.wrap(e.source, msg, () => updateDrop(e.source, ...msg.args)); } break;
    }
}

function updateDrop(client, id, info, key) {
    drops.set(info.kid, {
        client: client,
        id: id,

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

    return drop;
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
