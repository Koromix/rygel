// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let inPL = (function() {
    let self = this;

    let route_params = {};
    let route_url = null;

    let rows = [];

    function route(new_params, mark_history = true) {
        if (new_params.plid !== undefined) {
            route_params.plid = new_params.plid;
            query('#inpl_plid').value = route_params.plid;
        }

        let new_url = util.buildUrl('', route_params);
        if (mark_history && new_url !== route_url) {
            window.history.pushState(null, null, new_url);
        } else {
            window.history.replaceState(null, null, new_url);
        }
        route_url = new_url;
    };

    function refreshMainView()
    {
        report.refreshReport(rows);
        document.body.style.display = 'block';
    }

    this.url = function(new_params) {
        new_params = Object.assign({}, route_params, new_params);
        return util.buildUrl('', new_params);
    };
    this.go = function(new_params) {
        route(new_params);
        refreshMainView();
    }

    this.importAndRefresh = function() {
        let file = query('#inpl_file').files[0];
        let encoding = query('#inpl_encoding').value;

        let info = query('#inpl_file_info');
        let plid = query('#inpl_plid');

        rows.length = 0;
        if (file) {
            bridge.readFileAsync(file, {
                encoding: encoding,
                step: row => {
                    rows.push(row);
                },
                complete: () => {
                    rows.sort((row1, row2) => row1.rdv_plid - row2.rdv_plid);

                    info.replaceContent(`${file.name} (${rows.length} lignes).`);
                    plid.innerHTML = '';
                    for (let row of rows) {
                        let option = dom.h('option', {value: row.rdv_plid},
                                           `${row.rdv_plid} - ${row.consultant_nom} ${row.consultant_prenom} (${row.consultant_sexe})`);
                        plid.appendContent(option);
                    }

                    refreshMainView();
                }
            });
        } else {
            info.innerHTML = 'Pas de données.';
            plid.replaceContent(dom.h('option', 'Aucune donnée disponible'));

            refreshMainView();
        }
    };

    this.getRows = function() { return rows; }

    function initNavigation()
    {
        window.addEventListener('popstate', function(e) {
            route(util.parseUrl(window.location.href).params, false);
            refreshMainView();
        });

        document.body.addEventListener('click', function(e) {
            if (e.target && e.target.tagName == 'A' && !e.target.getAttribute('download')) {
                let href = e.target.getAttribute('href');
                if (href && !href.match(/^(?:[a-z]+:)?\/\//) && href[0] != '#') {
                    self.go(util.parseUrl(href).params);
                    e.preventDefault();
                }
            }
        });
    }

    window.addEventListener('load', function() {
        initNavigation();

        let params = Object.assign({}, route_params, util.parseUrl(window.location.href).params);
        route(params, false);

        self.importAndRefresh();
    });

    return this;
}).call({});
