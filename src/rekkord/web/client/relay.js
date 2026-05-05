// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';
import * as App from './main.js';

const PROGRESS_EXPIRATION = 2 * 60000;

let sw = null;

let next_message = 0;
let msg_handlers = new Map;

let progress_map = new Map;

async function initRelay() {
    navigator.serviceWorker.register('/sw.js');
    navigator.serviceWorker.addEventListener('message', handleMessage);

    sw = await navigator.serviceWorker.ready.then(registration => registration.active);
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

            progress_map.set(kid, {
                time: performance.now(),
                value: value,
                max: max
            });

            App.run();

            // Try to keep the service worker alive, especially on Firefox!
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
    let info = progress_map.get(kid);

    if (info == null)
        return null;
    if (performance.now() >= info.time + PROGRESS_EXPIRATION) {
        progress_map.delete(kid);
        return null;
    }

    let progress = {
        value: info.value,
        max: info.max
    };

    return progress;
}

export {
    initRelay,

    sendDrop,
    getProgress
}
