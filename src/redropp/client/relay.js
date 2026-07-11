// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import * as Async from 'lib/web/base/async.js';
import * as UI from 'lib/web/ui/ui.js';
import { ProgressMeter } from './format.js';

const STATUS_TIMEOUT = 40000; // 40 seconds
const STATUS_EXPIRATION = 5 * 60000; // 5 minutes

let sw = null;
let sw_resolve = null;

let next_id = 0;
let status_map = new Map;

async function initRelay() {
    navigator.serviceWorker.register('/sw.js');
    navigator.serviceWorker.addEventListener('controllerchange', updateController);
    navigator.serviceWorker.addEventListener('message', handleMessage);

    await navigator.serviceWorker.ready;

    sw = navigator.serviceWorker.controller;

    if (sw == null) {
        await new Promise((resolve, reject) => {
            sw_resolve = resolve;
            setTimeout(updateController, 10000);
        });

        if (sw == null)
            throw new Error('Failed to register service worker');
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
            let status = status_map.get(id);

            if (status == null)
                break;
            if (status.error != null)
                break;

            status.time = performance.now();
            status.meter.add(value, status.time);

            clearTimeout(status.timeout);
            status.timeout = setTimeout(() => handleTimeout(id), STATUS_TIMEOUT);

            if (value == max) {
                endDownload(status, null);
            } else if (status.lock == null) {
                status.lock = UI.blockClose();
            }

            // Try to keep the service worker alive, especially on Firefox!
            sw.postMessage({ kind: 'alive', args: [] });

            if (status.signal != null)
                status.signal();
        } break;

        case 'failed': {
            let [id, err] = msg.args;
            let status = status_map.get(id);

            if (status == null)
                break;

            if (err == null)
                err = new Error(T.message(`The download seems to have been cancelled`));

            endDownload(status, err);

            Log.error(err);

            if (status.signal != null)
                status.signal();
        } break;

        default: { Async.handle(msg); } break;
    }
}

function handleTimeout(id) {
    let status = status_map.get(id);

    if (status == null)
        return;

    let err = new Error(T.message(`Download seems to have timed out`));
    endDownload(status, err);

    if (status.signal != null)
        status.signal();
}

function endDownload(status, err) {
    status.error ??= err;
    status.busy = false;

    clearTimeout(status.timeout);
    UI.unblockClose(status.lock);
}

async function prepareDownload(info, key, signal = null) {
    let id = next_id++;

    let status = {
        kid: info.kid,

        time: performance.now(),
        meter: new ProgressMeter(info.size),
        busy: true,
        error: null,
        timeout: null,
        lock: null,

        signal: signal
    };

    // Clear existing status entry
    {
        let prev = status_map.get(info.kid);

        if (prev != null)
            status_map.delete(prev.kid);
    }

    status_map.set(id, status);
    status_map.set(info.kid, status);

    await Async.call(sw, 'drop', [id, info, key]);
}

function getDownloadStatus(kid) {
    let status = status_map.get(kid);

    if (status == null)
        return null;
    if (performance.now() >= status.time + STATUS_EXPIRATION) {
        status_map.delete(kid);
        return null;
    }

    return status;
}

export {
    initRelay,

    prepareDownload,
    getDownloadStatus
}
