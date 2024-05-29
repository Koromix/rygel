#!/usr/bin/env node

// Copyright 2023 Niels Martignène <niels.martignene@protonmail.com>
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
const { spawn, spawnSync } = require('child_process');
const { NodeSSH } = require('node-ssh');
const chalk = require('chalk');
const minimatch = require('minimatch');

const DefaultCommands = {
    'start': 'Start the machines but don\'t run anything',
    'stop': 'Stop running machines',
    'info': 'Print basic information about machine',
    'ssh': 'Start SSH session with specific machine',
    'reset': 'Reset initial disk snapshot'
};

// Globals

let qemu_prefix = null;

// Main

if (require.main === module)
    main();

async function main() {
    let config = parse_arguments('qemu.js', process.argv.slice(2), DefaultCommands);
    if (config == null)
        return;

    let runner = new QemuRunner();

    let command = null;
    if (!DefaultCommands.hasOwnProperty(config.command))
        throw new Error(`Unknown command '${config.command}'`);
    command = runner[config.command];

    // List matching machines
    if (config.patterns.length) {
        let use_machines = new Set;

        for (let pattern of config.patterns) {
            let re = minimatch.makeRe(pattern);
            let match = false;

            for (let machine of runner.machines) {
                if (machine.key.match(re) || machine.title.match(re)) {
                    use_machines.add(machine);
                    match = true;
                }
            }

            if (!match) {
                console.log(`Pattern '${pattern}' does not match any machine`);
                process.exit(1);
            }
        }

        for (let machine of runner.machines) {
            if (!use_machines.has(machine))
                runner.ignore(machine.key);
        }
    }

    try {
        let success = !!(await command());
        process.exit(0 + !success);
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

function print_usage(binary = 'qemu.js', commands = DefaultCommands) {
    let help = `Usage: node ${binary} <command> [options...]`;

    for (let cmd in commands) {
        let align = Math.max(0, 28 - cmd.length);
        help += `\n    ${cmd} ${' '.repeat(align)}${commands[keys]}`;
    }
    help = help.trim();

    console.log(help);
}

// Commands

function QemuRunner(registry = null) {
    let self = this;

    let machines = null;
    let map = null;

    let started_machines = new Set;
    let ignore_machines = new Set;
    let ignore_builds = new Set;

    let log_align = null;

    // Sanity checks
    if (parseInt(process.versions.node, 10) < 16)
        throw new Error('Please use Node version >= 16');
    if (spawnSync('ssh', ['-V']).status !== 0)
        throw new Error('Missing ssh binary in PATH');

    // Load registry
    {
        if (registry == null)
            registry = __dirname + '/machines.json';

        let json = fs.readFileSync(registry, { encoding: 'utf-8' });

        machines = JSON.parse(json);
        machines = Object.keys(machines).map(key => ({
            key: key,
            ...machines[key]
        }));
        map = machines.reduce((obj, machine) => { obj[machine.key] = machine; return obj }, {});
    };

    Object.defineProperties(this, {
        machines: { get: () => machines, enumerable: true },
        ignoreCount: { get: () => ignore_machines.size, enumerable: true },
        logAlign: { get: () => log_align, enumerable: true }
    });

    this.start = async function(detach = true) {
        let success = true;
        let missing = 0;

        check_qemu();

        console.log('>> Starting up machines...');
        await Promise.all(machines.map(async machine => {
            if (ignore_machines.has(machine))
                return;

            let dirname = `${__dirname}/machines/${machine.key}`;

            if (!fs.existsSync(dirname)) {
                self.log(machine, 'Missing files', chalk.bold.gray('[ignore]'));

                ignore_machines.add(machine);
                missing++;

                return;
            }

            // Version check
            {
                let filename = dirname + '/VERSION';
                let version = fs.existsSync(filename) ? parseInt(fs.readFileSync(filename).toString(), 10) : 0;

                if (version < machine.qemu.version) {
                    self.log(machine, 'Machine version mismatch', chalk.bold.gray('[ignore]'));

                    ignore_machines.add(machine);
                    success = false;

                    return;
                }
            }

            try {
                await boot(machine, dirname, detach);

                if (started_machines.has(machine)) {
                    let accelerator = get_machine_accelerator(machine);
                    let action = `Start (${accelerator ?? 'emulated'})`;

                    self.log(machine, action, chalk.bold.green('[ok]'));
                } else {
                    self.log(machine, 'Join', chalk.bold.green('[ok]'));
                }
            } catch (err) {
                self.log(machine, 'Start', chalk.bold.red('[error]'));

                ignore_machines.add(machine);
                success = false;
            }
        }));

        if (success && missing == machines.length)
            throw new Error('No machine available');

        return success;
    };

    this.stop = async function(all = true) {
        if (!started_machines.size && !all)
            return true;

        let success = true;

        console.log('>> Sending shutdown commands...');
        await Promise.all(machines.map(async machine => {
            if (!started_machines.has(machine) && !all)
                return;

            if (machine.ssh == null) {
                try {
                    await join(machine, 2);
                } catch (err) {
                    self.log(machine, 'Already down', chalk.bold.green('[ok]'));
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
                    wait(60000).then(() => { reject(new Error('Timeout')) });

                    self.exec(machine, machine.qemu.shutdown);
                });

                self.log(machine, 'Stop', chalk.bold.green('[ok]'));
            } catch (err) {
                self.log(machine, 'Stop', chalk.bold.red('[error]'));
                success = false;
            }
        }));

        return success;
    }

    this.info = function() {
        check_qemu();

        console.log('>> Machines:');
        for (let machine of machines) {
            let [binary, args] = make_qemu_command(machine);
            let cmd = [binary, ...args].map(v => String(v).match(/[^a-zA-Z0-9_\-=\:\.,]/) ? `"${v}"` : v).join(' ');

            console.log(`  - Machine: ${machine.title} (${machine.key})`);
            console.log(`    * Command-line: ${cmd}`);
            console.log(`    * SSH port: ${machine.qemu.ssh_port}`);
            console.log(`    * VNC port: ${machine.qemu.vnc_port}`);
            console.log(`    * Username: ${machine.qemu.username}`);
            console.log(`    * Password: ${machine.qemu.password}`);
        }
    };

    this.ssh = function() {
        if (spawnSync('sshpass', ['-V']).status !== 0)
            throw new Error('Missing sshpass binary in PATH');

        if (machines.length != 1) {
            console.error('The ssh command can only be used with one machine');
            return false;
        }

        let machine = machines[0];

        let args = [
            '-p' + machine.qemu.password,
            'ssh', '-o', 'StrictHostKeyChecking=no',
                   '-o', 'UserKnownHostsFile=' + (process.platform == 'win32' ? '\\\\.\\NUL' : '/dev/null'),
                   '-p', machine.qemu.ssh_port, machine.qemu.username + '@127.0.0.1'
        ];

        let proc = spawnSync('sshpass', args, { stdio: 'inherit' });
        if (proc.status !== 0) {
            console.error('Connection failed');
            return false;
        }

        return true;
    };

    this.reset = async function() {
        check_qemu();

        let binary = qemu_prefix + 'qemu-img' + (process.platform == 'win32' ? '.exe' : '');

        console.log('>> Restoring snapshots...')
        await Promise.all(machines.map(machine => {
            let dirname = `${__dirname}/machines/${machine.key}`;
            let disk = dirname + '/' + machine.qemu.disk;

            if (!fs.existsSync(dirname)) {
                self.log(machine, 'Missing files', chalk.bold.gray('[ignore]'));
                return;
            }

            let proc = spawnSync(binary, ['snapshot', disk, '-a', 'base'], { encoding: 'utf-8' });

            if (proc.status === 0) {
                self.log(machine, 'Reset disk', chalk.bold.green('[ok]'));
            } else {
                self.log(machine, 'Reset disk', chalk.bold.red('[error]'));

                if (proc.stderr) {
                    console.error('');

                    let align = log_align + 9;
                    let str = ' '.repeat(align) + 'Standard error:\n' +
                              chalk.yellow(proc.stderr.replace(/^/gm, ' '.repeat(align + 4))) + '\n';
                    console.error(str);
                }
            }
        }));
    };

    this.ignore = function(machine) {
        machine = self.machine(machine);
        ignore_machines.add(machine);
    };

    this.isIgnored = function(machine) {
        machine = self.machine(machine);
        return ignore_machines.has(machine);
    };

    this.machine = function(key) {
        if (typeof key != 'string')
            return key;

        let machine = map[key];
        if (machine == null)
            throw new Error(`Unknown machine '${key}'`);
        return machine;
    };

    this.log = function(machine, action, status) {
        machine = self.machine(machine);

        if (log_align == null) {
            let lengths = machines.map(machine => machine.title.length);
            log_align = Math.max(...lengths);
        }

        let align1 = Math.max(log_align - machine.title.length, 0);
        let align2 = Math.max(34 - action.length, 0);

        console.log(`     [${machine.title}]${' '.repeat(align1)}  ${action}${' '.repeat(align2)}  ${status}`);
    };

    async function boot(machine, dirname, detach) {
        let [binary, args] = make_qemu_command(machine);

        try {
            let proc = spawn(binary, args, {
                cwd: dirname,
                detached: detach,
                stdio: 'ignore'
            });
            if (detach)
                proc.unref();

            await new Promise((resolve, reject) => {
                proc.on('spawn', () => wait(2 * 1000).then(resolve));
                proc.on('error', reject);
                proc.on('exit', reject);
            });
            await join(machine, 30);

            started_machines.add(machine);
        } catch (err) {
            if (typeof err != 'number')
                throw err;

            await join(machine, 2);
        }
    }

    async function join(machine, tries) {
        let ssh = new NodeSSH;

        while (tries) {
            try {
                await ssh.connect({
                    host: '127.0.0.1',
                    port: machine.qemu.ssh_port,
                    username: machine.qemu.username,
                    password: machine.qemu.password,
                    tryKeyboard: true
                });

                break;
            } catch (err) {
                if (!--tries)
                    throw new Error(`Failed to connect to ${machine.title}`);

                // Try again... a few times
                await wait(10 * 1000);
            }
        }

        machine.ssh = ssh;
    }

    this.exec = async function(machine, cmd, cwd = null) {
        machine = self.machine(machine);

        if (typeof cmd == 'string') {
            cmd = {
                command: cmd,
                repeat: 1
            };
        }

        try {
            let ret = { code: 0 };

            if (machine.platform == 'win32') {
                let cmd_line = cmd.command;

                if (cwd != null) {
                    cwd = cwd.replaceAll('/', '\\');
                    cmd_line = `cd "${cwd}" && ${cmd_line}`;
                }

                for (let i = 0; ret.code === 0 && i < cmd.repeat; i++)
                    ret = await machine.ssh.execCommand(cmd_line);
            } else {
                for (let i = 0; ret.code === 0 && i < cmd.repeat; i++)
                    ret = await machine.ssh.execCommand(cmd.command, { cwd: cwd });
            }

            return ret;
        } catch (err) {
            console.log(err);
            return err;
        }
    };
}

// Utility

function parse_arguments(binary, args, commands = {}) {
    let command = null;
    let patterns = [];

    if (args[0] == '--help') {
        print_usage(binary);
        return;
    }
    if (args.length < 1 || args[0][0] == '-')
        throw new Error(`Missing command, use --help`);

    command = args[0];

    for (let i = 1; i < args.length; i++) {
        let arg = args[i];
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
            if (value == null && args[i + 1] != null && args[i + 1][0] != '-') {
                value = args[i + 1];
                i++; // Skip this value next iteration
            }
        }

        if (arg == '--help') {
            print_usage(binary);
            return;
        } else if (arg[0] == '-') {
            throw new Error(`Unexpected argument '${arg}'`);
        } else {
            if (arg.startsWith('__') || arg.match(/[\\/\.]/))
                throw new Error(`Build or machine pattern '${arg} is not valid`);

            patterns.push(arg);
        }
    }

    return {
        command: command,
        patterns: patterns
    };
}

function check_qemu() {
    if (qemu_prefix != null)
        return;

    if (spawnSync('qemu-img', ['--version']).status === 0) {
        qemu_prefix = '';
    } else if (process.platform == 'win32') {
        let proc = spawnSync('reg', ['query', 'HKEY_LOCAL_MACHINE\\SOFTWARE\\QEMU', '/v', 'Install_Dir']);

        if (proc.status === 0) {
            let matches = proc.stdout.toString('utf-8').match(/Install_Dir[ \t]+REG_[A-Z_]+[ \t]+(.*)+/);

            if (matches != null) {
                let prefix = matches[1].trim() + '/';
                let binary = prefix + 'qemu-img.exe';

                if (fs.existsSync(binary))
                    qemu_prefix = prefix;
            }
        }
    }

    if (qemu_prefix == null)
        throw new Error('QEMU does not seem to be installed');
}

function make_qemu_command(machine) {
    let binary = qemu_prefix + machine.qemu.binary + (process.platform == 'win32' ? '.exe' : '');
    let args = machine.qemu.arguments.slice();

    let accelerator = get_machine_accelerator(machine);

    if (accelerator != null)
        args.push('-accel', accelerator);

    return [binary, args];
}

function get_machine_accelerator(machine) {
    if (machine.qemu.binary == 'qemu-system-x86_64' ||
            machine.qemu.binary == 'qemu-system-i386') {
        switch (process.platform) {
            case 'linux': return 'kvm';
            case 'win32': return 'whpx';
        }
    }

    return null;
}

function copy_recursive(src, dest, validate = filename => true) {
    let proc = spawnSync('git', ['ls-files', '-i', '-o', '--exclude-standard', '--directory'], { cwd: src });
    let ignored = new Set(proc.stdout.toString().split('\n').map(it => it.trim().replace(/[\\\/+]$/, '').replaceAll('\\', '/')).filter(it => it));

    recurse(src, dest, '');

    function recurse(src, dest, nice) {
        let entries = fs.readdirSync(src, { withFileTypes: true });

        for (let entry of entries) {
            let src_filename = path.join(src, entry.name);
            let dest_filename = path.join(dest, entry.name);
            let filename = nice + (nice ? '/' : '') + entry.name;

            if (ignored.has(filename))
                continue;
            if (!validate(filename))
                continue;

            if (entry.isDirectory()) {
                fs.mkdirSync(dest_filename, { mode: 0o755 });
                recurse(src_filename, dest_filename, filename);
            } else if (entry.isFile()) {
                fs.copyFileSync(src_filename, dest_filename);
            }
        }
    }
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

function make_path_filter(paths) {
    let map = new Map;

    for (let path of paths) {
        let parts = path.split('/');
        let ptr = map;

        for (let part of parts) {
            let next = ptr.get(part);

            if (next == null) {
                next = new Map;
                ptr.set(part, next);
            }

            ptr = next;
        }
    }

    let filter = (path) => {
        let parts = path.split('/');
        let ptr = map;

        for (let part of parts) {
            let next = ptr.get(part);

            if (next == null)
                return false;
            if (!next.size)
                return true;

            ptr = next;
        }

        return true;
    };

    return filter;
}

function wait(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

module.exports = {
    DefaultCommands,
    parse_arguments,
    QemuRunner,
    copy_recursive,
    unlink_recursive,
    make_path_filter,
    wait
};
