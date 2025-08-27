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

const SOURCES = [
    {
        namespace: 'core/base',
        from: 'src/core/base',
        path: 'src/core/base/i18n'
    },
    {
        namespace: 'goupile',
        from: 'src/goupile',
        path: 'src/goupile/i18n'
    },
    {
        namespace: 'meestic',
        from: 'src/meestic',
        path: 'src/meestic/i18n'
    },
    {
        namespace: 'rekkord',
        from: 'src/rekkord',
        path: 'src/rekkord/i18n'
    },
    {
        namespace: 'staks',
        from: 'src/staks/src',
        path: 'src/staks/i18n'
    }
];
const LANGUAGES = ['en', 'fr'];

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
    let sets = loadSources();

    console.log('Fetching translations...');
    let translations = await fetchTranslations();

    console.log('Pushing new key strings...');
    for (let set of sets) {
        for (let key in set.keys) {
            if (translations.find(t => t.keyNamespace == set.namespace && t.keyName == key))
                continue;

            let json = {
                namespace: set.namespace,
                key: key,
                translations: set.keys[key]
            };

            await fetchOrFail(TOLGEE_URL + '/v2/projects/translations', {
                method: 'POST',
                headers: {
                    'X-API-Key': TOLGEE_API_KEY,
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(json)
            }).then(response => response.json());
        }
    }

    console.log('Pushing new message strings...');
    for (let set of sets) {
        for (let msg in set.messages) {
            if (translations.find(t => t.keyNamespace == set.namespace && t.keyName == msg))
                continue;

            let json = {
                namespace: set.namespace,
                key: msg,
                translations: set.messages[msg]
            };

            await fetchOrFail(TOLGEE_URL + '/v2/projects/translations', {
                method: 'POST',
                headers: {
                    'X-API-Key': TOLGEE_API_KEY,
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(json)
            }).then(response => response.json());
        }
    }

    console.log('Delete unneeded strings...');
    {
        let unused = translations.filter(t => !sets.some(set => t.keyNamespace == set.namespace &&
                                                                set.keys.hasOwnProperty(t.keyName) ||
                                                                set.messages.hasOwnProperty(t.keyName)));
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
    for (let set of sets) {
        for (let lang of set.languages) {
            let obj = {
                keys: {},
                messages: {}
            };

            for (let key in set.keys)
                obj.keys[key] = set.keys[key][lang] ?? null;
            for (let msg in set.messages)
                obj.messages[msg] = set.messages[msg][lang] ?? null;

            for (let item of translations) {
                if (item.keyNamespace != set.namespace)
                    continue;

                if (obj.keys.hasOwnProperty(item.keyName))
                    obj.keys[item.keyName] = item.translations[lang].text;
                if (obj.messages.hasOwnProperty(item.keyName))
                    obj.messages[item.keyName] = item.translations[lang].text;
            }

            if (lang == 'en')
                obj.messages = {};

            let filename = path.join(set.path, lang + '.json');
            fs.writeFileSync(filename, JSON.stringify(obj, null, 4));
        }
    }
}

function loadSources() {
    let sets = [];

    for (let src of SOURCES) {
        let set = {
            namespace: src.namespace,
            path: src.path,
            languages: [],
            keys: {},
            messages: {}
        };

        for (let lang of LANGUAGES) {
            let filename = path.join(src.path, lang + '.json');

            if (fs.existsSync(filename)) {
                let json = JSON.parse(fs.readFileSync(filename));

                json.keys ??= {};
                json.messages ??= {};

                set.languages.push(lang);

                for (let key in json.keys) {
                    if (set.keys[key] == null)
                        set.keys[key] = {};
                    set.keys[key][lang] = json.keys[key];
                }
                for (let msg in json.messages) {
                    if (set.messages[msg] == null)
                        set.messages[msg] = { en: msg };
                    set.messages[msg][lang] = json.messages[msg];
                }
            }
        }

        sets.push(set);
    }

    return sets;
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
