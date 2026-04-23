// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import fs from 'fs';

function determineArch() {
    let arch = process.arch;

    if (arch == 'riscv32' || arch == 'riscv64') {
        let buf = readFileHeader(process.execPath, 512);
        let header = decodeElfHeader(buf);
        let float_abi = (header.e_flags & 0x6);

        switch (float_abi) {
            case 0: {} break;
            case 2: { arch += 'f'; } break;
            case 4: { arch += 'd'; } break;
            case 6: { arch += 'q'; } break;
        }
    } else if (arch == 'arm') {
        let buf = readFileHeader(process.execPath, 512);
        let header = decodeElfHeader(buf);

        if (header.e_flags & 0x400) {
            arch += 'hf';
        } else if (header.e_flags & 0x200) {
            arch += 'sf';
        } else {
            throw new Error('Unknown ARM floating-point ABI');
        }
    }

    return arch;
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

    let machine = buf.readUInt16LE(18);

    switch (machine) {
        case 3: { header.e_machine = 'ia32'; } break;
        case 40: { header.e_machine = 'arm'; } break;
        case 62: { header.e_machine = 'amd64'; } break;
        case 183: { header.e_machine = 'arm64'; } break;
        case 243: {
            switch (buf[4]) {
                case 1: { header.e_machine = 'riscv32'; } break;
                case 2: { header.e_machine = 'riscv64'; } break;
            }
        } break;
        case 248: {
            switch (buf[4]) {
                case 1: { header.e_machine = 'loong32'; } break;
                case 2: { header.e_machine = 'loong64'; } break;
            }
        } break;
        default: throw new Error('Unknown ELF machine type');
    }

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

export { determineArch }
