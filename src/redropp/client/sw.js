// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import * as Async from 'lib/web/base/async.js';
import { CRC32 } from 'lib/web/base/mixer.js';
import {
    patchFooterCrc32,
    patchCentralCrc32,
    skipCentralHeader
} from './zip.js';
import { download } from '{{ BUNDLE file.js }}';

const EXPIRATION_DELAY = 5 * 60000; // 5 minutes
const STALL_DELAY = 20000; // 20 seconds

let entries = new Map;
let clear_timer = null;

onmessage = handleMessage;

function handleMessage(e) {
    let msg = e.data;

    switch (msg.kind) {
        case 'file': { Async.wrap(e.source, msg, () => prepareFile(e.source, ...msg.args)); } break;
        case 'zip': { Async.wrap(e.source, msg, () => prepareZip(e.source, ...msg.args)); } break;
    }
}

self.addEventListener('install', e => e.waitUntil(self.skipWaiting()));
self.addEventListener('activate', e => e.waitUntil(self.clients.claim()));

self.addEventListener('fetch', e => {
    let url = new URL(e.request.url);

    if (url.pathname.startsWith('/auto/download/')) {
        let [, , , kid] = url.pathname.split('/');

        if (e.request.method == 'HEAD') {
            handleHead(e, kid);
        } else if (e.request.method == 'GET') {
            let entry = findEntry(kid);

            let init = progress => download(entry.file, entry.key, progress);
            let [response, wait] = createStream(entry, 'application/octet-stream', init);

            e.waitUntil(wait);
            e.respondWith(response);
        }
    } else if (url.pathname.startsWith('/auto/zip/')) {
        let [, , , kid] = url.pathname.split('/');

        if (e.request.method == 'HEAD') {
            handleHead(e, kid);
        } else if (e.request.method == 'GET') {
            let entry = findEntry(kid);

            let init = progress => zip(entry.files, entry.keys, entry.headers, entry.footers, entry.directory);
            let [response, wait] = createStream(entry, 'application/zip', init);

            e.waitUntil(wait);
            e.respondWith(response);
        }
    }
});

function handleHead(e, kid) {
    try {
        let entry = findEntry(kid);

        let response = new Response('', {
            status: 200,
            headers: prepareHeaders(entry.total)
        });

        e.respondWith(response);
    } catch (err) {
        console.error(err);
        // Unknown drop/file, go on and fail hard (with server relay)
    }
}

function createStream(entry, type, init) {
    let client = entry.client;

    let pending_frag = null;
    let downloaded = 0;
    let download_complete = false;
    let stall_timer = null;

    let fragments = init(progress);
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
                findEntry(entry.kid);

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
                client.postMessage({ kind: 'failed', args: [entry.id, err] });

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
            client.postMessage({ kind: 'failed', args: [entry.id, null] });
        }

        pending_frag = pending_frag.subarray(slice.length);

        if (!pending_frag.length)
            pending_frag = null;
    }

    function progress(value) {
        client.postMessage({ kind: 'progress', args: [entry.id, value, entry.total] });

        clearTimeout(stall_timer);
        stall_timer = setTimeout(() => progress(value), STALL_DELAY);
    }

    function end() {
        clearTimeout(stall_timer);
        resolve();
    }

    let response = new Response(stream, {
        status: 200,
        headers: prepareHeaders(entry.total, type)
    });

    return [response, wait];
}

function prepareHeaders(size, type) {
    let headers = {
        'Content-Type': type,
        'Content-Disposition': 'attachment',
        'Content-Length': size,

        'X-Content-Type-Options': 'nosniff'
    };

    return headers;
}

function prepareFile(client, id, file, key) {
    entries.set(file.kid, {
        kid: file.kid,

        client: client,
        id: id,
        expire: performance.now() + EXPIRATION_DELAY,
        total: total.size,

        file: file,
        key: key
    });

    clearTimeout(clear_timer);
    clear_timer = setTimeout(expireEntries, 10000);
}

function prepareZip(client, id, drop, total, keys, headers, footers, directory) {
    entries.set(drop.kid, {
        kid: drop.kid,

        client: client,
        id: id,
        expire: performance.now() + EXPIRATION_DELAY,
        total: total,

        files: drop.files,
        keys: keys,
        headers: headers,
        footers: footers,
        directory: directory
    });

    clearTimeout(clear_timer);
    clear_timer = setTimeout(expireEntries, 10000);
}

function findEntry(kid) {
    let entry = entries.get(kid);

    if (entry == null)
        throw new Error('Missing or stale file information');

    // Keep at the the end of entries, so expiration times are sorted
    entry.expire = performance.now() + EXPIRATION_DELAY;
    entries.delete(kid);
    entries.set(kid, entry);

    expireEntries();

    return entry;
}

function expireEntries() {
    let now = performance.now();

    for (let [kid, entry] of entries.entries()) {
        if (entry.expire > now)
            break;
        entries.delete(kid);
    }

    clearTimeout(clear_timer);

    if (entries.size) {
        let first = entries.values().next().value;
        let timeout = Math.max(0, first.expire - performance.now());

        clear_timer = setTimeout(expireEntries, timeout);
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

async function* zip(files, keys, headers, footers, directory) {
    let directory_offset = 0;

    for (let i = 0; i < files.length; i++) {
        let file = files[i];
        let key = keys[i];
        let header = headers[i];
        let footer = footers[i];

        yield header;

        let crc32 = 0;

        for await (let frag of download(file, key)) {
            crc32 = CRC32(crc32, frag);
            yield frag;
        }

        patchFooterCrc32(footer, 0, crc32);
        patchCentralCrc32(directory, directory_offset, crc32);
        directory_offset += skipCentralHeader(directory, directory_offset);

        yield footer;
    }

    yield directory;
}
