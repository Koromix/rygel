// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import fs from 'node:fs';
import path from 'node:path';

function determineAbi() {
    let abi = process.arch.toString();

    if (abi == 'riscv32' || abi == 'riscv64') {
        let file = openFile(process.execPath, 'r');

        try {
            let header = readElfHeader(file);
            let float_abi = (header.e_flags & 0x6);

            switch (float_abi) {
                case 0: {} break;
                case 2: { abi += 'f'; } break;
                case 4: { abi += 'd'; } break;
                case 6: { abi += 'q'; } break;
            }
        } finally {
            file.close();
        }
    } else if (abi == 'arm') {
        let file = openFile(process.execPath, 'r');

        try {
            let header = readElfHeader(file);

            if (header.e_flags & 0x400) {
                abi += 'hf';
            } else if (header.e_flags & 0x200) {
                abi += 'sf';
            } else {
                throw new Error('Unknown ARM floating-point ABI');
            }
        } finally {
            file.close();
        }
    }

    return abi;
}

function determineLibc() {
    if (process.platform != 'linux')
        throw new Error('ELF libc detection only works on Linux');

    let file = openFile(process.execPath, 'r');

    let interp = null;

    try {
        let header = readElfHeader(file);

        if (!header.e_phoff)
            throw new Error('Cannot find program headers in process binary');

        switch (header.ei_class) {
            case 32: { interp = findInterpreter32(file, header); } break;
            case 64: { interp = findInterpreter64(file, header); } break;

            default: throw new Error('Unsupported ELF machine class');
        }
    } finally {
        file.close();
    }

    let basename = path.basename(interp);
    let libc = basename.startsWith('ld-musl-') ? 'musl': 'glibc';

    return libc;
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
                e_flags: buf.readUInt32LE(36),

                e_phoff: buf.readUInt32LE(28),
                e_phentsize: buf.readUInt16LE(42),
                e_phnum: buf.readUInt16LE(44)
            };
        } break;

        case 2: { // 64 bit
            if (buf.length < 120)
                throw new Error('Truncated ELF header');

            return {
                ei_class: 64,

                e_machine: buf.readUInt16LE(18),
                e_flags: buf.readUInt32LE(48),

                e_phoff: buf.readBigUInt64LE(32),
                e_phentsize: buf.readUInt16LE(54),
                e_phnum: buf.readUInt16LE(56)
            };
        } break;

        default: throw new Error('Invalid ELF class');
    }
}

function findInterpreter32(file, header) {
    if (header.e_phentsize != 32)
        throw new Error('Unsupport ELF program header format');

    let expected = header.e_phnum * header.e_phentsize;
    let buf = file.read(header.e_phoff, expected);

    if (buf.length != expected)
        throw new Error('Truncated ELF program headers');

    let interp = null;

    for (let offset = 0; offset < expected; offset += header.e_phentsize) {
        let p_type = buf.readUInt32LE(offset + 0);

        if (p_type == 3) {
            let p_offset = buf.readUInt32LE(offset + 4);
            let p_filesz = buf.readUInt32LE(offset + 16);

            let bytes = file.read(p_offset, p_filesz);

            if (!bytes.length || bytes.length != p_filesz || bytes[bytes.length - 1] != 0)
                throw new Error('Truncated PT_INTERP value');
            bytes = bytes.subarray(0, bytes.length - 1);

            interp = bytes.toString('ascii');
            break;
        }
    }

    if (interp == null)
        throw new Error('Failed to find PT_INTERP program header');

    return interp;
}

function findInterpreter64(file, header) {
    if (header.e_phentsize != 56)
        throw new Error('Unsupport ELF program header format');

    let expected = header.e_phnum * header.e_phentsize;
    let buf = file.read(header.e_phoff, expected);

    if (buf.length != expected)
        throw new Error('Truncated ELF program headers');

    let interp = null;

    for (let offset = 0; offset < expected; offset += header.e_phentsize) {
        let p_type = buf.readUInt32LE(offset + 0);

        if (p_type == 3) {
            let p_offset = buf.readBigUInt64LE(offset + 8);
            let p_filesz = Number(buf.readBigUInt64LE(offset + 32));

            let bytes = file.read(p_offset, p_filesz);

            if (!bytes.length || bytes.length != p_filesz || bytes[bytes.length - 1] != 0)
                throw new Error('Truncated PT_INTERP value');
            bytes = bytes.subarray(0, bytes.length - 1);

            interp = bytes.toString('ascii');
            break;
        }
    }

    if (interp == null)
        throw new Error('Failed to find PT_INTERP program header');

    return interp;
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
        let read = fs.readSync(this.fd, buf, 0, len, offset);

        return buf.subarray(0, read);
    }
}

export {
    determineAbi,
    determineLibc
}
