// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const ui = new function() {
    let self = this;

    let menu_render;
    let panels = new Map;
    let active_panels = 0;

    let dialogs = {
        prev: null,
        next: null
    };
    dialogs.prev = dialogs;
    dialogs.next = dialogs;

    let log_entries = [];

    this.init = function() {
        log.pushHandler(notifyHandler);

        window.addEventListener('resize', () => {
            adaptToViewport();
            ui.render();
        });
        adaptToViewport();

        document.addEventListener('click', handleDocumentClick);
    }

    function adaptToViewport(keep_panel = null) {
        let min_width = 580;

        let small = window.innerWidth < min_width;
        let max_panels = util.clamp(Math.floor(window.innerWidth / min_width), 1, 2);

        while (active_panels > max_panels) {
            let disable_priority = Number.MAX_SAFE_INTEGER;
            let disable_panel;

            for (let panel of panels.values()) {
                if (panel !== keep_panel && panel.active && panel.priority < disable_priority) {
                    disable_priority = panel.priority;
                    disable_panel = panel;
                }
            }

            disable_panel.active = null;
            active_panels--;
        }

        document.documentElement.classList.toggle('small', small);
    }

    this.render = function() {
        // Render main screen
        render(html`
            ${menu_render != null ? html`<nav class="ui_toolbar" style="z-index: 999999;">${menu_render()}</nav>` : ''}

            <main id="ui_panels">
                ${util.map(panels.values(), panel => panel.active ? panel.render() : '')}
            </main>
        `, document.querySelector('#ui_root'));

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
        menu_render = func;
    };

    this.createPanel = function(key, priority, active, func) {
        panels.set(key, {
            render: func,
            priority: priority,
            active: active
        });
        active_panels += active;

        adaptToViewport();
    };

    this.isPanelEnabled = function(key) {
        let panel = panels.get(key);
        return panel.active;
    };

    this.setPanelState = function(key, active) {
        let panel = panels.get(key);

        if (panel.active !== active) {
            panel.active = active;
            active_panels += active ? 1 : -1;

            if (active)
                adaptToViewport(panel);
        }
    };

    this.runScreen = function(func) {
        render('', document.querySelector('#ui_root'));
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
            dialog.state.changeHandler = dialog.render;

            // Complete linked list insertion
            dialogs.prev.next = dialog;
            dialogs.prev = dialog;

            // Modal or popup?
            dialog.el.className = `ui_${type}`;
            if (closeable) {
                dialog.el.addEventListener('keydown', e => {
                    if (e.keyCode == 27)
                        dialog.reject(null);
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
                            it.reject(null);
                            break;
                        }
                        target = target.parentNode;
                    }
                } else if (it.type === 'modal') {
                    let target = e.target;

                    if (target === it.el)
                        it.reject(null);
                }
            }

            it = it.next;
        }
    }

    function buildDialog(dialog, e, func) {
        let model = new FormModel;
        let builder = new FormBuilder(dialog.state, model);

        builder.pushOptions({
            missing_mode: 'disable',
            wide: true
        });
        func(builder, dialog.resolve, dialog.reject);
        if (dialog.closeable)
            builder.action('Annuler', {}, () => dialog.reject(null));

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
                setTimeout(() => widget0.focus(), 0);
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
                    reject(err);
                }
            });
        });
    };

    this.wrapAction = function(func) {
        return async e => {
            let target = e.target;

            if (target.disabled != null) {
                try {
                    target.disabled = true;
                    await func(e);
                } catch (err) {
                    if (err != null) {
                        log.error(err);
                        throw err;
                    }
                } finally {
                    target.disabled = false;
                }
            } else {
                if (target.classList.contains('disabled'))
                    return;

                try {
                    target.classList.add('disabled');
                    await func(e);
                } catch (err) {
                    if (err != null) {
                        log.error(err);
                        throw err;
                    }
                } finally {
                    target.classList.remove('disabled');
                }
            }
        };
    };

    function notifyHandler(action, entry) {
        if (typeof lithtml !== 'undefined' && entry.type !== 'debug') {
            switch (action) {
                case 'open': {
                    log_entries.unshift(entry);

                    if (entry.type === 'progress') {
                        // Wait a bit to show progress entries to prevent quick actions from showing up
                        setTimeout(renderLog, 300);
                    } else {
                        renderLog();
                    }
                } break;
                case 'edit': {
                    renderLog();
                } break;
                case 'close': {
                    log_entries = log_entries.filter(it => it !== entry);
                    renderLog();
                } break;
            }
        }

        log.defaultHandler(action, entry);
    };

    function renderLog() {
        let log_el = document.querySelector('#ui_log');
        if (!log_el) {
            log_el = document.createElement('div');
            log_el.id = 'ui_log';
            document.body.appendChild(log_el);
        }

        render(log_entries.map((entry, idx) => {
            let msg = (entry.msg instanceof Error) ? entry.msg.message : entry.msg;

            if (entry.type === 'progress') {
                return html`<div class="progress">
                    <div class="ui_log_spin"></div>
                    ${msg.split('\n').map(line => [line, html`<br/>`])}
                </div>`;
            } else {
                return html`<div class=${entry.type} @click=${e => entry.close()}>
                    <button class="ui_log_close">X</button>
                    ${msg.split('\n').map(line => [line, html`<br/>`])}
                </div>`;
            }
        }), log_el);
    }
};
