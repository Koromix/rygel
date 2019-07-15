// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let autoform_export = (function() {
    function orderColumns(variables) {
        let first_set = new Set;
        let before_map = {};
        for (let variable of variables) {
            if (variable.before == null) {
                first_set.add(variable);
            } else {
                let set_ptr = before_map[variable.before];
                if (!set_ptr) {
                    set_ptr = new Set;
                    before_map[variable.before] = set_ptr;
                }

                set_ptr.add(variable);
            }
        }

        let columns = [];
        {
            let set_candidates = [first_set];
            let set_idx = 0;

            while (set_idx < set_candidates.length) {
                let set_ptr = set_candidates[set_idx++];

                while (set_ptr.size) {
                    for (let variable of set_ptr) {
                        if (!set_ptr.has(variable.after)) {
                            let next_set = before_map[variable.key];
                            if (next_set) {
                                set_candidates.push(next_set);
                                delete before_map[variable.key];
                            }

                            columns.push(variable.key);
                            set_ptr.delete(variable);
                        }
                    }
                }
            }
        }

        // Remaining variables (probably from old forms)
        for (let key in before_map) {
            let variables = before_map[key];
            for (let variable of variables)
                columns.push(variable.key);
        }

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

                            return html`<td>${value}</td>`;
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
