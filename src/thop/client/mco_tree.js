// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let mco_tree = {};
(function() {
    'use strict';

    // Cache
    let tree_url = null;
    let nodes = [];
    let reactor = new Reactor;
    let collapse_nodes = new Set();

    function runTree(route, url, parameters, hash)
    {
        let errors = new Set(data.getErrors());

        // Parse route (model: tree/<date>)
        let url_parts = url.split('/');
        route.date = url_parts[1] || route.date;

        // Resources
        let indexes = mco_common.updateIndexes();
        if (!route.date && indexes.length)
            route.date = indexes[indexes.length - 1].begin_date;
        let main_index = indexes.findIndex(function(info) { return info.begin_date === route.date; });
        if (main_index >= 0)
            updateTree(indexes[main_index].begin_date);

        // Errors
        if (route.date !== null && indexes.length && main_index < 0)
            errors.add('Date incorrecte');

        // Refresh settings
        queryAll('#opt_indexes').removeClass('hide');
        mco_common.refreshIndexes(indexes, main_index);

        // Refresh view
        thop.refreshErrors(Array.from(errors));
        if (!data.isBusy()) {
            data.clearErrors();

            if (reactor.changed('tree', main_index, nodes.length))
                refreshTree(nodes);
            deploySelectedNode(hash);

            query('#tr').removeClass('hide');
        }
        thop.markBusy('#tr', data.isBusy());
    }

    function routeToUrl(args)
    {
        let new_route = thop.buildRoute(args);

        let url_parts = [thop.baseUrl('mco_tree'), new_route.date];
        while (!url_parts[url_parts.length - 1])
            url_parts.pop();
        let url = url_parts.join('/');

        return url;
    }
    this.routeToUrl = routeToUrl;

    function go(args, delay)
    {
        thop.route(routeToUrl(args), delay);
    }
    this.go = go;

    function updateTree(date)
    {
        let url = buildUrl(thop.baseUrl('api/mco_tree.json'), {date: date});
        if (url === tree_url)
            return;

        nodes = [];
        data.get(url, function(json) {
            nodes = json;
            tree_url = url;
        });
    }

    function refreshTree(nodes)
    {
        let ul;
        if (nodes.length) {
            ul = recurseNodes(0, '', []);
        } else {
            ul = html('ul');
        }

        let old_ul = query('#tr_tree');
        ul.copyAttributesFrom(old_ul);
        old_ul.replaceWith(ul);
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

                    li.appendChildren(children);
                    ul.appendChild(li);
                }
            } else if (node.children_count === 2 && node.reverse) {
                let recurse_str = chain_str + node.key;

                let children = recurseNodes(node.children_idx, recurse_str, indices);
                let li = createNodeLi(node_idx, node.reverse,
                                      children.tagName === 'UL' ? recurse_str  : null);

                li.appendChildren(children);
                ul.appendChild(li);
            } else if (node.children_count === 2) {
                let recurse_str = chain_str + node.key;

                let children = recurseNodes(node.children_idx + 1, recurse_str, indices);
                let li = createNodeLi(node_idx, node.text,
                                      children.tagName === 'UL' ? recurse_str : null);

                // Simplify OR GOTO chains
                while (indices.length && nodes[indices[0]].children_count === 2 &&
                       nodes[nodes[indices[0]].children_idx + 1].test === 20 &&
                       nodes[nodes[indices[0]].children_idx + 1].children_idx === node.children_idx + 1) {
                    li.appendChild(html('br'));

                    let li2 = createNodeLi(indices[0], nodes[indices[0]].text, recurse_str);
                    li2.children[0].id = li2.id;
                    li.appendChildren(li2.childNodes);

                    indices.shift();
                }

                li.appendChildren(children);
                ul.appendChild(li);
            } else {
                let recurse_str = chain_str + node.key;

                let li = createNodeLi(node_idx, node.text,
                                      (node.children_count && node.children_count > 1) ? recurse_str : null);
                ul.appendChild(li);

                for (let j = 1; j < node.children_count; j++) {
                    let children = recurseNodes(node.children_idx + j, recurse_str, indices);
                    li.appendChildren(children);
                }

                // Hide repeated subtrees, this happens with error-generating nodes 80 and 222
                if (node.test !== 20 && parent_next_indices.includes(node.children_idx)) {
                    if (node.children_idx != parent_next_indices[0]) {
                        let pseudo_idx = '' + node_idx + '-2';
                        let pseudo_text = 'Saut noeud ' + node.children_idx;
                        let goto_li = createNodeLi(pseudo_idx, pseudo_text, null);
                        ul.appendChild(goto_li);
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
        let content = mco_list.addSpecLinks(text);

        let li;
        if (chain_str) {
            li = html('li', {id: 'n' + idx, class: 'parent'},
                html('span',
                    html('span', {class: 'n', click: handleNodeClick}, '' + idx + ' '),
                    content
                )
            );
            if (collapse_nodes.has(chain_str))
                li.addClass('collapse');
            li.dataset.chain = chain_str;
        } else {
            li = html('li', {id: 'n' + idx, class: 'leaf'},
                html('span',
                    html('span', {class: 'n'}, '' + idx + ' '),
                    content
                )
            );
        }

        return li;
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
