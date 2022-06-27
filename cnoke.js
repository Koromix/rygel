#!/usr/bin/env node

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

'use strict';

const crypto = require('crypto');
const fs = require('fs');
const http = require('https');
const os = require('os');
const path = require('path');
const process = require('process');
const zlib = require('zlib');
const { spawnSync } = require('child_process');
const { Buffer } = require('buffer');

// Globals

const default_mode = 'RelWithDebInfo';

let app_dir = null;
let project_dir = null;
let package_dir = null;
let cache_dir = null;
let build_dir = null;
let work_dir = null;

let version = null;
let arch = null;
let toolset = null;
let mode = default_mode;
let targets = [];
let verbose = false;
let rebuild = false;
let prebuild = null;
let prebuild_req = null;

let cmake_bin = null;

// Main

main();

async function main() {
    let command = build;

    // Parse options
    {
        let i = 2;

        if (process.argv.length >= 3 && process.argv[2][0] != '-') {
            switch (process.argv[2]) {
                case 'build': { command = build; i++; } break;
                case 'configure': { command = configure; i++; } break;
                case 'clean': { command = clean; i++; } break;
            }
        }

        for (; i < process.argv.length; i++) {
            let arg = process.argv[i];
            let value = null;

            if (arg[0] == '-') {
                if (arg.length > 2 && arg[1] != '-') {
                    value = arg.substr(2);
                    arg = arg.substr(0, 2);
                } else if (arg[1] == '-') {
                    let offset = arg.indexOf('=');

                    if (offset > 2 && arg.length > offset + 1) {
                        value = arg.substr(offset + 1);
                        arg = arg.substr(0, offset);
                    }
                }
                if (value == null && process.argv[i + 1] != null && process.argv[i + 1][0] != '-') {
                    value = process.argv[i + 1];
                    i++; // Skip this value next iteration
                }
            }

            if (arg == '--help') {
                print_usage();
                return;
            } else if (arg == '-C' || arg == '--directory') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                project_dir = fs.realpathSync(value);
            } else if ((command == build || command == configure) && arg == '--version') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                version = value;
                if (!version.startsWith('v'))
                    version = 'v' + version;
            } else if ((command == build || command == configure) && arg == '--arch') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                arch = value;
            } else if ((command == build || command == configure) && arg == '--toolset') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                toolset = value;
            } else if ((command == build || command == configure) && (arg == '-m' || arg == '--mode')) {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                switch (value.toLowerCase()) {
                    case 'relwithdebinfo': { mode = 'RelWithDebInfo'; } break;
                    case 'debug': { mode = 'Debug'; } break;
                    case 'release': { mode = 'Release'; } break;

                    default: {
                        throw new Error(`Unknown value '${value}' for ${arg}`);
                    } break;
                }
            } else if ((command == build || command == configure) && arg == '--debug') {
                mode = 'Debug';
            } else if (command == build && (arg == '-v' || arg == '--verbose')) {
                verbose = true;
            } else if (command == build && arg == '--rebuild') {
                rebuild = true;
            } else if (command == build && arg == '--prebuild') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                prebuild = value;
            } else if (command == build && arg == '--require') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                prebuild_req = value;
            } else if (command == build && arg[0] != '-') {
                targets.push(arg);
            } else {
                if (arg[0] == '-') {
                    throw new Error(`Unexpected argument '${arg}'`);
                } else {
                    throw new Error(`Unexpected value '${arg}'`);
                }
            }
        }
    }

    if (version == null)
        version = process.version;
    if (arch == null)
        arch = determine_arch();

    app_dir = path.dirname(__filename);
    if (project_dir == null)
        project_dir = process.cwd();
    project_dir = project_dir.replace(/\\/g, '/');
    package_dir = find_parent_directory(project_dir, 'package.json');
    cache_dir = get_cache_directory();
    build_dir = project_dir + '/build';
    work_dir = build_dir + `/${version}_${arch}`;

    try {
        await command();
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

function print_usage() {
    let help = `Usage: cnoke [command] [options...] [targets...]

Commands:
    configure                    Configure CMake build
    build                        Build project (configure if needed)
    clean                        Clean build files

Global options:
    -C, --directory <DIR>        Change project directory
                                 (default: current working directory)

Build options:
    -v, --verbose                Show build commands while building

        --rebuild                Perform clean step before build

        --prebuild <URL>         Set URL template to download prebuilt binaries
        --require <PATH>         Require specified module, and drop prebuild if it fails

Configure options:
        --version <VERSION>      Change node version
                                 (default: ${process.version})
        --arch <ARCH>            Change architecture and ABI
                                 (default: ${determine_arch()})

        --toolset <TOOLSET>      Change default CMake toolset
    -m, --mode <MODE>            Change build type: RelWithDebInfo, Debug, Release
                                 (default: ${default_mode})
        --debug                  Shortcut for -m debug

The ARCH value is similar to process.arch, with the following differences:

- arm is changed to arm32hf or arm32sf depending on the floating-point ABI used (hard-float, soft-float)
- riscv32 is changed to riscv32sf, riscv32hf32, riscv32hf64 or riscv32hf128 depending on the floating-point ABI
- riscv64 is changed to riscv64sf, riscv64hf32, riscv64hf64 or riscv64hf128 depending on the floating-point ABI`;

    console.log(help);
}

// Commands

async function configure(retry = true) {
    let args = [project_dir];

    check_cmake();

    console.log('>> Platform:', process.platform);
    console.log('>> Architecture:', arch);

    // Prepare build directory
    fs.mkdirSync(cache_dir, { recursive: true, mode: 0o755 });
    fs.mkdirSync(build_dir, { recursive: true, mode: 0o755 });
    fs.mkdirSync(work_dir, { recursive: true, mode: 0o755 });

    retry &= fs.existsSync(work_dir + '/CMakeCache.txt');

    // Download Node headers
    {
        let basename = `node-${version}-headers.tar.gz`;
        let urls = [
            `https://nodejs.org/dist/${version}/${basename}`,
            `https://unofficial-builds.nodejs.org/download/release/${version}/${basename}`
        ];
        let destname = `${cache_dir}/${basename}`;

        if (!fs.existsSync(destname))
            await download(urls, destname);
        await extract_targz(destname, work_dir + '/headers', 1);
    }

    // Download Node import library (Windows)
    if (process.platform === 'win32') {
        let dirname;
        switch (arch) {
            case 'ia32': { dirname = 'win-x86'; } break;
            case 'x64': { dirname = 'win-x64'; } break;
            case 'arm64': { dirname = 'win-arm64'; } break;

            default: {
                throw new Error(`Unsupported architecture '${arch}' for Node on Windows`);
            } break;
        }

        let destname = `${cache_dir}/node_${version}_${arch}.lib`;

        if (!fs.existsSync(destname)) {
            let urls = [
                `https://nodejs.org/dist/${version}/${dirname}/node.lib`,
                `https://unofficial-builds.nodejs.org/download/release/${version}/${dirname}/node.lib`
            ];
            await download(urls, destname);
        }

        fs.copyFileSync(destname, work_dir + '/node.lib');
        fs.copyFileSync(`${app_dir}/assets/win_delay_hook.c`, work_dir + '/win_delay_hook.c');
    }

    fs.copyFileSync(`${app_dir}/assets/FindCNoke.cmake`, `${work_dir}/FindCNoke.cmake`);
    args.push(`-DCMAKE_MODULE_PATH=${work_dir}`);

    args.push(`-DNODE_JS_INCLUDE_DIRS=${work_dir}/headers/include/node`);

    switch (process.platform) {
        case 'win32': {
            args.push(`-DNODE_JS_SOURCES=${work_dir}/win_delay_hook.c`);
            args.push(`-DNODE_JS_LIBRARIES=${work_dir}/node.lib`);

            switch (arch) {
                case 'ia32': {
                    args.push('-DNODE_JS_LINK_FLAGS=/DELAYLOAD:node.exe;/SAFESEH:NO');
                    args.push('-A', 'Win32');
                } break;
                case 'arm64': {
                    args.push('-DNODE_JS_LINK_FLAGS=/DELAYLOAD:node.exe;/SAFESEH:NO');
                    args.push('-A', 'ARM64');
                } break;
                case 'x64': {
                    args.push('-DNODE_JS_LINK_FLAGS=/DELAYLOAD:node.exe');
                    args.push('-A', 'x64');
                } break;
            }
        } break;

        case 'darwin': {
            args.push('-DNODE_JS_LINK_FLAGS=-undefined;dynamic_lookup');

            switch (arch) {
                case 'arm64': { args.push('-DCMAKE_OSX_ARCHITECTURES=arm64'); } break;
                case 'x64': { args.push('-DCMAKE_OSX_ARCHITECTURES=x86_64'); } break;
            }
        } break;
    }

    if (process.platform != 'win32') {
        // Prefer Ninja if available
        if (spawnSync('ninja', ['--version']).status === 0)
            args.push('-G', 'Ninja');

        // Use CCache if available
        if (spawnSync('ccache', ['--version']).status === 0) {
            args.push('-DCMAKE_C_COMPILER_LAUNCHER=ccache');
            args.push('-DCMAKE_CXX_COMPILER_LAUNCHER=ccache');
        }
    }

    if (toolset != null)
        args.push('-T', toolset);

    args.push(`-DCMAKE_BUILD_TYPE=${mode}`);
    for (let type of ['ARCHIVE', 'RUNTIME', 'LIBRARY']) {
        for (let suffix of ['', '_DEBUG', '_RELEASE', '_RELWITHDEBINFO'])
            args.push(`-DCMAKE_${type}_OUTPUT_DIRECTORY${suffix}=${build_dir}`);
    }
    args.push('--no-warn-unused-cli');

    console.log('>> Running configuration');

    let proc = spawnSync(cmake_bin, args, { cwd: work_dir, stdio: 'inherit' });
    if (proc.status !== 0) {
        unlink_recursive(work_dir);
        if (retry)
            return configure(false);

        throw new Error('Failed to run configure step');
    }
}

async function build() {
    if (prebuild) {
        fs.mkdirSync(build_dir, { recursive: true, mode: 0o755 });

        let url = prebuild.replace(/{{([a-zA-Z_][a-zA-Z_0-9]*)}}/g, (match, p1) => {
            switch (p1) {
                case 'version': {
                    let json = fs.readFileSync(package_dir + '/package.json', { encoding: 'utf-8' });
                    let version = JSON.parse(json).version;

                    return version;
                } break;

                case 'platform': return process.platform;
                case 'arch': return arch;

                default: return match;
            }
        });
        let basename = path.basename(url);

        try {
            let archive_filename = null;

            if (url.startsWith('file:/')) {
                if (url.startsWith('file://localhost/')) {
                    url = url.substr(16);
                } else {
                    let offset = 6;
                    while (offset < 9 && url[offset] == '/')
                        offset++;
                    url = url.substr(offset - 1);
                }

                if (process.platform == 'win32' && url.match(/^\/[a-zA-Z]+:[\\\/]/))
                    url = url.substr(1);
            }

            if (url.match(/^[a-z]+:\/\//)) {
                archive_filename = build_dir + '/' + basename;
                await download(url, archive_filename);
            } else {
                archive_filename = project_dir + '/' + url;
                if (!fs.existsSync(archive_filename))
                    throw new Error('Cannot find local prebuilt archive');
            }

            console.log('>> Extracting prebuilt binaries...');
            await extract_targz(archive_filename, build_dir, 2);

            // Make sure the binary works
            if (prebuild_req) {
                let proc = spawnSync(process.execPath, ['-e', 'require(process.argv[1])', prebuild_req]);
                if (proc.status === 0)
                    return;
            } else {
                return;
            }

            // Clean it up if it does not, before source build. Only delete files, it is safer and it is enough!
            let entries = fs.readdirSync(build_dir, { withFileTypes: true });
            for (let entry of entries) {
                if (!entry.isDirectory()) {
                    let filename = path.join(build_dir, entry.name);
                    fs.unlinkSync(filename);
                }
            }

            console.error('Failed to load prebuilt binary, rebuilding from source');
        } catch (err) {
            console.error('Failed to find prebuilt binary for your platform, building manually');
        }
    }

    check_cmake();

    if (!fs.existsSync(work_dir + '/CMakeCache.txt'))
        await configure();

    // In case Make gets used
    if (process.env.MAKEFLAGS == null)
        process.env.MAKEFLAGS = '-j' + os.cpus().length;

    let args = [
        '--build', work_dir,
        '--config', mode
    ];

    if (verbose)
        args.push('--verbose');
    if (rebuild)
        args.push('--clean-first');
    for (let target of targets)
        args.push('--target', target);

    console.log('>> Running build');

    let proc = spawnSync(cmake_bin, args, { stdio: 'inherit' });
    if (proc.status !== 0)
        throw new Error('Failed to run build step');
}

async function clean() {
    unlink_recursive(build_dir);
}

// Utility

function get_cache_directory() {
    if (process.platform == 'win32') {
        let cache_dir = process.env['APPDATA'];
        if (cache_dir == null)
            throw new Error('Missing APPDATA environment variable');

        cache_dir = path.join(cache_dir, 'cnoke');
        return cache_dir;
    } else {
        let cache_dir = process.env['XDG_CACHE_HOME'];

        if (cache_dir == null) {
            let home = process.env['HOME'];
            if (home == null)
                throw new Error('Missing HOME environment variable');

            cache_dir = path.join(home, '.cache');
        }

        cache_dir = path.join(cache_dir, 'cnoke');
        return cache_dir;
    }
}

function check_cmake() {
    if (cmake_bin != null)
        return;

    // Check for CMakeLists.txt
    if (!fs.existsSync(project_dir + '/CMakeLists.txt'))
        throw new Error('This directory does not appear to have a CMakeLists.txt file');

    // Check for CMake
    {
        let proc = spawnSync('cmake', ['--version']);

        if (proc.status === 0) {
            cmake_bin = 'cmake';
        } else {
            if (process.platform == 'win32') {
                // I really don't want to depend on anything in CNoke, and Node.js does not provide
                // anything to read from the registry. This is okay, REG.exe exists since Windows XP.
                let proc = spawnSync('reg', ['query', 'HKEY_LOCAL_MACHINE\\SOFTWARE\\Kitware\\CMake', '/v', 'InstallDir']);

                if (proc.status === 0) {
                    let matches = proc.stdout.toString('utf-8').match(/InstallDir[ \t]+REG_[A-Z_]+[ \t]+(.*)+/);

                    if (matches != null) {
                        let bin = path.join(matches[1].trim(), 'bin\\cmake.exe');

                        if (fs.existsSync(bin))
                            cmake_bin = bin;
                    }
                }
            }

            if (cmake_bin == null)
                throw new Error('CMake does not seem to be available');
        }
    }

    console.log(`>> Using CMake binary: ${cmake_bin}`);
}

function unlink_recursive(path) {
    try {
        if (fs.rmSync != null) {
            fs.rmSync(path, { recursive: true, maxRetries: process.platform == 'win32' ? 3 : 0 });
        } else {
            fs.rmdirSync(path, { recursive: true, maxRetries: process.platform == 'win32' ? 3 : 0 });
        }
    } catch (err) {
        if (err.code !== 'ENOENT')
            throw err;
    }
}

async function download(url, dest) {
    if (Array.isArray(url)) {
        let urls = url;

        for (let url of urls) {
            try {
                await download(url, dest);
                return;
            } catch (err) {
                if (err.code != 404)
                    throw err;
            }
        }

        throw new Error('All URLS returned error 404');
    }

    console.log('>> Downloading ' + url);

    let [tmp_name, file] = open_temporary_stream(dest);

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
                        if (err.code != 'EBUSY')
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

function open_temporary_stream(prefix) {
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

function read_file_header(filename, read) {
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

function extract_targz(filename, dest_dir, strip = 0) {
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
                        if (header.filename[0] == '/')
                            throw new Error(`Insecure filename starting with / inside TAR archive`);
                        if (has_dotdot(header.filename))
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

// Does not care about Windows separators, normalize first
function has_dotdot(path) {
    let start = 0;

    for (;;) {
        let offset = path.indexOf('..', start);
        if (offset < 0)
            break;
        start = offset + 2;

        if (offset && path[offset - 1] != '/')
            continue;
        if (offset + 2 < path.length && path[offset + 2] != '/')
            continue;

        return true;
    }

    return false;
}

function find_parent_directory(dirname, basename)
{
    if (process.platform == 'win32')
        dirname = dirname.replaceAll('\\', '/');

    do {
        if (fs.existsSync(dirname + '/' + basename))
            return dirname;

        dirname = path.dirname(dirname);
    } while (dirname.includes('/'));

    return null;
}

function determine_arch() {
    let arch = process.arch;

    if (arch == 'riscv32' || arch == 'riscv64') {
        let buf = read_file_header(process.execPath, 512);
        let header = decode_elf_header(buf);
        let float_abi = (header.e_flags & 0x6) >> 1;

        switch (float_abi) {
            case 0: { arch += 'sf'; } break;
            case 1: { arch += 'hf32'; } break;
            case 2: { arch += 'hf64'; } break;
            case 3: { arch += 'hf128'; } break;
        }
    } else if (arch == 'arm') {
        arch = 'arm32';

        let buf = read_file_header(process.execPath, 512);
        let header = decode_elf_header(buf);

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

function decode_elf_header(buf) {
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
