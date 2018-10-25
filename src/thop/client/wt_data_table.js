// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function DataTable(widget)
{
    this.sortHandler = null;

    let self = this;

    let columns = [];
    let columns_map = {};
    let rows_rec = [];
    let rows_flat = [];
    let max_depth = 0;

    let ptr = null;

    let sort_idx = null;
    let sort_descending;

    let prev_offset;
    let prev_len;

    function handleHeaderClick(e)
    {
        let sort = columns[this.col_idx].key;
        if (sort_idx === this.col_idx && !sort_descending)
            sort = '-' + sort;

        if (self.sortHandler) {
            self.sortHandler(sort);
        } else {
            self.sort(sort);
            self.render(prev_offset, prev_len);
        }

        e.preventDefault();
    }

    this.addColumn = function(key) {
        let th = createElementProxy('html', 'th', arguments, 1);
        th.addEventListener('click', handleHeaderClick.bind(th));
        th.col_idx = columns.length;

        let column = {
            idx: columns.length,
            key: key,
            cell: th
        };

        columns.push(column);
        columns_map[key] = column;
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

    this.addCell = function(value) {
        let td;
        if (arguments.length >= 2) {
            td = createElementProxy('html', 'td', arguments, 1);
        } else {
            td = html('td', '' + value);
        }
        if (!ptr.cells.length && ptr.depth) {
            let spacer = html('span', {style: 'display: inline-block; width: ' + (ptr.depth * 0.8) + 'em;'});
            td.insertBefore(spacer, td.firstChild);
        }

        ptr.values.push(value);
        ptr.cells.push(td);
    };
    this.addCells = function(values) {
        for (let value of values) {
            if (value !== null && value !== undefined)
                self.addCell(value);
        }
    };

    this.sort = function(sort) {
        let col_idx;
        let descending;
        if (sort) {
            let column;
            if (sort[0] === '-') {
                column = columns_map[sort.substr(1)];
                descending = true;
            } else {
                column = columns_map[sort];
                descending = false;
            }

            if (column) {
                col_idx = column.idx;
            } else {
                col_idx = null;
                descending = false;
            }
        } else {
            col_idx = null;
            descending = false;
        }

        if (col_idx === sort_idx && descending === sort_descending)
            return false;

        let order = descending ? -1 : 1;

        function recursiveSort(rows)
        {
            if (sort) {
                rows.sort(function(row1, row2) {
                    if (row1.values[col_idx] < row2.values[col_idx]) {
                        return -order;
                    } else if (row1.values[col_idx] > row2.values[col_idx]) {
                        return order;
                    } else {
                        return row1.insert_idx - row2.insert_idx;
                    }
                });
            } else {
                rows.sort(function(row1, row2) { return row1.insert_idx - row2.insert_idx; });
            }

            for (let row of rows) {
                if (row.depth === max_depth)
                    rows_flat.push(row);
                recursiveSort(row.children);
            }
        }

        rows_flat = [];
        recursiveSort(rows_rec);
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
        widget.addClass('dtab');
        widget.appendChildren([
            html('p', {class: 'dtab_count'}),
            html('table', {class: 'dtab_table'},
                html('thead'),
                html('tbody')
            )
        ]);

        let p = widget.query('p');
        let thead = widget.query('thead');
        let tbody = widget.query('tbody');

        if (columns.length && rows_flat.length) {
            let tr = html('tr');
            for (let i = 0; i < columns.length; i++) {
                let th = columns[i].cell;
                if (i === sort_idx) {
                    th.toggleClass('descending', sort_descending);
                    th.toggleClass('ascending', !sort_descending);
                } else {
                    th.removeClass('descending');
                    th.removeClass('ascending');
                }

                tr.appendChild(th);
            }
            thead.appendChild(tr);
        }

        function addRow(row, parent)
        {
            let tr = html('tr', {class: parent ? 'parent' : null}, row.cells);
            tbody.appendChild(tr);
        }

        let render_count;
        {
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

            render_count = Math.max(end - offset, 0);
        }

        if (rows_flat.length) {
            let count_text = '';
            if (render_count)
                count_text += offset + ' - ' + (offset + render_count) + ' ';
            count_text += '(' + rows_flat.length + ' ' + (rows_flat.length > 1 ? 'lignes' : 'ligne') + ')';

            p.innerHTML = count_text;
        }

        prev_offset = offset;
        prev_len = len;

        return render_count;
    };

    this.getRowCount = function() { return rows_flat.length; }
    this.getWidget = function() { return widget; }

    widget.object = this;
}
