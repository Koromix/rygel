// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net, HttpError } from 'lib/web/base/base.js';

let upload_controller = null;

onmessage = handleMessage;

function handleMessage(e) {
    let msg = e.data;

    switch (msg.type) {
        case 'download': { wrapAsync(msg.id, downloadVault, msg.args); } break;
        case 'upload': { wrapAsync(msg.id, uploadVault, msg.args); } break;
    }
}

async function wrapAsync(id, func, args) {
    try {
        let ret = await func(...args);
        self.postMessage({ id: id, type: 'success', value: ret });
    } catch (err) {
        self.postMessage({ id: id, type: 'error', value: err });
    }
}

async function downloadVault(vid) {
    let response = await Net.fetch('/api/download', {
        headers: {
            'X-Vault-Id': vid
        }
    });

    if (!response.ok) {
        // First time, it's ok!
        if (response.status == 204)
            return;

        let msg = await Net.readError(response);
        throw new HttpError(response.status, msg);
    }

    let filename = 'ludivine/' + vid + '.db';
    let buf = await response.arrayBuffer();

    let root = await navigator.storage.getDirectory();
    let handle = await findFile(root, filename, true);
    let sync = await handle.createSyncAccessHandle();

    sync.write(buf);
    sync.truncate(buf.byteLength);
    sync.close();

    let header = response.headers.get('X-Vault-Generation');
    let generation = parseInt(header, 10) || null;

    return generation;
}

async function uploadVault(vid, prev) {
    let filename = 'ludivine/' + vid + '.db';

    let root = await navigator.storage.getDirectory();
    let handle = await findFile(root, filename);
    let src = await handle.getFile();
    let body = await src.arrayBuffer();

    let controller = new AbortController;

    if (upload_controller != null)
        upload_controller.abort();
    upload_controller = controller;

    let response = await Net.fetch('/api/upload', {
        method: 'PUT',
        headers: {
            'X-Vault-Id': vid,
            'X-Vault-Generation': prev
        },
        body: body,
        signal: controller.signal
    });

    let header = response.headers.get('X-Vault-Generation');
    let generation = parseInt(header, 10);

    if (upload_controller == controller)
        upload_controller = null;

    return generation;
}

async function findFile(root, filename, create = false) {
    let handle = root;

    let parts = filename.split('/');
    let basename = parts.pop();

    for (let part of parts)
        handle = await handle.getDirectoryHandle(part, { create: true });
    handle = await handle.getFileHandle(basename, { create: create });

    return handle;
}
