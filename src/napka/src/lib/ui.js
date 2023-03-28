import { render, html } from '../../node_modules/lit/html.js';
import { util, log } from '../../../web/libjs/util.js';

const ui = new (function() {
    let self = this;

    let log_entries = [];

    let dialog_el = null;
    let dialogs = [];

    let drag_items = null;
    let drag_src = null;
    let drag_restore = null;

    this.notify_handler = function(action, entry) {
        if (entry.type !== 'debug') {
            switch (action) {
                case 'open': {
                    log_entries.unshift(entry);

                    if (entry.type === 'progress') {
                        // Wait a bit to show progress entries to prevent quick actions from showing up
                        setTimeout(render_log, 300);
                    } else {
                        render_log();
                    }
                } break;
                case 'edit': {
                    render_log();
                } break;
                case 'close': {
                    log_entries = log_entries.filter(it => it !== entry);
                    render_log();
                } break;
            }
        }

        log.default_handler(action, entry);
    };

    function render_log() {
        let log_el = document.querySelector('#log');

        render(log_entries.map(entry => {
            let msg = (entry.msg instanceof Error) ? entry.msg.message : entry.msg;

            if (typeof msg === 'string')
                msg = msg.split('\n').map(line => [line, html`<br/>`]);

            if (entry.type === 'progress') {
                return html`<div class="progress">
                    <div class="log_spin"></div>
                    ${msg}
                </div>`;
            } else if (entry.type === 'error') {
                return html`<div class="error" @click=${e => entry.close()}>
                    <button class="log_close">X</button>
                    <b>An error has occured</b><br/>
                    ${msg}
                </div>`;
            } else {
                return html`<div class=${entry.type} @click=${e => entry.close()}>
                    <button class="log_close">X</button>
                    ${msg}
                </div>`;
            }
        }), log_el);
    }

    this.wrap = function(func) {
        return async e => {
            let target = e.target;
            let busy = e.target;

            if (target.tagName == 'FORM') {
                e.preventDefault();

                // Make submit button (if any) busy
                busy = target.querySelector('button[type=submit]') || busy;
            }
            e.stopPropagation();

            if (busy.disabled || busy.classList.contains('busy'))
                return;

            try {
                if (busy.disabled != null)
                    busy.disabled = true;
                busy.classList.add('busy');

                await func(e);
            } catch (err) {
                if (err != null) {
                    log.error(err);
                    throw err;
                }
            } finally {
                if (busy.disabled != null)
                    busy.disabled = false;
                busy.classList.remove('busy');
            }
        };
    };

    this.insist = function(func) {
        return self.wrap(async e => {
            let target = e.currentTarget;

            if (!target.classList.contains('insist')) {
                target.classList.add('insist');
                setTimeout(() => { target.classList.remove('insist'); }, 2000);

                if (e.target.tagName == 'FORM')
                    e.preventDefault();
                e.stopPropagation();
            } else {
                target.classList.remove('insist');
                await func(e);
            }
        });
    };

    this.confirm = function(action, func) {
        return self.wrap(() => self.dialog({
            run: (render, close) => html`
                <div class="title">${action}</div>
                <div>Attention cette action est irréversible</div>
                <div class="footer">
                    <button type="button" class="secondary" @click=${ui.wrap(close)}>Annuler</button>
                    <button type="submit" class="danger">Confirmer</button>
                </div>
            `,

            submit: () => func()
        }));
    };

    this.dialog = function(options = {}) {
        options = Object.assign({}, options);

        let dlg = {
            update: null,
            reject: null
        };

        if (dialog_el == null) {
            dialog_el = document.createElement('div');

            window.addEventListener('click', e => {
                if (dialogs.length) {
                    let dlg = dialogs[dialogs.length - 1];
                    dlg.reject();
                }
            });
            window.addEventListener('keydown', e => {
                if (e.keyCode == 27 && dialogs.length) {
                    let dlg = dialogs[dialogs.length - 1];
                    dlg.reject();
                }
            });
        }
        dialog_el.className = 'dialog';

        let p = new Promise((resolve, reject) => {
            dlg.update = () => {
                if (dlg != dialogs[dialogs.length - 1])
                    return;

                render(html`
                    <form @click=${e => e.stopPropagation()}
                          @submit=${self.wrap(e => submit(e, options.submit, resolve))}>
                        ${options.run(dlg.update, () => reject())}
                    </form>
                `, dialog_el);
            };
            dlg.reject = reject;

            setTimeout(() => {
                let widget0 = dialog_el.querySelector('form > label > *[autofocus]') ||
                              dialog_el.querySelector(`form > label > input:not(:disabled),
                                                       form > label > select:not(:disabled),
                                                       form > label > textarea:not(:disabled)`);

                if (widget0 != null) {
                    if (typeof widget0.select == 'function') {
                        widget0.select();
                    } else {
                        widget0.focus();
                    }
                }
            }, 0);
        });

        p.finally(() => {
            if (options.close != null)
                options.close();

            dialogs = dialogs.filter(it => it != dlg);

            if (dialogs.length) {
                let dlg = dialogs[dialogs.length - 1];
                dlg.update();
            } else {
                document.body.removeChild(dialog_el);
                dialog_el = null;
            }

            // setTimeout(app.go, 0);
        });

        dialogs.push(dlg);
        dlg.update();

        document.body.appendChild(dialog_el);

        if (options.open != null)
            options.open(dialog_el.children[0]);

        return p;
    }

    async function submit(e, apply, resolve) {
        if (apply == null)
            return;

        let ret = await apply(e.target.elements);
        resolve(ret);
    }

    this.is_dialog_running = function() { return !!dialogs.length; };

    this.run_dialog = async function() {
        if (dialogs.length) {
            let dlg = dialogs[dialogs.length - 1];
            dlg.update();
        }
    };
    this.close_dialog = function() {
        if (dialogs.length) {
            let dlg = dialogs[dialogs.length - 1];
            dlg.reject();
        }
    };

    this.drag_start = function(e, items, src) {
        drag_items = items;
        drag_src = src;
        drag_restore = items.indexOf(src);

        e.dataTransfer.setData('dummy', 'value');
        e.dataTransfer.effectAllowed = 'move';
    };

    this.drag_over = function(e, items, dest) {
        if (items != drag_items) {
            e.dataTransfer.dropEffect = 'none';
            return;
        }

        let idx1 = drag_items.indexOf(drag_src);
        let idx2 = drag_items.indexOf(dest);

        reorder(drag_items, idx1, idx2);

        e.dataTransfer.dropEffect = 'move';
        e.preventDefault();
    };

    this.drag_end = function(e) {
        if (e.dataTransfer.dropEffect != 'move') {
            let idx = drag_items.indexOf(drag_src);
            reorder(drag_items, idx, drag_restore);
        }

        drag_items = null;
        drag_src = null;
    };

    function reorder(items, idx1, idx2) {
        if (idx1 < idx2) {
            let reordered = [
                ...items.slice(0, idx1),
                ...items.slice(idx1 + 1, idx2 + 1),
                items[idx1],
                ...items.slice(idx2 + 1)
            ];

            items.length = 0;
            items.push(...reordered);
        } else if (idx1 > idx2) {
            let reordered = [
                ...items.slice(0, idx2),
                items[idx1],
                ...items.slice(idx2, idx1),
                ...items.slice(idx1 + 1)
            ];

            items.length = 0;
            items.push(...reordered);
        }
    };

    this.remove = function(items, item) {
        let idx = items.indexOf(item);

        if (idx >= 0)
            items.splice(idx, 1);
    };
})();

module.exports = {
    ui
};
