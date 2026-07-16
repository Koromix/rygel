// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, unsafeHTML } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, Mutex, LocalDate } from 'lib/web/base/base.js';
import { Base64 } from 'lib/web/base/mixer.js';
import * as Data from 'lib/web/base/data.js';
import * as goupile from './goupile.js';
import { profile } from './goupile.js';
import * as UI from './ui.js';

let XLSX = null;

let row_counters = new WeakMap;

async function runExportDialog(e, app) {
    let downloads = await Net.get(`${ENV.urls.instance}api/bulk/list`);

    let stores = app.stores.map(store => store.key);
    let template = app.exports;

    downloads.reverse();

    await UI.dialog(e, T.exports, {}, (d, resolve, reject) => {
        let intf = d.tabs('tabs', () => {
            d.tab(T.new_export, () => {
                if (downloads.length > 0) {
                    d.enumRadio('mode', T.export_mode, [
                        ['all', T.export_all],
                        ['thread', T.export_new],
                        ['anchor', T.export_anchor]
                    ], { value: 'all', untoggle: false });

                    if (d.values.mode != 'all') {
                        let props = downloads.map(download => [download.export, (new Date(download.ctime)).toLocaleString()]);
                        d.enumDrop('since', T.export_since, props, { value: downloads[0]?.export, untoggle: false });
                    }
                } else {
                    d.output(T.create_first_export);
                }

                d.action(T.create_export, {}, async () => {
                    let thread = null;
                    let anchor = null;

                    switch (d.values.mode) {
                        case 'thread': {
                            let download = downloads.find(download => download.export == d.values.since);
                            thread = download.thread + 1;
                        } break;

                        case 'anchor': {
                            let download = downloads.find(download => download.export == d.values.since);
                            anchor = download.anchor + 1;
                        } break;
                    }

                    await create(thread, anchor);

                    resolve();
                });
            }, { disabled: !goupile.hasPermission('bulk_export') });

            d.tab(T.past_exports, () => {
                d.output(html`
                    <table class="ui_table">
                        <colgroup>
                            <col/>
                            <col/>
                            <col/>
                            <col/>
                        </colgroup>

                        <thead>
                            <tr>
                                <th>${T.date}</th>
                                <th>${T.threads}</th>
                                <th>${T.automatic}</th>
                                <th>${T.download}</th>
                            </tr>
                        </thead>

                        <tbody>
                            ${downloads.map(download => html`
                                <tr>
                                    <td>${(new Date(download.ctime)).toLocaleString()}</td>
                                    <td>${download.threads}</td>
                                    <td>${download.scheduled ? T.yes : T.no}</td>
                                    <td>
                                        ${download.threads ? html`<a @click=${UI.wrap(e => exportRecords(download.export, null, stores, template))}>${T.download}</a>` : ''}
                                        ${!download.threads ? '(' + T.not_available.toLowerCase() + ')' : ''}
                                    </td>
                                </tr>
                            `)}
                            ${!downloads.length ? html`<tr><td colspan="4">${T.no_export}</td></tr>` : ''}
                        </tbody>
                    </table>
                `);
            }, { disabled: !goupile.hasPermission('bulk_download') });
        });
    });

    async function create(thread, anchor) {
        let progress = Log.progress(T.export_in_progress);

        try {
            let info = await createExport(thread, anchor);
            let stores = app.stores.map(store => store.key);

            await exportRecords(info.export, info.secret, stores, template);

            progress.success(T.export_done);
        } catch (err) {
            progress.close();
            Log.error(err);
        }
    }
}

async function createExport(thread = null, anchor = null) {
    let info = await Net.post(`${ENV.urls.instance}api/bulk/export`, {
        thread: thread,
        anchor: anchor
    });

    return info;
}

async function exportRecords(id, secret, stores, options = {}) {
    if (XLSX == null)
        XLSX = await import(`${ENV.urls.static}xlsx.js`);

    let { name, threads, tables, counters } = await walkThreads(id, secret);

    let wb = null;
    let sheets = null;

    if (options.xlsx != null) {
        let response = await Net.fetch(options.xlsx);
        if (!response.ok)
            throw new Error(T.message(`Cannot fetch XLSX template '{1}'`, options.xlsx));
        let buf = await response.bytes();

        wb = await XLSX.open(buf);
        sheets = Object.fromEntries(Object.keys(wb.sheets).map(key => [key, key]));
    } else {
        wb = new XLSX.Workbook({
            app: 'Goupile',
            version: ENV.version
        });
    }

    if (options.sheets != null)
        sheets = Object.fromEntries(Object.entries(options.sheets).map(e => [e[1], e[0]]));

    await fillWorkbook(wb, stores, threads, tables, counters, sheets);

    let filename = `export_${name}.xlsx`;
    let blob = await wb.save();

    Util.saveFile(blob, filename);
}

async function fillWorkbook(wb, stores, threads, tables, counters, sheets) {
    // Metadata worksheets
    let meta = {
        definitions: wb.createSheet(),
        propositions: wb.createSheet(),
        counters: null
    };

    wb.writeSpan(meta.definitions, refNextRow(meta.definitions), ['table', 'variable', 'label', 'type']);
    wb.writeSpan(meta.propositions, refNextRow(meta.propositions), ['table', 'variable', 'prop', 'label']);

    // Prepare definitions and propositions meta tablers
    for (let store of stores) {
        let table = tables[store];

        if (table == null)
            continue;

        // Definitions and propositions
        for (let variable of table.variables) {
            let values = [store, variable.key, variable.label, variable.type];
            wb.writeSpan(meta.definitions, refNextRow(meta.definitions), values);

            if (variable.props != null) {
                for (let prop of variable.props) {
                    if (prop.value == null)
                        continue;

                    let values = [store, variable.key, prop.value, prop.label];
                    wb.writeSpan(meta.propositions, refNextRow(meta.propositions), values);
                }
            }
        }
    }

    // Prepare worksheet for counters (if any)
    if (counters.length) {
        meta.counters = wb.createSheet();

        let columns = ['__tid', ...counters];
        wb.writeSpan(meta.counters, refNextRow(meta.counters), columns);
    }

    let worksheets = [];
    let headers = [];

    // Create or reuse base worksheets
    for (let store of stores) {
        let table = tables[store];
        let name = (sheets != null) ? sheets[store] : store;
        let ws = wb.sheets[name];

        if (ws != null) {
            let header = wb.readSpan(ws, 'A1');

            refNextRow(ws);

            worksheets.push(ws);
            headers.push(header);
        } else if (name != null && table != null) {
            let ws = wb.createSheet();

            let header = [
                '__tid', '__sequence', '__hid',
                ...table.columns.map(column => column.name)
            ];
            wb.writeSpan(ws, refNextRow(ws), header);

            worksheets.push(ws);
            headers.push(header);
        } else {
            worksheets.push(null);
            headers.push(null);
        }
    }

    // Export data
    for (let thread of threads) {
        for (let i = 0; i < stores.length; i++) {
            let store = stores[i];
            let ws = worksheets[i];
            let header = headers[i];

            let table = tables[store];
            let entry = thread.entries[store];

            if (table == null || entry == null || ws == null)
                continue;

            let values = {
                '__tid': thread.tid,
                '__sequence': thread.sequence,
                '__hid': thread.hid,

                ...Object.fromEntries(table.columns.map(column => {
                    let result = column.read(entry.data);
                    return [column.name, result ?? 'NA'];
                }))
            };
            let cells = header.map(key => values[key]);

            wb.writeSpan(ws, refNextRow(ws), cells);
        }

        if (meta.counters != null) {
            let values = [
                thread.tid,
                ...counters.map(key => {
                    let counter = thread.counters[key] ?? thread.secrets[key];
                    return counter ?? 'NA';
                })
            ];

            wb.writeSpan(meta.counters, refNextRow(meta.counters), values);
        }
    }

    // Prepare final set of Excel sheets
    for (let i = 0; i < stores.length; i++) {
        let name = (sheets != null) ? sheets[stores[i]] : stores[i];
        let sheet = worksheets[i];

        if (name == null || sheet == null)
            continue;

        // Respect XLSX limit
        name = name.substr(0, 30);

        wb.sheets[name] = sheet;
    }
    for (let key in meta) {
        let name = (sheets != null) ? sheets['@' + key] : ('@' + key);
        let sheet = meta[key];

        if (name == null || sheet == null)
            continue;

        wb.sheets[name] = sheet;
    }
}

function refNextRow(obj) {
    let row = row_counters.get(obj) ?? 0;
    let ref = XLSX.makeRef(0, row);

    row_counters.set(obj, row + 1);

    return ref;
}

async function walkThreads(id, secret) {
    let url = Util.pasteURL(`${ENV.urls.instance}api/bulk/download`, { export: id });
    let response = await Net.fetch(url, {
        headers: { 'X-Export-Secret': secret }
    });

    if (!response.ok) {
        let err = await Net.readError(response);
        throw new Error(err);
    }

    let json = await response.json();
    let name = response.headers.get('X-Export-Name');
    let threads = Array.isArray(json) ? json : json.threads;

    let tables = {};
    let counters = new Set;

    // Reconstitute logical order
    for (let thread of threads) {
        for (let key in thread.counters)
            counters.add(key);
        for (let key in thread.secrets)
            counters.add(key);

        for (let store in thread.entries) {
            let entry = thread.entries[store];
            let table = tables[store];

            if (table == null) {
                table = {
                    variables: new Map,

                    prev: null,
                    next: null,
                };
                table.previous = table;
                table.next = table;

                tables[store] = table;
            }

            let [raw, obj] = Data.wrap(entry.data);
            let keys = Object.keys(obj);

            // Skip undocumented keys
            keys = keys.filter(key => {
                let notes = Data.annotate(obj, key);

                if (!Util.isPodObject(notes.variable))
                    return false;
                if (!Object.keys(notes.variable).length)
                    return false;

                return true;
            });

            for (let i = 0; i < keys.length; i++) {
                let key = keys[i];
                let notes = Data.annotate(obj, key);

                if (!table.variables.has(key)) {
                    let previous = table.variables.get(keys[i - 1]) ?? table;

                    let head = {
                        variable: { key: key, ...notes.variable },
                        prev: previous,
                        next: previous.next
                    };

                    previous.next.prev = head;
                    previous.next = head;

                    table.variables.set(key, head);
                }
            }

            entry.data = Data.unwrap(obj);
        }
    }

    // Expand linked list to proper arrays
    for (let store in tables) {
        let table = tables[store];
        let variables = [];

        for (let head = table.next; head != table; head = head.next)
            variables.push(head.variable);

        tables[store] = {
            store: store,
            variables: variables,
            columns: expandColumns(variables),
            length: 0
        };
    }

    counters = Array.from(counters);

    return {
        name: name,
        threads: threads,
        tables: tables,
        counters: counters
    };
}

function expandColumns(variables) {
    let columns = [];

    for (let variable of variables) {
        if (variable.type == 'multi') {
            for (let prop of variable.props) {
                if (prop.value == null)
                    continue;

                let column = {
                    name: variable.key + '.' + prop.value,
                    read: data => {
                        let value = data[variable.key];
                        if (!Array.isArray(value))
                            return null;
                        return 0 + value.includes(prop.value);
                    }
                };

                columns.push(column);
            }
        } else {
            let column = {
                name: variable.key,
                read: data => data[variable.key]
            };

            columns.push(column);
        }
    }

    return columns;
}

export {
    runExportDialog,

    createExport,
    exportRecords
}
