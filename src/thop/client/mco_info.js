// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, LruMap, LocalDate } from 'src/core/web/base/base.js';
import * as thop from './thop.js';
import { settings } from './thop.js';
import { DataCache, formatDuration, formatPrice, parseDate } from './utility.js';
import { VersionLine } from 'src/core/web/widgets/versionline.js';
import { EasyTable } from 'src/core/web/widgets/easytable.js';

import './mco_info.css';

let route = {};

async function run() {
    switch (route.mode) {
        case 'ghm_roots': { await runGhmRoots(); } break;
        case 'ghmghs': { await runGhmGhs(); } break;
        case 'diagnoses': { await runDiagnoses(); } break;
        case 'procedures': { await runProcedures(); } break;
        case 'ghs': { await runGhs(); } break;
        case 'tree': { await runTree(); } break;

        default: {
            throw new Error(`Mode inconnu '${route.mode}'`);
        } break;
    }
}

function parseURL(path, params = {}) {
    let args = {
        version: parseDate(path[0] || null) ||
                 settings.mco.versions[settings.mco.versions.length - 1].begin_date,
        mode: path[1] || 'ghm_roots',

        ghmghs: {},
        diagnoses: {},
        procedures: {},

        ghs: {},

        tree: {}
    };

    // Mode-specific part
    switch (args.mode) {
        case 'ghm_roots': { /* Nothing to do */ } break;
        case 'ghmghs': {
            args.sector = path[2] || 'public';
            args[args.mode].offset = parseInt(params.offset, 10) || null;
            args[args.mode].sort = params.sort || null;
            args[args.mode].filter = params.filter || null;
        } break;
        case 'diagnoses':
        case 'procedures': {
            args[args.mode].list = path[2] || null;
            args[args.mode].offset = parseInt(params.offset, 10) || null;
            args[args.mode].sort = params.sort || null;
            args[args.mode].filter = params.filter || null;
        } break;

        case 'ghs': {
            args.sector = path[2] || 'public';
            args.ghs.ghm_root = path[3] || null;
            args.ghs.diff = parseDate(params.diff || null);
            args.ghs.duration = parseInt(params.duration, 10) || 200;
            args.ghs.coeff = !!parseInt(params.coeff, 10) || false;
            args.ghs.plot = !!parseInt(params.plot, 10) || false;
            if (window.document.documentMode && args.ghs.plot) {
                Log.error('Graphiques non disponibles sous Internet Explorer');
                args.ghs.plot = false;
            }
            args.ghs.raw = !!parseInt(params.raw, 10) || false;
        } break;

        case 'tree': {
            if (!path[2] && (params.diag || params.proc))
                path[2] = 'filter';

            if (path[2] && path[2] === 'filter') {
                args.tree.filter = true;
                args.tree.diag = params.diag || null;
                args.tree.proc = params.proc || null;
                args.tree.session = !!parseInt(params.session, 10) || false;
                args.tree.a7d = !!parseInt(params.a7d, 10) || false;
            } else {
                if (path[2])
                    Log.error('Mode incorrect et ignoré');

                args.tree.filter = false;
            }
        } break;
    }

    return args;
}

function makeURL(args = {}) {
    args = Util.assignDeep({}, route, args);

    let path = ['mco_info'];
    let params = {};

    // Common part
    path.push(args.version);
    path.push(args.mode);

    // Mode-specific part
    switch (args.mode) {
        case 'ghm_roots': { /* Nothing to do */ } break;
        case 'ghmghs': {
            path.push(args.sector);
            params.offset = args[args.mode].offset || null;
            params.sort = args[args.mode].sort || null;
            params.filter = args[args.mode].filter || null;
        } break;
        case 'diagnoses':
        case 'procedures': {
            if (args[args.mode].list)
                path.push(args[args.mode].list);
            params.offset = args[args.mode].offset || null;
            params.sort = args[args.mode].sort || null;
            params.filter = args[args.mode].filter || null;
        } break;

        case 'ghs': {
            path.push(args.sector);
            if (args.ghs.ghm_root)
                path.push(args.ghs.ghm_root);
            params.diff = args.ghs.diff;
            params.duration = (args.ghs.duration !== 200) ? args.ghs.duration : null;
            params.coeff = args.ghs.coeff ? 1 : null;
            params.plot = args.ghs.plot ? 1 : null;
            params.raw = args.ghs.raw ? 1 : null;
        } break;

        case 'tree': {
            if (args.tree.filter) {
                path.push('filter');
                params.diag = args.tree.diag || null;
                params.proc = args.tree.proc || null;
                params.session = args.tree.session ? 1 : null;
                params.a7d = args.tree.a7d ? 1 : null;
            }
        } break;
    }

    return Util.pasteURL(`${ENV.base_url}${path.join('/')}`, params);
}

// ------------------------------------------------------------------------
// Lists
// ------------------------------------------------------------------------

let input_timer_id;

async function runGhmRoots() {
    let version = findVersion(route.version);
    let [mco, ghmghs] = await Promise.all([
        DataCache.fetchDictionary('mco'),

        DataCache.fetchJson(Util.pasteURL(`${ENV.base_url}api/mco/ghmghs`, {
            sector: route.sector || 'public',
            date: version.begin_date
        }))
    ]);

    document.title = `THOP – Racines de GHM (${version.begin_date})`;

    let ghm_roots = ghmghs.filter((ghs, idx) => !idx || ghmghs[idx - 1].ghm_root !== ghs.ghm_root)
                          .map(ghs => ghs.ghm_root);

    // Options
    render(html`
        ${renderVersionLine(settings.mco.versions, version)}
    `, document.querySelector('#th_options'));

    // Table
    renderListTable(ghm_roots, {
        header: false,
        category: ghm_root => mco.ghm_roots.describeParent(ghm_root, 'cmd'),
        columns: [{ key: 'ghm_root', title: 'Racine de GHM', func: ghm_root => mco.ghm_roots.describe(ghm_root) }]
    });
}

async function runGhmGhs() {
    let version = findVersion(route.version);
    let [mco, ghmghs] = await Promise.all([
        DataCache.fetchDictionary('mco'),

        DataCache.fetchJson(Util.pasteURL(`${ENV.base_url}api/mco/ghmghs`, {
            sector: route.sector,
            date: version.begin_date
        }))
    ]);

    document.title = `THOP – GHM/GHS (${version.begin_date})`;

    // Options
    render(html`
        ${renderVersionLine(settings.mco.versions, version)}
        ${renderSectorSelector(route.sector)}
    `, document.querySelector('#th_options'));

    // Table
    renderListTable(ghmghs, {
        header: true,
        page_len: 300,

        offset: route.ghmghs.offset,
        filter: route.ghmghs.filter,
        sort: route.ghmghs.sort,
        route: (offset, filter, sort_key) => ({ ghmghs: { offset: offset, filter: filter, sort: sort_key } }),

        export: `GHM_GHS_${version.begin_date}`,

        category: ghs => mco.ghm_roots.describe(ghs.ghm_root),
        columns: [
            { key: 'code', title: 'Code', func: ghs => ghs.ghm },
            { key: 'label', title: 'Libellé', func: ghs => mco.ghm.label(ghs.ghm),
                sort: (label1, label2) => label1.localeCompare(label2) },
            { key: 'ghs', title: 'GHS', func: ghs => ghs.ghs },
            { key: 'durations', title: 'Durées', func: ghs => maskToRangeStr(ghs.durations) },
            { key: 'confirm', title: 'Confirmation',
                func: ghs => ghs.confirm_threshold ? `< ${formatDuration(ghs.confirm_threshold)}` : '' },
            { key: 'main_diagnosis', title: 'DP', func: ghs => ghs.main_diagnosis },
            { key: 'diagnoses', title: 'Diagnostics', func: ghs => ghs.diagnoses },
            { key: 'procedures', title: 'Actes', func: ghs => ghs.procedures ? ghs.procedures.join(', ') : '' },
            { key: 'authorizations', title: 'Autorisations', tooltip: 'Autorisations (unités et lits)',
                func: ghs => {
                    let ret = [];
                    if (ghs.unit_authorization)
                        ret.push(`Unité ${ghs.unit_authorization}`);
                    if (ghs.bed_authorization)
                        ret.push(`Lit ${ghs.bed_authorization}`);
                    return ret;
                }
            },
            { key: 'old_severity', title: 'Sévérité âgé',
                func: ghs => ghs.old_age_threshold ? `≥ ${ghs.old_age_threshold} et ` +
                                                    `< ${ghs.old_severity_limit + 1}` : null },
            { key: 'young_severity', title: 'Sévérité jeune',
                func: ghs => ghs.young_age_threshold ? `< ${ghs.young_age_threshold} et ` +
                                                      `< ${ghs.young_severity_limit + 1}` : null }
        ]
    });
}

async function runDiagnoses() {
    let version = findVersion(route.version);
    let [cim10, diagnoses] = await Promise.all([
        DataCache.fetchDictionary('cim10'),

        DataCache.fetchJson(Util.pasteURL(`${ENV.base_url}api/mco/diagnoses`, {
            date: version.begin_date,
            spec: route.diagnoses.list
        }))
    ]);

    document.title = `THOP – Diagnostics (${version.begin_date})`;

    // Options
    render(html`
        ${renderVersionLine(settings.mco.versions, version)}
        ${renderListInfo('diagnoses', 'diagnostics', route.diagnoses.list)}
    `, document.querySelector('#th_options'));

    // Table
    renderListTable(diagnoses, {
        header: true,
        page_len: 300,

        offset: route.diagnoses.offset,
        filter: route.diagnoses.filter,
        sort: route.diagnoses.sort,
        route: (offset, filter, sort_key) => ({ diagnoses: { offset: offset, filter: filter, sort: sort_key } }),

        export: `Diagnostics_${version.begin_date}`,

        columns: [
            { key: 'code', title: 'Code', func: diag => diag.diag },
            { key: 'label', title: 'Libellé', func: diag => cim10.diagnoses.label(diag.diag),
                sort: (label1, label2) => label1.localeCompare(label2) },
            { key: 'severity', title: 'Niveau', func: diag => (diag.severity || 0) + 1 },
            { key: 'cmd', title: 'CMD', tooltip: 'Catégorie majeure de diagnostics',
                func: diag => diag.cmd },
            { key: 'main_list', title: 'Liste principale', func: diag => diag.main_list }
        ]
    });
}

async function runProcedures() {
    let version = findVersion(route.version);
    let [ccam, procedures] = await Promise.all([
        DataCache.fetchDictionary('ccam'),

        DataCache.fetchJson(Util.pasteURL(`${ENV.base_url}api/mco/procedures`, {
            date: version.begin_date,
            spec: route.procedures.list
        }))
    ]);

    document.title = `THOP – Actes (${version.begin_date})`;

    // Options
    render(html`
        ${renderVersionLine(settings.mco.versions, version)}
        ${renderListInfo('procedures', 'actes', route.procedures.list)}
    `, document.querySelector('#th_options'));

    // Table
    renderListTable(procedures, {
        header: true,
        page_len: 300,

        offset: route.procedures.offset,
        filter: route.procedures.filter,
        sort: route.procedures.sort,
        route: (offset, filter, sort_key) => ({ procedures: { offset: offset, filter: filter, sort: sort_key } }),

        export: `Actes_${version.begin_date}`,

        columns: [
            { key: 'code', title: 'Code',
                func: proc => proc.proc + (proc.phase ? `/${proc.phase}` : '') },
            { key: 'label', title: 'Libellé', func: proc => ccam.procedures.label(proc.proc),
                sort: (label1, label2) => label1.localeCompare(label2) },
            { key: 'begin_date', title: 'Début', tooltip: 'Date de début incluse',
                func: proc => parseDate(proc.begin_date) },
            { key: 'end_date', title: 'Fin', tooltip: 'Date de fin exclue',
                func: proc => parseDate(proc.end_date) },
            { key: 'classifying', title: 'Classant', tooltip: 'Acte classant',
                func: proc => proc.classifying ? 'Majeur' : '' },
            { key: 'activities', title: 'Activités', func: proc => proc.activities },
            { key: 'extensions', title: 'Extensions', tooltip: 'CCAM descriptive',
                func: proc => proc.extensions }
        ]
    });
}

function renderListTable(records, handler) {
    let etab = new EasyTable;

    if (handler.route) {
        etab.urlBuilder = (offset, sort_key) => makeURL(handler.route(offset, sort_key));
        etab.clickHandler = (e, offset, sort_key) => {
            thop.goFake(route, handler.route(offset, handler.filter, sort_key));

            etab.setOptions({
                parents: !etab.sortKey
            });

            e.preventDefault();
        };
    }

    etab.pageLen = handler.page_len;
    etab.offset = handler.offset || 0;
    etab.sortKey = handler.sort;
    etab.filter = makeFilterFunction(handler.filter);

    etab.setOptions({
        header: handler.header,
        parents: !handler.sort,
        filter: true
    });

    etab.panel = html`
        ${'filter' in handler ?
            html`<input type="text" .value=${handler.filter || ''} placeholder="Filtre textuel"
                        @input=${e => handleFilterInput(e, etab, handler)} />` : ''}
        ${handler.export ?
            html`<a class="ls_excel" @click=${e => exportListToXLSX(records, handler)}></a>` : ''}
    `;

    for (let col of handler.columns) {
        etab.addColumn(col.key, col.title, {
            render: value => {
                if (typeof value === 'string') {
                    return addSpecLinks(value);
                } else if (value != null) {
                    return value.toLocaleString();
                } else {
                    return null;
                }
            },
            sort: col.sort,
            tooltip: col.tooltip
        });
    }

    let prev_category = null;
    for (let i = 0; i < records.length; i++) {
        let record = records[i];

        if (handler.category) {
            let category = handler.category(record);

            if (category !== prev_category) {
                etab.endRow();
                etab.beginRow();
                etab.addCell(category, { colspan: handler.columns.length });

                prev_category = category;
            }
        }

        etab.beginRow();
        for (let col of handler.columns) {
            let value = col.func(record);
            etab.addCell(value);
        }
        etab.endRow();
    }

    render(etab.render(), document.querySelector('#th_view'));
}

function makeFilterFunction(filter) {
    if (filter) {
        let re = '';
        for (let i = 0; i < filter.length; i++) {
            let c = filter[i].toLowerCase();

            switch (c) {
                case 'c':
                case 'ç': { re += '[cç]'; } break;
                case 'e':
                case 'ê':
                case 'é':
                case 'è':
                case 'ë': { re += '[eèéêë]'; } break;
                case 'a':
                case 'à':
                case 'â':
                case 'ä':
                case 'å': { re += '[aàâäå]'; } break;
                case 'i':
                case 'î':
                case 'ï': { re += '[iîï]'; } break;
                case 'u':
                case 'ù':
                case 'ü':
                case 'û':
                case 'ú': { re += '[uùüûú]'; } break;
                case 'n':
                case 'ñ': { re += '[nñ]'; } break;
                case 'o': {
                    if (filter[i + 1] === 'e') {
                        re += '(oe|œ)';
                        i++;
                    } else {
                        re += '[oô]';
                    }
                } break;
                case 'ó':
                case 'ö':
                case 'ô': { re += '[oôóö]'; } break;
                case 'œ': { re += '(oe|œ)'; } break;
                case 'y':
                case 'ÿ': { re += '[yÿ]'; } break;
                case '—':
                case '–':
                case '-': { re += '[—–\\-]'; } break;

                // Escape special regex characters
                case '/':
                case '+':
                case '*':
                case '?':
                case '<':
                case '>':
                case '&':
                case '|':
                case '\\':
                case '^':
                case '$':
                case '(':
                case ')':
                case '{':
                case '}':
                case '[':
                case ']': { re += `\\${c}`; } break;

                // Special case '.' for CIM-10 codes
                case '.': { re += `\\.?`; } break;

                default: { re += c; } break;
            }
        }
        re = new RegExp(re, 'i');

        let func = value => {
            if (value != null) {
                if (typeof value !== 'string')
                    value = value.toLocaleString();
                return value.match(re);
            } else {
                return false;
            }
        };

        return func;
    } else {
        return null;
    }
}

function handleFilterInput(e, etab, handler) {
    if (input_timer_id != null)
        clearTimeout(input_timer_id);

    input_timer_id = setTimeout(() => {
        thop.goFake(route, handler.route(handler.offset, e.target.value || null, handler.sort));

        etab.filter = makeFilterFunction(e.target.value);
        etab.render();
    }, 200);
}

function renderListInfo(type, label, current_list) {
    if (current_list) {
        let args = {};
        args[type] = { list: null, offset: 0, filter: null };

        return html`<div class="opt_list"><b>Liste :</b> ${current_list}
                                          <a href=${makeURL(args)}>(afficher tout)</a></div>`;
    } else {
        return html`<div class="opt_list"><b>Liste :</b> tous les ${label}</div>`;
    }
}

async function exportListToXLSX(records, handler) {
    let XLSX = await import(`${ENV.base_url}static/XLSX.bundle.js?${ENV.buster}`);

    let ws = XLSX.utils.aoa_to_sheet([
        handler.columns.map(col => col.key),
        ...records.map(record => handler.columns.map(col => {
            let value = col.func(record);

            if (value == null || typeof value === 'number') {
                return value;
            } else if (value.toJSDate) {
                return value.toJSDate(true);
            } else {
                return value.toString();
            }
        }))
    ]);

    let wb = XLSX.utils.book_new();
    XLSX.utils.book_append_sheet(wb, ws, handler.export);

    XLSX.writeFile(wb, `${handler.export}.xlsx`, { cellDates: true });
}

// ------------------------------------------------------------------------
// GHS
// ------------------------------------------------------------------------

let chart_obj;
let chart_canvas = document.createElement('canvas');

let max_map = new LruMap(256);

async function runGhs() {
    let version = findVersion(route.version);
    let version_diff = route.ghs.diff ? findVersion(route.ghs.diff) : null;
    let [mco, ghmghs, ghmghs_diff] = await Promise.all([
        DataCache.fetchDictionary('mco'),

        DataCache.fetchJson(Util.pasteURL(`${ENV.base_url}api/mco/ghmghs`, {
            sector: route.sector,
            date: version.begin_date
        })),
        version_diff ? DataCache.fetchJson(Util.pasteURL(`${ENV.base_url}api/mco/ghmghs`, {
            sector: route.sector,
            date: version_diff.begin_date
        })) : null
    ]);

    if (!route.ghs.ghm_root)
        route.ghs.ghm_root = mco.ghm_roots.definitions[0].code;

    document.title = `THOP – Tarifs ${route.ghs.ghm_root} (${version.begin_date})`;

    // Options
    render(html`
        ${renderVersionLine(settings.mco.versions, version)}
        ${renderSectorSelector(route.sector)}
        <label class=${route.ghs.plot ? 'disabled' : ''}>
            <span>Comparer</span>
            ${renderDiffSelector(settings.mco.versions, version_diff, !route.ghs.plot)}
        </label>
        <label>Durée <input type="number" step="5" min="0" max="500" .value=${route.ghs.duration}
                            @change=${e => thop.go(route, { ghs: { duration: parseInt(e.target.value, 10) } })}/></label>
        <label>Coefficient <input type="checkbox" .checked=${route.ghs.coeff}
                                  @change=${e => thop.go(route, { ghs: { coeff: e.target.checked } })}/></label>
        ${!window.document.documentMode ?
            html`<label>Graphique <input type="checkbox" .checked=${route.ghs.plot}
                                         @change=${e => thop.go(route, { ghs: { plot: e.target.checked } })}/></label>` : ''}
        ${renderGhmRootSelector(mco, route.ghs.ghm_root)}
    `, document.querySelector('#th_options'));

    let columns = ghmghs.filter(it => it.ghm_root === route.ghs.ghm_root);
    if (!columns.length)
        throw new Error(`Racine de GHM '${route.ghs.ghm_root}' inexistante`);

    // With the new gradation stuff, most short (J or T) medical GHMs start with a 9999 GHS
    // for ambulatory settings. This is noisy. Here we artifically hide this column and add
    // "Non-ambulatory" condition to remaining columns, unless they already are special.
    if (!route.ghs.raw) {
        let prev_ghm;
        let not_outpatient;
        let not_diabetes;

        let j = 0;
        for (let i = 0; i < columns.length; i++) {
            if (columns[i].ghm !== prev_ghm) {
                not_outpatient = false;
                not_diabetes = false;
            }
            prev_ghm = columns[i].ghm;

            if (columns[i].ghs === 9999) {
                if (columns[i].modes[0] === 'outpatient') {
                    not_outpatient = true;
                    continue;
                }
                if (columns[i].modes[0] === 'diabetes2' || columns[i].modes[0] === 'diabetes3') {
                    not_diabetes = true;
                    continue;
                }
            }

            columns[j] = columns[i];
            columns[j].modes = columns[j].modes.filter(mode => !mode.startsWith('!'));
            if (not_outpatient && !columns[j].modes.includes('intermediary'))
                columns[j].modes.push('!outpatient');
            if (not_diabetes)
                columns[j].modes.push('!diabetes');
            j++;
        }
        columns.length = j;
    }

    // Render grid or plot
    if (route.ghs.plot) {
        let chart = await import(`${ENV.base_url}static/chart.bundle.js?${ENV.buster}`);

        render(chart_canvas, document.querySelector('#th_view'));
        updatePriceChart(chart.Chart, mco.ghm_roots.describe(route.ghs.ghm_root), columns,
                                      route.ghs.duration, route.ghs.coeff);
    } else {
        let diff_map;
        if (version_diff) {
            diff_map = Util.arrayToObject(ghmghs_diff, it => it.ghs);
            if (!columns.some(col => !!diff_map[col.ghs]))
                throw new Error(`La racine de GHM '${route.ghs.ghm_root}' n'existait pas dans la version ${version_diff.begin_date.toLocaleString()}`);
        }

        render(renderPriceGrid(mco.ghm_roots.describe(route.ghs.ghm_root), columns,
                               diff_map, route.ghs.duration, route.ghs.coeff),
               document.querySelector('#th_view'));
    }
}

function renderDiffSelector(versions, current_version) {
    return html`
        <select @change=${e => thop.go(route, { ghs: { diff: LocalDate.parse(e.target.value || null) } })}>
            <option value="" .selected=${current_version == null}>Non</option>
            ${versions.map(version => {
                let label = version.begin_date.toLocaleString();
                return html`<option value=${version.begin_date}
                                    .selected=${version == current_version}>${label}</option>`;
            })}
        </select>
    `;
}

function renderGhmRootSelector(mco, current_ghm_root) {
    return html`
        <select @change=${e => thop.go(route, { ghs: { ghm_root: e.target.value } })}>
            ${mco.ghm_roots.definitions.map(ghm_root => {
                let disabled = false;
                let label = `${ghm_root.describe()}${disabled ? ' *' : ''}`;

                return html`<option value=${ghm_root.code} ?disabled=${disabled}
                                    .selected=${ghm_root.code === current_ghm_root}>${label}</option>`
            })}
        </select>
    `
}

function renderPriceGrid(ghm_root, columns, diff_map, max_duration, apply_coeff) {
    let conditions = columns.map(col => buildConditionsArray(col));

    return html`
        <table class="pr_grid" style=${`min-width: calc(${columns.length} * 6em);`}>
            <colgroup>${columns.map(col => html`<col/>`)}</colgroup>

            <thead>
                <tr><th colspan=${columns.length} class="ghm_root">${ghm_root}</th></tr>

                <tr><th>GHM</th>${Util.mapRLE(columns, col => col.ghm,
                    (ghm, _, colspan) => html`<td class="desc" colspan=${colspan}
                                                  style=${'font-weight: bold; color: ' + modeToColor(ghm.substr(5, 1))}>${ghm}</td>`)}</tr>
                <tr><th>Niveau</th>${Util.mapRLE(columns, col => col.ghm.substr(5, 1),
                    (mode, _, colspan) => html`<td class="desc" colspan=${colspan}
                                                   style=${'color: ' + modeToColor(mode)}>Niveau ${mode}</td>`)}</tr>
                <tr><th>GHS</th>${columns.map((col, idx) =>
                    html`<td class="desc">${col.ghs}</td>`)}</tr>
                <tr><th>Conditions</th>${columns.map((col, idx) =>
                    html`<td class="conditions">${conditions[idx].map(cond => html`${addSpecLinks(cond)}<br/>`)}</td>`)}</tr>
                <tr><th>Tarif €</th>${Util.mapRLE(columns, col =>
                        applyGhsCoefficient(col.ghs_cents, !apply_coeff || col.ghs_coefficient),
                    (cents, _, colspan) => html`<td class="noex" colspan=${colspan}>${formatPrice(cents)}</td>`)}</tr>
                <tr><th>Borne basse</th>${Util.mapRLE(columns, col => col.exb_threshold,
                    (threshold, _, colspan) => html`<td class="exb" colspan=${colspan}>${formatDuration(threshold)}</td>`)}</tr>
                <tr><th>Forfait EXB €</th>${Util.mapRLE(columns, col =>
                        applyGhsCoefficient(col.exb_once ? col.exb_cents : null, !apply_coeff || col.ghs_coefficient),
                    (cents, _, colspan) => html`<td class="exb" colspan=${colspan}>${formatPrice(cents)}</td>`)}</tr>
                <tr><th>Tarif EXB €</th>${Util.mapRLE(columns, col =>
                        applyGhsCoefficient(col.exb_once ? null : col.exb_cents, !apply_coeff || col.ghs_coefficient),
                    (cents, _, colspan) => html`<td class="exb" colspan=${colspan}>${formatPrice(cents)}</td>`)}</tr>
                <tr><th>Borne haute</th>${Util.mapRLE(columns, col => col.exh_threshold ? (col.exh_threshold - 1) : null,
                    (threshold, _, colspan) => html`<td class="exh" colspan=${colspan}>${formatDuration(threshold)}</td>`)}</tr>
                <tr><th>Tarif EXH €</th>${Util.mapRLE(columns, col =>
                        applyGhsCoefficient(col.exh_cents, !apply_coeff || col.ghs_coefficient),
                    (cents, _, colspan) => html`<td class="exh" colspan=${colspan}>${formatPrice(cents)}</td>`)}</tr>
                <tr><th>Age</th>${Util.mapRLE(columns, col => {
                        let texts = [];
                        let severity = col.ghm.charCodeAt(5) - '1'.charCodeAt(0);
                        if (severity >= 0 && severity < 4) {
                            if (severity < col.young_severity_limit)
                                texts.push('< ' + col.young_age_threshold.toString());
                            if (severity < col.old_severity_limit)
                                texts.push('≥ ' + col.old_age_threshold.toString());
                        }

                        return texts.join(', ');
                    },
                    (text, _, colspan) => html`<td class="age" colspan=${colspan}>${text}</td>`)}</tr>
            </thead>

            <tbody>${Util.mapRange(0, max_duration, duration =>
                html`<tr class="duration">
                    <th>${formatDuration(duration)}</th>
                    ${columns.map(col => {
                        let info;
                        if (diff_map) {
                            let col_diff = diff_map[col.ghs];
                            info = computeGhsDelta(col, col_diff, duration, apply_coeff);
                        } else {
                            info = computeGhsPrice(col, duration, apply_coeff);
                        }

                        if (info) {
                            let cls = info.mode;
                            let tooltip = '';
                            if (!duration && col.warn_cmd28) {
                                cls += ' warn';
                                tooltip += 'Devrait être orienté dans la CMD 28 (séance)\n';
                            }
                            if (testGhsDuration(col.raac_durations || 0, duration)) {
                                cls += ' warn';
                                tooltip += 'Accessible en cas de RAAC\n';
                            }
                            if (col.warn_ucd) {
                                cls += ' info';
                                tooltip += 'Possibilité de minoration UCD (40 €)\n';
                            }

                            let text = formatPrice(info.price, true);
                            return html`<td class=${cls} title=${tooltip}>${text}</td>`;
                        } else {
                            return html`<td></td>`;
                        }
                    })}
                </tr>`
            )}</tbody>
        </table>
    `;
}

function updatePriceChart(Chart, ghm_root, columns, max_duration, apply_coeff) {
    let conditions = columns.map(col => buildConditionsArray(col));
    let max_price = max_map.get(`${ghm_root}@${max_duration}`) || 0.0;

    let datasets = [];
    for (let i = columns.length - 1; i >= 0; i--) {
        let col = columns[i];

        let dataset = {
            label: `${col.ghs}${conditions[i].length ? '*' : ''} (${col.ghm})`,
            data: [],
            borderColor: modeToColor(col.ghm.substr(5, 1)),
            backgroundColor: modeToColor(col.ghm.substr(5, 1)),
            borderDash: (conditions[i].length ? [5, 5] : undefined),
            fill: false
        };

        for (let duration = 0; duration < max_duration; duration++) {
            let info = computeGhsPrice(col, duration, apply_coeff);

            if (info != null) {
                dataset.data.push({
                    x: duration,
                    y: info.price
                });

                max_price = Math.max(max_price, info.price);
            } else {
                dataset.data.push({});
            }
        }
        datasets.push(dataset);
    }

    // Stabilize Y maximum value across versions
    max_map.set(`${ghm_root}@${max_duration}`, max_price);

    if (chart_obj) {
        chart_obj.data.datasets = datasets;
        chart_obj.options.scales.y.ticks.suggestedMax = max_price;
        chart_obj.update();
    } else {
        let ctx = chart_canvas.getContext('2d');

        chart_obj = new Chart(ctx, {
            type: 'line',
            data: {
                datasets: datasets
            },
            options: {
                responsive: true,
                interaction: {
                    intersect: false,
                    mode: 'index',
                },
                animations: false,
                plugins: {
                    legend: {
                        reverse: true,
                        onClick: null
                    },
                    tooltip: {
                        callbacks: {
                            title: items => formatDuration(items[0].label),
                            label: item => `GHS ${item.dataset.label} : ${formatPrice(item.parsed.y, true)}`
                        }
                    }
                },
                elements: {
                    line: {
                        tension: 0
                    },
                    point: {
                        radius: 0,
                        hitRadius: 0
                    }
                },
                scales: {
                    x: {
                        type: 'linear',
                        ticks: {
                            stepSize: 10,
                            callback: value => formatDuration(value)
                        }
                    },
                    y: {
                        type: 'linear',
                        suggestedMin: 0.0,
                        suggestedMax: max_price,
                        ticks: {
                            callback: value => formatPrice(value)
                        }
                    }
                }
            }
        });
    }
}

function modeToColor(mode) {
    switch (mode) {
        case 'J': return '#9937aa';
        case 'T': return '#9937aa';
        case '1': return '#f2a10b';
        case '2': return '#ff6600';
        case '3': return '#ff0000';
        case '4': return '#870101';
        case 'A': return '#f2a10b';
        case 'B': return '#ff6600';
        case 'C': return '#ff0000';
        case 'D': return '#870101';
        case 'E': return '#7f2704';
        case 'Z': return '#525252';
        default: return 'black';
    };
}

function buildConditionsArray(ghs) {
    let conditions = [];

    if (ghs.unit_authorization)
        conditions.push('Autorisation Unité ' + ghs.unit_authorization);
    if (ghs.bed_authorization)
        conditions.push('Autorisation Lit ' + ghs.bed_authorization);
    if (ghs.minimum_duration)
        conditions.push('Durée ≥ ' + ghs.minimum_duration);
    if (ghs.minimum_age)
        conditions.push('Âge ≥ ' + ghs.minimum_age);
    for (mode of ghs.modes) {
        switch (mode) {
            case 'diabetes2': { conditions.push('Diabète < 2 nuits'); } break;
            case 'diabetes3': { conditions.push('Diabète < 3 nuits'); } break;
            case '!diabetes': { conditions.push('Hors diabète (FI)'); } break;
            case 'outpatient': { conditions.push('Ambulatoire'); } break;
            case '!outpatient': { conditions.push('Hors ambulatoire'); } break;
            case 'intermediary': { conditions.push('Intermédiaire'); } break;

        }
    }
    if (ghs.main_diagnosis)
        conditions.push('DP ' + ghs.main_diagnosis);
    if (ghs.diagnoses)
        conditions.push('Diagnostic ' + ghs.diagnoses);
    if (ghs.procedures)
        conditions.push('Acte ' + ghs.procedures.join(', '));

    return conditions;
}

function computeGhsPrice(ghs, duration, apply_coeff) {
    if (!ghs.ghs_cents)
        return null;
    if (!testGhsDuration(ghs.durations, duration))
        return null;

    let price_cents;
    let mode;
    if (ghs.exb_threshold && duration < ghs.exb_threshold) {
        price_cents = ghs.ghs_cents - ghs.exb_cents * (ghs.exb_once ? 1 : ghs.exb_threshold - duration);
        mode = 'exb';
    } else if (ghs.exh_threshold && duration >= ghs.exh_threshold) {
        price_cents = ghs.ghs_cents + (duration - ghs.exh_threshold + 1) * ghs.exh_cents;
        mode = 'exh';
    } else {
        price_cents = ghs.ghs_cents;
        mode = 'noex';
    }

    price_cents = applyGhsCoefficient(price_cents, !apply_coeff || ghs.ghs_coefficient);
    return { price: price_cents, mode: mode };
}

function applyGhsCoefficient(cents, coefficient) {
    return cents ? (cents * coefficient) : cents;
}

function testGhsDuration(mask, duration) {
    let duration_mask = (duration < 32) ? (1 << duration) : (1 << 31);
    return !!(mask & duration_mask);
}

function computeGhsDelta(ghs, ghs_diff, duration, apply_coeff) {
    let info = ghs ? computeGhsPrice(ghs, duration, apply_coeff) : null;
    let info_diff = ghs_diff ? computeGhsPrice(ghs_diff, duration, apply_coeff) : null;

    let delta;
    let mode;
    if (info != null && info_diff != null) {
        info.price -= info_diff.price;
        if (info.price < 0) {
            info.mode += ' diff lower';
        } else if (info.price > 0) {
            info.mode += ' diff higher';
        } else {
            info.mode += ' diff neutral';
        }
    } else if (info != null) {
        info.mode += ' added';
    } else if (info_diff != null) {
        info = { price: -info_diff.price, mode: info_diff.mode + ' removed' };
    } else {
        return null;
    }

    return info;
}

// ------------------------------------------------------------------------
// Tree
// ------------------------------------------------------------------------

let collapse_nodes = new Set;

async function runTree() {
    let version = findVersion(route.version);
    let [mco, tree_nodes, highlight_map] = await Promise.all([
        DataCache.fetchDictionary('mco'),

        DataCache.fetchJson(Util.pasteURL(`${ENV.base_url}api/mco/tree`, {
            date: version.begin_date
        })),
        route.tree.filter ? DataCache.fetchJson(Util.pasteURL(`${ENV.base_url}api/mco/highlight`, {
            date: version.begin_date,
            diag: route.tree.diag ? route.tree.diag.replace('.', '').trim() : null,
            proc: route.tree.proc ? route.tree.proc.trim() : null
        })) : null
    ]);

    document.title = `THOP – Arbre de groupage (${version.begin_date})`;

    // Options
    render(html`
        ${renderVersionLine(settings.mco.versions, version)}
        <label>
            Filtre sur diagnostic ou acte
            <input type="checkbox" .checked=${route.tree.filter}
                   @change=${e => thop.go(route, { tree: { filter: e.target.checked } })} />
        </label>
        <div class=${!route.tree.filter ? 'disabled' : ''}>
            <label>Diagnostic <input type="text" .value=${route.tree.diag || ''}
                                     placeholder="* pour tout accepter"
                                     @change=${e => thop.go(route, { tree: { diag: e.target.value || null } })} /></label>
            <label>Acte <input type="text" .value=${route.tree.proc || ''}
                               placeholder="* pour tout accepter"
                               @change=${e => thop.go(route, { tree: { proc: e.target.value || null } })} /></label>
            <label>Séance <input type="checkbox" .checked=${route.tree.session}
                                 @change=${e => thop.go(route, { tree: { session: e.target.checked } })} /></label>
            <label>Âge ≤ 7 jours <input type="checkbox" .checked=${route.tree.a7d}
                                        @change=${e => thop.go(route, { tree: { a7d: e.target.checked } })} /></label>
        </div>
    `, document.querySelector('#th_options'));

    // Tree
    render(renderTree(tree_nodes, highlight_map, route.tree.session, route.tree.a7d, mco),
           document.querySelector('#th_view'));
}

function renderTree(nodes, highlight_map, session, a7d, mco) {
    if (nodes.length) {
        let mask = (session ? (1 << 1) : (1 << 0)) |
                   (a7d ? (1 << 3) : (1 << 2));
        let children = buildTreeRec(nodes, highlight_map, mask, mco, 0, '', []);

        return html`
            <ul class="tr_tree">
                ${renderTreeChildren(children)}
            </ul>
        `;
    } else {
        return '';
    }
}

function buildTreeRec(nodes, highlight_map, mask, mco, start_idx, chain_str, parent_next_indices) {
    let elements = [];

    let indices = [];
    for (let node_idx = start_idx;;) {
        indices.push(node_idx);

        let node = nodes[node_idx];
        if (nodes[node_idx].test === 20)
            break;

        node_idx = node.children_idx;
        if (node_idx === undefined)
            break;
        node_idx += !!node.reverse;
    }

    while (indices.length) {
        let node_idx = indices[0];
        let node = nodes[node_idx];
        indices.shift();

        if (node.type === 'ghm') {
            let ghm_root = node.key.substr(0, 5);
            let label = mco.ghm_roots.label(ghm_root);

            let irrelevant = highlight_map && !((highlight_map[node_idx] & mask) === mask);
            let li = renderTreeGhm(node_idx, node.text, label, irrelevant);

            elements.push(li);
        } else if (node.test === 20 && node.children_idx === parent_next_indices[0]) {
            // Hide GOTO nodes at the end of a chain, the classifier uses those to
            // jump back to go back one level.
            break;
        } else if (node.children_count > 2 && nodes[node.children_idx + 1].header) {
            // Here we deal with jump lists (mainly D-xx and D-xxxx)
            for (let j = 1; j < node.children_count; j++) {
                let recurse_str = chain_str + node.key + ('0' + j).slice(-2);
                let pseudo_idx = (j > 1) ? `${node_idx}-${j}` : node_idx;
                let pseudo_text = `${node.text} ${nodes[node.children_idx + j].header}`;

                let children = buildTreeRec(nodes, highlight_map, mask, mco,
                                            node.children_idx + j, recurse_str, indices);

                let irrelevant = highlight_map && !((highlight_map[node.children_idx + j] & mask) === mask);
                let li = renderTreeTest(pseudo_idx, pseudo_text, irrelevant, children, recurse_str);

                elements.push(li);
            }
        } else if (node.children_count === 2) {
            let recurse_str = chain_str + node.key;

            let children = buildTreeRec(nodes, highlight_map, mask, mco,
                                        node.children_idx + !node.reverse, recurse_str, indices);

            let irrelevant = highlight_map && !((highlight_map[node_idx] & mask) === mask);
            let li = renderTreeTest(node_idx, node.reverse || node.text, irrelevant, children, recurse_str);

            elements.push(li);
        } else {
            let recurse_str = chain_str + node.key;
            let leaf = !node.children_count || node.children_count == 1;

            let children = Util.mapRange(1, node.children_count,
                                         j => buildTreeRec(nodes, highlight_map, mask, mco,
                                                           node.children_idx + j, recurse_str, indices));
            children = Array.from(children);

            let irrelevant = highlight_map && !((highlight_map[node_idx] & mask) === mask);
            let li = renderTreeTest(node_idx, node.text, irrelevant, children, recurse_str);

            elements.push(li);

            // Hide repeated subtrees, this happens with error-generating nodes 80 and 222
            if (node.test !== 20 && parent_next_indices.includes(node.children_idx)) {
                if (node.children_idx != parent_next_indices[0]) {
                    let pseudo_idx = `${node_idx}-2`;
                    let pseudo_text = 'Saut noeud ' + node.children_idx;

                    let li = renderTreeTest(pseudo_idx, pseudo_text, true);
                    elements.push(li);
                }

                break;
            }
        }
    }

    return elements;
}

function renderTreeTest(idx, text, irrelevant, children = [], chain_str = null) {
    if (children.length === 1 && children[0].type === 'leaf') {
        irrelevant &= children[0].irrelevant;

        // Simplify when there is only one leaf children
        let ret = {
            idx: idx,
            type: 'leaf',
            irrelevant: irrelevant,
            vdom: html`<span><span class="n">${idx} </span>${addSpecLinks(text)}</span>
                       <span class="direct">${children[0].vdom}</span>`
        };

        return ret;
    } else if (children.length) {
        irrelevant &= children.every(child => child.irrelevant);

        let ret = {
            idx: idx,
            type: 'parent',
            irrelevant: irrelevant,
            chain_str: chain_str,
            vdom: html`<span><span class="n" @click=${handleTreeTestClick}>${idx} </span>${addSpecLinks(text)}</span>
                       <ul>${renderTreeChildren(children)}</ul>`
        };

        return ret;
    } else {
        let ret = {
            idx: idx,
            type: 'leaf',
            irrelevant: irrelevant,
            vdom: html`<span><span class="n">${idx} </span>${addSpecLinks(text)}</span>`
        };

        return ret;
    }
}

function renderTreeChildren(children) {
    return children.map(child => {
        let cls = child.type;
        if (child.irrelevant) {
            cls += ' collapse irrelevant';
        } else if (child.chain_str && collapse_nodes.has(child.chain_str)) {
            cls += ' collapse';
        }

        return html`<li id=${'n' + child.idx} class=${cls}>${child.vdom}</li>`;
    });
}

function handleTreeTestClick(e) {
    let li = e.target.parentNode.parentNode;
    if (li.classList.toggle('collapse')) {
        collapse_nodes.add(li.dataset.chain);
    } else {
        collapse_nodes.delete(li.dataset.chain);
    }

    e.preventDefault();
}

function renderTreeGhm(idx, text, label, irrelevant) {
    let ret = {
        idx: idx,
        type: 'leaf',
        irrelevant: irrelevant,
        vdom: html`<span><span class="n">${idx} </span>${addSpecLinks(text)}
                         <span class="label">${label}</span></span>`
    };

    return ret;
}

// ------------------------------------------------------------------------
// Common options
// ------------------------------------------------------------------------

function renderVersionLine(versions, current_version) {
    let vlin = new VersionLine;

    vlin.urlBuilder = version => makeURL({ version: version.date });
    vlin.clickHandler = (e, version) => {
        thop.go(route, { version: version.date });
        e.preventDefault();
    };

    for (let version of versions) {
        let label = version.begin_date.toString();
        if (label.endsWith('-01'))
            label = label.substr(0, label.length - 3);

        vlin.addVersion(version.begin_date, label, version.begin_date, version.changed_prices);
    }
    if (current_version)
        vlin.currentDate = current_version.begin_date;

    return vlin.render();
}

function renderSectorSelector(current_sector) {
    let help = `Le périmètre des tarifs des GHS est différent :
– Secteur public : prestation complète
– Secteur privé : clinique et personnel non médical`;

    return html`
        <label>
            Secteur <abbr title=${help}>?</abbr>
            <select @change=${e => thop.go(route, { sector: e.target.value })}>
                <option value="public" .selected=${current_sector === 'public'}>Public</option>
                <option value="private" .selected=${current_sector === 'private'}>Privé</option>
            </select>
        </label>
    `;
}

// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

function findVersion(date) {
    let version = settings.mco.versions.find(version => version.begin_date.equals(date));
    if (!version)
        throw new Error(`Version MCO '${date}' inexistante`);
    return version;
}

function addSpecLinks(str) {
    let elements = [];
    for (;;) {
        let m;
        let frag;
        if (m = str.match(/\bA(\-[0-9]+|\$[0-9]+\.[0-9]+)/)) {
            frag = html`<a href=${makeURL({ mode: 'procedures', procedures: { list: m[0], offset: 0 } })}>${m[0]}</a>`;
        } else if (m = str.match(/\bD(\-[0-9]+|\$[0-9]+\.[0-9]+)/)) {
            frag = html`<a href=${makeURL({ mode: 'diagnoses', diagnoses: { list: m[0], offset: 0 } })}>${m[0]}</a>`;
        } else if (m = str.match(/\b[0-9]{2}[CMZKH][0-9]{2}[ZJT0-9ABCDE]?( \[([0-9]{1,3})\])?/)) {
            let ghm_root = m[0].substr(0, 5);
            let tooltip = findCachedLabel('mco', 'ghm_roots', ghm_root) || '';

            if (m[2]) {
                let error = findCachedLabel('mco', 'errors', m[2]);
                if (error)
                    tooltip += `\nErreur ${m[2]} : ${error}`;
            }

            frag = html`<a class="ghm" href=${makeURL({ mode: 'ghs', ghs: { ghm_root: ghm_root } })} title=${tooltip}>${m[0]}</a>`;
        } else if (m = str.match(/\b[A-Z]{4}[0-9+]{3}/)) {
            let tooltip = findCachedLabel('ccam', 'procedures', m[0]);
            frag = tooltip ? html`<abbr title=${tooltip}>${m[0]}</abbr>` : m[0];
        } else if (m = str.match(/\b[A-Z][0-9+]{2}(\.?[0-9+]{1,3})?/)) {
            let code = m[0].replace('.', '');
            let code_with_dot = code.length >= 4 ? `${code.substr(0, 3)}.${code.substr(3)}` : code;

            let tooltip = findCachedLabel('cim10', 'diagnoses', code);
            frag = tooltip ? html`<abbr title=${tooltip}>${code_with_dot}</abbr>` : code_with_dot;
        } else if (m = str.match(/\b[Nn]oeud ([0-9]+)/)) {
            frag = html`<a href=${makeURL({ mode: 'tree' }) + `#n${m[1]}`}>${m[0]}</a>`;
        } else {
            break;
        }

        elements.push(str.substr(0, m.index));
        elements.push(frag);
        str = str.substr(m.index + m[0].length);
    }
    elements.push(str);

    return elements;
}

function findCachedLabel(name, chapter, code) {
    let dict = DataCache.getCacheDictionary(name);
    return dict ? dict[chapter].label(code) : null;
}

function maskToRangeStr(mask) {
    if (mask === 0xFFFFFFFF)
        return '';

    let ranges = [];

    let i = 0;
    for (;;) {
        while (i < 32 && !(mask & (1 << i)))
            i++;
        if (i >= 32)
            break;

        let j = i + 1;
        while (j < 32 && (mask & (1 << j)))
            j++;
        j--;

        if (j == 31) {
            ranges.push(`≥ ${formatDuration(i)}`);
        } else if (j > i) {
            ranges.push(`${i}-${formatDuration(j)}`);
        } else {
            ranges.push(formatDuration(i));
        }

        i = j + 1;
    }

    return ranges.join(', ');
}

export {
    route,

    run,
    makeURL,
    parseURL,

    addSpecLinks
}
