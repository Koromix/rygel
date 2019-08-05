// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// These globals are initialized above
let g_assets = null;
let g_records = null;

let goupil = (function() {
    let self = this;

    let event_src;

    let gp_popup;
    let popup_builder;
    let popup_state;
    let popup_timer;

    let log_entries = [];

    function parseURL(href, base) {
        return new URL(href, base);
    }

    function initLog() {
        log.pushHandler(addLogEntry);
    }

    async function openDatabase() {
        let storage_warning = 'Local data may be cleared by the browser under storage pressure, ' +
                              'check your privacy settings for this website';

        if (navigator.storage && navigator.storage.persist) {
            navigator.storage.persist().then(granted => {
                // NOTE: For some reason this does not seem to work correctly on my Firefox profile,
                // where granted is always true. Might come from an extension or some obscure privacy
                // setting. Investigate.
                if (!granted)
                    log.error(storage_warning);
            });
        } else {
            log.error(storage_warning);
        }

        let db_name = `goupil_${settings.project_key}`;
        let db = await idb.open(db_name, 7, (db, old_version) => {
            switch (old_version) {
                case null: {
                    db.createObjectStore('pages', {keyPath: 'key'});
                    db.createObjectStore('settings');
                } // fallthrough
                case 1: {
                    db.createObjectStore('data', {keyPath: 'id'});
                } // fallthrough
                case 2: {
                    db.createObjectStore('variables', {keyPath: 'key'});
                } // fallthrough
                case 3: {
                    db.deleteObjectStore('data');
                    db.deleteObjectStore('settings');

                    db.createObjectStore('assets', {keyPath: 'key'});
                    db.createObjectStore('records', {keyPath: 'id'});
                } // fallthrough
                case 4: {
                    db.deleteObjectStore('records');
                    db.deleteObjectStore('variables');
                    db.createObjectStore('records', {keyPath: 'tkey'});
                    db.createObjectStore('variables', {keyPath: 'tkey'});
                } // fallthrough
                case 5: {
                    db.deleteObjectStore('pages');
                } // fallthrough
                case 6: {
                    db.deleteObjectStore('records');
                    db.deleteObjectStore('variables');
                    db.createObjectStore('form_records', {keyPath: 'tkey'});
                    db.createObjectStore('form_variables', {keyPath: 'tkey'});
                } // fallthrough
            }
        });

        return db;
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

    function renderLog() {
        let log_el = document.querySelector('#gp_log');

        render(log_entries.map((entry, idx) => {
            return html`<div class=${'gp_log_entry ' + entry.type}>
                <button class="gp_log_close" @click=${e => closeLogEntry(idx)}>X</button>
                ${entry.msg}
             </div>`;
        }), log_el);
    }

    function addLogEntry(type, msg) {
        if (type === 'error' || type === 'success') {
            let entry = {
                msg: msg,
                type: type
            };
            log_entries.unshift(entry);

            renderLog();

            setTimeout(() => {
                log_entries.pop();
                renderLog();
            }, 6000);
        }

        log.defaultHandler(type, msg);
    }

    function closeLogEntry(idx) {
        log_entries.splice(idx, 1);
        renderLog();
    }

    function openPopup(e, func) {
        if (!gp_popup)
            initPopup();

        let widgets = [];

        popup_builder = new FormBuilder(popup_state, widgets);
        popup_builder.changeHandler = () => openPopup(e, func);
        popup_builder.close = closePopup;
        popup_builder.pushOptions({missingMode: 'disable'});

        func(popup_builder);
        render(widgets.map(intf => intf.render(intf)), gp_popup);

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
        popup_state = new FormState();
        popup_builder = null;

        clearTimeout(popup_timer);
        popup_timer = null;

        if (gp_popup) {
            gp_popup.classList.remove('active');
            render(html``, gp_popup);
        }
    }

    function showDummyPage() {
        document.title = `${settings.project_key} — goupil autoform`;

        let main_el = document.querySelector('main');

        render(html`
            <div class="gp_wip">Page en chantier</div>
        `, main_el);
    }

    this.go = function(href, history = true) {
        // Asset key
        let asset_key = parseURL(href, window.location.href).pathname;
        if (asset_key.startsWith(settings.base_url))
            asset_key = asset_key.substr(settings.base_url.length);
        while (asset_key.endsWith('/'))
            asset_key = asset_key.substr(0, asset_key.length - 1);

        // Run asset
        pilot.go(asset_key);

        // Update history
        let full_path = `${settings.base_url}${asset_key}${asset_key ? '/' : ''}`;
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

    this.popup = function(e, func) {
        closePopup();
        openPopup(e, func);
    };

    async function initGoupil() {
        initLog();
        initNavigation();

        let db = await openDatabase();
        g_assets = new AssetManager(db);
        g_records = new RecordManager(db);

        self.go(window.location.href, false);
    }

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete')
            initGoupil();
    });

    return this;
}).call({});
