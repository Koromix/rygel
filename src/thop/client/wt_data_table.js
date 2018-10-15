// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function DataTable(widget)
{
    if (widget === undefined)
        widget = html('table');

    this.sortHandler = null;

    let self = this;

    let columns = [];
    let rows_rec = [];
    let rows_flat = [];
    let max_depth = 0;

    let ptr = null;

    let sort_idx = null;
    let sort_descending;

    let render_offset;
    let render_len;

    function handleHeaderClick(e)
    {
        let col_idx = this.col_idx;
        let descending = (sort_idx === col_idx) ? !sort_descending : false;

        if (self.sortHandler) {
            self.sortHandler(col_idx, descending);
        } else {
            self.sort(col_idx, descending);
            self.render(render_offset, render_len);
        }

        e.preventDefault();
    }

    this.addColumn = function(name) {
        columns.push(name);
    };
    this.addColumns = function(names) {
        for (let name of names) {
            if (name !== null && name !== undefined)
                columns.push(name);
        }
    };

    this.beginRow = function() {
        let row = {
            insert_idx: rows_flat.length,
            parent: ptr,
            depth: ptr ? (ptr.depth + 1) : 0,
            values: [],
            cells: [],
            children: []
        };

        if (ptr) {
            ptr.children.push(row);
        } else {
            rows_rec.push(row);
        }
        if (row.depth > max_depth) {
            rows_flat = [];
            max_depth = row.depth;
        }
        if (row.depth === max_depth)
            rows_flat.push(row);

        ptr = row;
    };
    this.endRow = function() {
        ptr = ptr.parent;
    };

    this.addCell = function(value, el) {
        if (el === undefined)
            el = document.createTextNode('' + value);

        ptr.values.push(value);
        ptr.cells.push(el);
    };
    this.addCells = function(values) {
        for (let value of values) {
            if (value !== null && value !== undefined) {
                ptr.values.push(value);
                ptr.cells.push(document.createTextNode('' + value));
            }
        }
    };

    this.sort = function(col_idx, descending) {
        if (col_idx === sort_idx && descending === sort_descending)
            return false;

        let order = descending ? -1 : 1;

        function recursiveSort(rows, col_idx)
        {
            rows.sort(function(row1, row2) {
                if (row1.values[col_idx] < row2.values[col_idx]) {
                    return -order;
                } else if (row1.values[col_idx] > row2.values[col_idx]) {
                    return order;
                } else {
                    return row1.insert_idx - row2.insert_idx;
                }
            });

            for (let row of rows) {
                if (row.depth === max_depth)
                    rows_flat.push(row);
                recursiveSort(row.children, col_idx);
            }
        }

        rows_flat = [];
        recursiveSort(rows_rec, col_idx);
        ptr = null;

        sort_idx = col_idx;
        sort_descending = descending;

        return true;
    };

    this.render = function(offset, len) {
        if (offset === undefined)
            offset = 0;
        if (len === undefined)
            len = rows_flat.length;

        widget.innerHTML = '';

        let thead = widget.appendChild(html('thead'));
        let tbody = widget.appendChild(html('tbody'));

        if (columns.length) {
            let tr = html('tr');
            for (let i = 0; i < columns.length; i++) {
                let th = html('th', {click: handleHeaderClick}, columns[i]);
                if (i === sort_idx)
                    th.addClass(sort_descending ? 'descending' : 'ascending');
                th.col_idx = i;

                tr.appendChild(th);
            }
            thead.appendChild(tr);
        }

        function addRow(row, parent)
        {
            let tr = html('tr', {class: parent ? 'parent' : null});
            for (let j = 0; j < row.cells.length; j++) {
                let td = html('td', row.cells[j]);
                if (row.depth && !j)
                    td.style.paddingLeft = '' + row.depth + 'em';
                tr.appendChild(td);
            }
            tbody.appendChild(tr);
        }

        let end = Math.min(offset + len, rows_flat.length);

        let parents = Array.apply(null, Array(max_depth));
        for (let i = offset; i < end; i++) {
            let row = rows_flat[i];

            let parent = row.parent;
            while (parent && parent !== parents[parent.depth]) {
                parents[parent.depth] = parent;
                parent = parent.parent;
            }
            for (let i = parent ? (parent.depth + 1) : 0; i < parents.length; i++)
                addRow(parents[i], true);

            addRow(row, false);
        }

        render_offset = offset;
        render_len = len;

        return Math.max(end - offset, 0);
    };

    this.getRowCount = function() { return rows_flat.length; }
    this.getWidget = function() { return widget; }

    widget.object = this;
}
