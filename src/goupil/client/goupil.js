// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let goupil = (function() {
    let self = this;

    let event_src;

    let gp_popup;
    let popup_builder;
    let popup_mem = {};
    let popup_timer;

    let db;

    function parseURL(href, base) {
        return new URL(href, base);
    }

    function initData() {
        if (navigator.storage && navigator.storage.persist) {
            navigator.storage.persist().then(granted => {
                if (!granted)
                    console.warn('Storage may be cleared by the UA under storage pressure');
            });
        } else {
            console.warn('Storage may be cleared by the UA under storage pressure');
        }

        let db_name = `goupil_${settings.project_key}`;
        self.database = data.open(db_name, db => {
            db.createObjectStore('settings');
            db.createObjectStore('pages', {keyPath: 'key'});
        });
    }

    function initNavigation() {
        window.addEventListener('popstate', e => {
            self.go(window.location.href, false);
        });

        document.body.addEventListener('click', e => {
            if (e.target && e.target.tagName == 'A' &&
                    !e.ctrlKey && !e.target.getAttribute('download')) {
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

        gp_popup.addEventListener('mouseleave', e => {
            popup_timer = setTimeout(closePopup, 3000);
        });
        gp_popup.addEventListener('mouseenter', e => {
            clearTimeout(popup_timer);
            popup_timer = null;
        });

        gp_popup.addEventListener('keydown', e => {
            switch (e.keyCode) {
                case 13: {
                    if (e.target.tagName !== 'BUTTON' && e.target.tagName !== 'A' &&
                            popup_builder.submit)
                        popup_builder.submit();
                } break;
                case 27: { closePopup(); } break;
            }

            clearTimeout(popup_timer);
            popup_timer = null;
        });

        gp_popup.addEventListener('click', e => e.stopPropagation());
        document.addEventListener('click', closePopup);
    }

    function openPopup(e, func) {
        if (!gp_popup)
            initPopup();

        let widgets = [];

        popup_builder = new FormBuilder(gp_popup, 'popup!', widgets, popup_mem);
        popup_builder.changeHandler = () => openPopup(e, func);
        popup_builder.submit = null;
        popup_builder.close = closePopup;

        func(popup_builder);
        render(html`${widgets.map(w => w.render(w.errors))}`, gp_popup);

        // We need to know popup width and height
        let give_focus = !gp_popup.classList.contains('active');
        gp_popup.style.visibility = 'hidden';
        gp_popup.classList.add('active');

        // Try different positions
        {
            let origin;
            if (e.clientX && e.clientY) {
                origin = {
                    x: e.clientX - 1,
                    y: e.clientY - 1
                };
            } else {
                let rect = e.target.getBoundingClientRect();
                origin = {
                    x: (rect.left + rect.right) / 2,
                    y: (rect.top + rect.bottom) / 2
                }
            }

            let pos = {
                x: origin.x,
                y: origin.y
            };
            if (pos.x > window.innerWidth - gp_popup.offsetWidth - 10) {
                pos.x = origin.x - gp_popup.offsetWidth;
                if (pos.x < 10) {
                    pos.x = Math.min(origin.x, window.innerWidth - gp_popup.offsetWidth - 10);
                    pos.x = Math.max(pos.x, 10);
                }
            }
            if (pos.y > window.innerHeight - gp_popup.offsetHeight - 10) {
                pos.y = origin.y - gp_popup.offsetHeight;
                if (pos.y < 10) {
                    pos.y = Math.min(origin.y, window.innerHeight - gp_popup.offsetHeight - 10);
                    pos.y = Math.max(pos.y, 10);
                }
            }

            gp_popup.style.left = pos.x + 'px';
            gp_popup.style.top = pos.y + 'px';
        }

        if (e.stopPropagation)
            e.stopPropagation();
        clearTimeout(popup_timer);
        popup_timer = null;

        // Reveal!
        gp_popup.style.visibility = 'visible';

        // Give focus to first input
        if (give_focus) {
            let first_widget = gp_popup.querySelector(`.af_widget input, .af_widget select,
                                                       .af_widget button, .af_widget textarea`);
            if (first_widget)
                first_widget.focus();
        }
    }

    function closePopup() {
        popup_mem = {};
        popup_builder = null;

        clearTimeout(popup_timer);
        popup_timer = null;

        if (gp_popup) {
            gp_popup.classList.remove('active');
            render(html``, gp_popup);
        }
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
        closePopup();
        openPopup(e, func);
    };

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete') {
            initData();
            initNavigation();

            self.go(window.location.href, false);
        }
    });

    return this;
}).call({});
