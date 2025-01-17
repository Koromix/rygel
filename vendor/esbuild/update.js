#!/bin/env node

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const { Readable } = require('node:stream');
const zlib = require('zlib');

const PLATFORMS = [
    'darwin-arm64',
    'darwin-x64',
    'freebsd-arm64',
    'freebsd-x64',
    'linux-arm64',
    'linux-ia32',
    'linux-x64',
    'openbsd-arm64',
    'openbsd-x64',
    'win32-arm64',
    'win32-ia32',
    'win32-x64'
];

main();

async function main() {
    try {
        await run();
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function run() {
    process.chdir(__dirname);

    let args = process.argv.slice(2);

    if (args.includes('--help')) {
        console.log('Usage: update.js version');
        return 0;
    } else {
        let opt = args.find(arg => arg.startsWith('-'));

        if (opt != null)
            throw new Error(`Invalid option '${opt}'`);
    }

    let version = args[0];
    if (version == null)
        throw new Error('Missing version argument');

    // Clean up
    fs.rmSync('native', { recursive: true, force: true });
    fs.rmSync('wasm', { recursive: true, force: true });
    fs.rmSync('src',  { recursive: true, force: true });

    // Fetch complete source
    {
        runCommand('git', 'clone', '--depth', 1, '--branch', 'v' + version, 'https://github.com/evanw/esbuild.git', 'src');
        runCommand('git', 'clone', 'https://go.googlesource.com/sys', 'src/sys');

        let mod = fs.readFileSync('src/go.mod').toString('utf-8');
        let [_, dependency, commit] = mod.match(/require (golang.org\/x\/sys v[0-9]+(?:\.[0-9]+)*-[0-9]+-([0-9a-f]+))/);

        runCommand('git', '-C', 'src/sys', 'checkout', commit); 

        fs.appendFileSync('src/go.mod', `\nreplace ${dependency} => ./sys\n`);
        fs.writeFileSync('src/go.sum', '');

        fs.rmSync('src/.git',  { recursive: true });
        fs.rmSync('src/sys/.git',  { recursive: true });
    }

    // Download native package
    downloadPackage('esbuild', version, 'native');

    // Download native binaries
    for (let platform of PLATFORMS) {
        let pkg = `@esbuild/${platform}`;
        let dest = `native/node_modules/${pkg}`;

        downloadPackage(pkg, version, dest);
    }

    // Download WASM package
    downloadPackage('esbuild-wasm', version, 'wasm');
}

async function downloadPackage(pkg, version, dest_dir) {
    console.log(`Download npm package ${pkg} (${version})`);

    let json = readCommand('npm', 'info', pkg + '@' + version, '--json');
    let info = JSON.parse(json);

    let response = await fetch(info.dist.tarball);
    let stream = Readable.from(response.body);

    extractTarGzStream(stream, dest_dir, 1);
}

async function extractTarGzStream(stream, dest_dir, strip = 0) {
    let reader = stream.pipe(zlib.createGunzip());

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

function runCommand(cmd, ...args) {
    let proc = spawnSync(cmd, args, { stdio: 'inherit' });

    if (proc.status === null) {
        throw new Error(`Failed to start '${cmd}'`);
    } else if (proc.status != 0) {
        throw new Error(`Failed to run '${cmd}'`);
    }
}

function readCommand(cmd, ...args) {
    let proc = spawnSync(cmd, args);

    if (proc.status === null) {
        throw new Error(`Failed to start '${cmd}'`);
    } else if (proc.status != 0) {
        let output = (proc.stderr || proc.stdout || 'unknown').toString('utf-8');
        throw new Error(`Failed to run '${cmd}': ${output}`);
    }

    let output = proc.stdout.toString('utf-8');
    return output;
}
