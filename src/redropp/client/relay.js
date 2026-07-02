// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net, ProgressMeter } from 'lib/web/base/base.js';
import * as App from './app.js';

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
    if (sw != null && navigator.serviceWorker.controller != sw) {
        sw = navigator.serviceWorker.controller;
        restartCalls();
    } else {
        sw = navigator.serviceWorker.controller;
    }

    if (sw_resolve != null) {
        sw_resolve();
        sw_resolve = null;
    }
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

            App.go();

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

async function sendDrop(info, key) {
    await callWorker('drop', info, key);
}

async function callWorker(type, ...args) {
    let p = new Promise((resolve, reject) => {
        let id = ++next_message;

        msg_handlers.set(id, {
            type: type,
            args: args,
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

function restartCalls() {
    let handlers = Array.from(msg_handlers.values());

    msg_handlers.clear();

    for (let handler of handlers) {
        let id = ++next_message;

        let msg = {
            id: id,
            type: handler.type,
            args: handler.args
        };

        msg_handlers.set(id, {
            type: handler.type,
            args: handler.args,
            resolve: handler.resolve,
            reject: handler.reject
        });

        sw.postMessage(msg);
    }
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
