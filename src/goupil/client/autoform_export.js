// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let autoform_export = (function() {
    function reverseLastColumns(columns, start_idx) {
        for (let i = 0; i < (columns.length - start_idx) / 2; i++) {
            let tmp = columns[start_idx + i];
            columns[start_idx + i] = columns[columns.length - i - 1];
            columns[columns.length - i - 1] = tmp;
        }
    }

    function orderColumns(variables) {
        variables = variables.slice();
        variables.sort((variable1, variable2) => util.compareValues(variable1.key, variable2.key));

        let first_set = new Set;
        let sets_map = {};
        let variables_map = {};
        for (let variable of variables) {
            if (variable.before == null) {
                first_set.add(variable.key);
            } else {
                let set_ptr = sets_map[variable.before];
                if (!set_ptr) {
                    set_ptr = new Set;
                    sets_map[variable.before] = set_ptr;
                }

                set_ptr.add(variable.key);
            }

            variables_map[variable.key] = variable;
        }

        let columns = [];
        {
            let next_sets = [first_set];
            let next_set_idx = 0;

            while (next_set_idx < next_sets.length) {
                let set_ptr = next_sets[next_set_idx++];
                let set_start_idx = columns.length;

                while (set_ptr.size) {
                    let frag_start_idx = columns.length;

                    for (let key of set_ptr) {
                        let variable = variables_map[key];

                        if (!set_ptr.has(variable.after))
                            columns.push(key);
                    }

                    reverseLastColumns(columns, frag_start_idx);

                    // Avoid infinite loop that may happen in rare cases
                    if (columns.length === frag_start_idx) {
                        let use_key = str_ptr.values().next().value;
                        columns.push(use_key);
                    }

                    for (let i = frag_start_idx; i < columns.length; i++) {
                        let key = columns[i];

                        let next_set = sets_map[key];
                        if (next_set) {
                            next_sets.push(next_set);
                            delete sets_map[key];
                        }

                        delete variables_map[key];
                        set_ptr.delete(key);
                    }
                }

                reverseLastColumns(columns, set_start_idx);
            }
        }

        // Remaining variables (probably from old forms)
        for (let key in variables_map)
            columns.push(key);

        return columns;
    }

    this.renderTable = function(rows, variables, root_el) {
        if (rows.length) {
            let columns = orderColumns(variables);

            render(html`
                <table class="af_export">
                    <thead>
                        <tr>${columns.map(key => html`<th>${key}</th>`)}</tr>
                    </thead>
                    <tbody>
                        ${rows.map(row => html`<tr>${columns.map(key => {
                            let value = row[key];
                            if (value == null)
                                value = '';
                            if (Array.isArray(value))
                                value = value.join('|');

                            return html`<td title=${value}>${value}</td>`;
                        })}</tr>`)}
                    </tbody>
                </table>
            `, root_el);
        } else {
            render(html``, root_el);
        }
    };

    return this;
}).call({});
