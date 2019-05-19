// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let goupil = (function() {
    let self = this;

    let event_src;

    let gp_popup;
    let popup_timer;

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

    function initPopup() {
        gp_popup = document.createElement('div');
        gp_popup.setAttribute('id', 'gp_popup');
        document.body.appendChild(gp_popup);

        gp_popup.addEventListener('click', e => e.stopPropagation());
        gp_popup.addEventListener('mousemove', e => {
            clearTimeout(popup_timer);
            popup_timer = null;

            e.stopPropagation();
        });
        document.addEventListener('click', closePopup);
        document.addEventListener('mousemove', e => {
            if (popup_timer == null)
                popup_timer = setTimeout(closePopup, 500);
        });
    }

    function closePopup() {
        gp_popup.classList.remove('active');
        render(html``, gp_popup);
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

    this.popup = function(e, func) {
        if (!gp_popup)
            initPopup();

        let widgets = [];

        let builder = new FormBuilder(gp_popup, widgets);
        builder.changeHandler = () => self.popup(e, func);
        builder.close = closePopup;

        func(builder);
        render(html`${widgets.map(w => w.render(w.errors))}`, gp_popup);

        // We need to know popup width and height
        gp_popup.style.visibility = 'hidden';
        gp_popup.classList.add('active');

        // Try different positions
        {
            let x = e.clientX - 1;
            if (x > window.innerWidth - gp_popup.scrollWidth - 10) {
                x = e.clientX - gp_popup.scrollWidth - 1;
                if (x < 10) {
                    x = Math.min(e.clientX - 1, window.innerWidth - gp_popup.scrollWidth - 10);
                    x = Math.max(x, 10);
                }
            }

            let y = e.clientY - 1;
            if (y > window.innerHeight - gp_popup.scrollHeight - 10) {
                y = e.clientY - gp_popup.scrollHeight - 1;
                if (y < 10) {
                    y = Math.min(e.clientY - 1, window.innerHeight - gp_popup.scrollHeight - 10);
                    y = Math.max(y, 10);
                }
            }

            gp_popup.style.left = x + 'px';
            gp_popup.style.top = y + 'px';
            gp_popup.style.visibility = 'visible';
        }

        if (e.stopPropagation)
            e.stopPropagation();

        clearTimeout(popup_timer);
        popup_timer = null;
    };

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete') {
            initNavigation();
            self.go(window.location.href, false);
        }
    });

    return this;
}).call({});
