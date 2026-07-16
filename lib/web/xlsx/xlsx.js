// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import * as zip from "vendor/zip.js/zip.bundle.js";
import * as constants from './constants.js';

function Workbook(options = {}) {
    let self = this;

    let parser = new DOMParser;

    let xmls = {
        '_rels/.rels': parser.parseFromString(constants.RELS_XML, 'text/xml'),
        'docProps/app.xml': parser.parseFromString(constants.APP_XML, 'text/xml'),
        'docProps/core.xml': parser.parseFromString(constants.CORE_XML, 'text/xml'),
        'xl/_rels/workbook.xml.rels': parser.parseFromString(constants.RELS_WORKBOOK_XML, 'text/xml'),
        'xl/theme/theme1.xml': parser.parseFromString(constants.THEME_XML, 'text/xml'),
        'xl/styles.xml': parser.parseFromString(constants.STYLES_XML, 'text/xml'),
        'xl/workbook.xml': parser.parseFromString(constants.WORKBOOK_XML, 'text/xml'),
        '[Content_Types].xml': parser.parseFromString(constants.CONTENT_TYPES_XML, 'text/xml')
    };
    let blobs = {};
    let strings = [];

    let sheets = {};

    // Set stored app name and version
    {
        let app = xmls['docProps/app.xml'];

        if (options.app != null)
            app.querySelector('Application').textContent += ' / ' + options.app;
        if (options.version != null)
            app.querySelector('AppVersion').textContent = options.version;
    }

    Object.defineProperties(this, {
        xmls: { get: () => xmls, enumerable: true },
        blobs: { get: () => blobs, enumerable: true },
        strings: { get: () => strings, enumerable: true },

        sheets: { get: () => sheets, set: obj => { sheets = obj; }, enumerable: true },
    });

    this.createSheet = function () {
        let sheet = parser.parseFromString(constants.WORKSHEET_XML, 'text/xml');
        return sheet;
    };

    this.save = async function (options = {}) {
        let formulas = options.formulas ?? true;

        let writer = new zip.ZipWriter(new zip.BlobWriter('application/zip'), { keepOrder: false });

        // Clean up sheet cells
        for (let sheet of Object.values(sheets))
            cleanSheet(sheet, formulas);

        // Update sheets and workbook rels
        {
            let rels = xmls['xl/_rels/workbook.xml.rels'];
            let workbook = xmls['xl/workbook.xml'];

            if (rels == null) {
                rels = parser.parseFromString(empty.RELS_WORKBOOK_XML, 'text/xml');
                xmls['xl/_rels/workbook.xml.rels'] = rels;
            }

            let rels_el = rels.documentElement;
            let sheets_el = workbook.querySelector(':root > sheets');

            let used_ids = new Set;
            let sheet_map = new Map;
            let rid_counter = 0;
            let sid_counter = 0;
            let file_counter = 0;
            let new_rels = [];
            let new_sheets = [];

            for (let node of rels_el.children) {
                let type = node.getAttribute('Type');

                if (type != constants.REL_WORKSHEET_NS) {
                    let id = node.getAttribute('Id');

                    used_ids.add(id);
                    new_rels.push(node);
                }
            }

            for (let node of sheets_el.children) {
                let name = node.getAttribute('name');
                let sid = parseInt(node.getAttribute('sheetId'), 10);

                sheet_map.set(name, node);
                used_ids.add(sid);
            }

            for (let [name, sheet] of Object.entries(sheets)) {
                let node1 = rels.createElementNS(constants.RELS_NS, 'Relationship')
                let node2 = sheet_map.get(name) ?? workbook.createElementNS(constants.WORKSHEET_NS, 'sheet');

                let rid = null;
                let sid = null;

                do {
                    rid_counter++;
                    rid = 'rId' + rid_counter;
                } while (used_ids.has(rid));
                do {
                    sid_counter++;
                    sid = sid_counter;
                } while (used_ids.has(sid));

                let filename = `worksheets/sheet${++file_counter}.xml`;
                xmls['xl/' + filename] = sheet;

                node1.setAttribute('Type', constants.WORKSHEET_TYPE);
                node1.setAttribute('Id', rid);
                node1.setAttribute('Target', filename);
                if (!node2.hasAttribute('name'))
                    node2.setAttribute('name', name);
                if (!node2.hasAttribute('state'))
                    node2.setAttribute('state', 'visible');
                node2.setAttribute('sheetId', sid);
                node2.setAttributeNS(constants.REL_NS, 'id', rid);

                new_rels.push(node1);
                new_sheets.push(node2);
            }

            rels_el.replaceChildren(...new_rels);
            sheets_el.replaceChildren(...new_sheets);
        }

        // Export XML files
        {
            let serializer = new XMLSerializer;

            for (let [name, doc] of Object.entries(xmls)) {
                let str = serializer.serializeToString(doc.documentElement);
                writer.add(name, new zip.TextReader(str));
            }
        }

        // Export blobs
        for (let [name, blob] of Object.entries(blobs))
            writer.add(name, new zip.BlobReader(blob));

        let blob = await writer.close();

        return blob;
    }

    this.readCells = function (sheet, options = {}) {
        if (typeof sheet == 'string')
            sheet = sheets[sheet];

        let columns = options.columns ?? Number.MAX_SAFE_INTEGER;
        let rows = options.rows ?? Number.MAX_SAFE_INTEGER;

        let cells = {};

        for (let cell of sheet.getElementsByTagName('c')) {
            let ref = cell.getAttribute('r');
            let type = cell.getAttribute('t');

            if (!ref)
                continue;

            let [column, row] = parseRef(ref);

            if (column >= columns || row >= rows)
                continue;

            // Rebuild the ref to make sure it looks good
            ref = makeRef(column, row);

            let value = null;

            switch (type) {
                case 'b': {
                    let v = cell.getElementsByTagName('v')[0];
                    value = !!v?.textContent;
                } break;

                case 'n': {
                    let v = cell.getElementsByTagName('v')[0];
                    value = parseFloat(v?.textContent);
                } break;

                case 's': {
                    let v = cell.getElementsByTagName('v')[0];
                    let idx = parseInt(v?.textContent, 10);

                    value = strings[idx] ?? '';
                } break;

                case 'inlineStr': {
                    let ts = cell.getElementsByTagName('t');
                    value = parseT(ts);
                } break;

                default: {
                    let v = cell.getElementsByTagName('v')[0];
                    value = v?.textContent ?? '';
                } break;
            }

            cells[makeRef(column, row)] = value;
        }

        return cells;
    };

    this.readSpan = function (sheet, start, count, options = {}) {
        if (typeof sheet == 'string')
            sheet = sheets[sheet];

        let [column, row] = parseRef(start);

        let h = options.h ?? 1;
        let v = options.v ?? 0;
        let truncate = options.truncate ?? (count == null);

        if (count == null)
            count = Number.MAX_SAFE_INTEGER;

        let cells = self.readCells(sheet);
        let values = [];

        while (count-- > 0) {
            let ref = makeRef(column, row);
            let value = cells[ref];

            if (truncate && value == null)
                break;

            values.push(value);

            column += h;
            row += v;
        }

        return values;
    };

    this.writeCells = function (sheet, changes) {
        if (typeof sheet == 'string')
            sheet = sheets[sheet];

        let data = sheet.querySelector('sheetData');

        if (data == null) {
            data = sheet.createElementNS(constants.WORKSHEET_NS, 'sheetData');
            sheet.documentElement.appendChild(data);
        }

        // Blindly put cells in sheetData, without row, without replacing existing cells.
        // The exporter will clean this up.

        for (let [ref, value] of Object.entries(changes)) {
            let c = createCell(sheet, ref, value);
            data.appendChild(c);
        }
    };

    this.writeSpan = function (sheet, start, values, options = {}) {
        if (typeof sheet == 'string')
            sheet = sheets[sheet];

        let h = options.h ?? 1;
        let v = options.v ?? 0;

        let data = sheet.querySelector('sheetData');

        if (data == null) {
            data = sheet.createElementNS(constants.WORKSHEET_NS, 'sheetData');
            sheet.documentElement.appendChild(data);
        }

        let [column, row] = parseRef(start);

        for (let value of values) {
            let ref = makeRef(column, row);
            let c = createCell(sheet, ref, value);

            data.appendChild(c);

            column += h;
            row += v;
        }
    };

    function createCell(sheet, ref, value) {
        let c = sheet.createElementNS(constants.WORKSHEET_NS, 'c');

        c.removeAttribute('xmlns');
        c.setAttribute('r', ref);
        c.setAttribute('s', '0');

        if (typeof value == 'boolean') {
            let v = sheet.createElementNS(constants.WORKSHEET_NS, 'v');
            v.textContent = value ? '1' : '0';

            c.setAttribute('t', 'b');
            c.appendChild(v);
        } else if (typeof value == 'number') {
            let v = sheet.createElementNS(constants.WORKSHEET_NS, 'v');
            v.textContent = String(value);

            c.setAttribute('t', 'n');
            c.appendChild(v);
        } else {
            let t = sheet.createElementNS(constants.WORKSHEET_NS, 't');

            let is = sheet.createElementNS(constants.WORKSHEET_NS, 'is');
            t.textContent = String(value);
            is.appendChild(t);

            c.setAttribute('t', 'inlineStr');
            c.appendChild(is);
        }

        return c;
    }

    function cleanSheet(sheet, formulas) {
        let data = sheet.querySelector('sheetData');

        let cells = new Map;
        let rows = new Map;

        // Map existing cells
        for (let node of data.querySelectorAll('c')) {
            let ref = node.getAttribute('r');

            if (ref) {
                ref = makeRef(...parseRef(ref));

                node.setAttribute('r', ref);
                cells.set(ref, node);
            }
        }

        // Map and clear existing rows
        for (let node of data.querySelectorAll('row')) {
            let row = parseInt(node.getAttribute('r'), 10) - 1;

            if (row >= 0) {
                node.replaceChildren();

                node.setAttribute('r', String(row + 1));
                rows.set(row, node);
            }
        }

        data.replaceChildren(...rows.values());

        // Reorganize cells
        for (let [ref, node] of cells.entries()) {
            let [column, row] = parseRef(ref);
            let parent = rows.get(row);

            if (parent == null) {
                parent = sheet.createElementNS(constants.WORKSHEET_NS, 'row');
                parent.setAttribute('r', String(row + 1));

                data.appendChild(parent);
                rows.set(row, parent);
            }

            parent.appendChild(node);
        }

        if (!formulas) {
            let nodes = data.querySelectorAll('c:has(> f) > v');

            for (let node of nodes)
                node.remove();
        }
    }
}

async function open(src) {
    let parser = new DOMParser;

    if (src instanceof Blob) {
        src = new zip.BlobReader(src);
    } else if (src instanceof Uint8Array) {
        src = new zip.Uint8ArrayReader(src);
    } else {
        throw new Error('Unsupported workbook source');
    }

    let reader = new zip.ZipReader(src);
    let entries = await reader.getEntries();

    let wb = new Workbook();

    // Load files
    for (let entry of entries) {
        if (entry.directory)
            continue;

        if (Object.hasOwn(wb.xmls, entry.filename) || entry.filename.endsWith('.xml')) {
            let str = await entry.getData(new zip.TextWriter());
            wb.xmls[entry.filename] = parser.parseFromString(str, 'text/xml');
        } else {
            let blob = await entry.getData(new zip.BlobWriter());
            wb.blobs[entry.filename] = blob;
        }
    }

    // Parse workbook rels
    {
        let rels = wb.xmls['xl/_rels/workbook.xml.rels'];
        let workbook = wb.xmls['xl/workbook.xml'];

        if (rels != null) {
            let rel_nodes = rels.querySelectorAll(':root > Relationship');
            let sheet_nodes = workbook.querySelectorAll(':root > sheets > sheet');

            let rid_map = new Map;

            for (let node of rel_nodes) {
                let type = node.getAttribute('Type');

                if (type == constants.REL_WORKSHEET_NS) {
                    let target = node.getAttribute('Target');
                    let rid = node.getAttribute('Id');

                    rid_map.set(rid, target);
                }
            }

            for (let node of sheet_nodes) {
                let name = node.getAttribute('name');
                let rid = node.getAttributeNS(constants.REL_NS, 'id');
                let filename = 'xl/' + rid_map.get(rid);

                wb.sheets[name] = wb.xmls[filename];
            }
        }
    }

    // Load shared strings (if any)
    {
        let shared = wb.xmls['xl/sharedStrings.xml'];

        if (shared != null) {
            for (let si of shared.getElementsByTagName('si')) {
                let ts = si.getElementsByTagName('t');
                let str = parseT(ts);

                wb.strings.push(str);
            }
        }
    }

    return wb;
}

function makeRef(column, row) {
    let letters = '';

    do {
        let value = column % 26;
        column = Math.floor(column / 26);

        letters += String.fromCharCode(65 + value);
    } while (column);

    return letters + String(row + 1);
}

function parseRef(ref) {
    let i = 0;

    let column = 0;
    let row = 0;

    while (i < ref.length) {
        let code = ref.charCodeAt(i);

        if (code < 65 || code > 90)
            break;

        column = column * 26 + (code - 64);
        i++;
    }

    row = parseInt(ref.substr(i), 10);

    return [column - 1, row - 1];
}

function parseT(nodes) {
    let parts = [];

    for (let node of nodes) {
        let value = node.textContent ?? '';
        parts.push(value);
    }

    return parts.join('');
}

export {
    Workbook,

    open,

    makeRef,
    parseRef
}
