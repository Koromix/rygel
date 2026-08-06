// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, LruMap, Log, Net } from 'lib/web/base/base.js';
import * as Async from 'lib/web/base/async.js';
import * as UI from 'lib/web/ui/ui.js';
import { ProgressMeter } from './format.js';
import {
    createLocalHeader,
    createLocalFooter,
    createCentralDirectory
} from './zip.js';

const STATUS_TIMEOUT = 40000; // 40 seconds
const STATUS_EXPIRATION = 5 * 60000; // 5 minutes

let sw = null;
let sw_resolve = null;

let next_id = 0;
let download_map = new LruMap(64);

async function initRelay() {
    navigator.serviceWorker.register('/sw.js', { type: 'module' });
    navigator.serviceWorker.addEventListener('controllerchange', updateController);
    navigator.serviceWorker.addEventListener('message', handleMessage);
}

async function waitForServiceWorker() {
    await navigator.serviceWorker.ready;

    sw ??= navigator.serviceWorker.controller;

    if (sw == null) {
        await new Promise((resolve, reject) => {
            sw_resolve = resolve;
            setTimeout(updateController, 10000);
        });

        if (sw == null)
            throw new Error(T.message(`Failed to register service worker. Refresh the page and try again.`));
    }
}

function updateController() {
    if (sw != null && navigator.serviceWorker.controller != sw)
        Async.restart(sw, navigator.serviceWorker.controller);
    sw = navigator.serviceWorker.controller;

    if (sw_resolve != null) {
        sw_resolve();
        sw_resolve = null;
    }
}

function handleMessage(e) {
    let msg = e.data;

    switch (msg.kind) {
        case 'progress': {
            let [id, value, max] = msg.args;
            let status = download_map.get(id);

            if (status == null)
                break;
            if (status.error != null)
                break;

            status.time = performance.now();
            status.meter.add(value, status.time);

            clearTimeout(status.timeout);
            status.timeout = setTimeout(() => handleTimeout(id), STATUS_TIMEOUT);

            if (value == max)
                endDownload(status, null);

            // Try to keep the service worker alive, especially on Firefox!
            sw.postMessage({ kind: 'alive', args: [] });

            if (status.signal != null)
                status.signal(status);
        } break;

        case 'failed': {
            let [id, err] = msg.args;
            let status = download_map.get(id);

            if (status == null)
                break;

            if (err != null) {
                let msg = T.message(err.message); // The SW cannot translate anything

                if (msg != null)
                    err = new Error(msg);
            } else {
                err = new Error(T.message(`The download seems to have been cancelled`));
            }

            endDownload(status, err);

            if (status.signal != null)
                status.signal(status);
        } break;

        default: { Async.handle(msg); } break;
    }
}

function handleTimeout(id) {
    let status = download_map.get(id);

    if (status == null)
        return;

    let err = new Error(T.message(`Download seems to have timed out`));
    endDownload(status, err);

    if (status.signal != null)
        status.signal(status);
}

function endDownload(status, err) {
    status.error ??= err;
    status.busy = false;

    clearTimeout(status.timeout);
}

async function prepareFile(file, key, signal = null) {
    await waitForServiceWorker();

    let id = next_id++;

    let status = {
        kid: file.kid,
        name: file.name,

        time: performance.now(),

        total: file.size,
        meter: new ProgressMeter(file.size),

        busy: true,
        error: null,
        timeout: null,

        signal: signal
    };

    download_map.set(id, status);
    download_map.set(file.kid, status);

    await Async.call(sw, 'file', [id, file, key]);
}

async function prepareZip(drop, keys, signal = null) {
    await waitForServiceWorker();

    let id = next_id++;

    let headers = [];
    let footers = [];
    let offsets = [];
    let offset = 0;

    for (let i = 0; i < drop.files.length; i++) {
        let file = drop.files[i];

        // The CRC-32 will be patched in by the service worker
        let header = createLocalHeader(file.name, drop.ctime);
        let footer = createLocalFooter(file.size, 0);

        headers.push(header);
        footers.push(footer);

        offsets.push(offset);
        offset += header.length + file.size + footer.length;
    }

    let files = drop.files.map(file => ({
        ...file,
        mtime: drop.ctime
    }));
    let directory = createCentralDirectory(offset, files, offsets);

    let total = offset + directory.length;

    let status = {
        kid: drop.kid,
        name: drop.name,

        time: performance.now(),

        total: total,
        meter: new ProgressMeter(total),

        busy: true,
        error: null,
        timeout: null,

        signal: signal
    };

    download_map.set(id, status);
    download_map.set(drop.kid, status);

    let args = [id, drop, total, keys, headers, footers, directory];
    let transferrables = [...headers, ...footers, directory].map(buf => buf.buffer);

    await Async.call(sw, 'zip', args, transferrables);
}

function getDownloadStatus(kid) {
    let status = download_map.get(kid);

    if (status == null)
        return null;
    if (performance.now() >= status.time + STATUS_EXPIRATION) {
        download_map.delete(kid);
        return null;
    }

    return status;
}

function isDownloading() {
    for (let status of download_map.values()) {
        if (status.busy)
            return true;
    }

    return false;
}

export {
    initRelay,

    prepareFile,
    prepareZip,

    getDownloadStatus,
    isDownloading
}
