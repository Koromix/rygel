// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import * as Async from 'lib/web/base/async.js';
import * as UI from 'lib/web/ui/ui.js';
import * as App from './app.js';
import { ProgressMeter } from './format.js';

const STATUS_EXPIRATION = 5 * 60000; // 5 minutes

let sw = null;
let sw_resolve = null;

let next_message = 0;
let msg_handlers = new Map;

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
            let [kid, value, max] = msg.args;
            let status = status_map.get(kid);

            if (status == null)
                break;
            if (status.error != null)
                break;

            status.time = performance.now();
            status.meter.add(value, status.time);

            if (value == max) {
                UI.unblockClose(status.lock);
            } else if (status.lock == null) {
                status.lock = UI.blockClose();
            }

            App.go();

            // Try to keep the service worker alive, especially on Firefox!
            sw.postMessage({ kind: 'alive', args: [] });
        } break;

        case 'failed': {
            let [kid, err] = msg.args;
            let status = status_map.get(kid);

            if (err == null)
                err = new Error(T.message(`The download seems to have been cancelled`));

            if (status != null) {
                status.error = err;
                UI.unblockClose(status.lock);
            }

            Log.error(err);

            App.go();
        } break;

        default: { Async.handle(msg); } break;
    }
}

async function prepareDownload(info, key) {
    status_map.set(info.kid, {
        time: performance.now(),
        meter: new ProgressMeter(info.size),
        error: null,
        lock: null
    });

    await Async.call(sw, 'drop', [info, key]);
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
