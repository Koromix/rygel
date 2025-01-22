#!/usr/bin/env node

// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

const fs = require('fs');
const path = require('path');

const HEADER = `// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
`;

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
    let args = process.argv.slice(2);

    if (args.includes('--help')) {
        console.log('Usage: list_images.js directory');
        return 0;
    } else {
        let opt = args.find(arg => arg.startsWith('-'));

        if (opt != null)
            throw new Error(`Invalid option '${opt}'`);
    }

    let root = args[0];
    if (root == null)
        throw new Error('Missing directory argument');

    process.chdir(root);

    let prefix = path.basename(process.cwd());
    let entries = fs.readdirSync('.', { recursive: true, withFileTypes: true });

    let files = entries.filter(entry => entry.isFile()).map(entry => {
        let key = path.join(entry.path, entry.name);
        let src = path.join(prefix, key);

        return {
            key: key,
            src: src
        };
    });

    process.stdout.write(HEADER + '\n');
    for (let i = 0; i < files.length; i++) {
        let file = files[i];
        process.stdout.write(`import asset${i} from './${file.src}';\n`);
    }
    process.stdout.write('\n');

    process.stdout.write('const assets = {\n');
    for (let i = 0; i < files.length; i++) {
        let file = files[i];
        process.stdout.write(`    '${file.key}': asset${i},\n`);
    }
    process.stdout.write('};\n\n');

    process.stdout.write('export { assets };\n');
}
