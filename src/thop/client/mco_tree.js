// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_tree = {};
(function() {
    // Cache
    let tree_url = null;
    let nodes = [];
    let collapse_nodes = new Set();

    function runTree(route, path, parameters, hash, errors)
    {
        // Parse route (model: tree/<date>)
        let path_parts = path.split('/');
        route.date = path_parts[1] || null;

        // Resources
        let indexes = mco_common.updateSettings().indexes;
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        let main_index = indexes.findIndex(function(info) { return info.begin_date === route.date; });
        if (main_index >= 0)
            updateTree(indexes[main_index].begin_date);

        // Errors
        if (route.date !== null && indexes.length && main_index < 0)
            errors.add('Date incorrecte');

        // Refresh settings
        queryAll('#opt_index').removeClass('hide');
        mco_common.refreshIndexesLine(indexes, main_index);

        // Refresh view
        if (!data.isBusy()) {
            refreshTree(nodes);
            deploySelectedNode(hash);
        }

        query('#tr').removeClass('hide');
    }

    function routeToUrl(args)
    {
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
    }
    this.routeToUrl = routeToUrl;

    function go(args, delay)
    {
        thop.route(routeToUrl(args).url, delay);
    }
    this.go = go;

    function updateTree(date)
    {
        let url = buildUrl(thop.baseUrl('api/mco_tree.json'), {date: date});
        if (url === tree_url)
            return;

        nodes = [];
        data.get(url, 'json', function(json) {
            nodes = json;
            tree_url = url;
        });
    }

    function refreshTree(nodes)
    {
        if (!thop.needsRefresh(refreshTree, arguments))
            return;

        if (nodes.length) {
            query('#tr_tree').replaceContent(recurseNodes(0, '', []).childNodes);
        } else {
            query('#tr_tree').innerHTML = '';
        }
    }

    function recurseNodes(start_idx, chain_str, parent_next_indices)
    {
        let ul = html('ul');

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

            // Hide GOTO nodes at the end of a chain, the classifier uses those to
            // jump back to go back one level.
            if (node.test === 20 && node.children_idx === parent_next_indices[0])
                break;

            if (node.children_count > 2 && nodes[node.children_idx + 1].header) {
                // Here we deal with jump lists (mainly D-xx and D-xxxx)
                for (let j = 1; j < node.children_count; j++) {
                    let recurse_str = chain_str + node.key + ('0' + j).slice(-2);

                    let pseudo_idx = (j > 1) ? ('' + node_idx + '-' + j) : node_idx;
                    let pseudo_text = node.text + ' ' + nodes[node.children_idx + j].header;
                    let children = recurseNodes(node.children_idx + j, recurse_str, indices);
                    let li = createNodeLi(pseudo_idx, pseudo_text,
                                          children.tagName === 'UL' ? recurse_str : null);
                    li.appendContent(children);
                    ul.appendContent(li);
                }
            } else if (node.children_count === 2 && node.reverse) {
                let recurse_str = chain_str + node.key;

                let children = recurseNodes(node.children_idx, recurse_str, indices);
                let li = createNodeLi(node_idx, node.reverse,
                                      children.tagName === 'UL' ? recurse_str  : null);
                li.appendContent(children);
                ul.appendContent(li);
            } else if (node.children_count === 2) {
                let recurse_str = chain_str + node.key;

                let children = recurseNodes(node.children_idx + 1, recurse_str, indices);
                let li = createNodeLi(node_idx, node.text,
                                      children.tagName === 'UL' ? recurse_str : null);

                // Simplify OR GOTO chains
                while (indices.length && nodes[indices[0]].children_count === 2 &&
                       nodes[nodes[indices[0]].children_idx + 1].test === 20 &&
                       nodes[nodes[indices[0]].children_idx + 1].children_idx === node.children_idx + 1) {
                    li.appendContent(html('br'));

                    let li2 = createNodeLi(indices[0], nodes[indices[0]].text, recurse_str);
                    li2.children[0].id = li2.id;
                    li.appendContent(li2.childNodes);

                    indices.shift();
                }

                li.appendContent(children);
                ul.appendContent(li);
            } else {
                let recurse_str = chain_str + node.key;

                let leaf = !node.children_count || node.children_count == 1;
                let li = createNodeLi(node_idx, node.text, leaf ? null : recurse_str);
                for (let j = 1; j < node.children_count; j++) {
                    let children = recurseNodes(node.children_idx + j, recurse_str, indices);
                    li.appendContent(children);
                }
                ul.appendContent(li);

                // Hide repeated subtrees, this happens with error-generating nodes 80 and 222
                if (node.test !== 20 && parent_next_indices.includes(node.children_idx)) {
                    if (node.children_idx != parent_next_indices[0]) {
                        let pseudo_idx = '' + node_idx + '-2';
                        let pseudo_text = 'Saut noeud ' + node.children_idx;
                        let li = createNodeLi(pseudo_idx, pseudo_text, null);
                        ul.appendContent(li);
                    }

                    break;
                }
            }
        }

        // Simplify when there is only one leaf children
        if (ul.queryAll('li').length === 1) {
            ul = ul.query('li').childNodes;
            ul.addClass('direct');
            ul = Array.prototype.slice.call(ul);
        }

        return ul;
    }

    function deploySelectedNode(hash)
    {
        let ul = query('#tr_tree');

        // Make sure the corresponding node is visible
        if (hash && hash.match(/^n[0-9]+$/)) {
            let el = ul.query('#' + hash);
            while (el && el !== ul) {
                if (el.tagName === 'LI' && el.dataset.chain) {
                    el.removeClass('collapse');
                    collapse_nodes.delete(el.dataset.chain);
                }
                el = el.parentNode;
            }
        }
    }

    function createNodeLi(idx, text, chain_str)
    {
        if (chain_str) {
            return (
                html('li',
                    {
                         id: 'n' + idx,
                         class: ['parent', collapse_nodes.has(chain_str) ? 'collapse' : null],
                         'data-chain': chain_str
                    },
                    html('span',
                        html('span', {class: 'n', click: handleNodeClick}, '' + idx + ' '),
                        mco_list.addSpecLinks(text)
                    )
                )
            );
        } else {
            return (
                html('li',
                    {id: 'n' + idx, class: 'leaf'},
                    html('span',
                        html('span', {class: 'n'}, '' + idx + ' '),
                        mco_list.addSpecLinks(text)
                    )
                )
            );
        }
    }

    function handleNodeClick(e)
    {
        let li = this.parentNode.parentNode;
        if (li.toggleClass('collapse')) {
            collapse_nodes.add(li.dataset.chain);
        } else {
            collapse_nodes.delete(li.dataset.chain);
        }

        e.preventDefault();
    };

    thop.registerUrl('mco_tree', this, runTree);
}).call(mco_tree);
