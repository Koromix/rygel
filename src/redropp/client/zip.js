// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

function createLocalHeader(name, mtime = null) {
    if (typeof name == 'string')
        name = (new TextEncoder).encode(name);

    let buf = new Uint8Array(30 + name.length + 20);
    let view = new DataView(buf.buffer);
    let p = 0;

    let [day, time] = decomposeTime(mtime ?? 0);

    view.setUint32(p, 0x04034B50, true); p += 4;
    view.setUint16(p, 45, true); p += 2;
    view.setUint16(p, 0x0008, true); p += 2;
    view.setUint16(p, 0, true); p += 2;
    view.setUint16(p, time, true); p += 2;
    view.setUint16(p, day, true); p += 2;
    view.setUint32(p, 0, true); p += 4;
    view.setUint32(p, 0xFFFFFFFF, true); p += 4;
    view.setUint32(p, 0xFFFFFFFF, true); p += 4;
    view.setUint16(p, name.length, true); p += 2;
    view.setUint16(p, 20, true); p += 2;
    buf.set(name, p); p += name.length;
    view.setUint16(p, 0x0001, true); p += 2;
    view.setUint16(p, 16, true); p += 2;
    view.setBigUint64(p, 0n, true); p += 8;
    view.setBigUint64(p, 0n, true); p += 8;

    return buf;
}

function createLocalFooter(size, crc32) {
    size = BigInt(size);

    let buf = new Uint8Array(20);
    let view = new DataView(buf.buffer);

    view.setUint32(0, crc32, true);
    view.setBigUint64(4, size, true);
    view.setBigUint64(12, size, true);

    return buf;
}

function createCentralDirectory(offset, files, offsets) {
    offset = BigInt(offset);

    let encoder = new TextEncoder;
    let names = files.map(f => encoder.encode(f.name));
    let total = names.reduce((acc, name) => acc + 46 + name.length + 28, 0) + 56 + 20 + 22;

    let buf = new Uint8Array(total);
    let view = new DataView(buf.buffer);
    let p = 0;

    for (let i = 0; i < files.length; i++) {
        let file = files[i];
        let name = names[i];
        let size = BigInt(file.size);
        let offset = BigInt(offsets[i]);

        let [day, time] = decomposeTime(file.mtime ?? 0);

        view.setUint32(p, 0x02014B50, true); p += 4;
        view.setUint16(p, 45, true); p += 2;
        view.setUint16(p, 45, true); p += 2;
        view.setUint16(p, 0x0008, true); p += 2;
        view.setUint16(p, 0, true); p += 2;
        view.setUint16(p, time, true); p += 2;
        view.setUint16(p, day, true); p += 2;
        view.setUint32(p, file.crc32, true); p += 4;
        view.setUint32(p, 0xFFFFFFFF, true); p += 4;
        view.setUint32(p, 0xFFFFFFFF, true); p += 4;
        view.setUint16(p, name.length, true); p += 2;
        view.setUint16(p, 28, true); p += 2;
        view.setUint16(p, 0, true); p += 2;
        view.setUint16(p, 0, true); p += 2;
        view.setUint16(p, 0, true); p += 2;
        view.setUint32(p, 0, true); p += 4;
        view.setUint32(p, 0xFFFFFFFF, true); p += 4;
        buf.set(name, p); p += name.length;
        view.setUint16(p, 0x0001, true); p += 2;
        view.setUint16(p, 24, true); p += 2;
        view.setBigUint64(p, size, true); p += 8;
        view.setBigUint64(p, size, true); p += 8;
        view.setBigUint64(p, offset, true); p += 8;
    }

    let cd_size = BigInt(p);
    let cd_offset = offset + cd_size;

    view.setUint32(p, 0x06064B50, true); p += 4;
    view.setBigUint64(p, 44n, true); p += 8;
    view.setUint16(p, 45, true); p += 2;
    view.setUint16(p, 45, true); p += 2;
    view.setUint32(p, 0, true); p += 4;
    view.setUint32(p, 0, true); p += 4;
    view.setBigUint64(p, BigInt(files.length), true); p += 8;
    view.setBigUint64(p, BigInt(files.length), true); p += 8;
    view.setBigUint64(p, cd_size, true); p += 8;
    view.setBigUint64(p, offset, true); p += 8;

    view.setUint32(p, 0x07064B50, true); p += 4;
    view.setUint32(p, 0, true); p += 4;
    view.setBigUint64(p, cd_offset, true); p += 8;
    view.setUint32(p, 1, true); p += 4;

    view.setUint32(p, 0x06054B50, true); p += 4;
    view.setUint16(p, 0xFFFF, true); p += 2;
    view.setUint16(p, 0xFFFF, true); p += 2;
    view.setUint16(p, 0xFFFF, true); p += 2;
    view.setUint16(p, 0xFFFF, true); p += 2;
    view.setUint32(p, 0xFFFFFFFF, true); p += 4;
    view.setUint32(p, 0xFFFFFFFF, true); p += 4;
    view.setUint16(p, 0, true); p += 2;

    return buf;
}

function decomposeTime(ms) {
    let date = new Date(ms);

    let day = 0;
    let time = 0;

    day = date.getUTCFullYear() - 1980;
    day = day << 4;
    day = day | (date.getUTCMonth() + 1);
    day = day << 5;
    day = day | date.getUTCDate();

    time = date.getHours();
    time = time << 6;
    time = time | date.getMinutes();
    time = time << 5;
    time = time | date.getSeconds() / 2;

    return [day, time];
}

function patchFooterCrc32(buf, offset, crc32) {
    let view = new DataView(buf.buffer, buf.byteOffset + offset);
    view.setUint32(0, crc32, true);
}

function patchCentralCrc32(buf, offset, crc32) {
    let view = new DataView(buf.buffer, buf.byteOffset + offset);
    view.setUint32(16, crc32, true);
}

function skipCentralHeader(buf, offset) {
    let view = new DataView(buf.buffer, buf.byteOffset + offset);
    let name_len = view.getUint16(28, true);

    return 46 + name_len + 28;
}

export {
    createLocalHeader,
    createLocalFooter,
    createCentralDirectory,

    patchFooterCrc32,
    patchCentralCrc32,
    skipCentralHeader
}
