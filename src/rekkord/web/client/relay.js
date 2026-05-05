// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';

const DAYS = ['monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday'];

let sw = null;

let next_message = 0;
let msg_handlers = new Map;

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
    let handler = msg_handlers.get(msg.id);

    switch (msg.type) {
        case 'success': { handler.resolve(msg.value); } break;
        case 'error': { handler.reject(msg.value); } break;
    }

    msg_handlers.delete(msg.id);
}

export {
    initRelay,
    sendDrop
}
