// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net, ProgressMeter } from 'lib/web/base/base.js';
import * as App from './m_main.js';

const PROGRESS_EXPIRATION = 2 * 60000;

let next_message = 0;
let msg_handlers = new Map;

let progress_map = new Map;

async function initRelay() {
    navigator.serviceWorker.register('/sw.js');
    navigator.serviceWorker.addEventListener('message', handleMessage);

    await navigator.serviceWorker.ready;
}

async function sendDrop(info, key) {
    await callWorker('drop', info, key);
}

async function callWorker(type, ...args) {
    let p = new Promise((resolve, reject) => {
        let id = ++next_message;

        msg_handlers.set(id, {
            resolve: resolve,
            reject: reject
        });

        let msg = {
            id: id,
            type: type,
            args: args
        };

        let sw = navigator.serviceWorker.controller;
        sw.postMessage(msg);
    });

    let ret = await p;
    return ret;
}

function handleMessage(e) {
    let msg = e.data;

    switch (msg.type) {
        case 'return': {
            let [id, success, value] = msg.args;
            let handler = msg_handlers.get(id);

            if (success) {
                handler.resolve(value);
            } else {
                handler.reject(value);
            }

            msg_handlers.delete(msg.id);
        } break;

        case 'progress': {
            let [kid, value, max] = msg.args;

            let meter = progress_map.getOrInsertComputed(kid, () => new ProgressMeter(max));
            meter.add(value);

            App.run();

            // Try to keep the service worker alive, especially on Firefox!
            let sw = navigator.serviceWorker.controller;
            sw.postMessage({ type: 'alive', args: [] });
        } break;

        case 'error': {
            let [err] = msg.args;
            Log.error(err);
        } break;
    }

    msg_handlers.delete(msg.id);
}

function getProgress(kid) {
    let meter = progress_map.get(kid);

    if (meter == null)
        return null;

    let stat = meter.measure();

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
