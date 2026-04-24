#!/usr/bin/env -S node --no-warnings
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import process from 'process';
import fs from 'fs';
import path from 'path';
import { spawnSync } from 'child_process';
import esbuild from '../../../vendor/esbuild/native/lib/main.js';
import {
    DefaultCommands,
    QemuRunner,
    parseArguments,
    copyRecursive,
    unlinkRecursive,
    makePathFilter,
    makeWildcardPattern,
    styleAnsi,
    waitDelay
} from '../../../tools/cross/qemu.js';

const ValidCommands = {
    ...DefaultCommands,

    test: 'Run the machines and perform the tests (default)',
    debug: 'Run the machines and perform the tests, with debug build',
    build: 'Prepare distribution-ready NPM package directory'
};
const CommandFunctions = {
    test: test,
    debug: debug,
    build: build
};

// Globals

const MONOLITH = true;
const BINARY_PREFIX = '@koromix/koffi-';

let script_dir = null;
let root_dir = null;

let runner = null;
let builds = null;
let all_builds = false;

let ignore_builds = new Set;

// Main

main();

async function main() {
    script_dir = fs.realpathSync(import.meta.dirname);
    root_dir = fs.realpathSync(script_dir + '/../../..');

    // Some code assumes we are working from the script directory
    process.chdir(script_dir);

    let config = parseArguments('brew.js', process.argv.slice(2), ValidCommands);
    if (config == null)
        return;

    runner = new QemuRunner();

    let command = null;
    if (!Object.hasOwn(ValidCommands, config.command))
        throw new Error(`Unknown command '${config.command}'`);
    command = runner[config.command] ?? CommandFunctions[config.command];

    // Load build registy
    let known_builds;
    {
        let json = fs.readFileSync('./brew.json', { encoding: 'utf-8' });
        known_builds = JSON.parse(json);

        for (let key in known_builds) {
            let build = known_builds[key];

            try {
                let machine = runner.machine(build.info.machine);
            } catch (err) {
                throw new Error(`Unknown machine '${build.info.machine}' for build '${build.title}'`);
            }

            known_builds[key] = {
                key: key,
                ...build
            };
        }

        if (!Object.keys(known_builds).length) {
            console.error('Could not find any build');
            process.exit(1);
        }
    }

    // List matching builds and machines
    if (config.patterns.length) {
        let use_builds = new Set;
        let use_machines = [];

        for (let pattern of config.patterns) {
            let re = makeWildcardPattern(pattern);
            let match = false;

            for (let key in known_builds) {
                let build = known_builds[key];

                if (key.match(re) || build.title.match(re)) {
                    use_builds.add(key);
                    use_machines.push(build.info.machine);

                    match = true;
                }
            }

            for (let machine of runner.machines) {
                if (machine.key.match(re) || machine.title.match(re)) {
                    for (let key in known_builds) {
                        let build = known_builds[key];

                        if (build.info.machine == machine.key)
                            use_builds.add(key);
                    }
                    use_machines.push(machine.key);

                    match = true;
                }
            }

            if (!match) {
                console.log(`Pattern '${pattern}' does not match any build or machine`);
                process.exit(1);
            }
        }

        for (let machine of use_machines)
            runner.select(machine);

        builds = Object.values(known_builds).filter(build => use_builds.has(build.key));
        all_builds = (builds.length == Object.keys(known_builds).length);
    } else {
        builds = Object.values(known_builds);
        all_builds = true;
    }

    try {
        let success = !!(await command());
        process.exit(0 + !success);
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

// Commands

async function build() {
    let build_dir = root_dir + '/bin/Koffi';
    let dist_dir = build_dir + '/packages';

    let snapshot_dir = await snapshot();

    let json = fs.readFileSync(root_dir + '/src/koffi/package.json', { encoding: 'utf-8' });
    let version = JSON.parse(json).version;

    console.log('>> Version:', version);
    console.log('>> Checking build archives...');
    {
        let needed_machines = new Set;

        for (let build of builds) {
            if (build.info.package == null) {
                ignore_builds.add(build);
                continue;
            }

            let machine = runner.machine(build.info.machine);

            let binary_filename = build_dir + `/qemu/${version}/${build.key}/koffi.node`;

            if (fs.existsSync(binary_filename)) {
                runner.logSuccess(machine, `${build.title} > Status`);
                ignore_builds.add(build);
            } else {
                runner.logError(machine, `${build.title} > Status`, 'missing');
                needed_machines.add(machine);
            }
        }

        for (let machine of runner.machines) {
            if (!needed_machines.has(machine))
                runner.ignore(machine);
        }
    }

    let ready_builds = ignore_builds.size;
    let ready_machines = runner.ignoreCount;

    let artifacts = [];
    let packages = [];

    // Run machine commands
    {
        let success = true;

        if (ready_builds < builds.length) {
            success &= await runner.start();
            success &= await upload(snapshot_dir);

            console.log('>> Run build commands...');
            await compile(build => build.info.release ?? (build.info.build + ' --release'));
        }

        console.log('>> Get build artifacts');
        await Promise.all(builds.map(async build => {
            let machine = runner.machine(build.info.machine);

            let src_name = machine.platform + '_' + build.info.arch;
            let src_dir = build.info.directory + `/bin/Koffi/${src_name}`;
            let dest_dir = build_dir + `/qemu/${version}/${build.key}`;

            if (build.info.package != null) {
                let artifact =  {
                    package: build.info.package,
                    build: dest_dir
                };
                artifacts.push(artifact);
            }

            if (runner.isIgnored(machine))
                return;
            if (ignore_builds.has(build))
                return;

            unlinkRecursive(dest_dir);
            fs.mkdirSync(dest_dir, { mode: 0o755, recursive: true });

            try {
                let ssh = await runner.join(machine);

                await ssh.getDirectory(dest_dir, src_dir, {
                    recursive: false,
                    concurrency: 4,
                    validate: filename => !path.basename(filename).match(/^v[0-9]+/)
                });

                runner.logSuccess(machine, `${build.title} > Download`);
            } catch (err) {
                runner.logError(machine, `${build.title} > Download`);

                ignore_builds.add(machine);
                success = false;
            }
        }));

        success &= (ignore_builds.size == ready_builds);
        success &= (runner.ignoreCount == ready_machines);

        if (!success) {
            console.log('');
            console.log('>> Status: ' + styleAnsi('FAILED', 'red bold'));
            return false;
        }
    }

    console.log('>> Clean up previous packages');
    unlinkRecursive(dist_dir);

    console.log('>> Regroup package builds');
    {
        let map = new Map;

        for (let artifact of artifacts) {
            let name = BINARY_PREFIX + artifact.package;
            let pkg = map.get(artifact.package);

            if (pkg == null) {
                pkg = {
                    target: artifact.package,
                    name: name,
                    path: dist_dir + '/' + name,
                    builds: []
                };

                map.set(artifact.package, pkg);
                packages.push(pkg);
            }

            pkg.builds.push({
                name: path.basename(artifact.build),
                path: artifact.build
            });
        }
    }

    if (!MONOLITH) {
        console.log('>> Prepare binary packages');

        for (let pkg of packages) {
            let main = JSON.parse(json);
            let [os, cpu] = pkg.target.split('-');

            for (let build of pkg.builds) {
                let binary_dir = pkg.path + '/' + build.name;

                fs.mkdirSync(binary_dir, { mode: 0o755, recursive: true });
                copyRecursive(build.path, binary_dir);
            }

            let binary = {
                name: pkg.name,
                version: main.version,
                description: main.description + ` (${pkg.name})`,
                author: main.author,
                homepage: main.homepage,
                repository: main.repository,
                license: main.license,
                funding: main.funding,
                main: './index.js',
                os: [os],
                cpu: [cpu]
            };

            let code = `
                let native = null;
                let err = null;

                ${pkg.builds.map(build => `
                    if (native == null) {
                        try {
                            native = require('./${build.name}/koffi.node');
                        } catch (e) {
                            err = e;
                        }
                    }
                `).join('\n')}

                if (native == null)
                    throw err;

                module.exports = native;
            `;
            code = esbuild.transformSync(code, { loader: 'js' }).code;

            fs.writeFileSync(pkg.path + '/index.js', code);
            fs.writeFileSync(pkg.path + '/package.json', JSON.stringify(binary, null, 4));
        }
    }

    console.log(`>> Prepare ${MONOLITH ? 'monolithic' : 'main'} package`);
    {
        let pkg_dir = dist_dir + '/koffi';
        fs.mkdirSync(pkg_dir, { mode: 0o755, recursive: true });

        copyRecursive(snapshot_dir, pkg_dir, makePathFilter([
            'lib/native/base',
            'src/core',
            'src/koffi/src',
            'src/koffi/CHANGELOG.md',
            'src/koffi/CMakeLists.txt',
            'src/koffi/index.d.ts',
            'src/koffi/LICENSE.txt',
            'src/koffi/package.json',
            'src/koffi/README.md',
            'vendor/node-addon-api/napi.h',
            'vendor/node-addon-api/napi-inl.h',
            'vendor/node-addon-api/napi-inl.deprecated.h',
            'vendor/node-addon-api/README.md',
            'vendor/node-addon-api/LICENSE.md',
            'vendor/node-api-headers/def',
            'vendor/node-api-headers/include',
            'vendor/node-api-headers/README.md',
            'vendor/node-api-headers/LICENSE',
            'web/koffi.dev'
        ]));

        let main = JSON.parse(json);

        main.scripts = {
            install: 'node src/build.js -P . -D src/koffi --prebuild --release'
        };
        main.cnoke.output = 'build/koffi/{{ toolchain }}';

        if (MONOLITH) {
            for (let artifact of artifacts) {
                let build = path.basename(artifact.build);
                let binary_dir = pkg_dir + '/build/koffi/' + build;

                fs.mkdirSync(binary_dir, { mode: 0o755, recursive: true });
                copyRecursive(artifact.build, binary_dir);
            }
        } else {
            main.optionalDependencies = packages.reduce((obj, pkg) => { obj[pkg.name] = main.version; return obj; }, {});
        }
        delete main.devDependencies;

        await bundleScripts(pkg_dir, packages);

        fs.writeFileSync(pkg_dir + '/package.json', JSON.stringify(main, null, 4));
        fs.unlinkSync(pkg_dir + '/src/koffi/package.json');
        fs.unlinkSync(pkg_dir + '/src/koffi/src/init.js');
        fs.unlinkSync(pkg_dir + '/src/koffi/src/static.js');
        fs.writeFileSync(pkg_dir + '/index.js', 'module.exports = require("./src/koffi/index.js");');
        fs.writeFileSync(pkg_dir + '/indirect.js', 'module.exports = require("./src/koffi/indirect.js");');
        fs.renameSync(pkg_dir + '/src/koffi/index.d.ts', pkg_dir + '/index.d.ts');
        fs.renameSync(pkg_dir + '/src/koffi/README.md', pkg_dir + '/README.md');
        fs.renameSync(pkg_dir + '/src/koffi/LICENSE.txt', pkg_dir + '/LICENSE.txt');
        fs.renameSync(pkg_dir + '/src/koffi/CHANGELOG.md', pkg_dir + '/CHANGELOG.md');
        fs.renameSync(pkg_dir + '/web/koffi.dev/pages', pkg_dir + '/doc');
        fs.rmSync(pkg_dir + '/doc/404.md');
        fs.rmSync(pkg_dir + '/web', { recursive: true });
    }

    console.log('>> Test package');
    {
        let pkg_dir = dist_dir + '/koffi';
        let proc = spawnSync(process.execPath, ['-e', 'require(process.argv[1])', pkg_dir], {
            cwd: pkg_dir,
            env: {
                ...process.env,
                NODE_PATH: dist_dir
            }
        });

        if (proc.status !== 0) {
            let stdout = proc.stdout.toString().trim();
            let stderr = proc.stderr.toString().trim();

            throw new Error('Failed to import package:\n' + (stderr || stdout));
        }
    }

    console.log('>> Write publish script');
    {
        let names = !MONOLITH ? packages.map(pkg => pkg.name) : [];
        names.push('koffi');

        let packs = names.map(name => ({
            name: name,
            filename: name.replace(/^@/, '').replace('/', '-') + `-${version}`
        }));

        let code = '#!/bin/sh -e\n\n';

        for (let pack of packs)
            code += `npm pack --pack-destination "${dist_dir}" "${dist_dir}/${pack.name}"\n`;
        code += '\n';

        code += 'echo "Unzipping packages"\n';
        for (let pack of packs)
            code += `gunzip ${pack.filename}.tgz\n`;
        code += '\n';

        for (let pack of packs) {
            code += `echo "Recompressing ${pack.name} with zopfli"\n`;
            code += `zopfli ${pack.filename}.tar\n`;
            code += `mv ${pack.filename}.tar.gz ${pack.filename}.tgz\n`;
            code += `rm ${pack.filename}.tar\n\n`;
        }

        for (let pack of packs)
            code += `npm publish --access public ${pack.filename}.tgz $*\n`;

        let script = dist_dir + '/publish.sh';
        fs.writeFileSync(script, code, { mode: 0o755 });
    }

    return true;
}

async function compile(command) {
    let success = true;

    await Promise.all(builds.map(async build => {
        let machine = runner.machine(build.info.machine);

        if (ignore_builds.has(build))
            return;
        if (runner.isIgnored(machine))
            return;

        let cmd = command(build);
        let cwd = build.info.directory + '/src/koffi';

        let start = process.hrtime.bigint();
        let ret = await runner.exec(machine, cmd, cwd);
        let time = Number((process.hrtime.bigint() - start) / 1000000n);

        if (ret.code == 0) {
            runner.logSuccess(machine, `${build.title} > Build`, (time / 1000).toFixed(2) + 's');
        } else {
            runner.logError(machine, `${build.title} > Build`);
            runner.logOutput(ret.stdout, ret.stderr);

            ignore_builds.add(build);
            success = false;
        }
    }));

    return success;
}

async function snapshot() {
    let snapshot_dir = root_dir + '/bin/Koffi/snapshot';

    unlinkRecursive(snapshot_dir);
    fs.mkdirSync(snapshot_dir, { mode: 0o755, recursive: true });

    console.log('>> Snapshot code...');

    copyRecursive(root_dir, snapshot_dir, makePathFilter([
        'lib/native/base',
        'src/cnoke',
        'src/koffi',
        'vendor/node-addon-api',
        'vendor/node-api-headers',
        'vendor/raylib',
        'vendor/sqlite3',
        'vendor/sqlite3mc',
        'web/koffi.dev'
    ]));

    await bundleScripts(snapshot_dir, []);

    return snapshot_dir;
}

async function bundleScripts(dest_dir, packages) {
    await esbuild.build({
        entryPoints: [root_dir + '/src/koffi/index.js'],
        bundle: true,
        minify: false,
        write: true,
        external: [BINARY_PREFIX + '*'],
        platform: 'node',
        outfile: dest_dir + '/src/koffi/index.js',
        plugins: [
            {
                name: 'static',
                setup: build => {
                    build.onLoad({ filter: /\/src\/static\.js$/ }, args => {
                        let code = `
                            function find(pkg) {
                                let native = null;

                                ${MONOLITH ? packages.flatMap(pkg => pkg.builds.map(build => `
                                    if (native == null && pkg == '${pkg.target}') {
                                        try {
                                            native = require('../../build/koffi/${build.name}/koffi.node');
                                        } catch (err) {
                                            // Go on
                                        }
                                    }
                                `)).join('') : ''}
                                ${!MONOLITH ? packages.flatMap(pkg => `
                                    if (pkg == '${pkg.target}')
                                        native = require('${pkg.name}');
                                `).join('') : ''}

                                return native;
                            }

                            module.exports = { find };
                        `;

                        return {
                            contents: code,
                            loader: 'js'
                        };
                    });
                }
            }
        ]
    });

    await esbuild.build({
        entryPoints: [root_dir + '/src/koffi/indirect.js'],
        bundle: true,
        minify: false,
        write: true,
        platform: 'node',
        outfile: dest_dir + '/src/koffi/indirect.js',
        plugins: [
            {
                name: 'package.json',
                setup: build => {
                    build.onResolve({ filter: /^\.\.\/package.json$/ }, args => {
                        return {
                            path: './package.json',
                            external: true
                        }
                    });
                }
            }
        ]
    });

    await esbuild.build({
        entryPoints: [root_dir + '/src/cnoke/cnoke.js'],
        bundle: true,
        minify: false,
        write: true,
        platform: 'node',
        outfile: dest_dir + '/src/build.js',
        plugins: [
            {
                name: 'dirname',
                setup: build => {
                    build.onLoad({ filter: /.*\.js$/ }, args => {
                        let js = fs.readFileSync(args.path, 'utf-8');
                        let patched = js.replaceAll('import.meta.dirname', '__dirname');

                        return {
                            contents: patched,
                            loader: 'js'
                        }
                    });
                }
            }
        ]
    });
}

async function upload(snapshot_dir) {
    let success = true;

    console.log('>> Upload source code...');
    await Promise.all(builds.map(async build => {
        let machine = runner.machine(build.info.machine);

        if (ignore_builds.has(build))
            return;
        if (runner.isIgnored(machine))
            return;

        let ssh = await runner.join(machine);

        for (let i = 0; i < 10; i++) {
            try {
                await ssh.exec('rm', ['-rf', build.info.directory]);
                break;
            } catch (err) {
                // Fails often on Windows (busy directory or whatever), but rarely a problem

                await waitDelay(1000);
                continue;
            }
        }

        try {
            await ssh.putDirectory(snapshot_dir, build.info.directory, {
                recursive: true,
                concurrency: (process.platform != 'win32') ? 4 : 1
            });

            runner.logSuccess(machine, `${build.title} > Upload`);
        } catch (err) {
            runner.logError(machine, `${build.title} > Upload`);

            ignore_builds.add(build);
            success = false;
        }
    }));

    return success;
}

async function debug() {
    let success = await test(true);
    return success;
}

async function test(debug = false) {
    let snapshot_dir = await snapshot();

    let success = true;

    for (let build of builds) {
        if (build.test == null)
            ignore_builds.add(build);
    }

    success &= await runner.start();
    success &= await upload(snapshot_dir);

    // Errors beyond here are actual failures
    let ignored_builds = ignore_builds.size;
    let ignored_machines = runner.ignoreCount;

    console.log('>> Run build commands...');
    await compile(build => build.info.build + (debug ? ' --debug' : ' --release'));

    console.log('>> Run test commands...');
    await Promise.all(builds.map(async build => {
        let machine = runner.machine(build.info.machine);

        if (ignore_builds.has(build))
            return;
        if (runner.isIgnored(machine))
            return;

        let commands = {
            'Build tests': build.test.build + (debug ? ' --debug' : ' --release'),
            ...build.test.commands
        };

        for (let name in commands) {
            let cmd = commands[name];
            let cwd = build.info.directory + '/src/koffi';

            let start = process.hrtime.bigint();
            let ret = await runner.exec(machine, cmd, cwd);
            let time = Number((process.hrtime.bigint() - start) / 1000000n);

            if (ret.code === 0) {
                runner.logSuccess(machine, `${build.title} > ${name}`, (time / 1000).toFixed(2) + 's');
            } else {
                runner.logError(machine, `${build.title} > ${name}`);
                runner.logOutput(ret.stdout, ret.stderr);

                success = false;

                if (name == 'Build tests')
                    break;
            }
        }
    }));

    // Build failures need to register as errors
    success &= (ignore_builds.size == ignored_builds);
    success &= (runner.ignoreCount == ignored_machines);

    if (success) {
        console.log('>> Status: ' + styleAnsi('SUCCESS', 'green bold'));
    } else {
        console.log('>> Status: ' + styleAnsi('FAILED', 'red bold'));
    }

    return success;
}
