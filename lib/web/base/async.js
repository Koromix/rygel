// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

let next_message = 0;
let msg_handlers = new Map;

async function call(worker, kind, args, transfers = null) {
    let p = new Promise((resolve, reject) => {
        let id = ++next_message;

        msg_handlers.set(id, {
            worker: worker,

            kind: kind,
            args: args,
            resolve: resolve,
            reject: reject
        });

        let msg = {
            id: id,
            kind: kind,
            args: args
        };

        worker.postMessage(msg, transfers);
    });

    let ret = await p;
    return ret;
}

async function wrap(worker, msg, func, transfer = false) {
    try {
        let ret = await func(...msg.args);
        let transfers = transfer ? [ret] : null;

        worker.postMessage({ kind: 'success', id: msg.id, value: ret }, transfers);
    } catch (err) {
        worker.postMessage({ kind: 'error', id: msg.id, value: err });
    }
}

async function handle(msg) {
    if (msg instanceof MessageEvent)
        msg = msg.data;

    switch (msg.kind) {
        case 'success': {
            let handler = msg_handlers.get(msg.id);
            handler.resolve(msg.value);
            msg_handlers.delete(msg.id);
        } break;

        case 'error': {
            let handler = msg_handlers.get(msg.id);
            handler.reject(msg.value);
            msg_handlers.delete(msg.id);
        } break;
    }
}

function restart(prev, worker) {
    let new_handlers = new Map;

    for (let [id, handler] of msg_handlers.entries()) {
        if (handler.worker == prev) {
            id = ++next_message;

            let msg = {
                id: id,
                kind: handler.kind,
                args: handler.args
            };

            new_handlers.set(id, {
                worker: worker,

                kind: handler.kind,
                args: handler.args,
                resolve: handler.resolve,
                reject: handler.reject
            });

            worker.postMessage(msg);
        } else {
            new_handlers.set(id, handler);
        }
    }

    msg_handlers = new_handlers;
}

export {
    call,
    wrap,
    handle,

    restart
}
