// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_info = (function() {
    let self = this;

    this.route = {};

    this.run = async function() {
        let version = settings.mco.versions.find(version => version.begin_date.equals(self.route.version));
        if (!version)
            throw new Error(`Version MCO '${self.route.version}' inexistante`);

        render(renderVersionLine(settings.mco.versions, version),
               document.querySelector('#th_options'));

        switch (self.route.mode) {
            case 'tree': {
                await concepts.load('mco');

                let nodes = await fetch(`${env.base_url}api/mco_tree.json?date=${version.begin_date}`).then(response => response.json());
                render(renderTree(nodes),
                       document.querySelector('main'));
            } break;

            default: {
                throw new Error(`Mode inconnu '${self.route.mode}'`);
            } break;
        }
    };

    this.parseURL = function(path, query) {
        let parts = path.split('/');

        let args = {
            version: dates.fromString(parts[0] || null) ||
                     settings.mco.versions[settings.mco.versions.length - 1].begin_date,
            mode: parts[1] || 'tree'
        };

        return args;
    };

    this.makeURL = function(args = {}) {
        args = {...self.route, ...args};
        return `${env.base_url}mco_info/${args.version}/${args.mode}`;
    };

    // ------------------------------------------------------------------------
    // Options
    // ------------------------------------------------------------------------

    function renderVersionLine(versions, current_version) {
        let vlin = new VersionLine;

        vlin.hrefBuilder = version => self.makeURL({version: version.date});
        vlin.changeHandler = version => thop.go(null, {version: version.date});

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

    // ------------------------------------------------------------------------
    // Tree
    // ------------------------------------------------------------------------

    let collapse_nodes = new Set;

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
    // Utility
    // ------------------------------------------------------------------------

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
