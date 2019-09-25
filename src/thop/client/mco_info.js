// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_info = (function() {
    let self = this;

    this.route = {};

    this.run = async function() {
        switch (self.route.mode) {
            case 'prices': { await runPrices(); } break;
            case 'tree': { await runTree(); } break;

            default: {
                throw new Error(`Mode inconnu '${self.route.mode}'`);
            } break;
        }
    };

    this.parseURL = function(path, query) {
        let parts = path.split('/');

        // Common part
        let args = {
            version: dates.fromString(parts[0] || null) ||
                     settings.mco.versions[settings.mco.versions.length - 1].begin_date,
            mode: parts[1] || 'tree'
        };

        // Mode-specific part
        switch (args.mode) {
            case 'prices': {
                args.ghm_root = parts[2] || null;
                args.prices_duration = parseInt(query.duration, 10) || 200;
                args.prices_coeff = !!parseInt(query.coeff, 10) || false;
            } break;
            case 'tree': { /* Nothing to do */ } break;
        }

        return args;
    };

    this.makeURL = function(args = {}) {
        args = {...self.route, ...args};

        // Common part
        let url = `${env.base_url}mco_info/${args.version}/${args.mode}`;

        // Mode-specific part
        switch (args.mode) {
            case 'prices': {
                if (args.ghm_root)
                    url += `/${args.ghm_root}`;

                url = util.buildUrl(url, {
                    duration: args.prices_duration,
                    coeff: 0 + args.prices_coeff
                })
            } break;
            case 'tree': { /* Nothing to do */ } break;
        }

        return url;
    };

    // ------------------------------------------------------------------------
    // Prices
    // ------------------------------------------------------------------------

    async function runPrices() {
        let version = findVersion(self.route.version);
        let [ghm_roots, ghmghs] = await Promise.all([
            concepts.load('mco').then(mco => mco.ghm_roots),
            fetch(`${env.base_url}api/mco_ghmghs.json?sector=public&date=${version.begin_date}`).then(response => response.json())
        ]);

        if (!self.route.ghm_root)
            self.route.ghm_root = ghm_roots[0].code;

        let columns = ghmghs.filter(it => it.ghm_root === self.route.ghm_root);
        if (!columns.length)
            throw new Error(`Racine de GHM '${self.route.ghm_root}' inexistante`);

        render(html`
            ${renderVersionLine(settings.mco.versions, version)}
            ${renderGhmRootSelector(ghm_roots, self.route.ghm_root)}
            ${renderPrices(self.route.ghm_root, columns, self.route.prices_duration, self.route.prices_coeff)}
        `, document.querySelector('main'));
    }

    function renderPrices(ghm_root, columns, max_duration, apply_coeff) {
        let conditions = columns.map(col => buildConditionsArray(col));

        return html`
            <table class="pr_grid">
                <thead>
                    <tr><td class="ghm_root" colspan=${columns.length + 1}>${concepts.completeGhmRoot(ghm_root)}</td></tr>

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
        let version = findVersion(self.route.version);

        let [_, tree_nodes] = await Promise.all([
            concepts.load('mco'),
            fetch(`${env.base_url}api/mco_tree.json?date=${version.begin_date}`).then(response => response.json()),
        ]);

        render(html`
            ${renderVersionLine(settings.mco.versions, version)}
            ${renderTree(tree_nodes)}
        `, document.querySelector('main'));
    }

    function renderTree(nodes) {
        if (nodes.length) {
            let children = buildTreeRec(nodes, 0, '', []);
            return html`
                <ul class="tr_tree">
                    ${renderTreeChildren(children)}
                </ul>
            `;
        } else {
            return html``;
        }
    }

    function buildTreeRec(nodes, start_idx, chain_str, parent_next_indices) {
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

            if (node.test === 20 && node.children_idx === parent_next_indices[0]) {
                // Hide GOTO nodes at the end of a chain, the classifier uses those to
                // jump back to go back one level.
                break;
            } else if (node.children_count > 2 && nodes[node.children_idx + 1].header) {
                // Here we deal with jump lists (mainly D-xx and D-xxxx)
                for (let j = 1; j < node.children_count; j++) {
                    let recurse_str = chain_str + node.key + ('0' + j).slice(-2);
                    let pseudo_idx = (j > 1) ? ('' + node_idx + '-' + j) : node_idx;
                    let pseudo_text = node.text + ' ' + nodes[node.children_idx + j].header;

                    let children = buildTreeRec(nodes, node.children_idx + j, recurse_str, indices);
                    let li = renderTreeNode(pseudo_idx, pseudo_text, children, recurse_str);
                    elements.push(li);
                }
            } else if (node.children_count === 2) {
                let recurse_str = chain_str + node.key;

                let children = buildTreeRec(nodes, node.children_idx + !node.reverse, recurse_str, indices);
                let li = renderTreeNode(node_idx, node.reverse || node.text, children, recurse_str);
                elements.push(li);
            } else {
                let recurse_str = chain_str + node.key;
                let leaf = !node.children_count || node.children_count == 1;

                let children = util.mapRange(1, node.children_count,
                                             j => buildTreeRec(nodes, node.children_idx + j, recurse_str, indices));
                let li = renderTreeNode(node_idx, node.text, children, recurse_str);
                elements.push(li);

                // Hide repeated subtrees, this happens with error-generating nodes 80 and 222
                if (node.test !== 20 && parent_next_indices.includes(node.children_idx)) {
                    if (node.children_idx != parent_next_indices[0]) {
                        let pseudo_idx = '' + node_idx + '-2';
                        let pseudo_text = 'Saut noeud ' + node.children_idx;

                        let li = renderTreeNode(pseudo_idx, pseudo_text);
                        elements.push(li);
                    }

                    break;
                }
            }
        }

        return elements;
    }

    function handleTreeNodeClick(e) {
        let li = this.parentNode.parentNode;
        if (li.toggleClass('collapse')) {
            collapse_nodes.add(li.dataset.chain);
        } else {
            collapse_nodes.delete(li.dataset.chain);
        }

        e.preventDefault();
    }

    function renderTreeNode(idx, text, children = [], chain_str = null) {
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
                vdom: html`<span><span class="n" @click=${handleTreeNodeClick}>${idx} </span>${self.addSpecLinks(text)}</span>
                           <ul>${renderTreeChildren(children)}</ul>`
            };

            return ret;
        } else {
            let ret = {
                idx: idx,
                type: 'leaf',
                vdom: html`<span><span class="n">${idx} </span>${self.addSpecLinks(text, true)}</span>`
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

    // ------------------------------------------------------------------------
    // Options
    // ------------------------------------------------------------------------

    function renderVersionLine(versions, current_version) {
        let vlin = new VersionLine;

        vlin.hrefBuilder = version => self.makeURL({version: version.date});
        vlin.changeHandler = version => thop.go(self, {version: version.date});

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

    function renderGhmRootSelector(ghm_roots, current_ghm_root) {
        return html`
            <select @change=${e => thop.go(self, {ghm_root: e.target.value})}>
                ${ghm_roots.map(ghm_root => {
                    let disabled = false;
                    let label = `${ghm_root.code} – ${ghm_root.desc}${disabled ? ' *' : ''}`;

                    return html`<option value=${ghm_root.code} ?disabled=${disabled}
                                        ?selected=${ghm_root.code === current_ghm_root}>${label}</option>`
                })}
            </select>
        `
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
            let m = str.match(/([AD](\-[0-9]+|\$[0-9]+\.[0-9]+)|[0-9]{2}[CMZKH][0-9]{2}[ZJT0-9ABCDE]?( \[[0-9]{1,3}\])?|[Nn]oeud [0-9]+)/);
            if (!m)
                break;

            elements.push(str.substr(0, m.index));
            elements.push(makeSpecAnchor(m[0], append_desc));
            str = str.substr(m.index + m[0].length);
        }
        elements.push(str);

        return elements;
    };

    function makeSpecAnchor(str, append_desc) {
        append_desc = false;

        if (str[0] === 'A') {
            let url = self.makeURL({list: 'procedures', spec: str});
            let desc = append_desc ? catalog.getDesc('ccam', str) : null;

            return html`<a href=${url}>${str}</a>${desc ? html` <span class="desc">${desc}</span>` : html``}`;
        } else if (str[0] === 'D') {
            let url = self.makeURL({list: 'diagnoses', spec: str});
            let desc = append_desc ? catalog.getDesc('cim10', str) : null;

            return html`<a href=${url}>${str}</a>${desc ? html` <span class="desc">${desc}</span>` : html``}`;
        } else if (str.match(/^[0-9]{2}[CMZKH][0-9]{2}[ZJT0-9ABCDE]?( \[[0-9]{1,3}\])?$/)) {
            let code = str.substr(0, 5);

            let url = self.makeURL({view: 'table', ghm_root: code});
            let desc = concepts.descGhmRoot(code);
            let tooltip = desc ? `${code} - ${desc}` : '';
            if (!append_desc)
                desc = '';

            return html`<a class="ghm" href=${url} title=${tooltip}>${str}</a>${desc ? html` <span class="desc">${desc}</span>` : html``}`;
        } else if (str.match(/[Nn]oeud [0-9]+/)) {
            let url = self.makeURL() + `#n${str.substr(6)}`;

            return html`<a href=${url}>${str}</a>`;
        } else {
            return str;
        }
    }

    return this;
}).call({});
