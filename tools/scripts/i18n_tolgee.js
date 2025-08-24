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

const PROJECTS = {
    'core/base': 'src/core/base',
    'goupile': 'src/goupile',
    'rekkord': 'src/rekkord'
};
const LANGUAGES = ['fr'];

const TOLGEE_URL = (process.env.TOLGEE_URL || 'https://app.tolgee.io').replace(/\/+$/, '');
const TOLGEE_API_KEY = process.env.TOLGEE_API_KEY || '';

main();

async function main() {
    // All the code assumes we are working from root directory
    process.chdir(__dirname + '/../..');

    try {
        await run();
        console.log('Done');
    } catch (err) {
        console.error(err);
        process.exit(1);
    }
}

async function run() {
    // Check prerequisites
    {
        let errors = [];

        if (!TOLGEE_URL)
            errors.push('Missing TOLGEE_URL');
        if (!TOLGEE_API_KEY)
            errors.push('Missing TOLGEE_API_KEY');

        if (errors.length)
            throw new Error(errors.join('\n'));
    }

    console.log('Loading languages...');
    let projects = loadProjects();

    console.log('Fetching translations...');
    let translations = await fetchTranslations();

    console.log('Pushing new message strings...');
    for (let project of projects) {
        for (let key of project.keys.values()) {
            if (translations.find(t => t.keyName == key))
                continue;

            let obj = { en: key };

            for (let lang of LANGUAGES)
                obj[lang] = project.translations[lang][key];

            let json = await fetchOrFail(TOLGEE_URL + '/v2/projects/translations', {
                method: 'POST',
                headers: {
                    'X-API-Key': TOLGEE_API_KEY,
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    key: key,
                    namespace: project.name,
                    translations: obj
                })
            }).then(response => response.json());
        }
    }

    console.log('Delete unneeded strings...');
    {
        let unused = translations.filter(t => !projects.some(project => project.keys.has(t.keyName)));
        let ids = unused.map(t => t.keyId);

        if (ids.length) {
            await fetchOrFail(TOLGEE_URL + '/v2/projects/keys', {
                method: 'DELETE',
                headers: {
                    'X-API-Key': TOLGEE_API_KEY,
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    ids: ids
                })
            });
        }
    }

    console.log('Fetching translations (again)...');
    translations = await fetchTranslations();

    console.log('Apply translations locally...');
    for (let project of projects) {
        for (let lang in project.translations) {
            let target = project.translations[lang];

            for (let item of translations) {
                if (!target.hasOwnProperty(item.keyName))
                    continue;

                target[item.keyName] = item.translations[lang].text;
            }

            let filename = path.join(project.root, 'i18n', lang + '.json');
            fs.writeFileSync(filename, JSON.stringify(target, null, 4));
        }
    }
}

function loadProjects() {
    let projects = [];

    for (let name in PROJECTS) {
        let root = PROJECTS[name];

        let project = {
            name: name,
            root: root,
            keys: [],
            translations: {}
        };

        for (let lang of LANGUAGES) {
            let filename = path.join(root, 'i18n', lang + '.json');

            if (fs.existsSync(filename)) {
                let translations = JSON.parse(fs.readFileSync(filename));

                project.translations[lang] = translations;
                project.keys.push(...Object.keys(translations));
            } else {
                project.translations[lang] = {};
            }
        }

        project.keys = new Set(project.keys);
        projects.push(project);
    }

    return projects;
}

async function fetchTranslations() {
    let translations = [];
    let cursor = null;

    for (;;) {
        let params = {
            size: 100
        };
        if (cursor != null)
            params.cursor = cursor;

        let url = TOLGEE_URL + '/v2/projects/translations?' + new URLSearchParams(params);

        let response = await fetchOrFail(url, {
            headers: {
                'X-API-Key': TOLGEE_API_KEY
            }
        });

        let json = await response.json();

        if (json._embedded == null)
            break;
        cursor = json.nextCursor;

        translations.push(...json._embedded.keys);
    }

    return translations;
}

async function fetchOrFail(url, options = {}) {
    let response = await fetch(url, options);

    if (!response.ok) {
        let text = (await response.text()).trim();
        throw new Error(text);
    }

    return response;
}
