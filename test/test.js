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
const exec = util.promisify(require('child_process').exec);
const { NodeSSH } = require('node-ssh');

async function main() {
    let root_dir = fs.realpathSync(path.dirname(__filename));
    process.chdir(root_dir);

    let machines = [];
    let display = false;
    let ignore = new Set;

    for (let i = 2; i < process.argv.length; i++) {
        let arg = process.argv[i];

        if (arg[0] == '-') {
            switch (arg) {
                case '-d':
                case '--display': { display = true; } break;

                default: {
                    console.error(`Unknown option '${arg}'`);
                    process.exit(1);
                }
            }
        } else {
            let filename = `qemu/${arg}/run.json`;

            if (arg.match(/[\\/\.]/) || !fs.existsSync(filename)) {
                console.error(`Machine '${arg} does not exist`);
                process.exit(1);
            }

            machines.push(arg);
        }
    }
    if (!machines.length) {
        machines = fs.readdirSync('qemu/').filter(dir => {
            let filename = `qemu/${dir}/run.json`;
            return fs.existsSync(filename);
        });

        if (!machines.length) {
            console.error('Could not detect any machine');
            process.exit(1);
        }
    }

    console.log('>> Starting up machines...');
    machines = await Promise.all(machines.map(async name => {
        let dirname = `qemu/${name}`;

        let json = fs.readFileSync(dirname + '/run.json', { encoding: 'utf-8' });
        let machine = JSON.parse(json);

        try {
            let [p, ssh] = await boot(machine, dirname, display);

            machine.p = p;
            machine.ssh = ssh;
        } catch (err) {
            ignore.add(machine);
            console.error(err);
        }

        return machine;
    }));

    console.log('>> Copy source code...');
    await Promise.all(machines.map(async machine => {
        if (ignore.has(machine)) {
            console.error(`     [${machine.name}] Ignored`);
            return;
        }

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
                           basename !== 'qemu' &&
                           basename !== 'node_modules' &&
                           basename !== 'node' &&
                           basename !== 'build';
                }
            });

            console.log(`     [${machine.name}] Copy ✓`);
        } catch (err) {
            console.log(`     [${machine.name}] Copy ☓`);
            ignore.add(machine);
        }
    }));

    let success = true;

    console.log('>> Run test commands...');
    await Promise.all(machines.map(async machine => {
        if (ignore.has(machine)) {
            console.error(`     [${machine.name}] Ignored`);
            return;
        }

        for (let name in machine.commands) {
            let cmd = machine.commands[name];
            let cwd = machine.info.home + '/luigi/koffi';

            let ret = await execCommand(machine, cmd, cwd);

            if (ret.code == 0) {
                console.log(`     [${machine.name}] ${name} ✓`);
            } else {
                console.error(`     [${machine.name}] ${name} ☓`);
                console.error(ret.stderr);

                success = false;
            }
        }
    }));

    if (!display) {
        console.log('>> Cleaning up code...');
        await Promise.all(machines.map(async machine => {
            let remote_dir = machine.info.home + '/luigi';

            try {
                await machine.ssh.exec('rm', ['-rf', remote_dir]);
            } catch (err) {
                // Fails often on Windows (busy directory or whatever), but rarely a problem
            }

            console.log(`     [${machine.name}] Delete ✓`);
        }));

        console.log('>> Sending shutdown commands...');
        for (let machine of machines)
            execCommand(machine, machine.info.shutdown);
        await Promise.all(machines.map(machine => machine.p));
    }

    process.exit(!success);
}
main();

async function boot(machine, dirname, display) {
    let qemu = machine.qemu;
    if (!display)
        qemu += ' -display none';

    let p = exec(qemu, { cwd: dirname });
    await wait(30 * 1000);

    let ssh = new NodeSSH;

    for (let i = 0; i < 30; i++) {
        try {
            await ssh.connect({
                host: '127.0.0.1',
                port: machine.info.port,
                username: machine.info.username,
                password: machine.info.password
            });

            break;
        } catch (err) {
            // Try again... a few times
            await wait(10 * 1000);
        }
    }

    if (!ssh.isConnected())
        throw new Error(`Failed to connect to ${machine.name}`);

    if (machine.upload != null) {
        for (let src in machine.upload) {
            let dest = machine.upload[src];

            await ssh.putFile(dirname + '/' + src, dest);
        }
    }

    console.log(`     [${machine.name}] Start ✓`);
    p.then(() => console.log(`     [${machine.name}] Stop ✓`));

    return [p, ssh];
}

function wait(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
}

async function execCommand(machine, cmd, cwd = null) {
    try {
        if (machine.info.windows) {
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

function print_usage() {

}
