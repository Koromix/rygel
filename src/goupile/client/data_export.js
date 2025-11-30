// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net, Mutex, LocalDate } from 'lib/web/base/base.js';
import * as Data from 'lib/web/base/data.js';
import { Base64 } from 'lib/web/base/mixer.js';
import * as goupile from './goupile.js';
import { profile } from './goupile.js';
import * as UI from './ui.js';

let XLSX = null;

async function createExport(thread = null, anchor = null) {
    let info = await Net.post(`${ENV.urls.instance}api/export/create`, {
        thread: thread,
        anchor: anchor
    });

    return info;
}

async function exportRecords(id, secret, stores, template = {}) {
    if (XLSX == null)
        XLSX = await import(`${ENV.urls.static}sheetjs/XLSX.bundle.js`);

    let { name, threads, tables, counters } = await walkThreads(id, secret);

    // Metadata worksheets
    let meta = {
        definitions: XLSX.utils.aoa_to_sheet([['table', 'variable', 'label', 'type']]),
        propositions: XLSX.utils.aoa_to_sheet([['table', 'variable', 'prop', 'label']]),
        counters: null
    };

    // Prepare worksheet for counters
    if (counters.length) {
        let columns = ['__tid', ...counters];
        meta.counters = XLSX.utils.aoa_to_sheet([columns]);
    }

    // Create base worksheets
    let worksheets = stores.map(store => {
        let table = tables[store];

        if (table == null)
            return null;

        // Definitions and propositions
        for (let variable of table.variables) {
            let info = [store, variable.key, variable.label, variable.type];
            XLSX.utils.sheet_add_aoa(meta.definitions, [info], { origin: -1 });

            if (variable.props != null) {
                let props = variable.props.filter(prop => prop.value != null).map(prop => [store, variable.key, prop.value, prop.label]);
                XLSX.utils.sheet_add_aoa(meta.propositions, props, { origin: -1 });
            }
        }

        // Create worksheet
        let header = [
            '__tid', '__sequence',
            ...table.columns.map(column => column.name)
        ];
        let ws = XLSX.utils.aoa_to_sheet([header]);

        return ws;
    });

    // Export data
    for (let thread of threads) {
        for (let i = 0; i < stores.length; i++) {
            let store = stores[i];
            let ws = worksheets[i];

            let table = tables[store];
            let entry = thread.entries[store];

            if (table == null || entry == null)
                continue;

            let row = [
                thread.tid, thread.sequence,
                ...table.columns.map(column => {
                    let result = column.read(entry.data);
                    return result ?? 'NA';
                })
            ];

            XLSX.utils.sheet_add_aoa(ws, [row], { origin: -1 });
            table.length++;
        }

        if (meta.counters != null) {
            let row = [
                thread.tid,
                ...counters.map(key => {
                    let counter = thread.counters[key] ?? thread.secrets[key];
                    return counter ?? 'NA';
                })
            ];

            XLSX.utils.sheet_add_aoa(meta.counters, [row], { origin: -1 });
        }
    }

    let wb = null;
    let sheets = null;

    if (template.sheets != null)
        sheets = Object.fromEntries(Object.entries(template.sheets).map(e => [e[1], e[0]]));

    // Create or load workbook...
    if (template.xlsx != null) {
        let response = await Net.fetch(template.xlsx);
        if (!response.ok)
            throw new Error(T.message(`Cannot fetch XLSX template '{1}'`, template.xlsx));
        let buf = await response.arrayBuffer();

        wb = XLSX.read(buf);

        for (let i = 0; i < stores.length; i++) {
            let sheet = (sheets != null) ? sheets[stores[i]] : stores[i];
            let ws = worksheets[i];

            if (sheet == null || ws == null)
                continue;
            if (wb.Sheets[sheet] == null)
                continue;

            updateSheet(wb.Sheets[sheet], ws);
        }
        for (let key in meta) {
            let sheet = (sheets != null) ? sheets['@' + key] : ('@' + key);
            let ws = meta[key];

            if (sheet == null || ws == null)
                continue;
            if (wb.Sheets[sheet] == null)
                continue;

            updateSheet(wb.Sheets[sheet], ws);
        }
    } else {
        wb = XLSX.utils.book_new();

        for (let i = 0; i < stores.length; i++) {
            let sheet = (sheets != null) ? sheets[stores[i]] : stores[i];
            let ws = worksheets[i];

            if (sheet == null || ws == null)
                continue;

            XLSX.utils.book_append_sheet(wb, ws, sheet);
        }
        for (let key in meta) {
            let sheet = (sheets != null) ? sheets['@' + key] : ('@' + key);
            let ws = meta[key];

            if (sheet == null || ws == null)
                continue;

            XLSX.utils.book_append_sheet(wb, ws, sheet);
        }
    }

    // ... and export it!
    let filename = `export_${name}.xlsx`;
    XLSX.writeFile(wb, filename);
}

async function walkThreads(id, secret) {
    let url = Util.pasteURL(`${ENV.urls.instance}api/export/download`, { export: id });
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

function updateSheet(dest, src) {
    let from = listColumns(src);
    let to = listColumns(dest);

    if (from == null || to == null)
        return;

    let map = to.reduce((obj, key, idx) => { obj[key] = idx; return obj }, {});
    let end = XLSX.utils.decode_range(src['!ref']).e.r + 1;

    for (let row = 1; row < end; row++) {
        let cells = new Array(to.length).fill('');

        for (let column = 0; column < from.length; column++) {
            let to = map[from[column]];
            if (to == null)
                continue;

            let addr = XLSX.utils.encode_cell({ r: row, c: column });
            cells[to] = src[addr]?.v ?? '';
        }

        XLSX.utils.sheet_add_aoa(dest, [cells], { origin: row });
    }
}

function listColumns(ws) {
    let columns = [];

    for (;;) {
        let addr = XLSX.utils.encode_cell({ r: 0, c: columns.length });
        let value = ws[addr]?.v;
        if (value == null)
            break;
        columns.push('' + value);
    }

    return columns.length ? columns : null;
}

export {
    createExport,
    exportRecords
}
