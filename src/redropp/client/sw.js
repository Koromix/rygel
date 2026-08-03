// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import * as Async from 'lib/web/base/async.js';
import { download } from '{{ BUNDLE file.js }}';

const EXPIRATION_DELAY = 5 * 60000; // 5 minutes
const STALL_DELAY = 20000; // 20 seconds

let files = new Map;
let clear_timer = null;

onmessage = handleMessage;

self.addEventListener('install', e => e.waitUntil(self.skipWaiting()));
self.addEventListener('activate', e => e.waitUntil(self.clients.claim()));

self.addEventListener('fetch', e => {
    let url = new URL(e.request.url);

    if (url.pathname.startsWith('/auto/download/')) {
        let [, , , kid] = url.pathname.split('/');

        if (e.request.method == 'HEAD') {
            try {
                let file = findFile(kid);

                let response = new Response('', {
                    status: 200,
                    headers: prepareHeaders(file.info)
                });
                e.respondWith(response);
            } catch (err) {
                console.error(err);
                // Unknown drop/file, go on and fail hard (with server relay)
            }
        } else if (e.request.method == 'GET') {
            let [response, wait] = createDownloadStream(kid);

            e.waitUntil(wait);
            e.respondWith(response);
        }
    }
});

function createDownloadStream(kid) {
    let file = findFile(kid);
    let client = file.client;

    let fragments = download(file.info, file.key, progress);

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

            try {
                let { value: next, done } = await fragments.next();

                // Reset expiration timer
                findFile(kid);

                if (!done) {
                    downloaded += next.length;
                    progress(downloaded);

                    pending_frag = next;
                    push(controller);
                } else if (!download_complete) {
                    download_complete = true;

                    setTimeout(() => {
                        controller.close();
                        end();
                    }, 1000);
                }
            } catch (err) {
                controller.error(err);

                console.error(err);
                client.postMessage({ kind: 'failed', args: [file.id, err] });

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
            client.postMessage({ kind: 'failed', args: [file.id, null] });
        }

        pending_frag = pending_frag.subarray(slice.length);

        if (!pending_frag.length)
            pending_frag = null;
    }

    function progress(value) {
        client.postMessage({ kind: 'progress', args: [file.id, value, file.info.size] });

        clearTimeout(stall_timer);
        stall_timer = setTimeout(() => progress(value), STALL_DELAY);
    }

    function end() {
        clearTimeout(stall_timer);
        resolve();
    }

    let response = new Response(stream, {
        status: 200,
        headers: prepareHeaders(file.info)
    });

    return [response, wait];
}

function prepareHeaders(info) {
    let headers = {
        'Content-Type': 'application/octet-stream',
        'Content-Disposition': 'attachment',
        'Content-Length': info.size,

        'X-Content-Type-Options': 'nosniff'
    };

    return headers;
}

function handleMessage(e) {
    let msg = e.data;

    switch (msg.kind) {
        case 'prepare': { Async.wrap(e.source, msg, () => updateFile(e.source, ...msg.args)); } break;
    }
}

function updateFile(client, id, info, key) {
    files.set(info.kid, {
        client: client,
        id: id,

        info: info,
        key: key,

        expire: performance.now() + EXPIRATION_DELAY
    });

    expireFiles();
}

function findFile(kid) {
    let file = files.get(kid);

    if (file == null)
        throw new Error('Missing or stale file drop information');

    // Keep at the the end of entries, so expiration times are sorted
    files.delete(kid);
    files.set(kid, file);

    expireFiles();

    return file;
}

function expireFiles() {
    let now = performance.now();

    for (let [kid, file] of files.entries()) {
        if (file.expire > now)
            break;
        files.delete(kid);
    }

    clearTimeout(clear_timer);

    if (files.size) {
        let first = files.values().next().value;
        let timeout = Math.max(0, first.expire - performance.now());

        clear_timer = setTimeout(expireFiles, timeout);
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
