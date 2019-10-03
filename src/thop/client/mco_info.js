// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_info = (function() {
    let self = this;

    let route = {};
    this.route = route;

    this.run = async function() {
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
    };

    this.parseURL = function(path, params = {}) {
        let args = {
            version: dates.fromString(path[0] || null) ||
                     settings.mco.versions[settings.mco.versions.length - 1].begin_date,
            mode: path[1] || 'ghm_roots',

            ghmghs: {},
            diagnoses: {},
            procedures: {},

            ghs: {}
        };

        // Mode-specific part
        switch (args.mode) {
            case 'ghm_roots': { /* Nothing to do */ } break;
            case 'ghmghs': {
                args.sector = path[2] || 'public';
                args[args.mode].offset = parseInt(params.offset, 10) || null;
                args[args.mode].sort = params.sort || null;
            } break;
            case 'diagnoses':
            case 'procedures': {
                args[args.mode].list = path[2] || null;
                args[args.mode].offset = parseInt(params.offset, 10) || null;
                args[args.mode].sort = params.sort || null;
            } break;

            case 'ghs': {
                args.sector = path[2] || 'public';
                args.ghm_root = path[3] || null;
                args.ghs.duration = parseInt(params.duration, 10) || 200;
                args.ghs.coeff = !!parseInt(params.coeff, 10) || false;
            } break;

            case 'tree': { /* Nothing to do */ } break;
        }

        return args;
    };

    this.makeURL = function(args = {}) {
        args = util.assignDeep({}, route, args);

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
            } break;
            case 'diagnoses':
            case 'procedures': {
                if (args[args.mode].list)
                    path.push(args[args.mode].list);
                params.offset = args[args.mode].offset || null;
                params.sort = args[args.mode].sort || null;
            } break;

            case 'ghs': {
                path.push(args.sector);
                if (args.ghm_root)
                    path.push(args.ghm_root);

                params.duration = args.ghs.duration;
                params.coeff = (args.ghs.coeff != null) ? (0 + args.ghs.coeff) : null;
            } break;

            case 'tree': { /* Nothing to do */ } break;
        }

        return util.pasteURL(`${env.base_url}${path.join('/')}`, params);
    };

    // ------------------------------------------------------------------------
    // Lists
    // ------------------------------------------------------------------------

    async function runGhmRoots() {
        let version = findVersion(route.version);
        let [mco, ghmghs] = await Promise.all([
            data.fetchDictionary('mco'),
            data.fetchJSON(`${env.base_url}api/mco_ghmghs.json?sector=${route.sector || 'public'}&date=${version.begin_date}`)
        ]);

        let ghm_roots = ghmghs.filter((ghs, idx) => !idx || ghmghs[idx - 1].ghm_root !== ghs.ghm_root)
                              .map(ghs => ghs.ghm_root);

        // Options
        render(html`
            ${renderVersionLine(settings.mco.versions, version)}
        `, document.querySelector('#th_options'));

        // Table
        renderList(ghm_roots, {
            header: false,
            columns: [{key: 'ghm_root', title: 'Racine de GHM', func: ghm_root => mco.ghm_roots.describe(ghm_root)}]
        });
    }

    async function runGhmGhs() {
        let version = findVersion(route.version);
        let [mco, ghmghs] = await Promise.all([
            data.fetchDictionary('mco'),
            data.fetchJSON(`${env.base_url}api/mco_ghmghs.json?sector=${route.sector}&date=${version.begin_date}`)
        ]);

        // Options
        render(html`
            ${renderVersionLine(settings.mco.versions, version)}
            ${renderSectorSelector(route.sector)}
        `, document.querySelector('#th_options'));

        // Table
        renderList(ghmghs, {
            header: true,
            page_len: 300,

            offset: route.ghmghs.offset,
            sort: route.ghmghs.sort,
            route: (offset, sort) => ({ghmghs: {offset: offset, sort: sort}}),

            columns: [
                {key: 'ghm', title: 'GHM', func: ghs => mco.ghm.describe(ghs.ghm)},
                {key: 'ghs', title: 'GHS', func: ghs => ghs.ghs},
                {key: 'durations', title: 'Durées', func: ghs => maskToRangeStr(ghs.durations)},
                {key: 'confirm', title: 'Confirmation',
                    func: ghs => ghs.confirm_treshold ? `< ${format.duration(ghs.confirm_treshold)}` : ''},
                {key: 'main_diagnosis', title: 'DP', func: ghs => ghs.main_diagnosis},
                {key: 'diagnoses', title: 'Diagnostics', func: ghs => ghs.diagnoses},
                {key: 'procedures', title: 'Actes', func: ghs => ghs.procedures ? ghs.procedures.join(', ') : ''},
                {key: 'authorizations', title: 'Autorisations', tooltip: 'Autorisations (unités et lits)',
                    func: ghs => {
                        let ret = [];
                        if (ghs.unit_authorization)
                            ret.push(`Unité ${ghs.unit_authorization}`);
                        if (ghs.bed_authorization)
                            ret.push(`Lit ${ghs.bed_authorization}`);
                        return ret;
                    }
                },
                {key: 'old_severity', title: 'Sévérité âgé',
                    func: ghs => ghs.old_age_treshold ? `≥ ${ghs.old_age_treshold} et ` +
                                                        `< ${ghs.old_severity_limit + 1}` : null},
                {key: 'young_severity', title: 'Sévérité jeune',
                    func: ghs => ghs.young_age_treshold ? `< ${ghs.young_age_treshold} et ` +
                                                          `< ${ghs.young_severity_limit + 1}` : null}
            ]
        });
    }

    async function runDiagnoses() {
        let version = findVersion(route.version);
        let [cim10, diagnoses] = await Promise.all([
            data.fetchDictionary('cim10'),
            data.fetchJSON(`${env.base_url}api/mco_diagnoses.json?date=${version.begin_date}&spec=${route.diagnoses.list || ''}`)
        ]);

        // Options
        render(html`
            ${renderVersionLine(settings.mco.versions, version)}
            ${renderListInfo('diagnoses', 'diagnostics', route.diagnoses.list)}
        `, document.querySelector('#th_options'));

        // Table
        renderList(diagnoses, {
            header: true,
            page_len: 300,

            offset: route.diagnoses.offset,
            sort: route.diagnoses.sort,
            route: (offset, sort) => ({diagnoses: {offset: offset, sort: sort}}),

            columns: [
                {key: 'code', title: 'Code', func: diag => cim10.diagnoses.describe(diag.diag)},
                {key: 'severity', title: 'Niveau', func: diag => diag.severity},
                {key: 'cmd', title: 'CMD', func: diag => diag.cmd},
                {key: 'main_list', title: 'Liste principale', func: diag => diag.main_list}
            ]
        });
    }

    async function runProcedures() {
        let version = findVersion(route.version);
        let [ccam, procedures] = await Promise.all([
            data.fetchDictionary('ccam'),
            data.fetchJSON(`${env.base_url}api/mco_procedures.json?date=${version.begin_date}&spec=${route.procedures.list || ''}`)
        ]);

        // Options
        render(html`
            ${renderVersionLine(settings.mco.versions, version)}
            ${renderListInfo('procedures', 'actes', route.procedures.list)}
        `, document.querySelector('#th_options'));

        // Table
        renderList(procedures, {
            header: true,
            page_len: 300,

            offset: route.procedures.offset,
            sort: route.procedures.sort,
            route: (offset, sort) => ({procedures: {offset: offset, sort: sort}}),

            columns: [
                {key: 'code', title: 'Code', func: proc => ccam.procedures.describe(proc.proc)},
                {key: 'begin_date', title: 'Début', func: proc => dates.fromString(proc.begin_date)},
                {key: 'end_date', title: 'Fin', func: proc => dates.fromString(proc.end_date)},
                {key: 'phase', title: 'Phase', func: proc => proc.phase || null},
                {key: 'activities', title: 'Activités', func: proc => proc.activities},
                {key: 'extensions', title: 'Extensions', func: proc => proc.extensions}
            ]
        });
    }

    function renderList(records, handler) {
        let route2 = route[name];

        let etab = new EasyTable;

        if (handler.route) {
            etab.urlBuilder = (offset, sort_key) => self.makeURL(handler.route(offset, sort_key));
            etab.clickHandler = (e, offset, sort_key) => {
                thop.goFake(self, handler.route(offset, sort_key));
                e.preventDefault();
            };
        }

        etab.setPageLen(handler.page_len);
        etab.setOffset(handler.offset || 0);
        etab.setOptions({header: handler.header});

        for (let col of handler.columns) {
            etab.addColumn(col.key, col.title, value => {
                if (value == null) {
                    return null;
                } else if (typeof value === 'string') {
                    return self.addSpecLinks(value);
                } else {
                    return value.toLocaleString();
                }
            });
        }

        for (let i = 0; i < records.length; i++) {
            let record = records[i];

            etab.beginRow();
            for (let col of handler.columns) {
                let value = col.func(record);
                etab.addCell(value);
            }
            etab.endRow();
        }

        etab.sort(handler.sort);
        render(etab.render(), document.querySelector('#th_view'));
    }

    // ------------------------------------------------------------------------
    // GHS
    // ------------------------------------------------------------------------

    async function runGhs() {
        let version = findVersion(route.version);
        let [mco, ghmghs] = await Promise.all([
            data.fetchDictionary('mco'),
            data.fetchJSON(`${env.base_url}api/mco_ghmghs.json?sector=${route.sector}&date=${version.begin_date}`)
        ]);

        if (!route.ghm_root)
            route.ghm_root = mco.ghm_roots.definitions[0].code;

        // Options
        render(html`
            ${renderVersionLine(settings.mco.versions, version)}
            ${renderSectorSelector(route.sector)}
            <label>Durée <input type="number" step="5" min="0" max="500" .value=${route.ghs.duration}
                                 @change=${e => thop.go(self, {ghs: {duration: e.target.value}})}/></label>
            <label>Coefficient <input type="checkbox" .checked=${route.ghs.coeff}
                                       @change=${e => thop.go(self, {ghs: {coeff: e.target.checked}})}/></label>
            ${renderGhmRootSelector(mco, route.ghm_root)}
        `, document.querySelector('#th_options'));

        let columns = ghmghs.filter(it => it.ghm_root === route.ghm_root);
        if (!columns.length)
            throw new Error(`Racine de GHM '${route.ghm_root}' inexistante`);

        // Grid
        render(renderPriceGrid(route.ghm_root, columns, route.ghs.duration, route.ghs.coeff),
               document.querySelector('#th_view'));
    }

    function renderPriceGrid(ghm_root, columns, max_duration, apply_coeff) {
        let conditions = columns.map(col => buildConditionsArray(col));

        return html`
            <table class="pr_grid">
                <thead>
                    <tr><th>GHM</th>${util.mapRLE(columns.map(col => col.ghm),
                        (ghm, colspan) => html`<td class="desc" colspan=${colspan}>${ghm}</td>`)}</tr>
                    <tr><th>Niveau</th>${util.mapRLE(columns.map(col => col.ghm.substr(5, 1)),
                        (mode, colspan) => html`<td class="desc" colspan=${colspan}>Niveau ${mode}</td>`)}</tr>
                    <tr><th>GHS</th>${columns.map((col, idx) =>
                        html`<td class="desc">${col.ghs}${conditions[idx].length ? '*' : ''}</td>`)}</tr>
                    <tr><th>Conditions</th>${columns.map((col, idx) =>
                        html`<td class="conditions">${conditions[idx].map(cond => html`${self.addSpecLinks(cond)}<br/>`)}</td>`)}</tr>
                    <tr><th>Borne basse</th>${util.mapRLE(columns.map(col => col.exb_treshold),
                        (treshold, colspan) => html`<td class="exb" colspan=${colspan}>${format.duration(treshold)}</td>`)}</tr>
                    <tr><th>Borne haute</th>${util.mapRLE(columns.map(col => col.exh_treshold ? (col.exh_treshold - 1) : null),
                        (treshold, colspan) => html`<td class="exh" colspan=${colspan}>${format.duration(treshold)}</td>`)}</tr>
                    <tr><th>Tarif €</th>${util.mapRLE(columns.map(col =>
                        applyGhsCoefficient(col.ghs_cents, !apply_coeff || col.ghs_coefficient)),
                        (cents, colspan) => html`<td class="noex" colspan=${colspan}>${format.price(cents)}</td>`)}</tr>
                    <tr><th>Forfait EXB €</th>${util.mapRLE(columns.map(col =>
                        applyGhsCoefficient(col.exb_once ? col.exb_cents : null, !apply_coeff || col.ghs_coefficient)),
                        (cents, colspan) => html`<td class="exb" colspan=${colspan}>${format.price(cents)}</td>`)}</tr>
                    <tr><th>Tarif EXB €</th>${util.mapRLE(columns.map(col =>
                        applyGhsCoefficient(col.exb_once ? null : col.exb_cents, !apply_coeff || col.ghs_coefficient)),
                        (cents, colspan) => html`<td class="exb" colspan=${colspan}>${format.price(cents)}</td>`)}</tr>
                    <tr><th>Tarif EXH €</th>${util.mapRLE(columns.map(col =>
                        applyGhsCoefficient(col.exh_cents, !apply_coeff || col.ghs_coefficient)),
                        (cents, colspan) => html`<td class="exh" colspan=${colspan}>${format.price(cents)}</td>`)}</tr>
                    <tr><th>Age</th>${util.mapRLE(columns.map(col => {
                        let texts = [];
                        let severity = col.ghm.charCodeAt(5) - '1'.charCodeAt(0);
                        if (severity >= 0 && severity < 4) {
                            if (severity < col.young_severity_limit)
                                texts.push('< ' + col.young_age_treshold.toString());
                            if (severity < col.old_severity_limit)
                                texts.push('≥ ' + col.old_age_treshold.toString());
                        }

                        return texts.join(', ');
                    }), (text, colspan) => html`<td class="age" colspan=${colspan}>${text}</td>`)}</tr>
                </thead>

                <tbody>${util.mapRange(0, max_duration, duration =>
                    html`<tr class="duration">
                        <th>${format.duration(duration)}</th>
                        ${columns.map(col => {
                            let info = computeGhsPrice(col, duration, apply_coeff);

                            if (info) {
                                let cls = info.mode;
                                let tooltip = '';
                                if (!duration && col.warn_cmd28) {
                                    cls += ' warn';
                                    tooltip += 'Devrait être orienté dans la CMD 28 (séance)\n';
                                }
                                if (self.testGhsDuration(col.raac_durations || 0, duration)) {
                                    cls += ' warn';
                                    tooltip += 'Accessible en cas de RAAC\n';
                                }
                                if (col.warn_ucd) {
                                    cls += ' info';
                                    tooltip += 'Possibilité de minoration UCD (40 €)\n';
                                }

                                let text = format.price(info.price, true);
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
        switch (ghs.special_mode) {
            case 'diabetes': { conditions.push('FI diabète < ' + ghs.special_duration + ' nuits'); } break;
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
        if (!self.testGhsDuration(ghs.durations, duration))
            return null;

        let price_cents;
        let mode;
        if (ghs.exb_treshold && duration < ghs.exb_treshold) {
            price_cents = ghs.ghs_cents;
            if (ghs.exb_once) {
                price_cents -= ghs.exb_cents;
            } else {
                price_cents -= (ghs.exb_treshold - duration) * ghs.exb_cents;
            }
            mode = 'exb';
        } else if (ghs.exh_treshold && duration >= ghs.exh_treshold) {
            price_cents = ghs.ghs_cents + (duration - ghs.exh_treshold + 1) * ghs.exh_cents;
            mode = 'exh';
        } else {
            price_cents = ghs.ghs_cents;
            mode = 'noex';
        }

        price_cents = applyGhsCoefficient(price_cents, !apply_coeff || ghs.ghs_coefficient);
        return {price: price_cents, mode: mode};
    }

    function applyGhsCoefficient(cents, coefficient) {
        return cents ? (cents * coefficient) : cents;
    }

    this.testGhsDuration = function(mask, duration) {
        let duration_mask = (duration < 32) ? (1 << duration) : (1 << 31);
        return !!(mask & duration_mask);
    };

    // ------------------------------------------------------------------------
    // Tree
    // ------------------------------------------------------------------------

    let collapse_nodes = new Set;

    async function runTree() {
        let version = findVersion(route.version);
        let [mco, tree_nodes] = await Promise.all([
            data.fetchDictionary('mco'),
            data.fetchJSON(`${env.base_url}api/mco_tree.json?date=${version.begin_date}`)
        ]);

        // Options
        render(renderVersionLine(settings.mco.versions, version),
               document.querySelector('#th_options'));

        // Tree
        render(renderTree(tree_nodes, mco),
               document.querySelector('#th_view'));
    }

    function renderTree(nodes, mco) {
        if (nodes.length) {
            let children = buildTreeRec(nodes, mco, 0, '', []);
            return html`
                <ul class="tr_tree">
                    ${renderTreeChildren(children)}
                </ul>
            `;
        } else {
            return '';
        }
    }

    function buildTreeRec(nodes, mco, start_idx, chain_str, parent_next_indices) {
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

                let li = renderTreeGhm(node_idx, node.text, label);
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

                    let children = buildTreeRec(nodes, mco, node.children_idx + j, recurse_str, indices);
                    let li = renderTreeTest(pseudo_idx, pseudo_text, children, recurse_str);
                    elements.push(li);
                }
            } else if (node.children_count === 2) {
                let recurse_str = chain_str + node.key;

                let children = buildTreeRec(nodes, mco, node.children_idx + !node.reverse, recurse_str, indices);
                let li = renderTreeTest(node_idx, node.reverse || node.text, children, recurse_str);
                elements.push(li);
            } else {
                let recurse_str = chain_str + node.key;
                let leaf = !node.children_count || node.children_count == 1;

                let children = util.mapRange(1, node.children_count,
                                             j => buildTreeRec(nodes, mco, node.children_idx + j, recurse_str, indices));
                let li = renderTreeTest(node_idx, node.text, children, recurse_str);
                elements.push(li);

                // Hide repeated subtrees, this happens with error-generating nodes 80 and 222
                if (node.test !== 20 && parent_next_indices.includes(node.children_idx)) {
                    if (node.children_idx != parent_next_indices[0]) {
                        let pseudo_idx = `${node_idx}-2`;
                        let pseudo_text = 'Saut noeud ' + node.children_idx;

                        let li = renderTreeTest(pseudo_idx, pseudo_text);
                        elements.push(li);
                    }

                    break;
                }
            }
        }

        return elements;
    }

    function renderTreeTest(idx, text, children = [], chain_str = null) {
        if (children.length === 1 && children[0].type === 'leaf') {
            // Simplify when there is only one leaf children
            let ret = {
                idx: idx,
                type: 'leaf',
                vdom: html`<span><span class="n">${idx} </span>${self.addSpecLinks(text)}</span>
                           <span class="direct">${children[0].vdom}</span>`
            };

            return ret;
        } else if (children.length) {
            let ret = {
                idx: idx,
                type: 'parent',
                chain_str: chain_str,
                vdom: html`<span><span class="n" @click=${handleTreeTestClick}>${idx} </span>${self.addSpecLinks(text)}</span>
                           <ul>${renderTreeChildren(children)}</ul>`
            };

            return ret;
        } else {
            let ret = {
                idx: idx,
                type: 'leaf',
                vdom: html`<span><span class="n">${idx} </span>${self.addSpecLinks(text)}</span>`
            };

            return ret;
        }
    }

    function renderTreeChildren(children) {
        return children.map(child => {
            let cls = child.type + (child.chain_str && collapse_nodes.has(child.chain_str) ? ' collapse' : '');
            return html`<li id=${'n' + child.idx} class=${cls}>${child.vdom}</li>`;
        });
    }

    function handleTreeTestClick(e) {
        let li = this.parentNode.parentNode;
        if (li.classList.toggle('collapse')) {
            collapse_nodes.add(li.dataset.chain);
        } else {
            collapse_nodes.delete(li.dataset.chain);
        }

        e.preventDefault();
    }

    function renderTreeGhm(idx, text, label) {
        let ret = {
            idx: idx,
            type: 'leaf',
            vdom: html`<span><span class="n">${idx} </span>${self.addSpecLinks(text)}
                             <span class="label">${label}</span></span>`
        };

        return ret;
    }

    // ------------------------------------------------------------------------
    // Options
    // ------------------------------------------------------------------------

    function renderVersionLine(versions, current_version) {
        let vlin = new VersionLine;

        vlin.urlBuilder = version => self.makeURL({version: version.date});
        vlin.clickHandler = (e, version) => {
            thop.go(self, {version: version.date});
            e.preventDefault();
        };

        for (let version of versions) {
            let label = version.begin_date.toString();
            if (label.endsWith('-01'))
                label = label.substr(0, label.length - 3);

            vlin.addVersion(version.begin_date, label, version.begin_date, version.changed_prices);
        }
        if (current_version)
            vlin.setDate(current_version.begin_date);

        return vlin.render();
    }

    function renderSectorSelector(current_sector) {
        let help = `Le périmètre des tarifs des GHS est différent :
– Secteur public : prestation complète
– Secteur privé : clinique et personnel non médical`;

        return html`
            <label>
                Secteur <abbr title="${help}">?</abbr>
                <select @change=${e => thop.go(self, {sector: e.target.value})}>
                    <option value="public" .selected=${current_sector === 'public'}>Public</option>
                    <option value="private" .selected=${current_sector === 'private'}>Privé</option>
                </select>
            </label>
        `;
    }

    function renderGhmRootSelector(mco, current_ghm_root) {
        return html`
            <select @change=${e => thop.go(self, {ghm_root: e.target.value})}>
                ${mco.ghm_roots.definitions.map(ghm_root => {
                    let disabled = false;
                    let label = `${ghm_root.describe()}${disabled ? ' *' : ''}`;

                    return html`<option value=${ghm_root.code} ?disabled=${disabled}
                                        .selected=${ghm_root.code === current_ghm_root}>${label}</option>`
                })}
            </select>
        `
    }

    function renderListInfo(type, label, current_list) {
        if (current_list) {
            let args = {};
            args[type] = {list: null, offset: 0};

            return html`
                <div class="opt_list">
                    <b>Liste :</b> ${current_list}
                    <a href=${self.makeURL(args)}>(afficher tout)</a>
                </div>
            `;
        } else {
            return html`
                <div class="opt_list">
                    <b>Liste :</b> tous les ${label}
                </div>
            `;
        }
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

    this.addSpecLinks = function(str, append_desc = false) {
        let elements = [];
        for (;;) {
            let m;
            let frag;
            if (m = str.match(/A(\-[0-9]+|\$[0-9]+\.[0-9]+)/)) {
                frag = html`<a href=${self.makeURL({mode: 'procedures', procedures: {list: m[0], offset: 0}})}>${m[0]}</a>`;
            } else if (m = str.match(/D(\-[0-9]+|\$[0-9]+\.[0-9]+)/)) {
                frag = html`<a href=${self.makeURL({mode: 'diagnoses', diagnoses: {list: m[0], offset: 0}})}>${m[0]}</a>`;
            } else if (m = str.match(/[0-9]{2}[CMZKH][0-9]{2}[ZJT0-9ABCDE]?( \[[0-9]{1,3}\])?/)) {
                let ghm_root = m[0].substr(0, 5);
                let tooltip = findCachedLabel('mco', 'ghm_roots', ghm_root) || '';

                frag = html`<a class="ghm" href=${self.makeURL({mode: 'ghs', ghm_root: ghm_root})} title=${tooltip}>${m[0]}</a>`;
            } else if (m = str.match(/[A-Z]{4}[0-9+]{3}/)) {
                let tooltip = findCachedLabel('ccam', 'procedures', m[0]);
                frag = tooltip ? html`<abbr title=${tooltip}>${m[0]}</abbr>` : m[0];
            } else if (m = str.match(/[A-Z][0-9+]{2,5}/)) {
                let tooltip = findCachedLabel('cim10', 'diagnoses', m[0]);
                frag = tooltip ? html`<abbr title=${tooltip}>${m[0]}</abbr>` : m[0];
            } else if (m = str.match(/[Nn]oeud ([0-9]+)/)) {
                frag = html`<a href=${self.makeURL({mode: 'tree'}) + `#n${m[1]}`}>${m[0]}</a>`;
            } else {
                break;
            }

            elements.push(str.substr(0, m.index));
            elements.push(frag);
            str = str.substr(m.index + m[0].length);
        }
        elements.push(str);

        return elements;
    };

    function findCachedLabel(name, chapter, code) {
        let dict = data.fetchCachedDictionary(name);
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
                ranges.push(`≥ ${format.duration(i)}`);
            } else if (j > i) {
                ranges.push(`${i}-${format.duration(j)}`);
            } else {
                ranges.push(format.duration(i));
            }

            i = j + 1;
        }

        return ranges.join(', ');
    }

    return this;
}).call({});
