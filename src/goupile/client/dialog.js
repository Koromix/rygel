// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dialog = new function() {
    let self = this;

    let mod_veil;
    let mod_el;
    let mod_state;
    let mod_builder;

    let popup_el;
    let popup_state;
    let popup_builder;

    this.modal = function(action, func) {
        if (!mod_veil) {
            mod_veil = document.createElement('div');
            mod_veil.setAttribute('id', 'dlg_veil');
            document.body.appendChild(mod_veil);

            mod_el = document.createElement('div');
            mod_veil.appendChild(mod_el);

            mod_veil.addEventListener('click', e => {
                if (e.target === mod_veil)
                    closeModal();
            });
            mod_el.addEventListener('keydown', e => {
                if (e.keyCode == 27)
                    closeModal();
            });
        }

        closeModal();
        openModal(action, func);
    };

    function openModal(action, func) {
        let model = new PageModel('@popup');

        mod_builder = new PageBuilder(mod_state, model);
        mod_builder.changeHandler = () => openModal(...arguments);
        mod_builder.pushOptions({
            missing_mode: 'disable',
            wide: true
        });

        func(mod_builder, closeModal);
        if (action != null)
            mod_builder.action(action, {disabled: !mod_builder.isValid()}, mod_builder.submit);
        mod_builder.action('Annuler', {}, closeModal);

        render(html`
            <form @submit=${e => e.preventDefault()}>
                ${model.render()}
            </form>
        `, mod_el);

        let give_focus = !mod_veil.classList.contains('active');
        mod_veil.classList.add('active');

        if (give_focus) {
            // Give focus to first input
            let first_widget = mod_el.querySelector(`.af_widget input, .af_widget select,
                                                       .af_widget button, .af_widget textarea`);
            if (first_widget)
                first_widget.focus();
        }
    }

    function closeModal() {
        mod_state = new PageState;
        mod_builder = null;

        if (mod_veil) {
            mod_veil.classList.remove('active');
            render('', mod_el);
        }
    }

    this.popup = function(e, action, func) {
        if (!goupile.isTablet()) {
            closePopup();
            openPopup(e, action, func);
        } else {
            self.modal(action, func);
        }
    };

    function openPopup(e, action, func) {
        if (!popup_el) {
            popup_el = document.createElement('div');
            popup_el.setAttribute('id', 'dlg_popup');
            document.body.appendChild(popup_el);

            popup_el.addEventListener('keydown', e => {
                if (e.keyCode == 27)
                    closePopup();
            });

            document.addEventListener('click', e => {
                let el = e.target;
                while (el) {
                    if (el === popup_el)
                        return;
                    el = el.parentNode;
                }
                closePopup();
            });
        }

        let model = new PageModel('@popup');

        popup_builder = new PageBuilder(popup_state, model);
        popup_builder.changeHandler = () => openPopup(...arguments);
        popup_builder.pushOptions({
            missing_mode: 'disable',
            wide: true
        });

        func(popup_builder, closePopup);
        if (action != null)
            popup_builder.action(action, {disabled: !popup_builder.isValid()}, popup_builder.submit);
        popup_builder.action('Annuler', {}, closePopup);

        render(html`
            <form @submit=${e => e.preventDefault()}>
                ${model.render()}
            </form>
        `, popup_el);

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

    function closePopup() {
        popup_state = new PageState;
        popup_builder = null;

        if (popup_el) {
            popup_el.classList.remove('active');
            popup_el.style.minWidth = '';

            render('', popup_el);
        }
    }

    this.refresh = function() {
        if (popup_builder != null)
            popup_builder.changeHandler();
    };
};
