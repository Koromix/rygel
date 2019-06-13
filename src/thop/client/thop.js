// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let thop = (function() {
    let self = this;

    // Modules and links
    let Modules = {
        'mco_casemix': mco_casemix,
        'mco_list': mco_list,
        'mco_pricing': mco_pricing,
        'mco_tree': mco_tree,
        'login': user
    };
    let Links = [
        {category: 'Tarifs MCO', title: 'Racines de GHM', func: () => mco_list.routeToUrl({list: 'ghm_roots'})},
        {category: 'Tarifs MCO', title: 'Tarifs (grille)', func: () => mco_pricing.routeToUrl({view: 'table'})},
        {category: 'Tarifs MCO', title: 'Tarifs (courbes)', func: () => mco_pricing.routeToUrl({view: 'chart'})},

        {category: 'Listes MCO', title: 'Arbre de groupage', func: () => mco_tree.routeToUrl()},
        {category: 'Listes MCO', title: 'GHM / GHS', func: () => mco_list.routeToUrl({list: 'ghm_ghs'})},
        {category: 'Listes MCO', title: 'Diagnostics', func: () => mco_list.routeToUrl({list: 'diagnoses'})},
        {category: 'Listes MCO', title: 'Actes', func: () => mco_list.routeToUrl({list: 'procedures'})},

        {category: 'Activité MCO', title: 'Unités médicales', func: () => mco_casemix.routeToUrl({view: 'units'})},
        {category: 'Activité MCO', title: 'Racines de GHM', func: () => mco_casemix.routeToUrl({view: 'ghm_roots'})},
        {category: 'Activité MCO', title: 'Valorisations', func: () => mco_casemix.routeToUrl({view: 'durations'})},
        {category: 'Activité MCO', title: 'Résumés (RSS)', func: () => mco_casemix.routeToUrl({view: 'results'})}
    ];

    // Go
    let go_timer_id = null;
    let route_values = {};
    let current_module = null;
    let current_url = null;
    let prev_module = null;

    // Errors
    let errors = new Set;

    // Scroll
    let scroll_state = {};

    // Refresh
    let ignore_busy = false;
    let data_busy = false;
    let force_idx = 0;

    // Cache
    let mco_settings = {};

    this.toggleMenu = function(selector, enable) {
        let el = query(selector);
        if (enable === undefined)
            enable = !el.classList.contains('active');
        if (enable) {
            queryAll('nav').forEach(nav => nav.toggleClass('active', nav === el));
        } else {
            el.removeClass('active');
        }
    };

    this.baseUrl = function(module_name) {
        return `${BaseUrl}${module_name}`;
    };

    this.buildRoute = function(args = {}) {
        return Object.assign({}, route_values, args);
    };

    function updateMenu() {
        let session_els = HasUsers ? queryAll('.session') : [];
        let menu_el = query('#side_menu');

        for (let el of session_els) {
            render(html`
                ${!user.isConnected() ?
                    html`<a href="${user.routeToUrl().url}">Se connecter</a>` : html``}
                ${user.isConnected() ?
                    html`${user.getUsername()} (<a href="${user.routeToUrl().url}">changer</a>, <a href="#" @click=${e => { user.logout(); e.preventDefault(); }}>déconnexion</a>)` : html``}
            `, el);
        }

        let prev_category = null;
        render(html`
            ${Links.map(link => {
                let path = link.func();

                let active = current_url && current_url.startsWith(path.url);
                if (active)
                    document.title = `${link.category} — ${link.title}`;

                if (path.allowed) {
                    if (link.category === prev_category) {
                        return html`<a class=${active ? 'active': ''} href=${path.url}>${link.title}</a>`;
                    } else {
                        prev_category = link.category;
                        return html`<a class="category">${link.category}</li>
                                    <a class=${active ? 'active': ''} href=${path.url}>${link.title}</a>`;
                    }
                } else {
                    return html``;
                }
            })}
        `, menu_el);
    }

    function run(module, args, hash, delay = 0, mark = true) {
        // Respect delay (if any)
        if (go_timer_id) {
            clearTimeout(go_timer_id);
            go_timer_id = null;
        }
        if (delay) {
            go_timer_id = setTimeout(() => run(module, args, hash, 0, mark), delay);
            return;
        }

        // Update route and session
        Object.assign(route_values, args);
        if (HasUsers)
            user.updateSession();

        // Hide previous options
        queryAll('#opt_menu > *').addClass('hide');

        // Run module
        let new_url;
        if (module) {
            if (module !== current_module) {
                prev_module = current_module;
                current_module = module;
            }

            module.runModule(route_values);
            new_url = module.routeToUrl({}).url;
        } else {
            new_url = util.parseUrl(window.location.href).path;
        }
        if (hash)
            new_url += `#${hash}`;

        // Update browser URL
        if (mark && new_url !== current_url) {
            window.history.pushState(null, null, new_url);
        } else {
            window.history.replaceState(null, null, new_url);
        }
        current_url = new_url;

        if (!data_busy) {
            // Show errors if any
            refreshErrors(Array.from(errors));
            if (!data_busy)
                errors.clear();

            // Update side menu state and links
            updateMenu();
            self.toggleMenu('#side_menu', false);

            // Hide page menu if empty
            let opt_hide = true;
            queryAll('#opt_menu > *').forEach(el => {
                if (!el.hasClass('hide'))
                    opt_hide = false;
            });
            queryAll('#opt_deploy, #opt_menu').toggleClass('hide', opt_hide);
            if (opt_hide)
                self.toggleMenu('#opt_menu', false);

            // Show page (first load)
            document.body.style.display = 'block';
        }

        query('main').toggleClass('busy', data_busy);
    }

    this.go = function(args, hash, delay, mark) {
        run(current_module, args, hash, delay, mark);
    };

    this.route = function(href, delay = 0, mark = true) {
        let url = util.parseUrl(href);
        if (url.path.startsWith(BaseUrl))
            url.path = url.path.substr(BaseUrl.length);

        let new_module_name = url.path.split('/')[0];
        let new_module = Modules[new_module_name];

        // Save scroll state
        if (mark)
            scroll_state[current_url] = [window.pageXOffset, window.pageYOffset];

        if (new_module && new_module.parseRoute)
            new_module.parseRoute(route_values, url.path, url.params, url.hash);
        run(new_module, {}, url.hash, 0, mark);

        // Try to restore scroll state (for new pages)
        if (mark) {
            if (url.hash) {
                let el = query(`#${url.hash}`);
                if (el && el.offsetTop)
                    window.scrollTo(0, el.offsetTop - 5);
            } else {
                let target = scroll_state[current_url] || [0, 0];
                window.scrollTo(target[0], target[1]);
            }
        }
    };

    this.routeToUrl = function(args) {
        return current_module.routeToUrl(args);
    };

    this.goHome = function() {
        let home_url = Links[0].func().url;
        self.route(home_url);
    };

    this.goBackOrHome = function() {
        if (prev_module) {
            run(prev_module, {});
        } else {
            self.goHome();
        }
    };

    this.needsRefresh = function(obj) {
        let args_json = JSON.stringify(Array.from(arguments).slice(1));

        if (force_idx !== obj.prev_force_idx || args_json !== obj.prev_args_json) {
            obj.prev_force_idx = force_idx;
            obj.prev_args_json = args_json;
            return true;
        } else {
            return false;
        }
    };

    this.error = function(err) { errors.add(err); };

    this.isBusy = function() { return data_busy; };
    this.setIgnoreBusy = function(ignore) { ignore_busy = !!ignore; };
    this.forceRefresh = function() { force_idx++; };

    this.updateMcoSettings = function() {
        if (user.getUrlKey() !== mco_settings.url_key) {
            mco_settings = {
                indexes: mco_settings.indexes || [],
                url_key: null
            };

            let url = util.buildUrl(self.baseUrl('api/mco_settings.json'), {key: user.getUrlKey()});
            data.get(url, 'json', function(json) {
                mco_settings = json;

                if (mco_settings.start_date) {
                    mco_settings.permissions = new Set(mco_settings.permissions);
                    for (let structure of mco_settings.structures) {
                        structure.units = {};
                        for (let ent of structure.entities) {
                            ent.path = ent.path.substr(1).split('|');
                            structure.units[ent.unit] = ent;
                        }
                    }
                }

                mco_settings.url_key = user.getUrlKey();
            });
        }

        return mco_settings;
    };

    function refreshErrors(errors) {
        let log = document.querySelector('#log');

        log.innerHTML = errors.map(err => err.replace('\n', '<br/>&nbsp;&nbsp;&nbsp;&nbsp;')).join('<br/>');
        log.toggleClass('hide', !errors.length);
    }

    function initUser() {
        user.addChangeHandler(() => {
            let view_el = query('#view');
            render(html``, view_el);
        });
    }

    function initData() {
        data.busyHandler = busy => {
            if (busy) {
                data_busy |= !ignore_busy;
            } else {
                data_busy = false;
                self.go({}, null, 0, false);
            }
        };
        data.errorHandler = self.error;
    }

    function initNavigation() {
        window.addEventListener('popstate', e => {
            self.route(window.location.href, 0, false);
        });

        document.body.addEventListener('click', e => {
            if (e.target && e.target.tagName == 'A' &&
                    !e.ctrlKey && !e.target.getAttribute('download')) {
                let href = e.target.getAttribute('href');
                if (href && !href.match(/^(?:[a-z]+:)?\/\//) && href[0] != '#') {
                    self.route(href);
                    e.preventDefault();
                }
            }
        });
    }

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete') {
            initUser();
            initData();
            initNavigation();

            let new_url;
            if (window.location.pathname !== BaseUrl) {
                new_url = window.location.href;
            } else {
                new_url = Links[0].func().url;
            }

            // Avoid history push
            self.route(new_url, 0, false);
        }
    });

    return this;
}).call({});
