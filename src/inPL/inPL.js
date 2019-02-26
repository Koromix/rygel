// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let inPL = (function() {
    let self = this;

    let route_params = {
        tab: 0
    };
    let route_url = null;
    let refresh_tabs = 0xFFFFFFFF;

    let rows = [];

    function route(new_params, mark_history = true) {
        Object.assign(route_params, new_params);

        if (new_params.tab !== undefined) {
            route_params.tab = parseInt(new_params.tab);
            self.openTab(route_params.tab);
        }
        if (new_params.plid !== undefined) {
            route_params.plid = new_params.plid;
            query('#inpl_option_plid').value = route_params.plid;
            refresh_tabs = 0xFFFFFFFF;
        }
        if (new_params.sex !== undefined) {
            route_params.sex = new_params.sex;
            query('#inpl_option_sex').value = route_params.sex;
            refresh_tabs = 0xFFFFFFFF;
        }
        if (new_params.rows !== undefined) {
            route_params.rows = new_params.rows;
            query('#inpl_option_rows').value = route_params.rows;
            refresh_tabs = 0xFFFFFFFF;
        }

        let new_url = buildUrl('', route_params);
        if (mark_history && new_url !== route_url) {
            window.history.pushState(null, null, new_url);
        } else {
            window.history.replaceState(null, null, new_url);
        }
        route_url = new_url;
    };

    function refreshMainView()
    {
        if (refresh_tabs & (1 << route_params.tab)) {
            refresh_tabs &= ~(1 << route_params.tab);

            switch (route_params.tab) {
                case 0: { tables.refreshList(rows); } break;
                case 1: { tables.refreshSummary(rows); } break;
                case 2: { report.refreshReport(rows); } break;
            }
        }

        document.body.style.display = 'block';
    }

    this.url = function(new_params) {
        new_params = Object.assign({}, route_params, new_params);
        return buildUrl('', new_params);
    };
    this.go = function(new_params) {
        route(new_params);
        refreshMainView();
    }

    this.openTab = function(idx) {
        let tabs = query('#inpl_menu').querySelectorAll('button');
        let pages = document.body.querySelectorAll('.inpl_page');

        tabs.removeClass('active');
        tabs[idx].addClass('active');
        pages.removeClass('active');
        pages[idx].addClass('active');
    }

    this.importAndRefresh = function() {
        let file = document.querySelector('#inpl_menu_file').files[0];
        let encoding = document.querySelector('#inpl_menu_encoding').value;

        let stat = document.querySelector('#inpl_menu_stat');
        refresh_tabs = 0xFFFFFFFF;

        rows.length = 0;
        if (file) {
            bridge.readFileAsync(file, {
                encoding: encoding,
                step: row => {
                    rows.push(row);
                },
                complete: () => {
                    rows.sort((row1, row2) => row1.rdv_plid - row2.rdv_plid);

                    stat.replaceContent(`(${file.name} contient ${rows.length} rendez-vous)`);
                    refreshMainView();
                }
            });
        } else {
            stat.innerHTML = '';
            refreshMainView();
        }
    };

    this.getRows = function() { return rows; }

    function initNavigation()
    {
        window.addEventListener('popstate', function(e) {
            route(parseUrl(window.location.href).params, false);
            refreshMainView();
        });

        document.body.addEventListener('click', function(e) {
            if (e.target && e.target.tagName == 'A' && !e.target.getAttribute('download')) {
                let href = e.target.getAttribute('href');
                if (href && !href.match(/^(?:[a-z]+:)?\/\//) && href[0] != '#') {
                    self.go(parseUrl(href).params);
                    e.preventDefault();
                }
            }
        });
    }

    window.addEventListener('load', function() {
        initNavigation();

        let params = Object.assign({}, route_params, parseUrl(window.location.href).params);
        route(params, false);

        self.importAndRefresh();
    });

    return this;
}).call({});
