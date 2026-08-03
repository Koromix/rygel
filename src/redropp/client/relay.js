// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, LruMap, Log, Net } from 'lib/web/base/base.js';
import * as Async from 'lib/web/base/async.js';
import * as UI from 'lib/web/ui/ui.js';
import { ProgressMeter } from './format.js';

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
            status.downloaded = value;
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

async function prepareDownload(file, key, signal = null) {
    await waitForServiceWorker();

    let id = next_id++;

    let status = {
        kid: file.kid,
        name: file.name,

        time: performance.now(),

        downloaded: 0,
        meter: new ProgressMeter(file.size),
        busy: true,
        error: null,
        timeout: null,

        signal: signal
    };

    // Clear existing status entry
    {
        let prev = download_map.get(file.kid);

        if (prev != null)
            download_map.delete(prev.kid);
    }

    download_map.set(id, status);
    download_map.set(file.kid, status);

    await Async.call(sw, 'prepare', [id, file, key]);
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

export {
    initRelay,

    prepareDownload,
    getDownloadStatus
}
