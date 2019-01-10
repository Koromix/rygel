// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function DataTable(widget)
{
    this.sortHandler = null;

    let self = this;

    let columns;
    let columns_map;

    let row_sets;
    let sorted_rows;
    let ptr;

    let sort_idx;
    let sort_descending;

    let prev_offset;
    let prev_len;

    function handleExcelClick(e)
    {
        if (typeof XLSX === 'undefined') {
            lazyLoad('xlsx', function() { handleExcelClick(e); });
            return;
        }

        let wb = {
            SheetNames: [],
            Sheets: {}
        };

        for (let i = 0; i < row_sets.length; i++) {
            const rows = row_sets[i];

            let ws;
            if (columns.length) {
                ws = XLSX.utils.aoa_to_sheet([
                    columns.map(function(col) { return col.key; })
                ]);
            } else {
                ws = XLSX.utils.aoa_to_sheet([]);
            }
            if (i)
                ws[XLSX.utils.encode_cell({c: columns.length, r: 0})] = {v: 'parent', t: 's'};

            for (const row of rows) {
                let values = i ? row.values.concat(row.parent.insert_idx + 1) : row.values;
                XLSX.utils.sheet_add_aoa(ws, [values], {origin: -1});
            }
            for (let j = 0; j < columns.length; j++) {
                const column = columns[j];

                if (column.format) {
                    for (let k = 1; k <= rows.length; k++) {
                        let cell = ws[XLSX.utils.encode_cell({c: j, r: k})];
                        if (cell)
                            cell.z = column.format;
                    }
                }
            }

            let name = 'Niveau ' + (wb.SheetNames.length + 1);
            wb.SheetNames.push(name);
            wb.Sheets[name] = ws;
        }

        XLSX.writeFile(wb, 'export.xlsx');

        e.preventDefault();
    }

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

    this.addColumn = function(key, format) {
        let th = createElementProxy('html', 'th', arguments, 2);
        th.addEventListener('click', handleHeaderClick.bind(th));
        th.col_idx = columns.length;

        let column = {
            idx: columns.length,
            key: key,
            cell: th,
            format: format
        };

        columns.push(column);
        columns_map[key] = column;
    };

    this.beginRow = function() {
        let depth = ptr ? (ptr.depth + 1) : 0;
        if (depth >= row_sets.length) {
            row_sets.push([]);
            sorted_rows = [];
        }

        let row = {
            insert_idx: row_sets[depth].length,
            parent: ptr,
            depth: depth,
            values: [],
            cells: [],
            children: []
        };

        if (ptr)
            ptr.children.push(row);
        row_sets[depth].push(row);
        if (depth + 1 === row_sets.length)
            sorted_rows.push(row);

        ptr = row;
    };
    this.endRow = function() {
        ptr = ptr.parent;
    };

    this.addCell = function(type, value) {
        let td;
        if (arguments.length >= 3) {
            td = createElementProxy('html', type, arguments, 2);
        } else {
            td = html(type, '' + value);
        }
        if (!ptr.cells.length && ptr.depth) {
            let spacer = html('span', {style: 'display: inline-block; width: ' + (ptr.depth * 0.8) + 'em;'});
            td.insertBefore(spacer, td.firstChild);
        }

        ptr.values.push(value);
        ptr.cells.push(td);
    };
    this.addCells = function(type, values) {
        for (let value of values) {
            if (value !== null && value !== undefined)
                self.addCell(type, value);
        }
    };

    this.sort = function(sort, sort_rec) {
        if (sort_rec === undefined)
            sort_rec = true;

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

        function sortRows(rows)
        {
            if (sort) {
                rows.sort(function(row1, row2) {
                    if (row1.values[col_idx] < row2.values[col_idx]) {
                        return -order;
                    } else if (row1.values[col_idx] > row2.values[col_idx]) {
                        return order;
                    } else {
                        return order * (row1.insert_idx - row2.insert_idx);
                    }
                });
            } else {
                rows.sort(function(row1, row2) { return row1.insert_idx - row2.insert_idx; });
            }
        }

        function sortRowsRecursive(rows)
        {
            sortRows(rows);

            for (const row of rows) {
                if (row.depth + 1 === row_sets.length)
                    sorted_rows.push(row);
                sortRowsRecursive(row.children);
            }
        }

        if (sort_rec) {
            sorted_rows = [];
            sortRowsRecursive(row_sets[0]);
        } else {
            sortRows(sorted_rows);
        }
        ptr = null;

        sort_idx = col_idx;
        sort_descending = descending;

        return true;
    };

    this.render = function(offset, len, options) {
        offset = (offset !== undefined) ? offset : 0;
        len = (len !== undefined) ? len : sorted_rows.length;
        options = (options !== undefined) ? options : {};
        options.render_header = (options.render_header !== undefined) ? options.render_header : true;
        options.render_parents = (options.render_parents !== undefined) ? options.render_parents : true;
        options.render_empty = (options.render_empty !== undefined) ? options.render_empty : true;

        widget.innerHTML = '';
        widget.addClass('dtab');
        if (sorted_rows.length) {
            widget.appendContent(
                html('p', {class: 'dtab_count'}),
                html('a', {class: 'dtab_excel', href: '#', click: handleExcelClick})
            );
        }
        widget.appendContent(
            html('table', {class: 'dtab_table'},
                html('thead'),
                html('tbody')
            )
        );

        let render_count = 0;
        if (sorted_rows.length || options.render_empty) {
            let p = widget.query('p');
            let thead = widget.query('thead');
            let tbody = widget.query('tbody');

            if (options.render_header && columns.length) {
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

            function addRow(row, leaf)
            {
                let tr = html('tr', {class: leaf ? null : 'parent'}, row.cells);
                tbody.appendChild(tr);
            }

            let end = Math.min(offset + len, sorted_rows.length);
            let parents = Array.apply(null, Array(row_sets.length - 1));
            for (let i = offset; i < end; i++) {
                let row = sorted_rows[i];

                if (options.render_parents) {
                    let parent = row.parent;
                    while (parent && parent !== parents[parent.depth]) {
                        parents[parent.depth] = parent;
                        parent = parent.parent;
                    }
                    for (let i = parent ? (parent.depth + 1) : 0; i < parents.length; i++)
                        addRow(parents[i], false);
                }

                addRow(row, true);
            }

            render_count = Math.max(end - offset, 0);
            if (!render_count) {
                let msg = sorted_rows.length ? 'Cette page n\'existe pas' : 'Aucun contenu à afficher';
                tbody.appendChild(html('tr',
                    html('td', {colspan: columns.length}, msg)
                ));
            }

            if (sorted_rows.length) {
                let count_text = '';
                if (render_count)
                    count_text += offset + ' - ' + (offset + render_count) + ' ';
                count_text += '(' + sorted_rows.length + ' ' + (sorted_rows.length > 1 ? 'lignes' : 'ligne') + ')';

                p.innerHTML = count_text;
            }
        }

        prev_offset = offset;
        prev_len = len;

        return render_count;
    };

    this.clear = function() {
        columns = [];
        columns_map = {};

        row_sets = [[]];
        sorted_rows = [];
        ptr = null;

        sort_idx = null;

        prev_offset = null;
        prev_len = null;
    };

    this.getRowCount = function() { return sorted_rows.length; }
    this.getWidget = function() { return widget; }

    self.clear();

    widget.object = this;
}

function createPagedDataTable(el)
{
    if (!el.childNodes.length) {
        el.appendContent(
            html('table', {class: 'pagr'}),
            html('div', {class: 'dtab'}),
            html('table', {class: 'pagr'})
        );
    }

    let dtab = new DataTable(el.query('.dtab'));

    return dtab;
}
