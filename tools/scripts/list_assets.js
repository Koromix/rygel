#!/usr/bin/env node

// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

const HEADER = `// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

const EXTENSIONS = [
    '.png',
    '.webp',
    '.jpg',
    '.webm',
    '.ogg',
    '.mp3',
    '.svg'
];

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
        console.log('Usage: list_assets.js directory');
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

    let entries = fs.readdirSync('.', { recursive: true, withFileTypes: true });

    let files = entries.filter(filterEntry).map(entry => {
        let filename = path.join(entry.path ?? entry.parentPath, entry.name);
        let ext = path.extname(filename);
        let key = filename.substr(0, filename.length - ext.length);

        return {
            key: key,
            filename: filename
        };
    });

    // Check for duplicates
    {
        let keys = new Set;

        for (let file of files) {
            if (keys.has(file.key))
                throw new Error(`Duplicate asset key '${file.key}'`);
            keys.add(file.key);
        }
    }

    process.stdout.write(HEADER + '\n');
    for (let i = 0; i < files.length; i++) {
        let file = files[i];
        process.stdout.write(`import asset${i} from './${file.filename}';\n`);
    }
    process.stdout.write('\n');

    process.stdout.write('const ASSETS = {\n');
    for (let i = 0; i < files.length; i++) {
        let file = files[i];
        process.stdout.write(`    '${file.key}': asset${i},\n`);
    }
    process.stdout.write('};\n\n');

    process.stdout.write('export { ASSETS }\n');
}

function filterEntry(entry) {
    let ext = path.extname(entry.name);

    if (!entry.isFile())
        return false;
    if (!EXTENSIONS.includes(ext))
        return false;

    return true;
}
