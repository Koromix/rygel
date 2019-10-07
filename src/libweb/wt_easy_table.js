// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function EasyTable() {
    let self = this;

    this.urlBuilder = page => '#';
    this.clickHandler = (e, offset, filter, sort_key) => {};

    let page_len = -1;
    let offset = 0;
    let sort_key;
    let filter = value => true;
    let options = {
        header: true,
        parents: true,
        empty: true
    };
    let panel = '';

    let columns;
    let columns_map;

    let row_sets;
    let row_ptr;

    let ready = false;
    let render_rows = [];

    let root_el;

    this.setPageLen = function(new_len) { page_len = new_len; };
    this.getPageLen = function() { return page_len; };
    this.setOffset = function(new_offset) { offset = new_offset; };
    this.getOffset = function() { return offset; };

    this.setFilter = function(new_filter) {
        filter = new_filter || (value => true);
        ready = false;
    };
    this.getFilter = function() { return filter; };
    this.setSortKey = function(new_key) {
        new_key = new_key || null;
        if (new_key !== sort_key)
            ready = false;
        sort_key = new_key;
    };
    this.getSortKey = function() { return sort_key; };

    this.setOptions = function(new_options) { Object.assign(options, new_options); };
    this.getOptions = function() { return options; };

    this.setPanel = function(new_panel) { panel = new_panel || ''; };

    this.addColumn = function(key, title, render = null, options = {}) {
        let column = {
            idx: columns.length,
            key: key,
            title: title,
            render: render || (value => value),
            tooltip: options.tooltip || ''
        };

        columns.push(column);
        columns_map[key] = column;
    };

    this.beginRow = function() {
        let depth = row_ptr ? (row_ptr.depth + 1) : 0;
        if (depth >= row_sets.length)
            row_sets.push([]);

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
        ready = false;

        row_ptr = row;
    };
    this.endRow = function() {
        if (row_ptr)
            row_ptr = row_ptr.parent;
    };

    this.addCell = function(value, options = {}) {
        let cell = {
            value: value,
            tooltip: options.tooltip,
            colspan: options.colspan || 1
        };

        row_ptr.cells.push(cell);
        ready = false;
    };
    this.addCells = function(values) {
        for (let value of values)
            self.addCell(value);
    };

    this.addRow = function(values) {
        self.beginRow();
        self.addCells(values);
        self.endRow();
    };

    this.clear = function() {
        columns = [];
        columns_map = {};

        row_sets = [[]];
        row_ptr = null;

        ready = false;
        render_rows.length = 0;
    };

    function sortAndFilter() {
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

        render_rows.length = 0;
        sortRowsRecursive(row_sets[0], col_idx, order, false);
    };

    function sortRows(rows, col_idx, order) {
        rows.sort((row1, row2) => {
            let cell1 = row1.cells[col_idx];
            let cell2 = row2.cells[col_idx];

            if (col_idx != null && cell1 && cell2) {
                return (util.compareValues(cell1.value, cell2.value) ||
                        row1.insert_idx - row2.insert_idx) * order;
            } else {
                return (row1.insert_idx - row2.insert_idx) * order;
            }
        });
    }

    function sortRowsRecursive(rows, col_idx, order, parent_match) {
        sortRows(rows, col_idx, order);

        for (let row of rows) {
            let match = parent_match || row.cells.some(cell => filter(cell.value));

            if (match && row.depth + 1 === row_sets.length)
                render_rows.push(row);

            sortRowsRecursive(row.children, col_idx, order, match);
        }
    }

    this.render = function() {
        if (!root_el) {
            root_el = document.createElement('div');
            root_el.className = 'etab';
        }

        // We don't return VDOM, because if we did the next render() we do after user interaction
        // would use a new binding, and replace the widget.
        render(renderWidget(), root_el);

        return root_el;
    };

    function renderWidget() {
        if (!ready) {
            sortAndFilter();
            ready = true;
        }

        let render_end = (page_len >= 0) ? Math.min(offset + page_len, render_rows.length) : render_rows.length;
        let render_count = Math.max(render_end - offset, 0);

        let stat_text;
        if (render_rows.length) {
            if (render_count) {
                stat_text = `${offset + 1} - ${offset + render_count} / ${render_rows.length}`;
            } else {
                stat_text = `0 / ${render_rows.length}`;
            }

            if (render_rows.length < row_sets[row_sets.length - 1].length)
                stat_text += ` (sans filtre : ${row_sets[row_sets.length - 1].length})`;
        } else {
            stat_text = '';
        }

        let parents_buf = Array.apply(null, Array(row_sets.length - 1));
        return html`
            <div class="etab_header">
                <p class="etab_stat">${stat_text}</p>
                ${renderPager()}
                <div class="etab_panel">${panel}</div>
            </div>

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
                        let row = render_rows[idx];

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

                    ${(options.empty && !render_count) ?
                        html`<tr><td colspan=${columns.length}>${render_rows.length ? 'Cette page n\'existe pas'
                                                                                    : 'Aucun contenu à afficher'}</td></tr>` : ''}
                </tbody>
            </table>

            ${renderPager()}
        `;
    }

    function handleHeaderClick(e, col_idx) {
        let key = columns[col_idx].key;
        if (key !== sort_key) {
            self.setSortKey(key);
        } else {
            self.setSortKey(`-${key}`);
        }

        self.clickHandler.call(self, e, offset, sort_key);
        e.preventDefault();

        render(renderWidget(), root_el);
    }

    function renderRowCells(row) {
        return row.cells.map((cell, idx) => {
            let render = columns[idx].render;
            let padding = !idx ? `padding-left: calc(3px + ${row.depth} * 0.8em);` : '';
            return html`<td colspan=${cell.colspan} style=${padding}>${render(cell.value)}</td>`;
        });
    }

    function renderPager() {
        if (page_len >= 0 && (offset || render_rows.length > page_len)) {
            let epag = new EasyPager;

            epag.urlBuilder = page => self.urlBuilder.call(self, (page - 1) * page_len, sort_key);
            epag.clickHandler = handlePageClick;

            let last_page = Math.floor((render_rows.length - 1) / page_len + 1);
            let page = Math.ceil(offset / page_len) + 1;

            epag.setLastPage(last_page);
            epag.setPage(page);

            return epag.render();
        } else {
            return html`<div></div>`;
        }
    }

    function handlePageClick(e, page) {
        let offset = (page - 1) * page_len;

        if (self.clickHandler.call(self, e, offset, sort_key)) {
            return true;
        } else {
            self.setOffset(offset);
            render(renderWidget(), root_el);
            return false;
        }
    }

    self.clear();
}
