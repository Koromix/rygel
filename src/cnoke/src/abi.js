// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import fs from 'fs';

function determineAbi() {
    let abi = process.arch;

    if (abi == 'riscv32' || abi == 'riscv64') {
        let buf = readFileHeader(process.execPath, 512);
        let header = decodeElfHeader(buf);
        let float_abi = (header.e_flags & 0x6);

        switch (float_abi) {
            case 0: {} break;
            case 2: { abi += 'f'; } break;
            case 4: { abi += 'd'; } break;
            case 6: { abi += 'q'; } break;
        }
    } else if (abi == 'arm') {
        let buf = readFileHeader(process.execPath, 512);
        let header = decodeElfHeader(buf);

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

function readFileHeader(filename, read) {
    let fd = null;

    try {
        let fd = fs.openSync(filename);

        let buf = Buffer.allocUnsafe(read);
        let len = fs.readSync(fd, buf);

        return buf.subarray(0, len);
    } finally {
        if (fd != null)
            fs.closeSync(fd);
    }
}

function decodeElfHeader(buf) {
    let header = {};

    if (buf.length < 16)
        throw new Error('Truncated header');
    if (buf[0] != 0x7F || buf[1] != 69 || buf[2] != 76 || buf[3] != 70)
        throw new Error('Invalid magic number');
    if (buf[6] != 1)
        throw new Error('Invalid ELF version');
    if (buf[5] != 1)
        throw new Error('Big-endian architectures are not supported');

    header.e_machine = buf.readUInt16LE(18);

    switch (buf[4]) {
        case 1: { // 32 bit
            buf = buf.subarray(0, 68);
            if (buf.length < 68)
                throw new Error('Truncated ELF header');

            header.ei_class = 32;
            header.e_flags = buf.readUInt32LE(36);
        } break;
        case 2: { // 64 bit
            buf = buf.subarray(0, 120);
            if (buf.length < 120)
                throw new Error('Truncated ELF header');

            header.ei_class = 64;
            header.e_flags = buf.readUInt32LE(48);
        } break;
        default: throw new Error('Invalid ELF class');
    }

    return header;
}

export { determineAbi }
