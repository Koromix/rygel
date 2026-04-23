// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import fs from 'fs';
import os from 'os';
import path from 'path';
import { spawnSync } from 'child_process';
import * as tools from './tools.js';
import TOOLCHAINS from '../assets/toolchains.json' with { type: 'json' };

const DefaultOptions = {
    mode: 'RelWithDebInfo'
};

function Builder(config = {}) {
    let self = this;

    let host = `${process.platform}_${tools.determineArch()}`;

    let app_dir = config.app_dir;
    let project_dir = config.project_dir;
    let package_dir = config.package_dir;

    if (app_dir == null)
        app_dir = import.meta.dirname.replace(/\\/g, '/') + '/..';
    if (project_dir == null)
        project_dir = process.cwd();
    app_dir = app_dir.replace(/\\/g, '/');
    project_dir = project_dir.replace(/\\/g, '/');
    if (package_dir == null)
        package_dir = findParentDirectory(project_dir, 'package.json');
    if (package_dir != null)
        package_dir = package_dir.replace(/\\/g, '/');

    let runtime_version = config.runtime_version;
    let toolchain = config.toolchain || null;
    let prefer_clang = config.prefer_clang || false;
    let mode = config.mode || DefaultOptions.mode;
    let targets = config.targets || [];
    let verbose = config.verbose || false;
    let prebuild = config.prebuild || false;
    let defines = config.defines || [];

    if (runtime_version == null)
        runtime_version = process.version;
    if (runtime_version.startsWith('v'))
        runtime_version = runtime_version.substr(1);

    if (toolchain != null && !Object.hasOwn(TOOLCHAINS, toolchain))
        throw new Error(`Unknown cross-compilation toolchain '${toolchain}'`);

    let options = null;

    let cache_dir = getCacheDirectory();
    let build_dir = config.build_dir;
    let work_dir = null;
    let output_dir = null;

    if (build_dir == null) {
        let options = readCNokeOptions();

        if (options.output != null) {
            build_dir = expandPath(options.output, options.directory);
        } else {
            build_dir = project_dir + '/build';
        }
    }
    build_dir = build_dir.replace(/\\/g, '/');
    work_dir = build_dir + `/v${runtime_version}_${toolchain ?? 'native'}/${mode}`;
    output_dir = work_dir + '/Output';

    let cmake_bin = null;

    this.configure = async function(retry = true) {
        let options = readCNokeOptions();

        let args = [project_dir];

        checkCMake();
        checkCompatibility();

        console.log(`>> Node: ${runtime_version}`);
        console.log(`>> Toolchain: ${toolchain ?? 'native'}`);

        // Prepare build directory
        fs.mkdirSync(build_dir, { recursive: true, mode: 0o755 });
        fs.mkdirSync(work_dir, { recursive: true, mode: 0o755 });
        fs.mkdirSync(output_dir, { recursive: true, mode: 0o755 });

        retry &= fs.existsSync(work_dir + '/CMakeCache.txt');

        args.push(`-DNODE_JS_EXECPATH=${process.execPath}`);

        // Download or use Node headers
        if (options.api == null) {
            let destname = `${cache_dir}/node-v${runtime_version}-headers.tar.gz`;

            if (!fs.existsSync(destname)) {
                fs.mkdirSync(cache_dir, { recursive: true, mode: 0o755 });

                let url = `https://nodejs.org/dist/v${runtime_version}/node-v${runtime_version}-headers.tar.gz`;
                await tools.downloadHttp(url, destname);
            }

            await tools.extractTarGz(destname, work_dir + '/headers', 1);

            args.push(`-DNODE_JS_INCLUDE_DIRS=${work_dir}/headers/include/node`);
        } else {
            console.log(`>> Using local node-api headers`);

            let api_dir = expandPath(options.api, project_dir);
            args.push(`-DNODE_JS_INCLUDE_DIRS=${api_dir}/include`);
        }

        args.push(`-DCMAKE_MODULE_PATH=${app_dir}/assets`);

        let win32 = (toolchain ?? host).startsWith('win32_');
        let mingw = (process.platform == 'win32' && process.env.MSYSTEM != null);

        // Handle Node import library on Windows
        if (win32) {
            if (mingw) {
                args.push(`-DNODE_JS_LINK_LIB=node.dll`);
            } else {
                let info = TOOLCHAINS[toolchain ?? host];

                if (options.api == null) {
                    let destname = `${cache_dir}/node_v${runtime_version}_${toolchain ?? host}.lib`;

                    if (!fs.existsSync(destname)) {
                        fs.mkdirSync(cache_dir, { recursive: true, mode: 0o755 });

                        let url = `https://nodejs.org/dist/v${runtime_version}/${info.lib}/node.lib`;
                        await tools.downloadHttp(url, destname);
                    }

                    fs.copyFileSync(destname, work_dir + '/node.lib');
                    args.push(`-DNODE_JS_LINK_LIB=${work_dir}/node.lib`);
                } else {
                    let api_dir = expandPath(options.api, project_dir);
                    args.push(`-DNODE_JS_LINK_DEF=${api_dir}/def/node_api.def`);
                }
            }

            fs.copyFileSync(`${app_dir}/assets/win_delay_hook.c`, work_dir + '/win_delay_hook.c');
            args.push(`-DNODE_JS_SOURCES=${work_dir}/win_delay_hook.c`);
        }

        if (process.platform != 'win32' || mingw) {
            if (spawnSync('ninja', ['--version']).status === 0) {
                args.push('-G', 'Ninja');
            } else if (process.platform == 'win32') {
                args.push('-G', 'MinGW Makefiles');
            }

            // Use CCache if available
            if (config.ccache && spawnSync('ccache', ['--version']).status === 0) {
                args.push('-DCMAKE_C_COMPILER_LAUNCHER=ccache');
                args.push('-DCMAKE_CXX_COMPILER_LAUNCHER=ccache');
            }
        }

        // Handle toolchain flags and cross-compilation
        {
            let info = TOOLCHAINS[toolchain ?? host];

            if (info != null) {
                if (Object.hasOwn(info, process.platform))
                    info = Object.assign({}, info, info[process.platform]);

                if (info.flags != null)
                    args.push(...info.flags);
            }

            if (toolchain == null && process.arch == 'ia32') {
                let proc = spawnSync('uname', ['-m']);
                let machine = (proc.stdout ?? '').toString('utf-8').trim();

                if (machine == 'x86_64') {
                    // Compiler probably does not default to 32-bit... just force it
                    args.push('-DCMAKE_ASM_FLAGS=-m32', '-DCMAKE_C_FLAGS=-m32', '-DCMAKE_CXX_FLAGS=-m32');
                }
            }

            if (toolchain != null && info.triplet != null) {
                let values = [
                    ['CMAKE_SYSTEM_NAME', info.system],
                    ['CMAKE_SYSTEM_PROCESSOR', info.processor]
                ];
                let sysroot = null;

                // Switch to Clang automatically if the GCC cross-compiler does not exist
                if (process.platform != 'win32' && !prefer_clang) {
                    let binary = info.triplet + '-gcc';

                    if (spawnSync(binary, ['-v']).status !== 0)
                        prefer_clang = true;
                }

                if (prefer_clang) {
                    values.push(['CMAKE_ASM_COMPILER_TARGET', info.triplet]);
                    values.push(['CMAKE_C_COMPILER_TARGET', info.triplet]);
                    values.push(['CMAKE_CXX_COMPILER_TARGET', info.triplet]);

                    values.push(['CMAKE_EXE_LINKER_FLAGS', '-fuse-ld=lld']);
                    values.push(['CMAKE_SHARED_LINKER_FLAGS', '-fuse-ld=lld']);
                    values.push(['CMAKE_STATIC_LINKER_FLAGS', '-fuse-ld=lld']);
                } else if (process.platform != 'win32') {
                    values.push(['CMAKE_ASM_COMPILER', info.triplet + '-gcc']);
                    values.push(['CMAKE_C_COMPILER', info.triplet + '-gcc']);
                    values.push(['CMAKE_CXX_COMPILER', info.triplet + '-g++']);
                }

                if (info.sysroot != null) {
                    sysroot = expandPath(info.sysroot, import.meta.dirname + '/..');
                    if (!fs.existsSync(sysroot))
                        throw new Error(`Cross-compilation sysroot '${sysroot}' does not exist`);

                    values.push(['CMAKE_SYSROOT', sysroot]);
                }

                if (info.variables != null) {
                    for (let key in info.variables) {
                        let value = expandString(info.variables[key], { sysroot: sysroot });
                        values.push([key, value]);
                    }
                }

                let filename = `${work_dir}/toolchain.cmake`;
                let text = values.map(pair => `set(${pair[0]} "${pair[1]}")`).join('\n');

                fs.writeFileSync(filename, text);
                args.push(`-DCMAKE_TOOLCHAIN_FILE=${filename}`);
            }
        }

        if (prefer_clang) {
            if (process.platform == 'win32' && !mingw) {
                args.push('-T', 'ClangCL');
            } else {
                args.push('-DCMAKE_C_COMPILER=clang');
                args.push('-DCMAKE_CXX_COMPILER=clang++');
            }
        }

        args.push(`-DCMAKE_BUILD_TYPE=${mode}`);
        for (let type of ['ARCHIVE', 'RUNTIME', 'LIBRARY']) {
            for (let suffix of ['', '_DEBUG', '_RELEASE', '_RELWITHDEBINFO'])
                args.push(`-DCMAKE_${type}_OUTPUT_DIRECTORY${suffix}=${output_dir}`);
        }
        for (let define of defines)
            args.push(`-D${define}`);
        args.push('--no-warn-unused-cli');

        console.log('>> Running configuration');

        let proc = spawnSync(cmake_bin, args, { cwd: work_dir, stdio: 'inherit' });
        if (proc.status !== 0) {
            tools.unlinkRecursive(work_dir);
            if (retry)
                return self.configure(false);

            throw new Error('Failed to run configure step');
        }
    };

    this.build = async function() {
        checkCompatibility();

        if (prebuild) {
            let valid = await checkPrebuild();
            if (valid)
                return;
        }

        checkCMake();

        if (!fs.existsSync(work_dir + '/CMakeCache.txt'))
            await self.configure();

        // In case Make gets used
        // os.cpus() returns [] on Android (no /proc/cpuinfo access), so fall back to 1
        if (process.env.MAKEFLAGS == null)
            process.env.MAKEFLAGS = '-j' + (os.cpus().length || 1);

        let args = [
            '--build', work_dir,
            '--config', mode
        ];

        if (verbose)
            args.push('--verbose');
        for (let target of targets)
            args.push('--target', target);

        console.log('>> Running build');

        let proc = spawnSync(cmake_bin, args, { stdio: 'inherit' });
        if (proc.status !== 0)
            throw new Error('Failed to run build step');

        console.log('>> Copy target files');

        tools.syncFiles(output_dir, build_dir);
    };

    async function checkPrebuild() {
        let options = readCNokeOptions();

        if (options.prebuild != null) {
            fs.mkdirSync(build_dir, { recursive: true, mode: 0o755 });

            let archive_filename = expandPath(options.prebuild, options.directory);

            if (fs.existsSync(archive_filename)) {
                try {
                    console.log('>> Extracting prebuilt binaries...');
                    await tools.extractTarGz(archive_filename, build_dir, 1);
                } catch (err) {
                    console.error('Failed to find prebuilt binary for your platform, building manually');
                }
            } else {
                console.error('Cannot find local prebuilt archive');
            }
        }

        if (options.require != null) {
            let require_filename = expandPath(options.require, options.directory);

            if (fs.existsSync(require_filename)) {
                let proc = spawnSync(process.execPath, ['-e', 'require(process.argv[1])', require_filename]);
                if (proc.status === 0)
                    return true;
            }

            console.error('Failed to load prebuilt binary, rebuilding from source');
        }

        return false;
    }

    this.clean = function() {
        tools.unlinkRecursive(build_dir);
    };

    function findParentDirectory(dirname, basename) {
        if (process.platform == 'win32')
            dirname = dirname.replace(/\\/g, '/');

        do {
            if (fs.existsSync(dirname + '/' + basename))
                return dirname;

            dirname = path.dirname(dirname);
        } while (!dirname.endsWith('/'));

        return null;
    }

    function getCacheDirectory() {
        if (process.platform == 'win32') {
            let cache_dir = process.env['LOCALAPPDATA'] || process.env['APPDATA'];
            if (cache_dir == null)
                throw new Error('Missing LOCALAPPDATA and APPDATA environment variable');

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

    function checkCMake() {
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

    function checkCompatibility() {
        let options = readCNokeOptions();

        if (options.node != null && tools.compareVersions(runtime_version, options.node) < 0)
            throw new Error(`Project ${options.name} requires Node.js >= ${options.node}`);

        if (options.napi != null) {
            let major = parseInt(runtime_version, 10);
            let required = tools.getNapiVersion(options.napi, major);

            if (required == null)
                throw new Error(`Project ${options.name} does not support the Node ${major}.x branch (old or missing N-API)`);
            if (tools.compareVersions(runtime_version, required) < 0)
                throw new Error(`Project ${options.name} requires Node >= ${required} in the Node ${major}.x branch (with N-API >= ${options.napi})`);
        }
    }

    function readCNokeOptions() {
        if (options != null)
            return options;

        let directory = project_dir;
        let pkg = null;
        let cnoke = null;

        if (package_dir != null) {
            try {
                let json = fs.readFileSync(package_dir + '/package.json', { encoding: 'utf-8' });

                pkg = JSON.parse(json);
                directory = package_dir;
            } catch (err) {
                if (err.code != 'ENOENT')
                    throw err;
            }
        }

        if (cnoke == null)
            cnoke = pkg?.cnoke ?? {};

        options = {
            name: pkg?.name ?? path.basename(project_dir),
            version: pkg?.version ?? null,

            directory: directory,
            ...cnoke
        };

        return options;
    }

    function expandString(str, values = {}) {
        let expanded = str.replace(/{{ *([a-zA-Z_][a-zA-Z_0-9]*) *}}/g, (match, p1) => {
            switch (p1) {
                case 'version': {
                    let options = readCNokeOptions();
                    return options.version || '';
                } break;

                case 'toolchain': return toolchain ?? host;

                default: {
                    if (Object.hasOwn(values, p1)) {
                        return values[p1];
                    } else {
                        return match;
                    }
                } break;
            }
        });

        return expanded;
    }

    function expandPath(str, root) {
        let expanded = expandString(str);

        if (!tools.pathIsAbsolute(expanded))
            expanded = path.join(root, expanded);
        expanded = path.normalize(expanded);

        return expanded;
    }
}

export {
    Builder,
    DefaultOptions
}
