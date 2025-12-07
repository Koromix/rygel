// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const MAX_TEMP_SIZE = 64 * 1024 * 1024;

let capi = null;
let wasm = null;
let io = null;
let vfs = null;

let files = new Map;
let handles = new Map;

const IO = {
    xCheckReservedLock: function (pFile, pOut) {
        wasm.poke(pOut, 0, 'i32');
        return 0;
    },

    xClose: function (pFile) {
        let fh = handles.get(pFile);

        if (fh != null) {
            handles.delete(pFile);

            if (fh.sq3 != null)
                fh.sq3.dispose();
        }

        return 0;
    },

    xDeviceCharacteristics: function (pFile) {
        return capi.SQLITE_IOCAP_UNDELETABLE_WHEN_OPEN;
    },

    xFileControl: function (pFile, opId, pArg) {
        return capi.SQLITE_NOTFOUND;
    },

    xFileSize: function (pFile, pSz64) {
        let fh = handles.get(pFile);
        let file = fh.file;

        let size = file.u8.byteLength;
        wasm.poke(pSz64, size, 'i64');

        return 0;
    },

    xRead: function (pFile, pDest, n, offset64) {
        let heap = wasm.heap8u();

        let fh = handles.get(pFile);
        let file = fh.file;

        let offset = Number(offset64);
        let dest = Number(pDest);

        let from = file.u8.subarray(offset, offset + n);

        heap.set(from, dest);

        if (from.byteLength < n) {
            heap.fill(0, dest + from.byteLength, dest + n);
            return capi.SQLITE_IOERR_SHORT_READ;
        }

        return 0;
    },

    xSync: function (pFile, flags) {
        return 0;
    },

    xTruncate: function (pFile, sz64) {
        let fh = handles.get(pFile);
        let file = fh.file;

        let size = Number(sz64);

        try {
            // Can't shrink SharedArrayBuffer :(
            if (size > file.buf.byteLength)
                file.buf.grow(size);
            file.u8 = new Uint8Array(file.buf, 0, size);
        } catch (err) {
            console.error(err);
            return capi.SQLITE_IOERR_TRUNCATE;
        }

        return 0;
    },

    xLock: function (pFile, lockType) {
        return 0;
    },

    xUnlock: function (pFile, lockType) {
        return 0;
    },

    xWrite: function (pFile, pSrc, n, offset64) {
        let heap = wasm.heap8u();

        let fh = handles.get(pFile);
        let file = fh.file;

        let offset = Number(offset64);
        let src = Number(pSrc);

        if (offset + n > file.buf.byteLength) {
            try {
                file.buf.grow(offset + n);
                file.u8 = new Uint8Array(file.buf);
            } catch (err) {
                console.error(err);
                return capi.SQLITE_IOERR_WRITE;
            }
        }

        file.u8.set(heap.subarray(src, src + n), offset);

        return 0;
    }
};

const VFS = {
    xAccess: function (pVfs, zName, flags, pOut) {
        let name = wasm.cstrToJs(zName);
        let exists = files.has(name);

        wasm.poke(pOut, exists ? 1 : 0, 'i32');

        return 0;
    },

    xCurrentTime: function (pVfs, pOut) {
        let time = 2440587.5 + (new Date().getTime() / 86400000);
        wasm.poke(pOut, time, 'double');

        return 0;
    },

    xCurrentTimeInt64: function (pVfs, pOut) {
        let time = 2440587.5 + (new Date().getTime() / 86400000);
        wasm.poke(pOut, time, 'i64');

        return 0;
    },

    xDelete: function (pVfs, zName, doSyncDir) {
        let name = wasm.cstrToJs(zName);
        files.delete(name);

        return 0;
    },

    xFullPathname: function (pVfs, zName, nOut, pOut) {
        let i = wasm.cstrncpy(pOut, zName, nOut);
        return i < nOut ? 0 : capi.SQLITE_CANTOPEN;
    },

    xGetLastError: function (pVfs, nOut, pOut) {
        console.warn('xGetLastError() has nothing sensible to return');
        return 0;
    },

    xOpen: function (pVfs, zName, pFile, flags, pOutFlags) {
        let name = wasm.cstrToJs(zName);
        let file = files.get(name);

        if (file == null) {
            if (!(flags & capi.SQLITE_OPEN_CREATE))
                return capi.SQLITE_NOTFOUND;

            let sab = new SharedArrayBuffer(0, { maxByteLength: MAX_TEMP_SIZE });

            file = {
                name: name,
                buf: sab,
                u8: new Uint8Array(sab)
            };
            files.set(name, file);
        }

        let fh = {
            file: file,
            sq3: new capi.sqlite3_file(pFile)
        };
        fh.sq3.$pMethods = io.pointer;

        wasm.poke(pOutFlags, flags, 'i32');

        handles.set(pFile, fh);

        return 0;
    }
};

function install(sqlite3) {
    capi = sqlite3.capi;
    wasm = sqlite3.wasm;

    io = new capi.sqlite3_io_methods();
    vfs = new capi.sqlite3_vfs();

    io.$iVersion = 1;
    vfs.$iVersion = 2;
    vfs.$szOsFile = capi.sqlite3_file.structInfo.sizeof;
    vfs.$mxPathname = 1024;
    vfs.$zName = wasm.allocCString('sabfs');

    sqlite3.vfs.installVfs({ io: { struct: io, methods: IO } });
    sqlite3.vfs.installVfs({ vfs: { struct: vfs, methods: VFS } });
}

function write(name, sab) {
    if (!(sab instanceof SharedArrayBuffer))
        throw new Error(`Unexpected value for file data (expected SharedArrayBuffer)`);
    if (!sab.growable)
        throw new Error('Cannot import non-growable SharedArrayBuffer');

    if (files.has(name))
        throw new Error(`Cannot replace ABFS file '${name}'`);

    let file = {
        name: name,
        buf: sab,
        u8: new Uint8Array(sab)
    };

    files.set(name, file);
}

export {
    install,
    write
}
