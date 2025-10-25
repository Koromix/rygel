#!/usr/bin/env node
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import * as esbuild from '../../../vendor/esbuild/native/lib/main.js';
import fs from 'fs';
import path from 'path';
import sqlite3 from 'better-sqlite3';
import { Util, LocalDate } from '../../web/core/base.js';
import { ProjectInfo, ProjectBuilder } from '../client/core/project.js';
import { wrap } from '../client/form/data.js';
import * as XLSX from '../../../vendor/sheetjs/XLSX.bundle.js';

let imports = new Map;

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
        console.log('Usage: export_results.js database project destination');
        return 0;
    } else {
        let opt = args.find(arg => arg.startsWith('-'));

        if (opt != null)
            throw new Error(`Invalid option '${opt}'`);
    }

    let options = {
        database: args[0],
        project: args[1],
        destination: args[2]
    };

    if (!options.database)
        throw new Error('Missing database argument');
    if (!options.project)
        throw new Error('Missing project argument');
    if (!options.destination)
        throw new Error('Missing destination argument');

    let db = sqlite3(options.database, { fileMustExist: true, readonly: true });

    let project = await buildProject(options.project);

    for (let test of project.tests) {
        switch (test.type) {
            case 'form': {
                let results = loadResults(db, project.index, test.key);

                let entries = [];
                for (let result of results) {
                    let [values, fields] = await extractFields(test.build, result.data);

                    let entry = {
                        participant: result.participant,
                        values: values,
                        fields: fields
                    };

                    entries.push(entry);
                }

                let [variables, columns] = structureTable(entries);

                let filename = path.join(options.destination, test.key.substr(1) + '.xlsx');
                let tab = path.basename(test.key);

                exportXLSX(filename, tab, columns, variables, entries);
            } break;

            case 'network': {
                // XXX
            } break;

            default: throw new Error(`Unknown test type '${test.type}'`);
        }
    }
}

async function buildProject(key) {
    let info = null;
    let bundle = null;
    let project = null;

    // Get basic project info
    {
        let { PROJECTS: projects } = await importScript('../projects/projects.js');

        info = projects.find(project => project.key == key);

        if (info == null)
            throw new Error(`Project '${key}' does not exist`);
    }

    // Build project script
    bundle = await importScript(`../projects/${key}.js`);

    // Create project structure
    {
        project = new ProjectInfo(info);

        let builder = new ProjectBuilder(project);
        let start = LocalDate.fromJSDate(new Date);
        let values = {};

        bundle.init(builder, start, values);
    }

    return project;
}

function loadResults(db, study, key) {
    let rows = db.prepare('SELECT participant, json FROM tests WHERE study = ? AND key = ?').all(study, key);

    let results = rows.map(row => ({
        participant: row.participant,
        data: JSON.parse(row.json)
    }));

    return results;
}

async function extractFields(build, data) {
    let { FormState, FormModel, FormBuilder } = await importScript('../client/form/builder.js');

    let state = new FormState(data);
    let model = new FormModel;
    let builder = new FormBuilder(state, model);

    build(builder, state.values, () => {});

    let fields = [];

    for (let widget of model.widgets) {
        let key = widget.key;

        if (key == null)
            continue;

        let field = {
            key: makePropertyName(state.values, key.obj, key.name),
            label: widget.label,
            value: widget.value,
            notes: widget.notes,
            type: widget.type
        };

        if (widget.props != null)
            field.props = widget.props;

        fields.push(field);
    }

    return [state.values, fields];
}

function makePropertyName(root, obj, key) {
    if (obj == root)
        return key;

    for (let sub in root) {
        let value = root[sub];

        if (value == obj) {
            return `${sub}.${key}`;
        } else if (Util.isPodObject(value)) {
            let prop = makePropertyName(value, obj, key);
            if (prop != null)
                return `${sub}.${key}.${prop}`;
        } else if (Array.isArray(value)) {
            for (let i = 0; i < value.length; i++) {
                let prop = makePropertyName(value[i], obj, key);
                if (prop != null)
                    return `${sub}.${key}[${i}].${prop}`;
            }
        }
    }

    return null;
}

function structureTable(entries) {
    let ctx = {
        map: new Map,

        prev: null,
        next: null
    };
    ctx.previous = ctx;
    ctx.next = ctx;

    for (let entry of entries) {
        let fields = entry.fields;

        for (let i = 0; i < fields.length; i++) {
            let field = fields[i];

            if (!ctx.map.has(field.key)) {
                let previous = ctx.map.get(fields[i - 1]?.key) ?? ctx;

                let head = {
                    variable: Object.assign({}, field),
                    prev: previous,
                    next: previous.next
                };
                delete head.variable.value;

                previous.next.prev = head;
                previous.next = head;

                ctx.map.set(field.key, head);
            }
        }
    }

    let variables = [];

    // Expand linked list to proper arrays
    for (let head = ctx.next; head != ctx; head = head.next)
        variables.push(head.variable);

    let columns = expandColumns(variables);

    return [variables, columns];
}

function expandColumns(variables) {
    let columns = [];

    for (let variable of variables) {
        if (variable.type.match(/^multi[A-Z]?/)) {
            for (let prop of variable.props) {
                if (prop[0] == null)
                    continue;

                let column = {
                    name: variable.key + '.' + prop[0],
                    read: data => {
                        let value = readProperty(data, variable.key);
                        if (!Array.isArray(value))
                            return null;
                        return 0 + value.includes(prop[0]);
                    }
                };

                columns.push(column);
            }
        } else {
            let column = {
                name: variable.key,
                read: data => readProperty(data, variable.key)
            };

            columns.push(column);
        }
    }

    return columns;
}

function readProperty(obj, prop) {
    let func = new Function('obj', 'return obj.' + prop);
    return func(obj);
}

function exportXLSX(filename, tab, columns, variables, entries) {
    let dirname = path.dirname(filename);
    fs.mkdirSync(dirname, { recursive: true });

    let ws = XLSX.utils.aoa_to_sheet([['participant', ...columns.map(column => column.name)]]);
    let comments = XLSX.utils.aoa_to_sheet([['participant', 'variable', 'comment']]);
    let definitions = XLSX.utils.aoa_to_sheet([['variable', 'label', 'type']]);
    let propositions = XLSX.utils.aoa_to_sheet([['variable', 'prop', 'label']]);

    // Results
    for (let entry of entries) {
        let row = [
            entry.participant,
            ...columns.map(column => {
                let result = column.read(entry.values);
                return result ?? 'NA';
            })
        ];

        XLSX.utils.sheet_add_aoa(ws, [row], { origin: -1 });
    }

    // Comments
    for (let entry of entries) {
        for (let field of entry.fields) {
            if (!field.notes?.comment)
                continue;

            let comment = [entry.participant, field.key, field.notes.comment];
            XLSX.utils.sheet_add_aoa(comments, [comment], { origin: -1 });
        }
    }

    // Definitions and propositions
    for (let variable of variables) {
        let info = [variable.key, variable.label, variable.type];
        XLSX.utils.sheet_add_aoa(definitions, [info], { origin: -1 });

        if (variable.props != null) {
            let props = variable.props.map(prop => [variable.key, prop[0], prop[1]]);
            XLSX.utils.sheet_add_aoa(propositions, props, { origin: -1 });
        }
    }

    let wb = XLSX.utils.book_new();
    XLSX.utils.book_append_sheet(wb, ws, tab);
    XLSX.utils.book_append_sheet(wb, comments, '@comments');
    XLSX.utils.book_append_sheet(wb, definitions, '@definitions');
    XLSX.utils.book_append_sheet(wb, propositions, '@propositions');

    let buf = XLSX.write(wb, { bookType: 'xlsx', type: 'buffer' });
    fs.writeFileSync(filename, buf);
}

async function importScript(filename) {
    filename = path.join(import.meta.dirname, filename);

    let mod = imports.get(filename);

    if (mod == null) {
        let basename = path.basename(filename);
        let outdir = import.meta.dirname + '/tmp';

        await esbuild.build({
            entryPoints: [filename],
            bundle: true,
            minify: false,
            platform: 'node',
            format: 'esm',
            loader: {
                '.woff2': 'dataurl',
                '.jpg': 'file',
                '.png': 'file',
                '.webp': 'file',
                '.webm': 'file',
                '.mp3': 'file'
            },
            inject: [import.meta.dirname + '/fake/globals.js'],
            plugins: [
                {
                    name: 'lit-html',
                    setup: build => {
                        build.onResolve({ filter: /\/lit-html.bundle.js$/ }, args => {
                            return { path: import.meta.dirname + '/fake/lit-html.js' };
                        })
                    }
                }
            ],
            outdir: outdir
        });

        let destname = path.join(outdir, basename);

        mod = await import(destname);
        imports.set(filename, mod);
    }

    return mod;
}
