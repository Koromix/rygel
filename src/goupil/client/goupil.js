// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let goupil = (function() {
    let self = this;

    let event_src;

    function parseURL(href, base) {
        return new URL(href, base);
    }

    function initNavigation() {
        window.addEventListener('popstate', e => {
            self.go(window.location.href, false);
        });

        document.body.addEventListener('click', e => {
            if (e.target && e.target.tagName == 'A' && !e.target.getAttribute('download')) {
                let href = e.target.getAttribute('href');
                if (href && !href.match(/^(?:[a-z]+:)?\/\//) && href[0] != '#') {
                    self.go(href);
                    e.preventDefault();
                }
            }
        });
    }

    this.go = function(href, history = true) {
        // Module path
        let path = parseURL(href, window.location.href).pathname;
        if (path.startsWith(settings.base_url))
            path = path.substr(settings.base_url.length);
        if (path.endsWith('/'))
            path = path.substr(0, path.length - 1);

        // Run module
        switch (path) {
            case '': { path = 'autoform' } // fallthrough
            case 'autoform': { autoform.activate(); } break;
            case 'schedule': { schedule.activate(); } break;
        }

        // Full path
        let full_path = `${settings.base_url}${path}/`;

        // Update menu state
        for (let el of document.querySelectorAll('#gp_menu > a'))
            el.classList.toggle('active', el.getAttribute('href') === full_path);

        // Update history
        if (history && full_path !== parseURL(window.location.href).pathname) {
            window.history.pushState(null, null, full_path);
        } else {
            window.history.replaceState(null, null, full_path);
        }
    };

    this.listenToServerEvent = function(event, func) {
        if (!event_src) {
            event_src = new EventSource(`${settings.base_url}goupil/events.json`);
            event_src.onerror = e => event_src = null;
        }

        event_src.addEventListener(event, func);
    };

    // TODO: React to onerror?
    this.loadScript = function(url) {
        let head = document.querySelector('script');
        let script = document.createElement('script');

        script.type = 'text/javascript';
        script.src = url;
        script.onreadystatechange = () => self.go(window.location.href);
        script.onload = () => self.go(window.location.href);

        head.appendChild(script);
    };

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete') {
            initNavigation();
            self.go(window.location.href, false);
        }
    });

    return this;
}).call({});
