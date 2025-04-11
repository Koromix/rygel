// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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

import { Util, Log, Net, Mutex, LocalDate } from '../../web/core/base.js';
import * as Data from '../../web/core/data.js';
import { Base64 } from '../../web/core/mixer.js';
import * as goupile from './goupile.js';
import { profile } from './goupile.js';
import * as UI from './ui.js';

async function createExport(sequence = null, anchor = null) {
    let json = await Net.post(`${ENV.urls.instance}api/export/create`, {
        sequence: sequence,
        anchor: anchor
    });
    let export_id = json.export;

    return export_id;
}

async function exportRecords(export_id, stores, filter = null) {
    if (filter == null)
        filter = () => true;

    let XLSX = await import(`${ENV.urls.static}sheetjs/XLSX.bundle.js`);

    let [threads, tables, counters] = await walkThreads(export_id);

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
            if (!filter(entry.data))
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

    // Create workbook...
    let wb = XLSX.utils.book_new();
    let wb_name = `export_${ENV.key}_${LocalDate.today()}`;
    for (let i = 0; i < stores.length; i++) {
        let store = stores[i];
        let ws = worksheets[i];

        if (ws == null)
            continue;

        XLSX.utils.book_append_sheet(wb, ws, store);
    }
    for (let key in meta) {
        let ws = meta[key];

        if (ws == null)
            continue;

        XLSX.utils.book_append_sheet(wb, ws, '@' + key);
    }

    // ... and export it!
    let filename = `${ENV.key}_${wb_name}.xlsx`;
    XLSX.writeFile(wb, filename);
}

async function walkThreads(export_id) {
    let url = Util.pasteURL(`${ENV.urls.instance}api/export/download`, { export: export_id });
    let json = await Net.get(url);
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
                return notes.variable != null;
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

    return [threads, tables, counters];
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
    createExport,
    exportRecords
}
