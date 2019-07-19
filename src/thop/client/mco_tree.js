// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_tree = (function() {
    let self = this;

    // Cache
    let tree_url = null;
    let nodes = [];
    let collapse_nodes = new Set();

    this.runModule = function(route) {
        // Resources
        let indexes = thop.updateMcoSettings().indexes;
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        let main_index = indexes.findIndex(info => info.begin_date === route.date);
        if (main_index >= 0)
            updateTree(indexes[main_index].begin_date);

        // Errors
        if (route.date !== null && indexes.length && main_index < 0)
            thop.error('Date incorrecte');

        // Refresh settings
        queryAll('#opt_index').removeClass('hide');
        refreshIndexesLine(indexes, main_index);

        // Refresh view
        if (!thop.isBusy()) {
            refreshTree(nodes);

            deploySelectedNode(route.highlight_node);
            route.highlight_node = null;
        }
    };

    this.parseRoute = function(route, path, parameters, hash) {
        // Mode: tree/<date>
        let path_parts = path.split('/');

        route.date = path_parts[1] || null;
        route.highlight_node = hash;
    };

    this.routeToUrl = function(args) {
        let new_route = thop.buildRoute(args);

        let url;
        {
            let url_parts = [thop.baseUrl('mco_tree'), new_route.date];
            while (!url_parts[url_parts.length - 1])
                url_parts.pop();

            url = url_parts.join('/');
        }

        return {
            url: url,
            allowed: true
        };
    };

    function updateTree(date) {
        let url = util.buildUrl(thop.baseUrl('api/mco_tree.json'), {date: date});
        if (url === tree_url)
            return;

        nodes = [];
        data.get(url, 'json', json => {
            nodes = json;
            tree_url = url;
        });
    }

    function refreshTree(nodes) {
        let root_el = query('#view');

        if (nodes.length) {
            let children = recurseNodes(0, '', []);
            render(html`
                <ul id="tr_tree">
                    ${renderChildren(children)}
                </ul>
            `, root_el);
        } else {
            render(html``, root_el);
        }
    }

    function recurseNodes(start_idx, chain_str, parent_next_indices) {
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

                    let children = recurseNodes(node.children_idx + j, recurse_str, indices);
                    let li = renderNode(pseudo_idx, pseudo_text, children, recurse_str);
                    elements.push(li);
                }
            } else if (node.children_count === 2) {
                let recurse_str = chain_str + node.key;

                let children = recurseNodes(node.children_idx + !node.reverse, recurse_str, indices);
                let li = renderNode(node_idx, node.reverse || node.text, children, recurse_str);
                elements.push(li);
            } else {
                let recurse_str = chain_str + node.key;
                let leaf = !node.children_count || node.children_count == 1;

                let children = util.mapRange(1, node.children_count,
                                             j => recurseNodes(node.children_idx + j, recurse_str, indices));
                let li = renderNode(node_idx, node.text, children, recurse_str);
                elements.push(li);

                // Hide repeated subtrees, this happens with error-generating nodes 80 and 222
                if (node.test !== 20 && parent_next_indices.includes(node.children_idx)) {
                    if (node.children_idx != parent_next_indices[0]) {
                        let pseudo_idx = '' + node_idx + '-2';
                        let pseudo_text = 'Saut noeud ' + node.children_idx;

                        let li = renderNode(pseudo_idx, pseudo_text);
                        elements.push(li);
                    }

                    break;
                }
            }
        }

        return elements;
    }

    function handleNodeClick(e) {
        let li = this.parentNode.parentNode;
        if (li.toggleClass('collapse')) {
            collapse_nodes.add(li.dataset.chain);
        } else {
            collapse_nodes.delete(li.dataset.chain);
        }

        e.preventDefault();
    }

    function renderNode(idx, text, children = [], chain_str = null) {
        if (children.length === 1 && children[0].type === 'leaf') {
            // Simplify when there is only one leaf children
            let ret = {
                idx: idx,
                type: 'leaf',
                vdom: html`<span><span class="n">${idx} </span>${mco_list.addSpecLinks(text)}</span>
                           <span class="direct">${children[0].vdom}</span>`
            };

            return ret;
        } else if (children.length) {
            let ret = {
                idx: idx,
                type: 'parent',
                chain_str: chain_str,
                vdom: html`<span><span class="n" @click=${handleNodeClick}>${idx} </span>${mco_list.addSpecLinks(text)}</span>
                           <ul>${renderChildren(children)}</ul>`
            };

            return ret;
        } else {
            let ret = {
                idx: idx,
                type: 'leaf',
                vdom: html`<span><span class="n">${idx} </span>${mco_list.addSpecLinks(text, true)}</span>`
            };

            return ret;
        }
    }

    function renderChildren(children) {
        return children.map(child => {
            let cls = child.type + (child.chain_str && collapse_nodes.has(child.chain_str) ? ' collapse' : '');
            return html`<li id=${'n' + child.idx} class=${cls}>${child.vdom}</li>`;
        });
    }

    function deploySelectedNode(hash) {
        let root_el = query('#tr_tree');

        // Make sure the corresponding node is visible
        if (hash && hash.match(/^n[0-9]+$/)) {
            let el = root_el.query('#' + hash);
            while (el && el !== root_el) {
                if (el.tagName === 'LI' && el.dataset.chain) {
                    el.removeClass('collapse');
                    collapse_nodes.delete(el.dataset.chain);
                }
                el = el.parentNode;
            }
        }
    }

    return this;
}).call({});
