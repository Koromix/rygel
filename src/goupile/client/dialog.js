// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dialog = new function() {
    let self = this;

    let init = false;

    let count = 0;
    let dialogs = {
        prev: null,
        next: null
    };
    dialogs.prev = dialogs;
    dialogs.next = dialogs;

    this.runScreen = function(func) {
        render('', document.querySelector('#gp_root'));

        // Close all dialogs and popups
        let it = dialogs.next;
        while (it !== dialogs) {
            it.reject();
            it = it.next;
        }

        return runDialog(null, 'modal', false, func);
    };

    this.run = function(e, func) {
        if (e != null) {
            e.stopPropagation();
            if (goupile.isTablet())
                e = null;
        }

        return runDialog(e, e ? 'popup' : 'modal', true, func);
    };

    function runDialog(e, type, closeable, func) {
        if (!init) {
            document.addEventListener('click', handleDocumentClick);
            init = true;
        }

        return new Promise((resolve, reject) => {
            let dialog = {
                prev: dialogs.prev,
                next: dialogs,

                type: type,
                el: document.createElement('div'),
                state: new PageState,
                refresh: () => buildDialog(dialog, e, func),

                resolve: value => {
                    resolve(value);
                    closeDialog(dialog);
                },
                reject: err => {
                    reject(err);
                    closeDialog(dialog);
                }
            };

            // Complete linked list insertion
            dialogs.prev.next = dialog;
            dialogs.prev = dialog;
            count++;

            // Modal or popup?
            dialog.el.setAttribute('id', `dlg_${type}`);
            if (closeable) {
                dialog.el.addEventListener('keydown', e => {
                    if (e.keyCode == 27)
                        dialog.reject(new Error('Action annulée'));
                });
            }
            dialog.el.style.zIndex = 999999 + count;

            // Show it!
            document.body.appendChild(dialog.el);
            dialog.refresh();
        });
    };

    function handleDocumentClick(e) {
        let it = dialogs.next;

        while (it !== dialogs) {
            let target = e.target.parentNode;

            if (it.type === 'popup') {
                for (;;) {
                    if (target === it.el) {
                        break;
                    } else if (target == null) {
                        it.reject(new Error('Action annulée'));
                        break;
                    }
                    target = target.parentNode;
                }
            }

            it = it.next;
        }
    }

    function buildDialog(dialog, e, func) {
        let model = new PageModel('@popup');

        let builder = new PageBuilder(dialog.state, model);
        builder.changeHandler = () => buildDialog(...arguments);
        builder.pushOptions({
            missing_mode: 'disable',
            wide: true
        });

        func(builder, dialog.resolve, dialog.reject);

        render(html`
            <form @submit=${e => e.preventDefault()}>
                ${model.render()}
            </form>
        `, dialog.el);

        // We need to know popup width and height
        let give_focus = !dialog.el.classList.contains('active');
        dialog.el.style.visibility = 'hidden';
        dialog.el.classList.add('active');

        // Try different popup positions
        if (e != null) {
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
            if (pos.x > window.innerWidth - dialog.el.offsetWidth - 10) {
                pos.x = origin.x - dialog.el.offsetWidth;
                if (pos.x < 10) {
                    pos.x = Math.min(origin.x, window.innerWidth - dialog.el.offsetWidth - 10);
                    pos.x = Math.max(pos.x, 10);
                }
            }
            if (pos.y > window.innerHeight - dialog.el.offsetHeight - 10) {
                pos.y = origin.y - dialog.el.offsetHeight;
                if (pos.y < 10) {
                    pos.y = Math.min(origin.y, window.innerHeight - dialog.el.offsetHeight - 10);
                    pos.y = Math.max(pos.y, 10);
                }
            }

            dialog.el.style.left = pos.x + 'px';
            dialog.el.style.top = pos.y + 'px';
        }

        // Reveal!
        dialog.el.style.visibility = 'visible';
        if (give_focus) {
            dialog.el.style.minWidth = dialog.el.offsetWidth + 'px';

            let widget0 = dialog.el.querySelector(`.af_widget input, .af_widget select,
                                                   .af_widget button, .af_widget textarea`);
            if (widget0 != null)
                widget0.focus();
        }
    }

    function closeDialog(dialog) {
        dialog.next.prev = dialog.prev;
        dialog.prev.next = dialog.next;
        count--;

        document.body.removeChild(dialog.el);
    }

    this.confirm = function(e, msg, action, func) {
        return self.run(e, (d, resolve, reject) => {
            d.output(msg);

            d.action(action, {disabled: !d.isValid()}, async () => {
                try {
                    await func();
                    resolve();
                } catch (err) {
                    log.error(err);
                    reject(err);
                }
            });
            d.action('Annuler', {}, () => reject('Action annulée'));
        });
    };

    this.refreshAll = function() {
        let it = dialogs.next;

        while (it !== dialogs) {
            it.refresh();
            it = it.next;
        }
    };
};
