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

const DIRECTORIES = [
    'src/core/base',
    'src/goupile',
    'src/rekkord'
];
const LANGUAGES = ['en', 'fr'];

main();

async function main() {
    // All the code assumes we are working from root directory
    process.chdir(__dirname + '/../..');

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
        console.log('Usage: i18n_list.js');
        return 0;
    } else {
        let opt = args.find(arg => arg.startsWith('-'));

        if (opt != null)
            throw new Error(`Invalid option '${opt}'`);
    }

    for (let root of DIRECTORIES) {
        let i18n = path.join(root, 'i18n');

        if (!fs.existsSync(i18n))
            fs.mkdirSync(i18n);

        let keys = [
            ...listFilesRec(root, '.js').flatMap(detectJsKeys)
        ];
        let messages = [
            ...listFilesRec(root, '.cc').flatMap(detectCxxMessages),
            ...listFilesRec(root, '.js').flatMap(detectJsMessages)
        ];

        keys = Array.from(new Set(keys)).sort();
        messages = Array.from(new Set(messages)).sort();

        for (let lang of LANGUAGES) {
            let filename = path.join(i18n, lang + '.json');
            let translations = fs.existsSync(filename) ? JSON.parse(fs.readFileSync(filename)) : {};

            translations = {
                keys: translations.keys ?? {},
                messages: translations.messages ?? {}
            };

            translations.keys = keys.reduce((obj, str) => { obj[str] = translations.keys[str] ?? null; return obj; }, {});
            translations.messages = messages.reduce((obj, str) => { obj[str] = translations.messages[str] ?? null; return obj; }, {});

            if (lang == 'en')
                translations.messages = {};

            fs.writeFileSync(filename, JSON.stringify(translations, null, 4));
        }
    }
}

function listFilesRec(path, ext = null) {
    let filenames = [];

    let entries = fs.readdirSync(path, { withFileTypes: true });

    for (let entry of entries) {
        let filename = path + '/' + entry.name;

        if (entry.isDirectory()) {
            let ret = listFilesRec(filename, ext);
            filenames.push(...ret);
        } else if (entry.isFile()) {
            if (ext == null || entry.name.endsWith(ext))
                filenames.push(filename);
        }
    }

    return filenames;
}

function completeObject(dest, src) {
    for (let key in src) {
        let value = src[key];

        if (value != null && typeof value == 'object') {
            if (dest[key] == null || typeof dest[key] != 'object')
                dest[key] = {};

            dest[key] = completeObject(dest[key], value);
        } else if (!dest.hasOwnProperty(key)) {
            dest[key] = null;
        }
    }

    dest = Object.keys(src).sort().reduce((obj, key) => { obj[key] = dest[key]; return obj; }, {});
    return dest;
}

function detectCxxMessages(filename) {
    let code = fs.readFileSync(filename).toString();

    let matches = [
        ...code.matchAll(/(?:T|Print|PrintLn|LogError|LogWarning|LogInfo)\(\"(.+)\"(?:, .*)?\)/g)
    ];

    return matches.map(m => m[1]);
}

function detectJsKeys(filename) {
    let code = fs.readFileSync(filename).toString();

    let matches = [
        ...code.matchAll(/T\.([a-zA-Z_0-9]+)/g)
    ];

    return matches.map(m => m[1]).filter(key => key != 'lang' && key != 'format' && key != 'message');
}

function detectJsMessages(filename) {
    let code = fs.readFileSync(filename).toString();

    let matches = [
        ...code.matchAll(/T\.message\(`(.+)`/g)
    ];

    return matches.map(m => m[1]);
}
