// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let thop = {};
(function() {
    'use strict';

    let route_modules = {};

    let route_timer_id = null;
    let history_count = 0;

    let route_url = null;
    let route_url_parts = null;
    let route_values = {
        'view': 'table',
        'date': null,
        'ghm_root': null,
        'diff': null,
        'apply_coefficient': false,

        'list': 'classifier_tree',
        'page': 1,
        'spec': null,

        'cm_view': 'summary',
        'period': [null, null],
        'prev_period': [null, null],
        'mode': 'none',
        'units': [],
        'algorithm': null,

        'apply': false
    };
    let scroll_cache = {};
    let module = null;

    function markBusy(selector, busy)
    {
        queryAll(selector).toggleClass('busy', busy);
    }
    this.markBusy = markBusy;

    function toggleMenu(selector, enable)
    {
        let el = query(selector);
        if (enable === undefined)
            enable = !el.classList.contains('active');
        if (enable) {
            for (let nav of queryAll('nav'))
                nav.toggleClass('active', nav === el);
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

        // Parse new URL
        let url_parts = new_url ? parseUrl(new_url) : route_url_parts;
        let app_url = url_parts.path.substr(BaseUrl.length);

        // Update scroll cache and history
        if (!route_url_parts || url_parts.href !== route_url_parts.href) {
            if (route_url_parts)
                scroll_cache[route_url_parts.path] = [window.pageXOffset, window.pageYOffset];
            if (mark_history) {
                history_count++;
                window.history.pushState(null, null, url_parts.href);
            }
        }

        // Update user stuff
        user.runSession();

        // Find relevant module and run
        {
            let new_module_name = app_url.split('/')[0];
            let new_module = route_modules[new_module_name];

            if (new_module !== module)
                queryAll('main > div').addClass('hide');
            queryAll('#opt_menu > *').addClass('hide');

            module = new_module;
            if (module)
                module.func(route_values, app_url, url_parts.params, url_parts.hash);
        }

        // Update URL to reflect real state (module may have set default values, etc.)
        {
            let real_url = null;
            if (module)
                real_url = module.object.routeToUrl({});
            if (real_url) {
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
        for (let anchor of queryAll('#side_menu li a')) {
            if (anchor.dataset.url) {
                let url = eval(anchor.dataset.url);
                anchor.classList.toggle('hide', !url);
                if (url)
                    anchor.href = url;
            }

            let active = (route_url_parts.href.startsWith(anchor.href) &&
                          !anchor.hasClass('category'));
            anchor.toggleClass('active', active);
        }
        toggleMenu('#side_menu', false);

        // Hide page menu if empty
        let opt_hide = true;
        for (let el of queryAll('#opt_menu > *')) {
            if (!el.hasClass('hide')) {
                opt_hide = false;
                break;
            }
        }
        queryAll('#opt_deploy, #opt_menu').toggleClass('hide', opt_hide);

        // Update scroll target
        let scroll_target = scroll_cache[route_url_parts.path];
        if (route_url_parts.hash) {
            window.location.hash = route_url_parts.hash;
        } else if (scroll_target) {
            window.scrollTo(scroll_target[0], scroll_target[1]);
        } else {
            window.scrollTo(0, 0);
        }
    }
    this.route = route;

    function go(args, delay) {
        module.object.go(args, delay);
    }
    this.go = go;

    function goBackOrHome()
    {
        if (history_count) {
            window.history.back();
        } else {
            let first_anchor = query('#side_menu a[data-url]');
            route(first_anchor.href);
        }
    }
    this.goBackOrHome = goBackOrHome;

    function refreshErrors(errors)
    {
        let log = query('#log');

        log.innerHTML = errors.join('<br/>');
        log.toggleClass('hide', !errors.length);
    }
    this.refreshErrors = refreshErrors;

    function init()
    {
        let new_url;
        if (window.location.pathname !== BaseUrl) {
            new_url = window.location.href;
        } else {
            let first_anchor = query('#side_menu a[data-url]');
            new_url = eval(first_anchor.dataset.url);
        }
        route(new_url, 0, false);

        window.addEventListener('popstate', function(e) {
            history_count--;
            route(window.location.href, 0, false);
        });
        document.body.addEventListener('click', function(e) {
            if (e.target && e.target.tagName == 'A') {
                let href = e.target.getAttribute('href');
                if (href && !href.match(/^(?:[a-z]+:)?\/\//) && href[0] != '#') {
                    route(href);
                    e.preventDefault();
                }
            }
        });

        data.busyHandler = function(busy) {
            if (busy) {
                document.body.style.cursor = 'wait';
            } else {
                document.body.style.cursor = 'default';
                route();
            }
        }
    }

    if (document.readyState === 'complete') {
        init();
    } else {
        document.addEventListener('DOMContentLoaded', init);
    }
}).call(thop);
