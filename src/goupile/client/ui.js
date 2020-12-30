// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const ui = new function() {
    let self = this;

    let menu;
    let panels = new Map;

    let init_dialogs = false;

    let dialogs = {
        prev: null,
        next: null
    };
    dialogs.prev = dialogs;
    dialogs.next = dialogs;

    this.render = function() {
        // Render main screen
        render(html`
            ${menu != null ? html`<nav id="ui_menu">${menu()}</nav>` : ''}

            <main id="ui_panels">
                ${util.map(panels.values(), panel => panel.enabled ? panel.render() : '')}
            </main>
        `, document.querySelector('#gp_root'));

        // Run dialog functions
        {
            let it = dialogs.next;

            while (it !== dialogs) {
                it.render();
                it = it.next;
            }
        }
    };

    this.setMenu = function(func) {
        menu = func;
    };

    this.createPanel = function(key, func) {
        panels.set(key, {
            render: func,
            enabled: true
        });
    };

    this.isPanelEnabled = function(key) {
        let panel = panels.get(key);
        return panel.enabled;
    };

    this.togglePanel = function(key, enable = undefined) {
        let panel = panels.get(key);

        if (enable != null) {
            panel.enabled = enable;
        } else {
            panel.enabled = !panel.enabled;
        }

        self.render();
    };

    this.runScreen = function(func) {
        render('', document.querySelector('#gp_root'));
        return runDialog(null, 'modal', false, func);
    };

    this.runDialog = function(e, func) {
        if (e != null) {
            e.stopPropagation();
            if (window.matchMedia('(pointer: coarse)').matches)
                e = null;
        }

        return runDialog(e, e ? 'popup' : 'modal', true, func);
    };

    function runDialog(e, type, closeable, func) {
        if (!init_dialogs) {
            document.addEventListener('click', handleDocumentClick);
            init_dialogs = true;
        }

        return new Promise((resolve, reject) => {
            let dialog = {
                prev: dialogs.prev,
                next: dialogs,

                type: type,
                el: document.createElement('div'),
                state: new FormState,
                render: () => buildDialog(dialog, e, func),
                closeable: closeable,

                resolve: value => {
                    if (dialog.el != null) {
                        resolve(value);
                        closeDialog(dialog);
                    }
                },
                reject: err => {
                    if (dialog.el != null) {
                        reject(err);
                        closeDialog(dialog);
                    }
                }
            };

            // Complete linked list insertion
            dialogs.prev.next = dialog;
            dialogs.prev = dialog;

            // Modal or popup?
            dialog.el.className = `ui_${type}`;
            if (closeable) {
                dialog.el.addEventListener('keydown', e => {
                    if (e.keyCode == 27)
                        dialog.reject(new Error('Action annulée'));
                });
            }
            dialog.el.style.zIndex = 999999;

            // Show it!
            document.body.appendChild(dialog.el);
            dialog.render();
        });
    };

    function handleDocumentClick(e) {
        let it = dialogs.next;

        while (it !== dialogs) {
            if (it.closeable) {
                if (it.type === 'popup') {
                    let target = e.target.parentNode;

                    for (;;) {
                        if (target === it.el) {
                            break;
                        } else if (target == null) {
                            it.reject(new Error('Action annulée'));
                            break;
                        }
                        target = target.parentNode;
                    }
                } else if (it.type === 'modal') {
                    let target = e.target;

                    if (target === it.el)
                        it.reject(new Error('Action annulée'));
                }
            }

            it = it.next;
        }
    }

    function buildDialog(dialog, e, func) {
        let model = new FormModel('@popup');

        let builder = new FormBuilder(dialog.state, model);
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
            let widget0 = dialog.el.querySelector('input, select, button, textarea');
            if (widget0 != null)
                widget0.focus();
        }
    }

    function closeDialog(dialog) {
        dialog.next.prev = dialog.prev;
        dialog.prev.next = dialog.next;

        document.body.removeChild(dialog.el);
        dialog.el = null;
    }

    this.runConfirm = function(e, msg, action, func) {
        return self.runDialog(e, (d, resolve, reject) => {
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
            d.action('Annuler', {}, () => reject(new Error('Action annulée')));
        });
    };
};
