// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import crypto from 'crypto';
import fs from 'fs';
import http from 'https';
import path from 'path';
import zlib from 'zlib';

async function downloadHttp(url, dest) {
    console.log('>> Downloading ' + url);

    let [tmp_name, file] = openTemporaryStream(dest);

    try {
        await new Promise((resolve, reject) => {
            let request = http.get(url, response => {
                if (response.statusCode != 200) {
                    let err = new Error(`Download failed: ${response.statusMessage} [${response.statusCode}]`);
                    err.code = response.statusCode;

                    reject(err);

                    return;
                }

                response.pipe(file);

                file.on('finish', () => file.close(() => {
                    try {
                        fs.renameSync(file.path, dest);
                    } catch (err) {
                        if (!fs.existsSync(dest))
                            reject(err);
                    }

                    resolve();
                }));
            });

            request.on('error', reject);
            file.on('error', reject);
        });
    } catch (err) {
        file.close();

        try {
            fs.unlinkSync(tmp_name);
        } catch (err) {
            if (err.code != 'ENOENT')
                throw err;
        }

        throw err;
    }
}

function openTemporaryStream(prefix) {
    let buf = Buffer.allocUnsafe(4);

    for (;;) {
        try {
            crypto.randomFillSync(buf);

            let suffix = buf.toString('hex').padStart(8, '0');
            let filename = `${prefix}.${suffix}`;

            let file = fs.createWriteStream(filename, { flags: 'wx', mode: 0o644 });
            return [filename, file];
        } catch (err) {
            if (err.code != 'EEXIST')
                throw err;
        }
    }
}

function extractTarGz(filename, dest_dir, strip = 0) {
    let reader = fs.createReadStream(filename).pipe(zlib.createGunzip());

    return new Promise((resolve, reject) => {
        let header = null;
        let extended = {};

        reader.on('readable', () => {
            try {
                for (;;) {
                    if (header == null) {
                        let buf = reader.read(512);
                        if (buf == null)
                            break;

                        // Two zeroed 512-byte blocks end the stream
                        if (!buf[0])
                            continue;

                        header = {
                            filename: buf.toString('utf-8', 0, 100).replace(/\0/g, ''),
                            mode: parseInt(buf.toString('ascii', 100, 109), 8),
                            size: parseInt(buf.toString('ascii', 124, 137), 8),
                            type: String.fromCharCode(buf[156])
                        };

                        // UStar filename prefix
                        /*if (buf.toString('ascii', 257, 263) == 'ustar\0') {
                            let prefix = buf.toString('utf-8', 345, 500).replace(/\0/g, '');
                            console.log(prefix);
                            header.filename = prefix ? (prefix + '/' + header.filename) : header.filename;
                        }*/

                        Object.assign(header, extended);
                        extended = {};

                        // Safety checks
                        header.filename = header.filename.replace(/\\/g, '/');
                        if (!header.filename.length)
                            throw new Error(`Insecure empty filename inside TAR archive`);
                        if (pathIsAbsolute(header.filename[0]))
                            throw new Error(`Insecure filename starting with / inside TAR archive`);
                        if (pathHasDotDot(header.filename))
                            throw new Error(`Insecure filename containing '..' inside TAR archive`);

                        for (let i = 0; i < strip; i++)
                            header.filename = header.filename.substr(header.filename.indexOf('/') + 1);
                    }

                    let aligned = Math.floor((header.size + 511) / 512) * 512;
                    let data = header.size ? reader.read(aligned) : null;
                    if (data == null) {
                        if (header.size)
                            break;
                        data = Buffer.alloc(0);
                    }
                    data = data.subarray(0, header.size);

                    if (header.type == '0' || header.type == '7') {
                        let filename = dest_dir + '/' + header.filename;
                        let dirname = path.dirname(filename);

                        fs.mkdirSync(dirname, { recursive: true, mode: 0o755 });
                        fs.writeFileSync(filename, data, { mode: header.mode });
                    } else if (header.type == '5') {
                        let filename = dest_dir + '/' + header.filename;
                        fs.mkdirSync(filename, { recursive: true, mode: header.mode });
                    } else if (header.type == 'L') { // GNU tar
                        extended.filename = data.toString('utf-8').replace(/\0/g, '');
                    } else if (header.type == 'x') { // PAX entry
                        let str = data.toString('utf-8');

                        try {
                            while (str.length) {
                                let matches = str.match(/^([0-9]+) ([a-zA-Z0-9\._]+)=(.*)\n/);

                                let skip = parseInt(matches[1], 10);
                                let key = matches[2];
                                let value = matches[3];

                                switch (key) {
                                    case 'path': { extended.filename = value; } break;
                                    case 'size': { extended.size = parseInt(value, 10); } break;
                                }

                                str = str.substr(skip).trimStart();
                            }
                        } catch (err) {
                            throw new Error('Malformed PAX entry');
                        }
                    }

                    header = null;
                }
            } catch (err) {
                reject(err);
            }
        });

        reader.on('error', reject);
        reader.on('end', resolve);
    });
}

function pathIsAbsolute(path) {
    if (process.platform == 'win32' && path.match(/^[a-zA-Z]:/))
        path = path.substr(2);
    return isPathSeparator(path[0]);
}

function pathHasDotDot(path) {
    let start = 0;

    for (;;) {
        let offset = path.indexOf('..', start);
        if (offset < 0)
            break;
        start = offset + 2;

        if (offset && !isPathSeparator(path[offset - 1]))
            continue;
        if (offset + 2 < path.length && !isPathSeparator(path[offset + 2]))
            continue;

        return true;
    }

    return false;
}

function isPathSeparator(c) {
    if (c == '/')
        return true;
    if (process.platform == 'win32' && c == '\\')
        return true;

    return false;
}

function syncFiles(src_dir, dest_dir) {
    let keep = new Set;

    // Copy source files
    {
        let entries = fs.readdirSync(src_dir, { withFileTypes: true });

        for (let entry of entries) {
            if (!entry.isFile())
                continue;

            keep.add(entry.name);
            fs.copyFileSync(src_dir + `/${entry.name}`, dest_dir + `/${entry.name}`);
        }
    }

    // Delete old destination files
    {
        let entries = fs.readdirSync(dest_dir, { withFileTypes: true });

        for (let entry of entries) {
            if (!entry.isFile())
                continue;
            if (keep.has(entry.name))
                continue;

            fs.unlinkSync(dest_dir + `/${entry.name}`);
        }
    }
}

function unlinkRecursive(path) {
    try {
        fs.rmSync(path, { recursive: true, maxRetries: process.platform == 'win32' ? 3 : 0 });
    } catch (err) {
        if (err.code !== 'ENOENT')
            throw err;
    }
}

function getNapiVersion(napi, major) {
    if (napi > 8)
        return null;

    // https://nodejs.org/api/n-api.html#node-api-version-matrix
    const support = {
        6:  ['6.14.2', '6.14.2', '6.14.2'],
        8:  ['8.6.0',  '8.10.0', '8.11.2'],
        9:  ['9.0.0',  '9.3.0',  '9.11.0'],
        10: ['10.0.0', '10.0.0', '10.0.0', '10.16.0', '10.17.0', '10.20.0', '10.23.0'],
        11: ['11.0.0', '11.0.0', '11.0.0', '11.8.0'],
        12: ['12.0.0', '12.0.0', '12.0.0', '12.0.0',  '12.11.0', '12.17.0', '12.19.0', '12.22.0'],
        13: ['13.0.0', '13.0.0', '13.0.0', '13.0.0',  '13.0.0'],
        14: ['14.0.0', '14.0.0', '14.0.0', '14.0.0',  '14.0.0',  '14.0.0',  '14.12.0', '14.17.0'],
        15: ['15.0.0', '15.0.0', '15.0.0', '15.0.0',  '15.0.0',  '15.0.0',  '15.0.0',  '15.12.0']
    };
    const max = Math.max(...Object.keys(support).map(k => parseInt(k, 10)));

    if (major > max)
        return major + '.0.0';
    if (support[major] == null)
        return null;

    let required = support[major][napi - 1] || null;
    return required;
}

// Ignores prerelease suffixes
function compareVersions(ver1, ver2) {
    ver1 = String(ver1).replace(/-.*$/, '').split('.').reduce((acc, v, idx) => acc + parseInt(v, 10) * Math.pow(10, 2 * (5 - idx)), 0);
    ver2 = String(ver2).replace(/-.*$/, '').split('.').reduce((acc, v, idx) => acc + parseInt(v, 10) * Math.pow(10, 2 * (5 - idx)), 0);

    let cmp = Math.min(Math.max(ver1 - ver2, -1), 1);
    return cmp;
}

export {
    downloadHttp,
    extractTarGz,
    pathIsAbsolute,
    pathHasDotDot,
    syncFiles,
    unlinkRecursive,
    getNapiVersion,
    compareVersions
}
