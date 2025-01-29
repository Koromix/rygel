#!/usr/bin/env node

// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the “Software”), to deal in
// the Software without restriction, including without limitation the rights to use,
// copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
// Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
// HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
// WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
// OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const process = require('process');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const esbuild = require('../../../vendor/esbuild/native');
const { DefaultCommands, parse_arguments, QemuRunner,
        copy_recursive, unlink_recursive, make_path_filter,
        make_wildcard_pattern, style_ansi, wait_delay } = require('../../../tools/qemu/qemu.js');

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

let script_dir = null;
let root_dir = null;

let runner = null;
let builds = null;
let all_builds = false;

let ignore_builds = new Set;

// Main

main();

async function main() {
    script_dir = fs.realpathSync(__dirname);
    root_dir = fs.realpathSync(script_dir + '/../../..');

    // Some code assumes we are working from the script directory
    process.chdir(script_dir);

    let config = parse_arguments('koffi.js', process.argv.slice(2), ValidCommands);
    if (config == null)
        return;

    runner = new QemuRunner();

    let command = null;
    if (!ValidCommands.hasOwnProperty(config.command))
        throw new Error(`Unknown command '${config.command}'`);
    command = runner[config.command] ?? CommandFunctions[config.command];

    // Load build registy
    let known_builds;
    {
        let json = fs.readFileSync('./koffi.json', { encoding: 'utf-8' });
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
            let re = make_wildcard_pattern(pattern);
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
    let dist_dir = build_dir + '/package';

    let snapshot_dir = snapshot();

    let json = fs.readFileSync(root_dir + '/src/koffi/package.json', { encoding: 'utf-8' });
    let version = JSON.parse(json).version;

    console.log('>> Version:', version);
    console.log('>> Checking build archives...');
    {
        let needed_machines = new Set;

        for (let build of builds) {
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

    // Run machine commands
    {
        let success = true;

        if (ready_builds < builds.length) {
            success &= await runner.start();
            success &= await upload(snapshot_dir);

            console.log('>> Run build commands...');
            await compile();
        }

        console.log('>> Get build artifacts');
        await Promise.all(builds.map(async build => {
            let machine = runner.machine(build.info.machine);

            let src_name = machine.platform + '_' + build.info.arch;
            let src_dir = build.info.directory + `/bin/Koffi/${src_name}`;
            let dest_dir = build_dir + `/qemu/${version}/${build.key}`;

            artifacts.push(dest_dir);

            if (runner.isIgnored(machine))
                return;
            if (ignore_builds.has(build))
                return;

            unlink_recursive(dest_dir);
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
            console.log('>> Status: ' + style_ansi('FAILED', 'red bold'));
            return null;
        }
    }

    console.log('>> Prepare NPM package');
    {
        let binary_dir = dist_dir + '/build/koffi';

        unlink_recursive(dist_dir);
        fs.mkdirSync(dist_dir, { mode: 0o755, recursive: true });

        copy_recursive(snapshot_dir, dist_dir, make_path_filter([
            'src/core',
            'src/cnoke',
            'src/koffi/cmake',
            'src/koffi/examples',
            'src/koffi/src',
            'src/koffi/CHANGELOG.md',
            'src/koffi/CMakeLists.txt',
            'src/koffi/index.d.ts',
            'src/koffi/index.js',
            'src/koffi/indirect.js',
            'src/koffi/LICENSE.txt',
            'src/koffi/package.json',
            'src/koffi/README.md',
            'vendor/node-addon-api',
            'vendor/node-api-headers',
            'web/koffi.dev'
        ]));
        fs.mkdirSync(binary_dir, { mode: 0o755, recursive: true });

        for (let artifact of artifacts) {
            let dest_dir = binary_dir + '/' + path.basename(artifact);

            fs.mkdirSync(dest_dir, { mode: 0o755 });
            copy_recursive(artifact, dest_dir);
        }

        let pkg = JSON.parse(json);

        pkg.scripts = {
            install: 'node src/cnoke/cnoke.js -p . -d src/koffi --prebuild'
        };
        pkg.cnoke.output = 'build/koffi/{{ platform }}_{{ arch }}';
        delete pkg.devDependencies;

        esbuild.buildSync({
            entryPoints: [dist_dir + '/src/koffi/index.js'],
            bundle: true,
            minify: false,
            write: true,
            platform: 'node',
            outfile: dist_dir + '/index.js'
        });
        esbuild.buildSync({
            entryPoints: [dist_dir + '/src/koffi/indirect.js'],
            bundle: true,
            minify: false,
            write: true,
            platform: 'node',
            outfile: dist_dir + '/indirect.js'
        });

        fs.writeFileSync(dist_dir + '/package.json', JSON.stringify(pkg, null, 4));
        fs.unlinkSync(dist_dir + '/src/koffi/package.json');
        fs.unlinkSync(dist_dir + '/src/koffi/index.js');
        fs.unlinkSync(dist_dir + '/src/koffi/indirect.js');
        fs.renameSync(dist_dir + '/src/koffi/index.d.ts', dist_dir + '/index.d.ts');
        fs.renameSync(dist_dir + '/src/koffi/README.md', dist_dir + '/README.md');
        fs.renameSync(dist_dir + '/src/koffi/LICENSE.txt', dist_dir + '/LICENSE.txt');
        fs.renameSync(dist_dir + '/src/koffi/CHANGELOG.md', dist_dir + '/CHANGELOG.md');
        fs.renameSync(dist_dir + '/web/koffi.dev', dist_dir + '/doc');
        fs.rmdirSync(dist_dir + '/web');
    }

    console.log('>> Test prebuild');
    {
        let pkg = JSON.parse(fs.readFileSync(dist_dir + '/package.json'));
        let require_filename = path.join(dist_dir, pkg.cnoke.require);

        let proc = spawnSync(process.execPath, ['-e', 'require(process.argv[1])', require_filename]);
        if (proc.status !== 0)
            throw new Error('Failed to use prebuild:\n' + (proc.stderr || proc.stdout));
    }

    return dist_dir;
}

async function compile(debug = false) {
    let success = true;

    await Promise.all(builds.map(async build => {
        let machine = runner.machine(build.info.machine);

        if (ignore_builds.has(build))
            return;
        if (runner.isIgnored(machine))
            return;

        let cmd = build.info.build + (debug ? ' --debug' : ' --config release');
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

function snapshot() {
    let snapshot_dir = root_dir + '/bin/Koffi/snapshot';

    unlink_recursive(snapshot_dir);
    fs.mkdirSync(snapshot_dir, { mode: 0o755, recursive: true });

    console.log('>> Snapshot code...');
    copy_recursive(root_dir, snapshot_dir, make_path_filter([
        'src/core/base',
        'src/core/unicode',
        'src/cnoke',
        'src/koffi',
        'vendor/node-addon-api',
        'vendor/node-api-headers',
        'vendor/raylib',
        'vendor/sqlite3',
        'vendor/sqlite3mc',
        'web/koffi.dev'
    ]));

    return snapshot_dir;
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

                await wait_delay(1000);
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
    let snapshot_dir = snapshot();

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
    await compile(debug);

    console.log('>> Run test commands...');
    await Promise.all(builds.map(async build => {
        let machine = runner.machine(build.info.machine);

        if (ignore_builds.has(build))
            return;
        if (runner.isIgnored(machine))
            return;

        let commands = {
            'Build tests': build.test.build + (debug ? ' --debug' : ' --config release'),
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
        console.log('>> Status: ' + style_ansi('SUCCESS', 'green bold'));
    } else {
        console.log('>> Status: ' + style_ansi('FAILED', 'red bold'));
    }

    return success;
}
