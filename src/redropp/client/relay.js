// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import * as Async from 'lib/web/base/async.js';
import * as UI from 'lib/web/ui/ui.js';
import * as App from './app.js';
import { ProgressMeter } from './format.js';

const PROGRESS_EXPIRATION = 2 * 60000;

let sw = null;
let sw_resolve = null;

let next_message = 0;
let msg_handlers = new Map;

let progress_map = new Map;

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

            let progress = progress_map.getOrInsertComputed(kid, () => {
                let status = {
                    lock: UI.blockClose(),
                    meter: new ProgressMeter(max)
                };

                return status;
            });

            progress.meter.add(value);

            if (value == max)
                UI.unblockClose(progress.lock);

            App.go();

            // Try to keep the service worker alive, especially on Firefox!
            sw.postMessage({ kind: 'alive', args: [] });
        } break;

        case 'failed': {
            let [kid, err] = msg.args;
            let progress = progress_map.get(kid);

            if (progress != null) {
                UI.unblockClose(progress.lock);
                progress_map.delete(kid);
            }

            Log.error(err);
        } break;

        default: { Async.handle(msg); } break;
    }
}

async function sendDrop(info, key) {
    await Async.call(sw, 'drop', [info, key]);
}

function getProgress(kid) {
    let progress = progress_map.get(kid);

    if (progress == null)
        return null;

    let stat = progress.meter.measure();

    if (performance.now() >= stat.time + PROGRESS_EXPIRATION) {
        progress_map.delete(kid);
        return null;
    }

    return stat;
}

export {
    initRelay,

    sendDrop,
    getProgress
}
