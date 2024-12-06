// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
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

import { render, html, noChange, directive, Directive } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from '../../../web/core/common.js';

if (typeof T == 'undefined')
    T = {};

Object.assign(T, {
    cancel: 'Annuler',
    confirm: 'Confirmer',
    confirm_action: 'Confirmer l\'action',
    error_has_occured: 'Une erreur est survenue',
    filter: 'Filtrer'
});

let run_func = () => {};

let log_entries = [];

let init_dialogs = false;
let dialogs = [];

let drag_items = null;
let drag_src = null;
let drag_restore = null;

let table_orders = {};
let table_filters = {};

function setRunFunction(func) {
    run_func = func;
}

function notifyHandler(action, entry) {
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
}

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

function wrap(func) {
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

            setTimeout(run_func, 0);
        }
    };
}

function insist(func) {
    let wrapped = wrap(func);

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
}

function confirm(action, func = null) {
    if (func != null) {
        return wrap(run);
    } else {
        func = () => {};
        return run();
    }

    function run() {
        return dialog({
            run: (render, close) => html`
                <div class="title">
                    ${T.confirm_action}
                    <div style="flex: 1;"></div>
                    <button type="button" class="secondary" @click=${wrap(close)}>✖\uFE0E</button>
                </div>

                <div class="main">${action}</div>

                <div class="footer">
                    <button type="button" class="secondary" @click=${wrap(close)}>${T.cancel}</button>
                    <button type="submit" class="danger">${T.confirm}</button>
                </div>
            `,

            submit: () => func()
        });
    }
}

function dialog(options = {}) {
    options = Object.assign({}, options);

    if (options.resolve_on_submit == null)
        options.resolve_on_submit = true;
    if (options.can_be_closed == null)
        options.can_be_closed = true;

    let dlg = {
        update: null,
        resolve: null,
        reject: null,

        resolve_on_submit: options.resolve_on_submit,
        can_be_closed: options.can_be_closed
    };

    if (!init_dialogs) {
        window.addEventListener('keydown', e => {
            if (e.keyCode == 27 && dialogs.length) {
                let dlg = dialogs[dialogs.length - 1];

                if (dlg.can_be_closed)
                    dlg.reject();
            }
        });

        init_dialogs = true;
    }

    let dlg_el = document.createElement('div');
    dlg_el.className = 'dialog';

    let p = new Promise((resolve, reject) => {
        dlg.update = () => {
            if (dlg != dialogs[dialogs.length - 1])
                return;

            render(html`
                <form @click=${e => e.stopPropagation()}
                      @submit=${wrap(e => submit(e, options.submit, dlg))}>
                    ${options.run(dlg.update, () => reject())}
                </form>
            `, dlg_el);
        };

        dlg.resolve = resolve;
        dlg.reject = reject;

        window.requestAnimationFrame(() => {
            let widget0 = dlg_el.querySelector('.main > label > *[autofocus]') ||
                          dlg_el.querySelector(`.main > label > input:not(:disabled),
                                                .main > label > select:not(:disabled),
                                                .main > label > textarea:not(:disabled)`);

            if (widget0 != null) {
                if (typeof widget0.select == 'function') {
                    widget0.select();
                } else {
                    widget0.focus();
                }
            }
        });
    });

    p.finally(() => {
        if (options.close != null)
            options.close();

        document.body.removeChild(dlg_el);

        dialogs = dialogs.filter(it => it != dlg);

        if (dialogs.length) {
            let dlg = dialogs[dialogs.length - 1];
            dlg.update();
        }

        setTimeout(run_func, 0);
    });

    dialogs.push(dlg);
    dlg.update();

    document.body.appendChild(dlg_el);

    if (options.open != null)
        options.open(dlg_el.children[0]);
    if (options.can_be_closed)
        p.close = () => dlg.reject();

    return p;
}

async function submit(e, apply, dlg) {
    try {
        let values = [];

        if (apply != null) {
            let ret = await apply(e.target.elements);
            values.push(ret);
        }

        if (dlg.resolve_on_submit) {
            dlg.resolve(...values);
        } else {
            window.requestAnimationFrame(dlg.update, 0);
        }
    } catch (err) {
        window.requestAnimationFrame(dlg.update, 0);
        throw err;
    }
}

async function runDialog() {
    if (dialogs.length) {
        let dlg = dialogs[dialogs.length - 1];
        dlg.update();
    }
}

function closeDialog() {
    if (dialogs.length) {
        let dlg = dialogs[dialogs.length - 1];
        dlg.reject();
    }
}

function isDialogOpen() {
    return !!dialogs.length;
}

class SelectDirective extends Directive {
    update(part, [value]) {
        window.requestAnimationFrame(() => { part.element.value = value; });
        return noChange;
    }
}

const selectValue = directive(SelectDirective);

class ReorderDirective extends Directive {
    item = null;
    installed = false;

    update(part, [items, item, enable, finalize]) {
        let el = part.element;
        let grab = el.querySelector('.grab') || el;

        if (enable == null)
            enable = true;

        grab.setAttribute('draggable', enable ? 'true' : 'false');
        this.item = item;

        if (!this.installed) {
            grab.addEventListener('dragstart', wrap(e => dragStart(e, items, this.item)));
            grab.addEventListener('dragover', wrap(e => dragOver(e, items, this.item)));
            grab.addEventListener('dragend', wrap(e => dragEnd(e, finalize)));

            this.installed = true;
        }

        return noChange;
    }
}

const reorderItems = directive(ReorderDirective);

function dragStart(e, items, src) {
    drag_items = items;
    drag_src = src;
    drag_restore = items.indexOf(src);

    e.dataTransfer.setData('dummy', 'value');
    e.dataTransfer.effectAllowed = 'move';
}

function dragOver(e, items, dest) {
    if (items != drag_items) {
        e.dataTransfer.dropEffect = 'none';
        return;
    }

    let idx1 = drag_items.indexOf(drag_src);
    let idx2 = drag_items.indexOf(dest);

    if (idx1 == idx2)
        return;

    reorder(drag_items, idx1, idx2);

    e.dataTransfer.dropEffect = 'move';
    e.preventDefault();

    return run_func();
}

function dragEnd(e, finalize) {
    if (e.dataTransfer.dropEffect != 'move') {
        let idx = drag_items.indexOf(drag_src);
        reorder(drag_items, idx, drag_restore);
    }

    drag_items = null;
    drag_src = null;

    if (finalize != null)
        finalize();

    return run_func();
}

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
}

function tableHeader(key, by, title) {
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
}

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

    order.language = document.documentElement.lang || 'en';
    order.comparator = makeComparator(order, order.language);

    table_orders[key] = order;
}

function tableFilter(key, ...keys) {
    let filter = table_filters[key];

    return html`<input type="search" placeholder=${T.filter + '...'} style="width: 10em;"
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

function tableValues(key, values, default_by = null) {
    let order = table_orders[key];
    let filter = table_filters[key];

    if (order == null && default_by != null) {
        setOrder(key, default_by);
        order = table_orders[key];
    }
    if (order != null) {
        let lang = document.documentElement.lang || 'en';

        if (lang != order.language) {
            order.language = lang;
            order.comparator = makeComparator(order, lang);
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

function makeComparator(order, language) {
    let collator = new Intl.Collator(language, {
        numeric: true,
        ignorePunctuation: true,
        sensitivity: 'base'
    });

    let comparator = (obj1, obj2) => {
        let value1 = order.by_func(obj1);
        let value2 = order.by_func(obj2);

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
            return order.ascending ? ret : -ret;
        }
    };

    return comparator;
}

export {
    run_func as runFunction,
    setRunFunction,

    notifyHandler,

    wrap,
    insist,
    confirm,

    dialog,
    runDialog,
    closeDialog,
    isDialogOpen,

    selectValue,
    reorderItems,

    tableHeader,
    tableFilter,
    tableValues
}
