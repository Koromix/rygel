// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let wt_data_table = (function() {
    function DataTable()
    {
        let self = this;

        this.sortHandler = null;

        let root_el;

        let columns;
        let columns_map;

        let row_sets;
        let sorted_rows = [];
        let row_ptr;

        let sort_idx;
        let sort_order;

        let prev_offset;
        let prev_len;

        this.getRootElement = function() { return root_el; };

        this.getRowCount = function() { return sorted_rows.length; }

        this.addColumn = function(key, title, options = {}) {
            let column = {
                idx: columns.length,
                key: key,
                title: title,
                format: options.format,
                tooltip: options.tooltip
            };

            columns.push(column);
            columns_map[key] = column;
        };

        this.beginRow = function() {
            let depth = row_ptr ? (row_ptr.depth + 1) : 0;
            if (depth >= row_sets.length) {
                row_sets.push([]);
                sorted_rows.length = 0;
            }

            let row = {
                insert_idx: row_sets[depth].length,
                parent: row_ptr,
                depth: depth,
                cells: [],
                children: []
            };

            if (row_ptr)
                row_ptr.children.push(row);
            row_sets[depth].push(row);
            if (depth + 1 === row_sets.length)
                sorted_rows.push(row);

            row_ptr = row;
        };
        this.endRow = function() {
            row_ptr = row_ptr.parent;
        };

        this.addCell = function(value, render = null, options = {}) {
            let cell = {
                value: value,
                render: render || (value => value),
                tooltip: options.tooltip,
                colspan: options.colspan || 1
            };

            row_ptr.cells.push(cell);
        };
        this.addCells = function(values) {
            for (let value of values) {
                if (value != null)
                    self.addCell(value);
            }
        };

        this.clear = function() {
            columns = [];
            columns_map = {};

            row_sets = [[]];
            sorted_rows.length = 0;
            row_ptr = null;

            sort_idx = null;

            prev_offset = null;
            prev_len = null;
        };

        function sortRows(rows, col_idx, order)
        {
            if (col_idx != null) {
                rows.sort((row1, row2) => (util.compareValues(row1.cells[col_idx].value, row2.cells[col_idx].value) ||
                                           row1.insert_idx - row2.insert_idx) * order);
            } else {
                rows.sort((row1, row2) => row1.insert_idx - row2.insert_idx);
            }
        }

        function sortRowsRecursive(rows, col_idx, order)
        {
            sortRows(rows, col_idx, order);

            for (const row of rows) {
                if (row.depth + 1 === row_sets.length)
                    sorted_rows.push(row);
                sortRowsRecursive(row.children, col_idx, order);
            }
        }

        this.sort = function(sort_key, sort_rec = true) {
            let col_idx;
            let order;
            if (sort_key) {
                let column;
                if (sort_key[0] === '-') {
                    column = columns_map[sort_key.substr(1)];
                    order = -1;
                } else {
                    column = columns_map[sort_key];
                    order = 1;
                }

                if (column) {
                    col_idx = column.idx;
                } else {
                    col_idx = null;
                    order = 1;
                }
            } else {
                col_idx = null;
                order = 1;
            }

            if (col_idx === sort_idx && order === sort_order)
                return false;

            if (sort_rec) {
                sorted_rows.length = 0;
                sortRowsRecursive(row_sets[0], col_idx, order);
            } else {
                sortRows(sorted_rows, col_idx, order);
            }
            row_ptr = null;

            sort_idx = col_idx;
            sort_order = order;

            return true;
        };

        function handleExcelClick(e)
        {
            if (typeof XLSX === 'undefined') {
                // FIXME: Fix dependency on THOP-specific code
                data.lazyLoad('xlsx', () => handleExcelClick(e));
                return;
            }

            let wb = self.exportWorkbook();
            XLSX.writeFile(wb, 'export.xlsx');

            e.preventDefault();
        }

        function handleHeaderClick(e, col_idx)
        {
            let sort_key = columns[col_idx].key;
            if (sort_idx === col_idx && sort_order > 0)
                sort_key = '-' + sort_key;

            if (self.sortHandler) {
                self.sortHandler(sort_key);
            } else {
                self.sort(sort_key);
                self.render(root_el, prev_offset, prev_len);
            }

            e.preventDefault();
        }

        function renderRowCells(row)
        {
            return row.cells.map((cell, idx) => {
                let padding = !idx ? `padding-left: calc(3px + ${row.depth} * 0.8em);` : '';
                return html`<td colspan=${cell.colspan} style=${padding}>${cell.render(cell.value)}</td>`;
            });
        }

        this.render = function(new_root_el, offset = 0, len = -1, options = {}) {
            root_el = new_root_el;

            if (len < 0)
                len = sorted_rows.length;
            options.hide_header = options.hide_header || false;
            options.hide_parents = options.hide_parents || false;
            options.hide_empty = options.hide_empty || false;

            let render_end = Math.min(offset + len, sorted_rows.length);
            let render_count = Math.max(render_end - offset, 0);

            let stat_text;
            if (render_count) {
                stat_text = `${offset} - ${offset + render_count} / ${sorted_rows.length}`;
            } else if (sorted_rows.length) {
                stat_text = `0 / ${sorted_rows.length}`;
            } else {
                stat_text = '';
            }

            let parents_buf = Array.apply(null, Array(row_sets.length - 1));

            render(html`
                <div class="dtab">
                    ${sorted_rows.length ?
                        html`<p class="dtab_stat">${stat_text}</p>
                             <a class="dtab_excel" href="#" @click=${handleExcelClick}></a>` : html``}

                    <table class="dtab_table">
                        <thead><tr>${(!options.hide_header ? columns : []).map((col, idx) => {
                            let cls;
                            if (idx === sort_idx) {
                                cls = sort_order > 0 ? 'ascending' : 'descending';
                            } else {
                                cls = '';
                            }

                            return html`<th class=${cls} title=${col.tooltip}
                                            @click=${e => handleHeaderClick(e, col.idx)}>${col.title}</th>`;
                        })}</tr></thead>

                        <tbody>
                            ${util.mapRange(offset, render_end, idx => {
                                let row = sorted_rows[idx];

                                let trs = [];
                                if (!options.hide_parents) {
                                    let parent = row.parent;
                                    while (parent && parent !== parents_buf[parent.depth]) {
                                        parents_buf[parent.depth] = parent;
                                        parent = parent.parent;
                                    }
                                    for (let i = parent ? (parent.depth + 1) : 0; i < parents_buf.length; i++)
                                        trs.push(html`<tr class="parent">${renderRowCells(parents_buf[i])}</tr>`);
                                }
                                trs.push(html`<tr>${renderRowCells(row)}</tr>`);

                                return trs;
                            })}

                            ${!options.hide_empty && !render_count ?
                                html`<tr><td colspan=${columns.length}>${sorted_rows.length ? 'Cette page n\'existe pas'
                                                                                            : 'Aucun contenu à afficher'}</td></tr>` : html``}
                        </tbody>
                    </table>
                </div>
            `, root_el);

            prev_offset = offset;
            prev_len = len;

            return render_count;
        };

        // Requires sheetjs
        this.exportWorkbook = function() {
            let wb = {
                SheetNames: [],
                Sheets: {}
            };

            for (let i = 0; i < row_sets.length; i++) {
                let rows = row_sets[i];

                let ws = XLSX.utils.aoa_to_sheet([columns.map(col => col.key)]);
                if (i)
                    ws[XLSX.utils.encode_cell({c: columns.length, r: 0})] = {v: 'parent', t: 's'};

                for (let row of rows) {
                    let values = row.cells.map(cell => cell.value);
                    if (i)
                        values.push(row.parent.insert_idx + 1);
                    XLSX.utils.sheet_add_aoa(ws, [values], {origin: -1});
                }
                for (let j = 0; j < columns.length; j++) {
                    let column = columns[j];

                    if (column.format) {
                        for (let k = 1; k <= rows.length; k++) {
                            let cell = ws[XLSX.utils.encode_cell({c: j, r: k})];
                            if (cell)
                                cell.z = column.format;
                        }
                    }
                }

                let ws_name = `Sheet${wb.SheetNames.length + 1}`;
                wb.SheetNames.push(ws_name);
                wb.Sheets[ws_name] = ws;
            }

            return wb;
        };

        self.clear();
    }

    this.create = function() { return new DataTable(); };

    return this;
}).call({});
