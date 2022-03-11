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

const process = require('process');
const fs = require('fs');
const path = require('path');
const util = require('util');
const { spawn, spawnSync } = require('child_process');
const { NodeSSH } = require('node-ssh');
const chalk = require('chalk');

// Globals

let machines = [];
let display = false;
let shutdown = true;
let ignore = new Set;

// Main

main();

async function main() {
    let root_dir = fs.realpathSync(path.dirname(__filename));
    process.chdir(root_dir);

    let command = 'test';

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
            } else if (arg == '-d' || arg == '--display') {
                display = true;
            } else if (arg == '-l' || arg == '--leave') {
                shutdown = false;
            } else if (arg[0] == '-') {
                throw new Error(`Unexpected argument '${arg}'`);
            } else {
                if (arg.startsWith('__') || arg.match(/[\\/\.]/))
                    throw new Error(`Machine name '${arg} is not valid`);

                machines.push(arg);
            }
        }

        switch (command) {
            case 'test': { command = test; } break;
            case 'start': { command = start; } break;
            case 'stop': { command = stop; } break;
            case 'ssh': { command = ssh; } break;

            default: {
                throw new Error(`Unknown command '${command}'`);
            } break;
        }
    }

    // Load machine registry
    let machines_map;
    {
        let json = fs.readFileSync('registry/machines.json', { encoding: 'utf-8' });

        machines_map = JSON.parse(json);
        for (let key in machines_map)
            machines_map[key].key = key;
    }

    if (!machines.length) {
        for (let name in machines_map)
            machines.push(name);

        if (!machines.length) {
            console.error('Could not detect any machine');
            process.exit(1);
        }
    }
    machines = machines.map(name => {
        let machine = machines_map[name];
        if (machine == null) {
            machine = Object.values(machines_map).find(machine => machine.name == name);
            if (machine == null)
                throw new Error(`Could not find machine ${name}`);
        }
        return machine;
    });

    try {
        let success = await command();
        process.exit(!success);
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

function print_usage() {
    let help = `Usage: node test.js [command] [options...]

Commands:
    test                         Run the machines and perform the tests (default)
    start                        Start the machines but don't run anythingh
    stop                         Stop running machines
    ssh                          Start SSH session with specific machine

Options:
    -d, --display                Show the QEMU display during the procedure
    -l, --leave                  Leave the systems running after test
`;

    console.log(help);
}

// Commands

async function start(detach = true) {
    let success = true;
    let missing = 0;

    console.log('>> Starting up machines...');
    await Promise.all(machines.map(async machine => {
        let dirname = `qemu/${machine.key}`;

        if (!fs.existsSync(dirname)) {
            log(machine, 'Missing files', chalk.bold.gray('[ignore]'));

            ignore.add(machine);
            missing++;

            return;
        }

        try {
           await boot(machine, dirname, detach, display);
        } catch (err) {
            console.error(err);

            ignore.add(machine);
            success = false;
        }
    }));

    if (success && missing == machines.length)
        throw new Error('No machine available');

    return success;
}

async function test() {
    let success = true;

    success &= await start(!shutdown);

    console.log('>> Copy source code...');
    await Promise.all(machines.map(async machine => {
        if (ignore.has(machine))
            return;

        let local_dir = '../..';
        let remote_dir = machine.info.home + '/luigi';

        try {
            await machine.ssh.exec('rm', ['-rf', remote_dir]);
        } catch (err) {
            // Fails often on Windows (busy directory or whatever), but rarely a problem
        }

        try {
            await machine.ssh.putDirectory(local_dir, remote_dir, {
                recursive: true,
                concurrency: 2,

                validate: filename => {
                    let basename = path.basename(filename);

                    return basename !== '.git' &&
                           basename !== 'qemu' && !basename.startsWith('qemu_') &&
                           basename !== 'node_modules' &&
                           basename !== 'node' &&
                           basename !== 'build' &&
                           basename !== 'luigi';
                }
            });

            log(machine, 'Copy', chalk.bold.green('[ok]'));
        } catch (err) {
            log(machine, 'Copy', chalk.bold.red('[error]'));

            ignore.add(machine);
            success = false;
        }
    }));

    console.log('>> Run test commands...');
    await Promise.all(machines.map(async machine => {
        if (ignore.has(machine))
            return;

        for (let name in machine.commands) {
            let cmd = machine.commands[name];
            let cwd = machine.info.home + '/luigi/koffi';

            let start = process.hrtime.bigint();
            let ret = await execRemote(machine, cmd, cwd);
            let time = Number((process.hrtime.bigint() - start) / 1000000n);

            if (ret.code == 0) {
                log(machine, name, chalk.bold.green(`[${(time / 1000).toFixed(2)}s]`));
            } else {
                log(machine, name, chalk.bold.red('[error]'));

                if (ret.stdout || ret.stderr)
                    console.error('');

                let align = log.align + 9;
                if (ret.stdout) {
                    let str = ' '.repeat(align) + 'Standard output:\n' +
                              chalk.yellow(ret.stdout.replace(/^/gm, ' '.repeat(align + 4))) + '\n';
                    console.error(str);
                }
                if (ret.stderr) {
                    let str = ' '.repeat(align) + 'Standard error:\n' +
                              chalk.yellow(ret.stderr.replace(/^/gm, ' '.repeat(align + 4))) + '\n';
                    console.error(str);
                }

                success = false;
            }
        }
    }));

    console.log('>> Cleaning up code...');
    await Promise.all(machines.map(async machine => {
        if (ignore.has(machine))
            return;

        let remote_dir = machine.info.home + '/luigi';

        try {
            await machine.ssh.exec('rm', ['-rf', remote_dir]);
            log(machine, 'Delete', chalk.bold.green('[ok]'));
        } catch (err) {
            if (process.platform == 'win32') {
                await wait(1000);

                try {
                    await machine.ssh.exec('rm', ['-rf', remote_dir]);
                    log(machine, 'Delete', chalk.bold.green('[ok]'));
                } catch (err) {
                    log(machine, 'Delete', chalk.bold.red('[error]'));
                }
            } else {
                log(machine, 'Delete', chalk.bold.red('[error]'));
            }
        }
    }));

    if (shutdown)
        success &= await stop();

    console.log('');
    if (success) {
        console.log('>> Status: ' + chalk.bold.green('SUCCESS'));
        if (ignore.size)
            console.log('   (but some machines could not be tested)');
    } else {
        console.log('>> Status: ' + chalk.bold.red('FAILED'));
    }

    return success;
}

async function stop() {
    let success = true;

    console.log('>> Sending shutdown commands...');
    await Promise.all(machines.map(async machine => {
        if (ignore.has(machine))
            return;

        if (machine.ssh == null) {
            try {
                await join(machine, 2);
            } catch (err) {
                log(machine, 'Already down', chalk.bold.green('[ok]'));
                return;
            }
        }

        try {
            await new Promise(async (resolve, reject) => {
                if (machine.ssh.connection == null) {
                    reject();
                    return;
                }

                machine.ssh.connection.on('close', resolve);
                machine.ssh.connection.on('end', resolve);
                wait(10000).then(reject);

                execRemote(machine, machine.info.shutdown);
            });

            log(machine, 'Stop', chalk.bold.green('[ok]'));
        } catch (err) {
            log(machine, 'Stop', chalk.bold.red('[error]'));
            success = false;
        }
    }));

    return success;
}

async function ssh() {
    if (machines.length != 1) {
        console.error('The ssh command can only be used with one machine');
        return false;
    }

    let machine = machines[0];

    let args = [
        '-p' + machine.info.password,
        'ssh', '-p', machine.info.port, machine.info.username + '@127.0.0.1'
    ];

    let proc = spawnSync('sshpass', args, { stdio: 'inherit' });
    if (proc.status != 0) {
        console.error('Connection failed');
        return false;
    }

    return true;
}

// Utility

async function boot(machine, dirname, detach, display) {
    let qemu = machine.qemu;

    if (process.platform == 'linux' && process.arch == 'x64' && (machine.info.arch == 'x64' ||
                                                                 machine.info.arch == 'ia32'))
        qemu += ' -accel kvm';
    if (!display)
        qemu += ' -display none';

    let started = null;

    try {
        let proc = spawn(qemu, [], {
            cwd: dirname,
            shell: true,
            detached: detach,
            stdio: 'ignore'
        });

        if (detach) {
            proc.unref();
        }

        await new Promise((resolve, reject) => {
            proc.on('spawn', () => wait(2 * 1000).then(resolve));
            proc.on('error', reject);
            proc.on('exit', reject);
        });

        await join(machine, 30);
        started = true;
    } catch (err) {
        await join(machine, 2);
        started = false;
    }

    if (machine.uploads != null) {
        for (let src in machine.uploads) {
            let dest = machine.uploads[src];

            await machine.ssh.putFile(dirname + '/' + src, dest);
        }
    }

    log(machine, (started ? 'Start' : 'Join'), chalk.bold.green('[ok]'));
}

async function join(machine, tries) {
    let ssh = new NodeSSH;

    while (tries) {
        try {
            await ssh.connect({
                host: '127.0.0.1',
                port: machine.info.port,
                username: machine.info.username,
                password: machine.info.password
            });

            break;
        } catch (err) {
            if (!--tries)
                throw new Error(`Failed to connect to ${machine.name}`);

            // Try again... a few times
            await wait(10 * 1000);
        }
    }

    machine.ssh = ssh;
}

function wait(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

function log(machine, action, status) {
    if (log.align == null) {
        let lengths = machines.map(machine => machine.name.length);
        log.align = Math.max(...lengths);
    }

    let align1 = log.align - machine.name.length;
    let align2 = 22 - action.length;

    console.log(`     [${machine.name}]${' '.repeat(align1)}  ${action}${' '.repeat(align2)}  ${status}`);
}

async function execRemote(machine, cmd, cwd = null) {
    try {
        if (machine.info.platform == 'win32') {
            if (cwd != null) {
                cwd = cwd.replaceAll('/', '\\');
                cmd = `cd "${cwd}" && ${cmd}`;
            }

            let ret = await machine.ssh.execCommand(cmd);
            return ret;
        } else {
            let ret = await machine.ssh.execCommand(cmd, { cwd: cwd });
            return ret;
        }
    } catch (err) {
        console.log(err);
        return err;
    }
}
