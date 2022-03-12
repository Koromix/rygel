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

const fs = require('fs');
const http = require('https');
const os = require('os');
const path = require('path');
const process = require('process');
const zlib = require('zlib');
const { spawnSync } = require('child_process');

// Globals

let version = null;
let arch = null;
let debug = false;

let project_dir = null;
let build_dir = null;
let download_dir = null;
let work_dir = null;

// Main

main();

async function main() {
    let command = 'build';

    // Parse options
    {
        let i = 2;

        if (process.argv.length >= 3 && process.argv[2][0] != '-') {
            command = process.argv[2];
            i++;
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
            } else if (arg == '-v' || arg == '--version') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                version = value;
                if (!version.startsWith('v'))
                    version = 'v' + version;
            } else if (arg == '-a' || arg == '--arch') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                arch = value;
            } else if (arg == '-C' || arg == '--directory') {
                if (value == null)
                    throw new Error(`Missing value for ${arg}`);

                project_dir = fs.realpathSync(value);
            } else if (arg == '--debug') {
                debug = true;
            } else {
                if (arg[0] == '-') {
                    throw new Error(`Unexpected argument '${arg}'`);
                } else {
                    throw new Error(`Unexpected value '${arg}'`);
                }
            }
        }

        switch (command) {
            case 'build': { command = build; } break;
            case 'configure': { command = configure; } break;
            case 'clean': { command = clean; } break;

            default: {
                throw new Error(`Unknown command '${command}'`);
            } break;
        }
    }

    if (version == null)
        version = process.version;
    if (arch == null)
        arch = process.arch;

    if (project_dir == null)
        project_dir = process.cwd();
    project_dir = project_dir.replace(/\\/g, '/');
    build_dir = project_dir + '/build';
    download_dir = build_dir + `/downloads`;
    work_dir = build_dir + `/cmake/${version}_${arch}`;

    try {
        await command();
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

function print_usage() {
    let help = `Usage: cnoke [command] [options...]

Commands:
    configure                    Configure CMake build
    build                        Build project (configure if needed)
    clean                        Clean build files

Global options:
    -C, --directory <DIR>        Change project directory
                                 (default: current working directory)

Configure options:
    -v, --version <VERSION>      Change node version
                                 (default: ${process.version})
    -a, --arch <ARCH>            Change architecture
                                 (default: ${process.arch})

        --debug                  Build in debug mode
`;

    console.log(help);
}

// Commands

async function configure(retry = true) {
    let args = [project_dir];

    // Check for CMakeLists.txt
    if (!fs.existsSync(project_dir + '/CMakeLists.txt'))
        throw new Error('This directory does not appear to have a CMakeLists.txt file');

    // Check for CMake
    {
        let proc = spawnSync('cmake', ['--version']);
        if (proc.status != 0)
            throw new Error('CMake does not seem to be available');
    }

    // Prepare build directory
    fs.mkdirSync(build_dir, { recursive: true, mode: 0o755 });
    fs.mkdirSync(download_dir, { recursive: true, mode: 0o755 });
    fs.mkdirSync(work_dir, { recursive: true, mode: 0o755 });

    // Download Node headers
    {
        let basename = `node-${version}-headers.tar.gz`;
        let url = `https://nodejs.org/dist/${version}/${basename}`;
        let destname = `${download_dir}/${basename}`;

        if (!fs.existsSync(destname))
            await download(url, destname);
        await extract_targz(destname, work_dir + '/headers', 1);
    }

    // Download Node import library (Windows)
    if (process.platform === 'win32') {
        let dirname;
        switch (arch) {
            case 'ia32': { dirname = 'win-x86'; } break;
            case 'x64': { dirname = 'win-x64'; } break;

            default: {
                throw new Error(`Unsupported architecture '${arch}' for Node on Windows`);
            } break;
        }

        let url = `https://nodejs.org/dist/${version}/${dirname}/node.lib`;
        await download(url, work_dir + `/node.lib`);
    }

    args.push(`-DNODE_JS_INC=${work_dir}/headers/include/node`);

    switch (process.platform) {
        case 'win32': {
            write_delay_hook_c(`${work_dir}/win_delay_hook.c`);

            args.push(`-DNODE_JS_SRC=${work_dir}/win_delay_hook.c`);
            args.push(`-DNODE_JS_LIB=${work_dir}/node.lib`);

            if (arch === 'ia32') {
                args.push('-DCMAKE_SHARED_LINKER_FLAGS=/DELAYLOAD:node.exe /SAFESEH:NO');
                args.push('-A', 'Win32');
            } else {
                args.push('-DCMAKE_SHARED_LINKER_FLAGS=/DELAYLOAD:node.exe');
            }
        } break;

        case 'darwin': {
            args.push('-DCMAKE_SHARED_LINKER_FLAGS=-undefined dynamic_lookup');
        } break;
    }

    if (process.platform != 'win32') {
        // Prefer Ninja if available
        if (spawnSync('ninja', ['--version']).status == 0)
            args.push('-G', 'Ninja');

        // Use CCache if available
        if (spawnSync('ccache', ['--version']).status == 0) {
            args.push('-DCMAKE_C_COMPILER_LAUNCHER=ccache');
            args.push('-DCMAKE_CXX_COMPILER_LAUNCHER=ccache');
        }
    }

    args.push(`-DCMAKE_BUILD_TYPE=${debug ? 'Debug' : 'Release'}`);
    for (let type of ['RUNTIME', 'LIBRARY']) {
        for (let suffix of ['', '_DEBUG', '_RELEASE'])
            args.push(`-DCMAKE_${type}_OUTPUT_DIRECTORY${suffix}=${build_dir}`);
    }
    args.push('--no-warn-unused-cli');

    let proc = spawnSync('cmake', args, { cwd: work_dir, stdio: 'inherit' });
    if (proc.status != 0) {
        if (retry && fs.existsSync(work_dir + '/CMakeCache.txt')) {
            unlink_recursive(work_dir);
            return configure(false);
        }

        throw new Error('Failed to run configure step');
    }
}

async function build() {
    if (!fs.existsSync(work_dir + '/CMakeCache.txt'))
        await configure();

    // In case Make gets used
    if (process.env.MAKEFLAGS == null)
        process.env.MAKEFLAGS = '-j' + os.cpus().length;

    let args = [
        '--build', work_dir,
        '--config', debug ? 'Debug' : 'Release'
    ];

    let proc = spawnSync('cmake', args, { stdio: 'inherit' });
    if (proc.status != 0)
        throw new Error('Failed to run build step');
}

async function clean() {
    let entries = fs.readdirSync(build_dir);

    for (let basename of entries) {
        if (basename == 'downloads')
            continue;

        let filename = build_dir + '/' + basename;
        unlink_recursive(filename);
    }
}

// Utility

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

function download(url, dest) {
    return new Promise((resolve, reject) => {
        try {
            let file = fs.createWriteStream(dest);

            http.get(url, response => {
                response.pipe(file);
                file.on('finish', () => file.close(resolve));
            });
        } catch (err) {
            reject(err);
        }
    });
}

function extract_targz(filename, dest_dir, strip = 0) {
    let reader = fs.createReadStream(filename).pipe(zlib.createGunzip());

    return new Promise((resolve, reject) => {
        let header = null;

        reader.on('readable', () => {
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
                        type: buf[156] - 48
                    };

                    if (buf.toString('ascii', 257, 263) == 'ustar\0') {
                        let prefix = buf.toString('utf-8', 345, 500).replace(/\0/g, '');
                        header.filename = prefix + header.filename;
                    }

                    // Safety checks
                    header.filename = header.filename.replace(/\\/g, '/');
                    if (!header.filename.length)
                        reject(new Error(`Insecure empty filename inside TAR archive`));
                    if (header.filename[0] == '/')
                        reject(new Error(`Insecure filename starting with / inside TAR archive`));
                    if (has_dotdot(header.filename))
                        reject(new Error(`Insecure filename containing '..' inside TAR archive`));

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

                if (header.type == 0 || header.type == 7) {
                    let filename = dest_dir + '/' + header.filename;
                    let dirname = path.dirname(filename);

                    fs.mkdirSync(dirname, { recursive: true, mode: 0o755 });
                    fs.writeFileSync(filename, data, { mode: header.mode });
                } else if (header.type == 5) {
                    let filename = dest_dir + '/' + header.filename;
                    fs.mkdirSync(filename, { recursive: true, mode: header.mode });
                }

                header = null;
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

// Windows

function write_delay_hook_c(filename) {
    let code = `// This program is free software: you can redistribute it and/or modify
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

#include <stdlib.h>
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <delayimp.h>

static FARPROC WINAPI self_exe_hook(unsigned int event, DelayLoadInfo *info)
{
    HMODULE m;

    if (event == dliNotePreLoadLibrary && !stricmp(info->szDll, "node.exe")) {
        HMODULE h = GetModuleHandle(NULL);
        return (FARPROC)h;
    }

    return NULL;
}

const PfnDliHook __pfnDliNotifyHook2 = self_exe_hook;
`;

    fs.writeFileSync(filename, code);
}
