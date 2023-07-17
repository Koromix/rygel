// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

async function exportRecords(stores) {
    if (typeof XSLX === 'undefined')
        await net.loadScript(`${ENV.urls.static}sheetjs/xlsx.mini.min.js`);

    let threads = [];
    {
        let from = 0;

        do {
            let url = util.pasteURL(`${ENV.urls.instance}api/records/export`, { from: from });
            let json = await net.get(url);

            threads.push(...json.threads);

            from = json.next;
        } while (from != null);
    }

    let definitions = XLSX.utils.aoa_to_sheet([['table', 'variable', 'label', 'type']]);
    let propositions = XLSX.utils.aoa_to_sheet([['table', 'variable', 'prop', 'label']]);

    // Create data worksheets
    let worksheets = stores.map(store => {
        let variables = orderVariables(store, threads);
        let columns = expandColumns(variables);

        for (let variable of variables) {
            let info = [store, variable.key, variable.label, variable.type];
            XLSX.utils.sheet_add_aoa(definitions, [info], { origin: -1 });

            if (variable.props != null) {
                let props = variable.props.filter(prop => prop.value != null).map(prop => [store, variable.key, prop.value, prop.label]);
                XLSX.utils.sheet_add_aoa(propositions, props, { origin: -1 });
            }
        }

        let header = [
            '__tid', '__sequence',
            ...columns.map(column => column.name)
        ];

        let ws = XLSX.utils.aoa_to_sheet([header]);

        for (let thread of threads) {
            let entry = thread.entries[store];

            if (entry == null)
                continue;

            let row = [
                thread.tid, findSequence(thread),
                ...columns.map(column => {
                    let result = column.read(entry.data);
                    if (result == null)
                        return 'NA';
                    return result;
                })
            ];
            XLSX.utils.sheet_add_aoa(ws, [row], { origin: -1 });
        }

        return [store, ws];
    });

    // Create workbook...
    let wb = XLSX.utils.book_new();
    let wb_name = `export_${ENV.key}_${dates.today()}`;
    for (let [store, ws] of worksheets)
        XLSX.utils.book_append_sheet(wb, ws, store);
    XLSX.utils.book_append_sheet(wb, definitions, '@definitions');
    XLSX.utils.book_append_sheet(wb, propositions, '@propositions');

    // ... and export it!
    let filename = `${ENV.key}_${wb_name}.xlsx`;
    XLSX.writeFile(wb, filename);
}

function orderVariables(store, threads) {
    let variables = [];

    // Use linked list and map for fast inserts and to avoid potential O^2 behavior
    let first_head = null;
    let last_head = null;
    let heads_map = new Map;

    // Reconstitute logical order
    for (let thread of threads) {
        let entry = thread.entries[store];

        if (entry == null)
            continue;
        if (entry.meta.notes.variables == null)
            continue;

        let notes = entry.meta.notes;
        let keys = Object.keys(notes.variables);

        for (let i = 0; i < keys.length; i++) {
            let key = keys[i];

            if (!heads_map.has(key)) {
                let previous = heads_map.get(keys[i - 1]);

                let head = {
                    variable: { key: key, ...notes.variables[key] },
                    previous: previous,
                    next: null
                };

                if (previous == null) {
                    first_head = head;
                } else {
                    head.next = previous.next;
                }
                if (last_head != null)
                    last_head.next = head;
                last_head = head;

                heads_map.set(key, head);
            }
        }
    }

    // Transform linked list to simple array
    for (let head = first_head; head != null; head = head.next)
        variables.push(head.variable);

    return variables;
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

function findSequence(thread) {
    for (let store in thread.entries) {
        let entry = thread.entries[store];

        if (entry.sequence != null)
            return entry.sequence;
    }

    return null;
}
