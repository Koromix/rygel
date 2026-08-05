#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

const fs = require('node:fs');

const LANGUAGES = ['en', 'fr'];

main();

function main() {
    process.chdir(__dirname);

    let code =
`// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

#include "lib/native/base/base.hh"

namespace K {

`;

    for (let i = 0; i < LANGUAGES.length; i++) {
        let text = fs.readFileSync(LANGUAGES[i] + '.txt').toString('utf-8');
        let words = new Set(text.split('\n').filter(word => word).map(capitalize));

        code += `static const char *const Words${i}[] = {\n`;
        for (let word of words.values())
            code += `    "${word}",\n`;
        code += '};\n\n';
    }

    code += 'static const HashMap<const char *, Span<const char *const>> CodeWords = {\n';
    for (let i = 0; i < LANGUAGES.length; i++) {
        code += `    { "${LANGUAGES[i]}", Words${i} },\n`;
    }
    code += '};\n\n';

    code += '}\n';

    fs.writeFileSync('../words.hh', code);
}

function capitalize(str) {
    if (!str)
        return str;

    return str[0].toUpperCase() + str.substr(1).toLowerCase();
}
