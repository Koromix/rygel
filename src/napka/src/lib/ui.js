// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { render, html, noChange } from '../../node_modules/lit/html.js';
import { directive, Directive } from '../../node_modules/lit/directive.js';
import { Util, Log } from '../../../web/core/base.js';

let T = {};

Object.assign(T, {
    cancel: 'Cancel',
    confirm: 'Confirm',
    confirm_not_reversible: 'Be careful, this action cannot be reversed!',
    error_has_occured: 'An error has occured',
    filter: 'Filter'
});

const UI = new (function() {
    let self = this;

    let run_func = () => {};

    let log_entries = [];

    let dialog_el = null;
    let dialogs = [];

    let drag_items = null;
    let drag_src = null;
    let drag_restore = null;

    let table_orders = {};
    let table_filters = {};

    Object.defineProperties(this, {
        runFunction: { get: () => run_func, set: func => { run_func = func; }, enumerable: true }
    });

    this.notifyHandler = function(action, entry) {
        if (typeof render == 'function' && entry.type !== 'debug') {
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

        Log.defaultHandler(action, entry);
    };

    function renderLog() {
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
                    <b>${T.error_has_occured}</b><br/>
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
            let target = e.currentTarget || e.target;
            let busy = target;

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
                    Log.error(err);
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
        let wrapped = self.wrap(func);

        return async e => {
            let target = e.currentTarget || e.target;

            if (!target.classList.contains('insist')) {
                target.classList.add('insist');
                setTimeout(() => { target.classList.remove('insist'); }, 2000);

                if (e.target.tagName == 'FORM')
                    e.preventDefault();
                e.stopPropagation();
            } else {
                target.classList.remove('insist');
                await wrapped(e);
            }
        };
    };

    this.confirm = function(action, func) {
        return self.wrap(() => self.dialog({
            run: (render, close) => html`
                <div class="title">${action}</div>
                <div>${T.confirm_not_reversible}</div>
                <div class="footer">
                    <button type="button" class="secondary" @click=${UI.wrap(close)}>${T.cancel}</button>
                    <button type="submit" class="danger">${T.confirm}</button>
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

            setTimeout(run_func, 0);
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

    this.isDialogOpen = function() { return !!dialogs.length; };

    this.runDialog = async function() {
        if (dialogs.length) {
            let dlg = dialogs[dialogs.length - 1];
            dlg.update();
        }
    };
    this.closeDialog = function() {
        if (dialogs.length) {
            let dlg = dialogs[dialogs.length - 1];
            dlg.reject();
        }
    };

    class ReorderDirective extends Directive {
        item = null;
        installed = false;

        update(part, [items, item, enable]) {
            let el = part.element;

            if (enable == null)
                enable = true;

            el.setAttribute('draggable', enable ? 'true' : 'false');
            this.item = item;

            if (!this.installed) {
                el.addEventListener('dragstart', self.wrap(e => dragStart(e, items, this.item)));
                el.addEventListener('dragover', self.wrap(e => dragOver(e, items, this.item)));
                el.addEventListener('dragend', self.wrap(dragEnd));

                this.installed = true;
            }

            return noChange;
        }
    }

    this.reorderItems = directive(ReorderDirective);

    function dragStart(e, items, src) {
        drag_items = items;
        drag_src = src;
        drag_restore = items.indexOf(src);

        e.dataTransfer.setData('dummy', 'value');
        e.dataTransfer.effectAllowed = 'move';
    };

    function dragOver(e, items, dest) {
        if (items != drag_items) {
            e.dataTransfer.dropEffect = 'none';
            return;
        }

        let idx1 = drag_items.indexOf(drag_src);
        let idx2 = drag_items.indexOf(dest);

        reorder(drag_items, idx1, idx2);

        e.dataTransfer.dropEffect = 'move';
        e.preventDefault();

        return run_func();
    };

    function dragEnd(e) {
        if (e.dataTransfer.dropEffect != 'move') {
            let idx = drag_items.indexOf(drag_src);
            reorder(drag_items, idx, drag_restore);
        }

        drag_items = null;
        drag_src = null;

        return run_func();
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

    this.tableHeader = function(key, by, title) {
        let info = table_orders[key];

        let ascending = (info != null && info.by_str == String(by)) ? info.ascending : null;

        return html`
            <th class="item" @click=${e => { setOrder(key, by); run_func(); }}>
                ${title}
                <div class="arrows">
                    <span class=${'up' + (ascending === false ? ' active' : '')}></span>
                    <span class=${'down' + (ascending === true ? ' active' : '')}></span>
                </div>
            </th>
        `;
    };

    function setOrder(key, by, ascending = null) {
        let by_str = String(by);
        let by_func = (typeof by == 'function') ? by : (value => value[by_str]);

        if (ascending == null) {
            let order = table_orders[key];

            if (order != null && order.by_str == String(by)) {
                ascending = !order.ascending;
            } else {
                ascending = true;
            }
        }

        let order = {
            by_str: by_str,
            by_func: by_func,
            ascending: ascending,

            comparator: null,
            language: null
        };

        order.comparator = makeComparator(order, app.userLanguage);
        order.language = app.userLanguage;

        table_orders[key] = order;
    }

    this.tableFilter = function(key, ...keys) {
        let filter = table_filters[key];

        return html`<input type="search" placeholder=${T.filter + '...'}
                           value=${(filter != null) ? filter.str : ''}
                           @input=${e => { setFilter(key, e.target.value, keys); run_func(); }} />`;
    };

    function setFilter(key, str, keys) {
        if (str) {
            let filter = {
                str: str,
                keys: keys
            };

            table_filters[key] = filter;
        } else {
            delete table_filters[key];
        }
    }

    this.tableValues = function(key, values) {
        let order = table_orders[key];
        let filter = table_filters[key];

        if (order != null) {
            if (order.language != app.userLanguage) {
                order.comparator = makeComparator(info, app.userLanguage);
                order.language = app.userLanguage;
            }

            values = values.slice().sort(order.comparator);
        }

        if (filter != null) {
            let against = filter.str.toLowerCase();

            values = values.filter(it => {
                for (let key of filter.keys) {
                    let value = it[key];

                    if (value == null)
                        continue;
                    value = String(value).toLowerCase();

                    return value.includes(against);
                }
            });
        }

        return values;
    };

    function makeComparator(info, language) {
        let collator = new Intl.Collator(language, {
            numeric: true,
            ignorePunctuation: true,
            sensitivity: 'base'
        });

        let comparator = (obj1, obj2) => {
            let value1 = info.by_func(obj1);
            let value2 = info.by_func(obj2);

            if (value1 === '')
                value1 = null;
            if (value2 === '')
                value2 = null;

            if (value1 == null && value2 == null) {
                return 0;
            } else if (value1 == null) {
                return 1;
            } else if (value2 == null) {
                return -1;
            } else {
                let ret = collator.compare(String(value1), String(value2));
                return info.ascending ? ret : -ret;
            }
        };

        return comparator;
    }
})();

export { UI }
