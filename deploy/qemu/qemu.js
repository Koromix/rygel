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
const { spawn, spawnSync } = require('child_process');
const tty = require('tty');
const { NodeSSH } = require('../../vendor/node-ssh/node-ssh.bundle.js');

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
        let machines = runner.machines;

        for (let pattern of config.patterns) {
            let re = make_wildcard_pattern(pattern);
            let match = false;

            for (let machine of machines) {
                if (machine.key.match(re) || machine.title.match(re)) {
                    runner.select(machine);
                    match = true;
                }
            }

            if (!match) {
                console.log(`Pattern '${pattern}' does not match any machine`);
                process.exit(1);
            }
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

function print_usage(binary, commands) {
    let help = `Usage: node ${binary} <command> [options...]

Commands:
`;

    for (let cmd in commands) {
        let align = Math.max(0, 28 - cmd.length);
        help += `\n    ${cmd} ${' '.repeat(align)}${commands[cmd]}`;
    }
    help = help.trim();

    console.log(help);
}

// Commands

function QemuRunner(registry = null) {
    let self = this;

    let all_machines = null;
    let known_machines = null;
    let select_machines = false;

    let ignore_machines = new Set;
    let ignore_builds = new Set;
    let connections = new WeakMap;

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

        all_machines = JSON.parse(json);
        all_machines = Object.keys(all_machines).map(key => ({
            key: key,
            ...all_machines[key]
        }));

        known_machines = all_machines.reduce((map, machine) => { map.set(machine.key, machine); return map; }, new Map);
    };

    Object.defineProperties(this, {
        machines: { get: () => all_machines.filter(machine => !ignore_machines.has(machine)), enumerable: true },
        ignoreCount: { get: () => ignore_machines.size, enumerable: true }
    });

    this.select = function(machine) {
        if (!select_machines) {
            for (let machine of all_machines)
                ignore_machines.add(machine);
            select_machines = true;
        }

        machine = self.machine(machine);
        ignore_machines.delete(machine);
    };

    this.ignore = function(machine) {
        machine = self.machine(machine);
        ignore_machines.add(machine);
    };

    this.isIgnored = function(machine) {
        machine = self.machine(machine);
        return ignore_machines.has(machine);
    };

    this.start = async function() {
        let success = true;
        let missing = 0;

        check_qemu();

        let machines = self.machines;

        console.log('>> Starting up machines...');
        await Promise.all(machines.map(async machine => {
            let dirname = `${__dirname}/machines/${machine.key}`;

            if (!fs.existsSync(dirname)) {
                self.log(machine, 'Missing files', style_ansi('[ignore]', 'gray bold'));

                ignore_machines.add(machine);
                missing++;

                return;
            }

            // Version check
            {
                let filename = dirname + '/VERSION';
                let version = fs.existsSync(filename) ? parseInt(fs.readFileSync(filename).toString(), 10) : 0;

                if (version < machine.qemu.version) {
                    self.log(machine, 'Machine version mismatch', style_ansi('[ignore]', 'gray bold'));

                    ignore_machines.add(machine);
                    success = false;

                    return;
                }
            }

            try {
                let started = await boot(machine, dirname);

                if (started) {
                    let accelerator = get_machine_accelerator(machine);
                    let action = `Start (${accelerator ?? 'emulated'})`;

                    self.logSuccess(machine, action);
                } else {
                    self.logSuccess(machine, 'Join');
                }
            } catch (err) {
                self.logError(machine, 'Start');

                ignore_machines.add(machine);
                success = false;
            }
        }));

        if (success && missing == machines.length)
            throw new Error('No machine available');

        return success;
    };

    this.stop = async function() {
        let machines = self.machines;

        if (!machines.length)
            return true;

        let success = true;

        console.log('>> Sending shutdown commands...');
        await Promise.all(machines.map(async machine => {
            let ssh = connections.get(machine);

            if (ssh == null) {
                try {
                    ssh = await self.join(machine, 2);
                } catch (err) {
                    self.logSuccess(machine, 'Already down');
                    return;
                }
            }

            try {
                await new Promise(async (resolve, reject) => {
                    ssh.connection.on('close', resolve);
                    ssh.connection.on('end', resolve);
                    wait_delay(60000).then(() => { reject(new Error('Timeout')) });

                    self.exec(machine, machine.qemu.shutdown);
                });

                self.logSuccess(machine, 'Stop');
            } catch (err) {
                self.logError(machine, 'Stop');
                success = false;
            }
        }));

        return success;
    }

    this.info = function() {
        check_qemu();

        let machines = self.machines;

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
        let machines = self.machines;
        let machine = machines[0];

        if (machines.length != 1) {
            console.error('The ssh command can only be used with one machine');
            return false;
        }

        let args = [
            '-p' + machine.qemu.password,
            'ssh', '-o', 'StrictHostKeyChecking=no',
                   '-o', 'UserKnownHostsFile=' + (process.platform == 'win32' ? '\\\\.\\NUL' : '/dev/null'),
                   '-p', machine.qemu.ssh_port, machine.qemu.username + '@127.0.0.1'
        ];

        let proc = spawnSync('sshpass', args, { stdio: 'inherit' });

        if (proc.status !== 0) {
            if (spawnSync('sshpass', ['-h']).status !== 0)
                throw new Error('Missing sshpass binary in PATH');

            console.error('Connection failed');
            return false;
        }

        return true;
    };

    this.reset = async function() {
        check_qemu();

        let machines = self.machines;

        let binary = qemu_prefix + 'qemu-img' + (process.platform == 'win32' ? '.exe' : '');

        console.log('>> Restoring snapshots...')
        await Promise.all(machines.map(machine => {
            let dirname = `${__dirname}/machines/${machine.key}`;
            let disk = dirname + '/' + machine.qemu.disk;

            if (!fs.existsSync(dirname)) {
                self.log(machine, 'Missing files', style_ansi('[ignore]', 'gray bold'));
                return;
            }

            let proc = spawnSync(binary, ['snapshot', disk, '-a', 'base'], { encoding: 'utf-8' });

            if (proc.status === 0) {
                self.logSuccess(machine, 'Reset disk');;
            } else {
                self.logError(machine, 'Reset disk');
                self.logOutput(null, proc.stderr);
            }
        }));
    };

    this.join = async function(machine, tries = 1) {
        machine = self.machine(machine);

        let ssh = connections.get(machine);

        if (ssh == null) {
            ssh = new NodeSSH;

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
                    await wait_delay(10 * 1000);
                }
            }

            ssh.connection.on('end', () => connections.delete(machine));
            ssh.connection.on('close', () => connections.delete(machine));

            connections.set(machine, ssh);
        }

        return ssh;
    };

    this.exec = async function(machine, cmd, cwd = null) {
        machine = self.machine(machine);

        if (typeof cmd == 'string') {
            cmd = {
                command: cmd,
                repeat: 1
            };
        }

        let ssh = await self.join(machine);

        try {
            let ret = { code: 0 };

            if (machine.platform == 'win32') {
                let cmd_line = cmd.command;

                if (cwd != null) {
                    cwd = cwd.replaceAll('/', '\\');
                    cmd_line = `cd "${cwd}" && ${cmd_line}`;
                }

                for (let i = 0; ret.code === 0 && i < cmd.repeat; i++)
                    ret = await ssh.execCommand(cmd_line);
            } else {
                for (let i = 0; ret.code === 0 && i < cmd.repeat; i++)
                    ret = await ssh.execCommand(cmd.command, { cwd: cwd });
            }

            return ret;
        } catch (err) {
            console.log(err);
            return err;
        }
    };

    this.machine = function(key) {
        if (typeof key != 'string')
            return key;

        let machine = known_machines.get(key);
        if (machine == null)
            throw new Error(`Unknown machine '${key}'`);
        return machine;
    };

    this.log = function(machine, action, status) {
        machine = self.machine(machine);

        if (log_align == null) {
            let lengths = self.machines.map(machine => machine.title.length);
            log_align = Math.max(...lengths);
        }

        let align1 = Math.max(log_align - machine.title.length, 0);
        let align2 = Math.max(34 - action.length, 0);

        console.log(`     [${machine.title}]${' '.repeat(align1)}  ${action}${' '.repeat(align2)}  ${status}`);
    };

    this.logSuccess = function(machine, action, message = 'ok') {
        self.log(machine, action, style_ansi('[' + message + ']', 'green bold'));
    };

    this.logError = function(machine, action, message = 'error') {
        self.log(machine, action, style_ansi('[' + message + ']', 'red bold'));
    };

    this.logOutput = function(machine, stdout, stderr) {
        if (!stdout && !stderr)
            return;
        console.error('');

        let align = log_align + 9;

        if (stdout) {
            let str = ' '.repeat(align) + 'Standard output:\n' +
                      style_ansi(stdout.replace(/^/gm, ' '.repeat(align + 4)), 'yellow') + '\n';
            console.error(str);
        }

        if (stderr) {
            let str = ' '.repeat(align) + 'Standard error:\n' +
                      style_ansi(stderr.replace(/^/gm, ' '.repeat(align + 4)), 'yellow') + '\n';
            console.error(str);
        }
    };

    async function boot(machine, dirname) {
        let [binary, args] = make_qemu_command(machine);

        try {
            let proc = spawn(binary, args, {
                cwd: dirname,
                detached: true,
                stdio: 'ignore'
            });
            proc.unref();

            await new Promise((resolve, reject) => {
                proc.on('spawn', () => wait_delay(2 * 1000).then(resolve));
                proc.on('error', reject);
                proc.on('exit', reject);
            });
            await self.join(machine, 30);

            return true;
        } catch (err) {
            if (typeof err != 'number')
                throw err;

            await self.join(machine, 2);

            return false;
        }
    }
}

// Utility

function parse_arguments(binary, args, commands) {
    let command = null;
    let patterns = [];

    if (args[0] == '--help') {
        print_usage(binary, commands);
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
            print_usage(binary, commands);
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

function make_wildcard_pattern(str) {
    str = str.replaceAll(/[\/\^\$\.\|\[\]\(\)\*\+\?\{\}]/g, c => {
        switch (c) {
            case '*': return '[^/]*';
            case '?': return '[^/]';
            default: return `\\${c}`;
        }
    });

    let pattern = `^${str}$`;
    let re = new RegExp(pattern);

    return re;
}

function style_ansi(text, styles = []) {
    if (!tty.isatty(process.stdout.fd))
        return text;

    if (typeof styles == 'string')
        styles = styles.split(' ');

    let ansi = [];

    for (let style of styles) {
        switch (style) {
            case 'black': { ansi.push('30'); } break;
            case 'red': { ansi.push('31'); } break;
            case 'green': { ansi.push('32'); } break;
            case 'yellow': { ansi.push('33'); } break;
            case 'blue': { ansi.push('34'); } break;
            case 'magenta': { ansi.push('35'); } break;
            case 'cyan': { ansi.push('36'); } break;
            case 'white': { ansi.push('37'); } break;

            case 'gray':
            case 'black+': { ansi.push('90'); } break;
            case 'red+': { ansi.push('91'); } break;
            case 'green+': { ansi.push('92'); } break;
            case 'yellow+': { ansi.push('93'); } break;
            case 'blue+': { ansi.push('94'); } break;
            case 'magenta': { ansi.push('95'); } break;
            case 'cyan+': { ansi.push('96'); } break;
            case 'white+': { ansi.push('97'); } break;

            case 'bold': { ansi.push('1'); } break;
            case 'dim': { ansi.push('2'); } break;
            case 'underline': { ansi.push('4'); } break;
        }
    }

    if (!ansi.length)
        return text;

    let str = `\x1b[${ansi.join(';')}m${text}\x1b[0m`;
    return str;
}

function wait_delay(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

module.exports = {
    DefaultCommands,
    parse_arguments,

    QemuRunner,

    copy_recursive,
    unlink_recursive,
    make_path_filter,
    make_wildcard_pattern,
    style_ansi,
    wait_delay
};
