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
import { Base64 } from '../../web/core/mixer.js';
import * as goupile from './goupile.js';
import { profile } from './goupile.js';
import * as UI from './ui.js';

async function exportRecords(stores, filter = null) {
    if (filter == null)
        filter = () => true;

    let XLSX = await import(`${ENV.urls.static}sheetjs/XLSX.bundle.js`);

    let definitions = XLSX.utils.aoa_to_sheet([['table', 'variable', 'label', 'type']]);
    let propositions = XLSX.utils.aoa_to_sheet([['table', 'variable', 'prop', 'label']]);

    // Assemble information about tables and columns
    let tables = await structureTables();

    // Create base worksheets
    let worksheets = stores.map(store => {
        let table = tables[store];

        if (table == null)
            return null;

        // Definitions and propositions
        for (let variable of table.variables) {
            let info = [store, variable.key, variable.label, variable.type];
            XLSX.utils.sheet_add_aoa(definitions, [info], { origin: -1 });

            if (variable.props != null) {
                let props = variable.props.filter(prop => prop.value != null).map(prop => [store, variable.key, prop.value, prop.label]);
                XLSX.utils.sheet_add_aoa(propositions, props, { origin: -1 });
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
    await walkThreads('data', thread => {
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
                thread.tid, findSequence(thread),
                ...table.columns.map(column => {
                    let result = column.read(entry.data);
                    if (result == null)
                        return 'NA';
                    return result;
                })
            ];

            XLSX.utils.sheet_add_aoa(ws, [row], { origin: -1 });
            table.length++;
        }
    });

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
    XLSX.utils.book_append_sheet(wb, definitions, '@definitions');
    XLSX.utils.book_append_sheet(wb, propositions, '@propositions');

    // ... and export it!
    let filename = `${ENV.key}_${wb_name}.xlsx`;
    XLSX.writeFile(wb, filename);
}

async function structureTables() {
    let tables = {};

    // Reconstitute logical order
    await walkThreads('meta', thread => {
        for (let store in thread.entries) {
            let entry = thread.entries[store];

            if (entry.meta.notes.variables == null)
                continue;

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

            let notes = entry.meta.notes;
            let keys = Object.keys(notes.variables);

            for (let i = 0; i < keys.length; i++) {
                let key = keys[i];

                if (!table.variables.has(key)) {
                    let previous = table.variables.get(keys[i - 1]) ?? table;

                    let head = {
                        variable: { key: key, ...notes.variables[key] },
                        prev: previous,
                        next: previous.next
                    };

                    previous.next.prev = head;
                    previous.next = head;

                    table.variables.set(key, head);
                }
            }
        }
    });

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

    return tables;
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

async function walkThreads(method, func) {
    let from = 0;

    do {
        let url = Util.pasteURL(`${ENV.urls.instance}api/export/${method}`, { from: from });
        let json = await Net.get(url);

        for (let thread of json.threads)
            func(thread);

        from = json.next;
    } while (from != null);
}

function findSequence(thread) {
    for (let store in thread.entries) {
        let entry = thread.entries[store];

        if (entry.sequence != null)
            return entry.sequence;
    }

    return null;
}

export {
    // runExportDialog,
    exportRecords
}
