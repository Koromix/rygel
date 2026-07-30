// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import fs from 'node:fs';

function determineAbi() {
    let abi = process.arch.toString();

    if (abi == 'riscv32' || abi == 'riscv64') {
        using file = openFile(process.execPath, 'r');

        let header = readElfHeader(file);
        let float_abi = (header.e_flags & 0x6);

        switch (float_abi) {
            case 0: {} break;
            case 2: { abi += 'f'; } break;
            case 4: { abi += 'd'; } break;
            case 6: { abi += 'q'; } break;
        }
    } else if (abi == 'arm') {
        using file = openFile(process.execPath, 'r');

        let header = readElfHeader(file);

        if (header.e_flags & 0x400) {
            abi += 'hf';
        } else if (header.e_flags & 0x200) {
            abi += 'sf';
        } else {
            throw new Error('Unknown ARM floating-point ABI');
        }
    }

    return abi;
}

function readElfHeader(file, offset = 0) {
    let buf = file.read(offset, 512);

    if (buf.length < 16)
        throw new Error('Truncated header');
    if (buf[0] != 0x7F || buf[1] != 69 || buf[2] != 76 || buf[3] != 70)
        throw new Error('Invalid magic number');
    if (buf[6] != 1)
        throw new Error('Invalid ELF version');
    if (buf[5] != 1)
        throw new Error('Big-endian architectures are not supported');

    switch (buf[4]) {
        case 1: { // 32 bit
            if (buf.length < 68)
                throw new Error('Truncated ELF header');

            return {
                ei_class: 32,

                e_machine: buf.readUInt16LE(18),
                e_flags: buf.readUInt32LE(36)
            };
        } break;

        case 2: { // 64 bit
            if (buf.length < 120)
                throw new Error('Truncated ELF header');

            return {
                ei_class: 64,

                e_machine: buf.readUInt16LE(18),
                e_flags: buf.readUInt32LE(48)
            };
        } break;

        default: throw new Error('Invalid ELF class');
    }
}

function openFile(filename, flags) {
    let fd = fs.openSync(filename, flags);
    return new FileHandle(fd);
}

class FileHandle {
    constructor(fd) {
        this.fd = fd;
    }

    close() { fs.closeSync(this.fd); }
    [Symbol.dispose]() { fs.closeSync(this.fd); }

    read(offset, len) {
        let buf = Buffer.allocUnsafe(len);
        let read = fs.readSync(this.fd, buf, offset, len);

        return buf.subarray(0, read);
    }
}

export { determineAbi }
