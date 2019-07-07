// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let autoform_export = (function() {
    this.renderTable = function(rows, root_el) {
        if (rows.length) {
            let keys = new Set();
            for (let row of rows) {
                for (let key in row)
                    keys.add(key);
            }
            keys.delete('id');
            keys = Array.from(keys).sort();
            // keys.unshift('id');

            render(html`
                <table class="af_export">
                    <thead>
                        <tr>${keys.map(key => html`<th>${key}</th>`)}</tr>
                    </thead>
                    <tbody>
                        ${rows.map(row => html`<tr>${keys.map(key => {
                            let value = row[key] || '';
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
