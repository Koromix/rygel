// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var mco_tree = {};
(function() {
    'use strict';

    // Cache
    var tree_url = null;
    var nodes = [];
    var reactor = new Reactor;
    var collapse_nodes = new Set();

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
        drdw.refreshErrors(Array.from(errors));
        if (!data.isBusy()) {
            data.clearErrors();

            if (reactor.changed('tree', main_index, nodes.length))
                refreshTree(nodes);
            deploySelectedNode(hash);

            query('#tr').removeClass('hide');
        }
        drdw.markBusy('#tr', data.isBusy());
    }

    function routeToUrl(args)
    {
        let new_route = drdw.buildRoute(args);

        let url_parts = [drdw.baseUrl('mco_tree'), new_route.date];
        while (!url_parts[url_parts.length - 1])
            url_parts.pop();
        let url = url_parts.join('/');

        return url;
    }
    this.routeToUrl = routeToUrl;

    function go(args, delay)
    {
        drdw.route(routeToUrl(args), delay);
    }
    this.go = go;

    function updateTree(date)
    {
        let url = buildUrl(drdw.baseUrl('api/mco_tree.json'), {date: date});
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
        if (nodes.length) {
            var ul = recurseNodes(0, '', []);
        } else {
            var ul = html('ul');
        }

        var old_ul = query('#tr_tree');
        ul.copyAttributesFrom(old_ul);
        old_ul.replaceWith(ul);
    }

    function recurseNodes(start_idx, chain_str, parent_next_indices)
    {
        var ul = html('ul');

        var indices = [];
        for (var node_idx = start_idx;;) {
            indices.push(node_idx);

            var node = nodes[node_idx];
            if (nodes[node_idx].test === 20)
                break;

            node_idx = node.children_idx;
            if (node_idx === undefined)
                break;
            node_idx += !!node.reverse;
        }

        while (indices.length) {
            var node_idx = indices[0];
            var node = nodes[node_idx];
            indices.shift();

            // Hide GOTO nodes at the end of a chain, the classifier uses those to
            // jump back to go back one level.
            if (node.test === 20 && node.children_idx === parent_next_indices[0])
                break;

            if (node.children_count > 2 && nodes[node.children_idx + 1].header) {
                // Here we deal with jump lists (mainly D-xx and D-xxxx)
                for (var j = 1; j < node.children_count; j++) {
                    let recurse_str = chain_str + node.key + ('0' + j).slice(-2);

                    var pseudo_idx = (j > 1) ? ('' + node_idx + '-' + j) : node_idx;
                    var pseudo_text = node.text + ' ' + nodes[node.children_idx + j].header;
                    var children = recurseNodes(node.children_idx + j, recurse_str, indices);
                    var li = createNodeLi(pseudo_idx, pseudo_text,
                                          children.tagName === 'UL' ? recurse_str : null);

                    li.appendChildren(children);
                    ul.appendChild(li);
                }
            } else if (node.children_count === 2 && node.reverse) {
                let recurse_str = chain_str + node.key;

                var children = recurseNodes(node.children_idx, recurse_str, indices);
                var li = createNodeLi(node_idx, node.reverse,
                                      children.tagName === 'UL' ? recurse_str  : null);

                li.appendChildren(children);
                ul.appendChild(li);
            } else if (node.children_count === 2) {
                let recurse_str = chain_str + node.key;

                var children = recurseNodes(node.children_idx + 1, recurse_str, indices);
                var li = createNodeLi(node_idx, node.text,
                                      children.tagName === 'UL' ? recurse_str : null);

                // Simplify OR GOTO chains
                while (indices.length && nodes[indices[0]].children_count === 2 &&
                       nodes[nodes[indices[0]].children_idx + 1].test === 20 &&
                       nodes[nodes[indices[0]].children_idx + 1].children_idx === node.children_idx + 1) {
                    li.appendChild(html('br'));

                    var li2 = createNodeLi(indices[0], nodes[indices[0]].text, recurse_str);
                    li2.children[0].id = li2.id;
                    li.appendChildren(li2.childNodes);

                    indices.shift();
                }

                li.appendChildren(children);
                ul.appendChild(li);
            } else {
                let recurse_str = chain_str + node.key;

                var li = createNodeLi(node_idx, node.text,
                                      (node.children_count && node.children_count > 1) ? recurse_str : null);
                ul.appendChild(li);

                for (var j = 1; j < node.children_count; j++) {
                    var children = recurseNodes(node.children_idx + j, recurse_str, indices);
                    li.appendChildren(children);
                }

                // Hide repeated subtrees, this happens with error-generating nodes 80 and 222
                if (node.test !== 20 && parent_next_indices.includes(node.children_idx)) {
                    if (node.children_idx != parent_next_indices[0]) {
                        let pseudo_idx = '' + node_idx + '-2';
                        let pseudo_text = 'Saut noeud ' + node.children_idx;
                        var goto_li = createNodeLi(pseudo_idx, pseudo_text, null);
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
            var el = ul.query('#' + hash);
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
        var content = mco_list.addSpecLinks(text);

        if (chain_str) {
            var li = html('li', {id: 'n' + idx, class: 'parent'},
                html('span',
                    html('span', {class: 'n', click: handleNodeClick}, '' + idx + ' '),
                    content
                )
            );
            if (collapse_nodes.has(chain_str))
                li.addClass('collapse');
            li.dataset.chain = chain_str;
        } else {
            var li = html('li', {id: 'n' + idx, class: 'leaf'},
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
        var li = this.parentNode.parentNode;
        if (li.toggleClass('collapse')) {
            collapse_nodes.add(li.dataset.chain);
        } else {
            collapse_nodes.delete(li.dataset.chain);
        }
        e.preventDefault();
    };

    drdw.registerUrl('mco_tree', this, runTree);
}).call(mco_tree);
