// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let thop = {};
(function() {
    let modules = {};

    // Go
    let go_timer_id = null;
    let route_values = {};
    let current_module = null;
    let current_url = null;
    let prev_module = null;

    // Scroll
    let scroll_state = {};

    // Refresh
    let ignore_busy = false;
    let data_busy = false;
    let force_idx = 0;

    // Cache
    let mco_settings = {};

    function toggleMenu(selector, enable)
    {
        let el = query(selector);
        if (enable === undefined)
            enable = !el.classList.contains('active');
        if (enable) {
            queryAll('nav').forEach(function(nav) {
                nav.toggleClass('active', nav === el);
            });
        } else {
            el.removeClass('active');
        }
    }
    this.toggleMenu = toggleMenu;

    function baseUrl(module_name)
    {
        return BaseUrl + module_name;
    }
    this.baseUrl = baseUrl;

    function buildRoute(args)
    {
        if (args === undefined)
            args = {};

        return Object.assign({}, route_values, args);
    }
    this.buildRoute = buildRoute;

    function registerUrl(prefix, object)
    {
        modules[prefix] = object;
    }
    this.registerUrl = registerUrl;

    function run(module, args, hash, delay, mark)
    {
        if (mark === undefined)
            mark = true;

        // Respect delay (if any)
        if (go_timer_id) {
            clearTimeout(go_timer_id);
            go_timer_id = null;
        }
        if (delay) {
            go_timer_id = setTimeout(function() { go(args, 0); }, delay);
            return;
        }

        // Update session information
        user.runSession();

        // Prepare route and errors
        let errors = new Set(data.getErrors());
        Object.assign(route_values, args);

        // Hide previous UI (if needed)
        if (module !== current_module) {
            queryAll('main > div').addClass('hide');

            prev_module = current_module;
            current_module = module;
        }
        queryAll('#opt_menu > *').addClass('hide');
        document.body.removeClass('hide');

        // Run module
        let new_url;
        if (module) {
            module.runModule(route_values, errors);
            new_url = module.routeToUrl({}).url;
        } else {
            new_url = util.parseUrl(window.location.href).path;
        }
        if (hash)
            new_url += '#' + hash;

        // Update browser URL
        if (mark && new_url !== current_url) {
            window.history.pushState(null, null, new_url);
        } else {
            window.history.replaceState(null, null, new_url);
        }
        current_url = new_url;

        // Show errors if any
        refreshErrors(Array.from(errors));
        if (!data_busy)
            data.clearErrors();

        // Update side menu state and links
        queryAll('#side_menu li a').forEach(function(anchor) {
            if (anchor.dataset.path) {
                let path = eval(anchor.dataset.path);

                anchor.classList.toggle('hide', !path.allowed);
                anchor.href = path.url;

                let active = new_url && new_url.startsWith(path.url) && !anchor.hasClass('category');
                anchor.toggleClass('active', active);
            }
        });
        toggleMenu('#side_menu', false);

        // Hide page menu if empty
        let opt_hide = true;
        queryAll('#opt_menu > *').forEach(function(el) {
            if (!el.hasClass('hide'))
                opt_hide = false;
        });
        queryAll('#opt_deploy, #opt_menu').toggleClass('hide', opt_hide);

        // Done
        query('main').toggleClass('busy', isBusy());
    }

    function go(args, hash, delay, mark)
    {
        run(current_module, args, hash, delay, mark);
    }
    this.go = go;

    function route(href, delay, mark)
    {
        if (mark === undefined)
            mark = true;

        let url = util.parseUrl(href);
        if (url.path.startsWith(BaseUrl))
            url.path = url.path.substr(BaseUrl.length);

        let new_module_name = url.path.split('/')[0];
        let new_module = modules[new_module_name];

        // Save scroll state
        if (mark)
            scroll_state[current_url] = [window.pageXOffset, window.pageYOffset];

        if (new_module && new_module.parseRoute)
            new_module.parseRoute(route_values, url.path, url.params, url.hash);
        run(new_module, {}, url.hash, 0, mark);

        // Try to restore scroll state (for new pages)
        if (mark) {
            if (url.hash) {
                let el = query('#' + url.hash);
                if (el && el.offsetTop)
                    window.scrollTo(0, el.offsetTop - 5);
            } else {
                let target = scroll_state[current_url] || [0, 0];
                window.scrollTo(target[0], target[1]);
            }
        }
    }
    this.route = route;

    function routeToUrl(args)
    {
        return current_module.routeToUrl(args);
    }
    this.routeToUrl = routeToUrl;

    function goHome()
    {
        let first_anchor = query('#side_menu a[data-path]');
        route(first_anchor.href);
    }
    this.goHome = goHome;

    function goBackOrHome()
    {
        if (prev_module) {
            run(prev_module, {});
        } else {
            goHome();
        }
    }
    this.goBackOrHome = goBackOrHome;

    function needsRefresh(obj) {
        let args_json = JSON.stringify(Array.from(arguments).slice(1));

        if (force_idx !== obj.prev_force_idx || args_json !== obj.prev_args_json) {
            obj.prev_force_idx = force_idx;
            obj.prev_args_json = args_json;
            return true;
        } else {
            return false;
        }
    }
    this.needsRefresh = needsRefresh;

    function isBusy() { return data_busy; }
    this.isBusy = isBusy;

    function setIgnoreBusy(ignore) { ignore_busy = !!ignore; }
    this.setIgnoreBusy = setIgnoreBusy;

    function forceRefresh() { force_idx++; }
    this.forceRefresh = forceRefresh;

    function updateMcoSettings()
    {
        if (user.getUrlKey() !== mco_settings.url_key) {
            mco_settings = {
                indexes: mco_settings.indexes || [],
                url_key: null
            };

            let url = util.buildUrl(thop.baseUrl('api/mco_settings.json'), {key: user.getUrlKey()});
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
    }
    this.updateMcoSettings = updateMcoSettings;

    function refreshErrors(errors)
    {
        let log = query('#log');

        log.innerHTML = errors.map(function(err) { return err.replace('\n', '<br/>&nbsp;&nbsp;&nbsp;&nbsp;'); })
                              .join('<br/>');
        log.toggleClass('hide', !errors.length);
    }

    function init()
    {
        let new_url;
        if (window.location.pathname !== BaseUrl) {
            new_url = window.location.href;
        } else {
            let first_anchor = query('#side_menu a[data-path]');
            new_url = eval(first_anchor.dataset.path).url;
        }

        // Avoid history push
        route(new_url, 0, false);

        window.addEventListener('popstate', function(e) {
            route(window.location.href, 0, false);
        });
        document.body.addEventListener('click', function(e) {
            if (e.target && e.target.tagName == 'A' &&
                    !e.ctrlKey && !e.target.getAttribute('download')) {
                let href = e.target.getAttribute('href');
                if (href && !href.match(/^(?:[a-z]+:)?\/\//) && href[0] != '#') {
                    route(href);
                    e.preventDefault();
                }
            }
        });

        data.busyHandler = function(busy) {
            if (busy) {
                data_busy |= !ignore_busy;
            } else {
                data_busy = false;
                go({});
            }
        }
    }

    document.addEventListener('readystatechange', function() {
        if (document.readyState === 'complete')
            init();
    });
}).call(thop);
