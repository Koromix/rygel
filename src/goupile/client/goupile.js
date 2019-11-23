// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// These globals are initialized below or in dev.js
let app = null;
let file_manager = null;
let record_manager = null;

let goupile = new function() {
    let self = this;

    let event_src;

    let tablet_mq = window.matchMedia('(pointer: coarse)');

    let popup_el;
    let popup_state;
    let popup_builder;

    document.addEventListener('readystatechange', e => {
        if (document.readyState === 'complete')
            initGoupil();
    });

    async function initGoupil() {
        log.pushHandler(log.notifyHandler);
        initNavigation();

        let db = await openDatabase();
        file_manager = new FileManager(db);
        record_manager = new RecordManager(db);

        if (typeof dev !== 'undefined')
            await dev.init();

        app.go(window.location.href, false);
    }

    function initNavigation() {
        window.addEventListener('popstate', e => app.go(window.location.href, false));

        util.interceptLocalAnchors((e, href) => {
            app.go(href);
            e.preventDefault();
        });
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

        let db_name = `goupile_${env.app_key}`;
        let db = await idb.open(db_name, 11, (db, old_version) => {
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
                case 7: {
                    db.deleteObjectStore('assets');
                    db.createObjectStore('assets');
                } // fallthrough
                case 8: {
                    db.deleteObjectStore('assets');
                    db.createObjectStore('files');
                } // fallthrough
                case 9: {
                    db.deleteObjectStore('files');
                    db.createObjectStore('files', {keyPath: 'path'});
                    db.createObjectStore('files_data');
                } // fallthrough
                case 10: {
                    db.createObjectStore('files_remote', {keyPath: 'path'});
                } // fallthrough
            }
        });

        return db;
    }

    this.listenToServerEvent = function(event, func) {
        if (!event_src) {
            event_src = new EventSource(`${env.base_url}api/events.json`);
            event_src.onerror = e => event_src = null;
        }

        event_src.addEventListener(event, func);
    };

    this.isTablet = function() { return tablet_mq.matches; };
    this.isOnline = function() { return false; };

    this.popup = function(e, func) {
        closePopup();
        openPopup(e, func);
    };

    function openPopup(e, func) {
        if (!popup_el)
            initPopup();

        let page = new Page;

        popup_builder = new PageBuilder(popup_state, page);
        popup_builder.changeHandler = () => openPopup(e, func);
        popup_builder.close = closePopup;

        popup_builder.pushOptions({
            missingMode: 'disable',
            wide: true
        });

        func(popup_builder);
        page.render(popup_el);

        // We need to know popup width and height
        let give_focus = !popup_el.classList.contains('active');
        popup_el.style.visibility = 'hidden';
        popup_el.classList.add('active');

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
                };
            }

            let pos = {
                x: origin.x,
                y: origin.y
            };
            if (pos.x > window.innerWidth - popup_el.offsetWidth - 10) {
                pos.x = origin.x - popup_el.offsetWidth;
                if (pos.x < 10) {
                    pos.x = Math.min(origin.x, window.innerWidth - popup_el.offsetWidth - 10);
                    pos.x = Math.max(pos.x, 10);
                }
            }
            if (pos.y > window.innerHeight - popup_el.offsetHeight - 10) {
                pos.y = origin.y - popup_el.offsetHeight;
                if (pos.y < 10) {
                    pos.y = Math.min(origin.y, window.innerHeight - popup_el.offsetHeight - 10);
                    pos.y = Math.max(pos.y, 10);
                }
            }

            popup_el.style.left = pos.x + 'px';
            popup_el.style.top = pos.y + 'px';
        }

        if (e.stopPropagation)
            e.stopPropagation();

        // Reveal!
        popup_el.style.visibility = 'visible';

        if (give_focus) {
            // Avoid shrinking popups
            popup_el.style.minWidth = popup_el.offsetWidth + 'px';

            // Give focus to first input
            let first_widget = popup_el.querySelector(`.af_widget input, .af_widget select,
                                                       .af_widget button, .af_widget textarea`);
            if (first_widget)
                first_widget.focus();
        }
    }

    function initPopup() {
        popup_el = document.createElement('div');
        popup_el.setAttribute('id', 'gp_popup');
        document.body.appendChild(popup_el);

        popup_el.addEventListener('keydown', e => {
            switch (e.keyCode) {
                case 13: {
                    if (e.target.tagName !== 'BUTTON' && e.target.tagName !== 'A' &&
                            popup_builder.submit)
                        popup_builder.submit();
                } break;
                case 27: { closePopup(); } break;
            }
        });

        popup_el.addEventListener('click', e => e.stopPropagation());
        document.addEventListener('click', closePopup);
    }

    function closePopup() {
        popup_state = new PageState;
        popup_builder = null;

        if (popup_el) {
            popup_el.classList.remove('active');
            popup_el.style.minWidth = '';
            render('', popup_el);
        }
    }
};
