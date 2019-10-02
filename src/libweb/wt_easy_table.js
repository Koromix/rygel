// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function EasyTable() {
    let self = this;

    this.hrefBuilder = page => '#';
    this.changeHandler = (offset, key) => {};

    let page_len = -1;
    let offset = 0;
    let options = {
        header: true,
        parents: true,
        empty: true
    };

    let columns;
    let columns_map;

    let row_sets;
    let sorted_rows = [];
    let row_ptr;

    let sort_key;

    this.setPageLen = function(new_len) { page_len = new_len; };
    this.getPageLen = function() { return page_len; };
    this.setOffset = function(new_offset) { offset = new_offset; };
    this.getOffset = function() { return offset; };
    this.setOptions = function(new_options) { options = new_options; };
    this.getOptions = function() { return options; }

    this.addColumn = function(key, title, render = null, options = {}) {
        let column = {
            idx: columns.length,
            key: key,
            title: title,
            render: render || (value => value),
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

    this.addCell = function(value, options = {}) {
        let cell = {
            value: value,
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

        sort_key = null;
    };

    this.render = function() {
        let root_el = document.createElement('div');
        root_el.className = 'etab';

        // We don't return VDOM, because if we did the next render() we do after user interaction
        // would use a new binding, and replace the widget.
        render(renderWidget(), root_el);

        return root_el;
    };

    function renderWidget() {
        let render_end = (page_len >= 0) ? Math.min(offset + page_len, sorted_rows.length) : sorted_rows.length;
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
        return html`
            ${renderPager()}

            ${sorted_rows.length ? html`<p class="etab_stat">${stat_text}</p>` : ''}

            <table class="etab_table">
                <thead><tr>${(options.header ? columns : []).map((col, idx) => {
                    let cls;
                    if (col.key === sort_key) {
                        cls = 'ascending';
                    } else if (`-${col.key}` === sort_key) {
                        cls = 'descending';
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
                        if (options.parents) {
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

                    ${options.empty && !render_count ?
                        html`<tr><td colspan=${columns.length}>${sorted_rows.length ? 'Cette page n\'existe pas'
                                                                                    : 'Aucun contenu à afficher'}</td></tr>` : ''}
                </tbody>
            </table>

            ${renderPager()}
        `;
    }

    function handleHeaderClick(e, col_idx) {
        let root_el = util.findParent(e.target, el => el.classList.contains('etab'));

        let key = columns[col_idx].key;
        if (key === sort_key)
            key = `-${key}`;

        self.sort(key);
        render(renderWidget(), root_el);

        self.changeHandler.call(self, offset, key);

        e.preventDefault();
    }

    function renderRowCells(row) {
        return row.cells.map((cell, idx) => {
            let render = columns[idx].render;
            let padding = !idx ? `padding-left: calc(3px + ${row.depth} * 0.8em);` : '';
            return html`<td colspan=${cell.colspan} style=${padding}>${render(cell.value)}</td>`;
        });
    }

    function renderPager() {
        if (page_len >= 0 && (offset || sorted_rows.length > page_len)) {
            let epag = new EasyPager;

            epag.hrefBuilder = page => self.hrefBuilder((page - 1) * page_len, sort_key);
            epag.changeHandler = handlePageClick;

            let last_page = Math.floor((sorted_rows.length - 1) / page_len + 1);
            let page = Math.ceil(offset / page_len) + 1;

            epag.setLastPage(last_page);
            epag.setPage(page);

            return epag.render();
        } else {
            return '';
        }
    }

    function handlePageClick(e, page) {
        let root_el = util.findParent(e.target, el => el.classList.contains('etab'));

        let offset = (page - 1) * page_len;

        self.setOffset(offset);
        render(renderWidget(), root_el);

        self.changeHandler.call(self, offset, sort_key);

        e.preventDefault();
    }

    this.sort = function(key, sort_rec = true) {
        key = key || null;
        if (key === sort_key)
            return;

        let col_idx;
        let order;
        if (key) {
            let column;
            if (key[0] === '-') {
                column = columns_map[key.substr(1)];
                order = -1;
            } else {
                column = columns_map[key];
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

        if (sort_rec) {
            sorted_rows.length = 0;
            sortRowsRecursive(row_sets[0], col_idx, order);
        } else {
            sortRows(sorted_rows, col_idx, order);
        }
        row_ptr = null;

        sort_key = key;
    };

    function sortRows(rows, col_idx, order) {
        if (col_idx != null) {
            rows.sort((row1, row2) => (util.compareValues(row1.cells[col_idx].value, row2.cells[col_idx].value) ||
                                       row1.insert_idx - row2.insert_idx) * order);
        } else {
            rows.sort((row1, row2) => row1.insert_idx - row2.insert_idx);
        }
    }

    function sortRowsRecursive(rows, col_idx, order) {
        sortRows(rows, col_idx, order);

        for (const row of rows) {
            if (row.depth + 1 === row_sets.length)
                sorted_rows.push(row);
            sortRowsRecursive(row.children, col_idx, order);
        }
    }

    self.clear();
}
