// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// ------------------------------------------------------------------------
// Popup
// ------------------------------------------------------------------------

let popup = (function() {
    let self = this;

    let popup_el;
    let popup_page;
    let popup_state;
    let popup_timer;

    this.form = function(e, func) {
        closePopup();
        openPopup(e, func);
    };

    function openPopup(e, func) {
        if (!popup_el)
            initPopup();

        let widgets = [];

        popup_page = new FormPage(popup_state, widgets);
        popup_page.changeHandler = () => openPopup(e, func);
        popup_page.close = closePopup;
        popup_page.pushOptions({missingMode: 'disable'});

        func(popup_page);
        render(widgets.map(intf => intf.render(intf)), popup_el);

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
                }
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
        clearTimeout(popup_timer);
        popup_timer = null;

        // Reveal!
        popup_el.style.visibility = 'visible';

        // Give focus to first input
        if (give_focus) {
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

        popup_el.addEventListener('mouseleave', e => {
            popup_timer = setTimeout(closePopup, 3000);
        });
        popup_el.addEventListener('mouseenter', e => {
            clearTimeout(popup_timer);
            popup_timer = null;
        });

        popup_el.addEventListener('keydown', e => {
            switch (e.keyCode) {
                case 13: {
                    if (e.target.tagName !== 'BUTTON' && e.target.tagName !== 'A' &&
                            popup_page.submit)
                        popup_page.submit();
                } break;
                case 27: { closePopup(); } break;
            }

            clearTimeout(popup_timer);
            popup_timer = null;
        });

        popup_el.addEventListener('click', e => e.stopPropagation());
        document.addEventListener('click', closePopup);
    }

    function closePopup() {
        popup_state = new FormState();
        popup_page = null;

        clearTimeout(popup_timer);
        popup_timer = null;

        if (popup_el) {
            popup_el.classList.remove('active');
            render('', popup_el);
        }
    }

    return this;
}).call({});
