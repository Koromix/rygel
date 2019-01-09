// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let thop = {};
(function() {
    let route_modules = {};

    let route_timer_id = null;

    let route_url = null;
    let route_url_parts = null;
    let route_values = {};
    let scroll_cache = {};
    let module = null;
    let prev_module = null;

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

    function registerUrl(prefix, object, func)
    {
        route_modules[prefix] = {
            object: object,
            func: func
        };
    }
    this.registerUrl = registerUrl;

    function route(new_url, delay, mark_history)
    {
        if (new_url === undefined)
            new_url = null;
        if (mark_history === undefined)
            mark_history = true;

        if (route_timer_id) {
            clearTimeout(route_timer_id);
            route_timer_id = null;
        }
        if (delay) {
            route_timer_id = setTimeout(function() {
                route(new_url, 0, mark_history);
            }, delay);
            return;
        }

        // Busy
        query('main').toggleClass('busy', true);

        // Parse new URL
        let url_parts = new_url ? parseUrl(new_url) : route_url_parts;
        let app_url = url_parts.path.substr(BaseUrl.length);

        // Update scroll cache and history
        if (!route_url_parts || url_parts.href !== route_url_parts.href) {
            if (route_url_parts)
                scroll_cache[route_url_parts.path] = [window.pageXOffset, window.pageYOffset];
            if (mark_history)
                window.history.pushState(null, null, url_parts.href);
        }

        // Update user stuff
        user.runSession();

        // Find relevant module and run
        let errors = new Set(data.getErrors());
        {
            let new_module_name = app_url.split('/')[0];
            let new_module = route_modules[new_module_name];

            if (new_module !== module) {
                queryAll('main > div').addClass('hide');
                prev_module = module;
            }
            queryAll('#opt_menu > *').addClass('hide');
            document.body.removeClass('hide');

            module = new_module;
            if (module)
                module.func(route_values, app_url, url_parts.params, url_parts.hash, errors);
        }

        // Show errors if any
        refreshErrors(Array.from(errors));
        if (!data_busy)
            data.clearErrors();

        // Update URL to reflect real state (module may have set default values, etc.)
        {
            let real_url = null;
            if (module) {
                real_url = module.object.routeToUrl({}).url;

                if (url_parts.hash)
                    real_url += '#' + url_parts.hash;

                window.history.replaceState(null, null, real_url);
                route_url_parts = parseUrl(real_url);
                route_url = real_url.substr(BaseUrl.length);
            } else {
                route_url_parts = url_parts;
                route_url = app_url;
            }
        }

        // Update side menu state and links
        queryAll('#side_menu li a').forEach(function(anchor) {
            if (anchor.dataset.path) {
                let path = eval(anchor.dataset.path);
                anchor.classList.toggle('hide', !path.allowed);
                anchor.href = path.url;
            }

            let active = (route_url_parts.href.startsWith(anchor.href) &&
                          !anchor.hasClass('category'));
            anchor.toggleClass('active', active);
        });
        toggleMenu('#side_menu', false);

        // Hide page menu if empty
        let opt_hide = true;
        queryAll('#opt_menu > *').forEach(function(el) {
            if (!el.hasClass('hide'))
                opt_hide = false;
        });
        queryAll('#opt_deploy, #opt_menu').toggleClass('hide', opt_hide);

        // Update scroll target
        let scroll_target = scroll_cache[route_url_parts.path];
        if (route_url_parts.hash) {
            let el = query('#' + route_url_parts.hash);
            if (el && el.offsetTop)
                window.scrollTo(0, el.offsetTop - 5);
        } else if (scroll_target) {
            window.scrollTo(scroll_target[0], scroll_target[1]);
        } else {
            window.scrollTo(0, 0);
        }

        // Done
        query('main').toggleClass('busy', isBusy());
    }
    this.route = route;

    function routeToUrl(args)
    {
        return module.object.routeToUrl(args);
    }
    this.routeToUrl = routeToUrl;

    function go(args, delay)
    {
        module.object.go(args, delay);
    }
    this.go = go;

    function goHome()
    {
        let first_anchor = query('#side_menu a[data-path]');
        route(first_anchor.href);
    }
    this.goHome = goHome;

    function goBackOrHome()
    {
        if (prev_module) {
            prev_module.object.go({});
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

            let url = buildUrl(thop.baseUrl('api/mco_settings.json'), {key: user.getUrlKey()});
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

        log.innerHTML = errors.join('<br/>');
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
        route(new_url, 0, false);

        window.addEventListener('popstate', function(e) {
            route(window.location.href, 0, false);
        });
        document.body.addEventListener('click', function(e) {
            if (e.target && e.target.tagName == 'A' && !e.target.getAttribute('download')) {
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
                route();
            }
        }
    }

    document.addEventListener('readystatechange', function() {
        if (document.readyState === 'complete')
            init();
    });
}).call(thop);
