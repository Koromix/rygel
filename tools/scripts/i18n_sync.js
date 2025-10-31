#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

const fs = require('fs');
const path = require('path');

const CONFIG_FILENAME = 'LanguageConfig.json';

const TOLGEE_URL = (process.env.TOLGEE_URL || '').replace(/\/+$/, '');
const TOLGEE_API_KEY = process.env.TOLGEE_API_KEY || '';

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

    let action = null;
    let push = false;

    for (let arg of args) {
        if (arg == '--help') {
            printUsage();
            return 0;
        } else if (arg == '--scan') {
            action = 'scan';
        } else if (arg == '--sync') {
            action = 'sync';
        } else if (arg == '--push') {
            push = true;
        } else if (arg[0] == '-') {
            throw new Error(`Invalid option '${arg}'`);
        }
    }

    let { languages, sources } = JSON.parse(fs.readFileSync(CONFIG_FILENAME));

    if (action == null || action == 'scan')
        await scanCode(languages, sources);

    if (action == null) {
        if (TOLGEE_URL) {
            await syncTolgee(languages, sources, push);
        } else {
            console.error('Ignoring Tolgee sync because TOLGEE_URL is missing');
        }
    } else if (action == 'sync') {
        await syncTolgee(languages, sources, push);
    }

    console.log('Done');
}

function printUsage() {
    let help = `Usage: i18n_sync.js [command] [options...]

Options:

    --scan                               Only scan code
    --sync                               Only sync with Tolgee`;

    console.log(help);
}

async function scanCode(languages, sources) {
    console.log('Scanning code...');

    for (let src of sources) {
        if (!fs.existsSync(src.path))
            fs.mkdirSync(src.path);

        let keys = [
            ...src.sources.flatMap(src => listFilesRec(src, '.js').flatMap(detectJsKeys))
        ];
        let messages = [
            ...src.sources.flatMap(src => listFilesRec(src, '.cc').flatMap(detectCxxMessages)),
            ...src.sources.flatMap(src => listFilesRec(src, '.hh').flatMap(detectCxxMessages)),
            ...src.sources.flatMap(src => listFilesRec(src, '.js').flatMap(detectJsMessages))
        ];

        keys = Array.from(new Set(keys)).sort();
        messages = Array.from(new Set(messages)).sort();

        for (let lang of languages) {
            let filename = path.join(src.path, lang + '.json');
            let translations = fs.existsSync(filename) ? readTranslations(filename, false) : {};

            translations = {
                keys: translations.keys ?? {},
                messages: translations.messages ?? {}
            };

            translations.keys = keys.reduce((obj, str) => { obj[str] = translations.keys[str] ?? null; return obj; }, {});
            translations.messages = messages.reduce((obj, str) => { obj[str] = translations.messages[str] ?? null; return obj; }, {});

            if (lang == 'en')
                translations.messages = {};

            writeTranslations(filename, translations);
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
        ...code.matchAll(/(?:LogError|LogWarning|LogInfo)\(\"(.+?)\"(?:, .*)?\)/g).map(m => unescapeLiteral(m[1])),
        ...code.matchAll(/T\(\"(.+?)\"\)/g).map(m => unescapeLiteral(m[1])),
        ...code.matchAll(/T\(R\"\((.+?)\)\"\)/gs).map(m => m[1])
    ];

    return matches;
}

function detectJsKeys(filename) {
    let code = fs.readFileSync(filename).toString();

    let matches = [
        ...code.matchAll(/T\.([a-zA-Z_0-9]+)/g).map(m => m[1])
    ];

    return matches.filter(key => key != 'lang' && key != 'format' && key != 'message');
}

function detectJsMessages(filename) {
    let code = fs.readFileSync(filename).toString();

    let matches = [
        ...code.matchAll(/T\.message\(`(.+?)`/g).map(m => unescapeLiteral(m[1]))
    ];

    return matches;
}

function unescapeLiteral(str) {
    str = str.replace(/\\[\\nr"']/g, p => p[1]);
    return str;
}

async function syncTolgee(languages, sources, push = false) {
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
    let sets = loadSources(languages, sources);

    console.log('Fetching translations...');
    let translations = await fetchTranslations();

    console.log('Pushing new key strings...');
    for (let set of sets) {
        for (let key in set.keys) {
            let t = translations.find(t => t.keyNamespace == set.namespace && t.keyName == key);
            let texts = set.keys[key];

            if (t != null) {
                let changed = Object.keys(texts).some(lang => t.translations[lang].text != texts[lang]);

                if (!push || !changed)
                    continue;
            }

            let json = {
                namespace: set.namespace,
                key: key,
                translations: texts
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
            let t = translations.find(t => t.keyNamespace == set.namespace && t.keyName == msg);
            let texts = set.messages[msg];

            if (t != null) {
                let changed = Object.keys(texts).some(lang => t.translations[lang].text != texts[lang]);

                if (!push || !changed)
                    continue;
            }

            let json = {
                namespace: set.namespace,
                key: msg,
                translations: texts
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

    console.log('Update outdated markers...');
    for (let item of translations) {
        let unused = !sets.some(set => item.keyNamespace == set.namespace &&
                                       set.keys.hasOwnProperty(item.keyName) ||
                                       set.messages.hasOwnProperty(item.keyName));

        for (let lang in item.translations) {
            let translation = item.translations[lang];

            if (translation.outdated != unused) {
                let url = TOLGEE_URL + `/v2/projects/translations/${translation.id}/set-outdated-flag/${unused ? 'true' : 'false'}`;

                await fetchOrFail(url, {
                    method: 'PUT',
                    headers: {
                        'X-API-Key': TOLGEE_API_KEY,
                        'Content-Type': 'application/json'
                    },
                    body: '{}'
                });
            }
        }
    }

    console.log('Fetching translations...');
    translations = await fetchTranslations();

    console.log('Apply translations locally...');
    for (let set of sets) {
        for (let lang of set.languages) {
            let obj = {
                keys: {},
                messages: {}
            };

            let keys = Object.keys(set.keys).sort();
            let messages = Object.keys(set.messages).sort();

            for (let key of keys)
                obj.keys[key] = set.keys[key][lang] ?? null;
            for (let msg of messages)
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
            writeTranslations(filename, obj);
        }
    }
}

function loadSources(languages, sources) {
    let sets = [];

    for (let src of sources) {
        let set = {
            namespace: src.namespace,
            path: src.path,
            languages: [],
            keys: {},
            messages: {}
        };

        for (let lang of languages) {
            let filename = path.join(src.path, lang + '.json');

            if (fs.existsSync(filename)) {
                let json = readTranslations(filename, true);

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

function readTranslations(filename, flat) {
    let json = JSON.parse(fs.readFileSync(filename));

    if (flat) {
        let obj = {
            keys: flatten({}, json.keys ?? {}, ''),
            messages: json.messages ?? {}
        };

        return obj;
    } else {
        return json;
    }
}

function flatten(ret, obj, prefix) {
    for (let key in obj) {
        let value = obj[key];

        if (value != null && typeof value == 'object') {
            flatten(ret, value, prefix + key + '.');
        } else {
            ret[prefix + key] = value;
        }
    }

    return ret;
}

function writeTranslations(filename, obj) {
    let json = {
        keys: expand(obj.keys),
        messages: obj.messages
    };

    fs.writeFileSync(filename, JSON.stringify(json, null, 4));
}

function expand(obj) {
    let ret = {};

    for (let key in obj) {
        let parts = key.split('.');
        let last = parts.pop();

        let ptr = ret;

        for (let part of parts) {
            ptr[part] ??= {};
            ptr = ptr[part];
        }

        ptr[last] = obj[key];
    }

    return ret;
}
