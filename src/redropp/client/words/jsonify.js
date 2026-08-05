#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const fs = require('node:fs');

const LANGUAGES = ['en', 'fr'];

main();

function main() {
    process.chdir(__dirname);

    let dictionaries = {};

    for (let language of LANGUAGES) {
        let text = fs.readFileSync(language + '.txt').toString('utf-8');
        let words = Array.from(new Set(text.split('\n').map(word => word.trim())));

        dictionaries[language] = words;
    }

    let json = JSON.stringify(dictionaries, null, 4);
    fs.writeFileSync('../words.json', json);
}
