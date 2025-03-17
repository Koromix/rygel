// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

onmessage = handleMessage;

function handleMessage(e) {
    let msg = e.data;

    switch (msg.type) {
        case 'replace': { wrapAsync(msg.id, replaceFile, msg.args); } break;
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

async function replaceFile(filename, buf) {
    let root = await navigator.storage.getDirectory();
    let handle = await findFile(root, filename, true);
    let sync = await handle.createSyncAccessHandle();

    sync.write(buf);
    sync.truncate(buf.byteLength);
    sync.close();
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
