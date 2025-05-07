// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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

import { Util, Log } from '../core/base.js';
import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { EasyPager } from './easypager.js';

import './easytable.css';

function EasyTable() {
    let self = this;

    let url_builder = page => '#';
    let click_handler = (e, offset, filter, sort_key) => {};

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

    Object.defineProperties(this, {
        urlBuilder: { get: () => url_builder, set: builder => { url_builder = builder; }, enumerable: true },
        clickHandler: { get: () => click_handler, set: handler => { click_handler = handler; }, enumerable: true },

        pageLen: { get: () => page_len, set: new_len => { page_len = new_len; }, enumerable: true },
        offset: { get: () => offset, set: new_offset => { offset = new_offset; }, enumerable: true },

        filter: { get: () => filter, set: setFilter, enumerable: true },
        sortKey: { get: () => filter, set: setSortKey, enumerable: true },
        options: { get: () => options, enumerable: true },

        panel: { get: () => panel, set: content => { panel = content; }, enumerable: true }
    });

    function setFilter(new_filter) {
        filter = new_filter ?? (value => true);
        ready = false;
    }

    function setSortKey(new_key) {
        new_key = new_key || null;
        if (new_key != sort_key)
            ready = false;
        sort_key = new_key;
    }

    this.setOptions = function(new_options) {
        if (new_options.hasOwnProperty('parents') && new_options.parents != options.parents)
            ready = false;
        Object.assign(options, new_options);
    };

    this.addColumn = function(key, title, options = {}) {
        let column = {
            idx: columns.length,
            key: key,
            title: title,
            render: options.render || (value => value),
            sort: options.sort || Util.makeComparator(null, navigator.language, {
                numeric: true,
                ignorePunctuation: true,
                sensitivity: 'base'
            }),
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

        let set_idx = options.parents ? 0 : (row_sets.length - 1);
        sortRowsRecursive(row_sets[set_idx], col_idx, order, false);
    };

    function sortRowsRecursive(rows, col_idx, order, parent_match) {
        sortRows(rows, col_idx, order);

        for (let row of rows) {
            let match = parent_match || row.cells.some(cell => filter(cell.value));

            if (match && row.depth + 1 === row_sets.length)
                render_rows.push(row);

            sortRowsRecursive(row.children, col_idx, order, match);
        }
    }

    function sortRows(rows, col_idx, order) {
        rows.sort((row1, row2) => {
            let cell1 = row1.cells[col_idx];
            let cell2 = row2.cells[col_idx];

            if (col_idx != null && cell1 && cell2) {
                return (columns[col_idx].sort(cell1.value, cell2.value) ||
                        row1.insert_idx - row2.insert_idx) * order;
            } else {
                return (row1.insert_idx - row2.insert_idx) * order;
            }
        });
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
                    ${Util.mapRange(offset, render_end, idx => {
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
            self.sortKey = key;
        } else {
            self.sortKey = '-' + key;
        }

        click_handler.call(self, e, offset, sort_key);
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

            epag.urlBuilder = page => url_builder.call(self, (page - 1) * page_len, sort_key);
            epag.clickHandler = handlePageClick;

            let last_page = Math.floor((render_rows.length - 1) / page_len + 1);
            let page = Math.ceil(offset / page_len) + 1;

            epag.lastPage = last_page;
            epag.currentPage = page;

            return epag.render();
        } else {
            return html`<div></div>`;
        }
    }

    function handlePageClick(e, page) {
        let offset = (page - 1) * page_len;

        if (click_handler.call(self, e, offset, sort_key)) {
            return true;
        } else {
            self.offset = offset;
            render(renderWidget(), root_el);
            return false;
        }
    }

    self.clear();
}

export { EasyTable }
