// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, noChange, directive, Directive } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from './base.js';

if (typeof T == 'undefined')
    T = {};

Object.assign(T, {
    cancel: 'Cancel',
    confirm: 'Confirm',
    confirm_not_reversible: 'Be careful, this action cannot be reversed!',
    error_has_occured: 'An error has occured',
    filter: 'Filter'
});

let run_func = () => {};
let render_func = () => {};

let log_entries = [];

let main_el = null;
let fullscreen = false;
let prev_main = null;

let init_dialogs = false;
let dialogs = [];

let init_popups = false;
let current_popup = null;

let drag_items = null;
let drag_src = null;
let drag_restore = null;

let table_orders = {};
let table_filters = {};

function init(run = null, render = null) {
    Log.pushHandler(notifyHandler);

    // Detect fullscreen exit
    document.body.addEventListener('fullscreenchange', () => {
        if (fullscreen && document.fullscreenElement != document.body) {
            fullscreen = false;
            run();
        }
    });

    if (run != null)
        run_func = run;
    if (render != null)
        render_func = render;
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
        } else {
            return html`<div class=${entry.type} @click=${e => entry.close()}>
                ${msg}
            </div>`;
        }
    }), log_el);
}

function wrap(func) {
    return async e => {
        let target = e?.currentTarget ?? e?.target;
        let busy = target;

        if (target != null) {
            if (target.tagName == 'FORM') {
                e.preventDefault();

                // Make submit button (if any) busy
                busy = target.querySelector('button[type=submit]') ?? busy;
            }
            e.stopPropagation();

            if (busy.disabled || busy.classList.contains('busy'))
                return;
        }

        try {
            if (busy != null) {
                if (busy.disabled != null)
                    busy.disabled = true;
                busy.classList.add('busy');
            }

            await func(e);
        } catch (err) {
            if (err != null) {
                Log.error(err);
                throw err;
            }
        } finally {
            if (busy != null) {
                if (busy.disabled != null)
                    busy.disabled = false;
                busy.classList.remove('busy');
            }

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
                <div class="tabbar">
                    <a class="active">${action}</a>
                </div>

                <div class="tab">
                    <div class="box">${T.confirm_not_reversible}</div>

                    <div class="actions">
                        <button type="button" class="secondary" @click=${wrap(close)}>${T.cancel}</button>
                        <button type="submit">${T.confirm}</button>
                    </div>
                </div>
            `,

            submit: () => func()
        });
    }
}

function main(content = null) {
    if (main_el == null) {
        main_el = document.createElement('main');
        main_el.className = 'primary';
    }

    let overlay = dialogs.find(dlg => dlg.overlay);

    if (overlay != null) {
        render_func(overlay.el, fullscreen);
    } else {
        render_func(main_el, fullscreen);
        main_el.classList.toggle('fullscreen', fullscreen);
    }

    if (content != null) {
        render(content, main_el);

        if (content != prev_main) {
            autoFocus(main_el);
            prev_main = content;
        }
    }
}

async function toggleFullscreen(enable = null) {
    if (enable == null)
        enable = !fullscreen;

    fullscreen = enable;

    try {
        if (fullscreen) {
            document.body.requestFullscreen();
        } else {
            document.exitFullscreen();
        }
    } catch (err) {
        console.error(err);
    }

    await run_func();
}

function dialog(options = {}) {
    options = Object.assign({
        overlay: false,
        resolve_on_submit: true,
        can_be_closed: true
    }, options);

    let dlg = {
        el: null,
        overlay: options.overlay,

        update: null,
        resolve: null,
        reject: null,

        resolve_on_submit: options.resolve_on_submit,
        can_be_closed: options.can_be_closed,

        submit: options.submit,
        submitted: false
    };

    if (!init_dialogs) {
        window.addEventListener('keydown', e => {
            if (e.keyCode == 27 && dialogs.length && current_popup == null) {
                let dlg = dialogs[dialogs.length - 1];

                if (dlg.can_be_closed)
                    dlg.reject();
            }
        });

        init_dialogs = true;
    }

    if (dlg.overlay) {
        dlg.el = document.createElement('main');
        dlg.el.className = 'dialog';
    } else {
        dlg.el = document.createElement('dialog');
    }

    let p = new Promise((resolve, reject) => {
        dlg.update = () => {
            if (dlg != dialogs[dialogs.length - 1])
                return;

            render(html`
                <form class=${dlg.submitted ? 'submitted' : ''}
                      @click=${e => e.stopPropagation()}
                      @invalid=${{ handleEvent: e => handleInvalid(e, dlg), capture: true }}
                      @submit=${wrap(e => handleSubmit(e, dlg))}>
                    ${options.run(dlg.update, () => reject())}
                </form>
            `, dlg.el);
        };

        dlg.resolve = resolve;
        dlg.reject = reject;

        autoFocus(dlg.el);
    });

    p.finally(() => {
        if (options.close != null)
            options.close();

        dialogs = dialogs.filter(it => it != dlg);

        if (dialogs.length) {
            let dlg = dialogs[dialogs.length - 1];
            dlg.update();
        }

        if (dlg.overlay) {
            main();
        } else {
            document.body.removeChild(dlg.el);
        }

        setTimeout(run_func, 0);
    });

    dialogs.push(dlg);
    dlg.update();

    if (!dlg.overlay)
        document.body.appendChild(dlg.el);

    if (options.open != null)
        options.open(dlg.el.children[0]);
    if (options.can_be_closed)
        p.close = () => dlg.reject();

    main();

    if (dlg.el.show != null)
        dlg.el.show();

    return p;
}

function handleInvalid(e, dlg) {
    dlg.submitted = true;
    window.requestAnimationFrame(dlg.update);
}

async function handleSubmit(e, dlg) {
    try {
        let values = [];

        if (dlg.submit != null) {
            let ret = await dlg.submit(e.target.elements);
            values.push(ret);
        }

        dlg.submitted = true;

        if (dlg.resolve_on_submit) {
            dlg.resolve(...values);
        } else {
            window.requestAnimationFrame(dlg.update);
        }
    } catch (err) {
        window.requestAnimationFrame(dlg.update);
        throw err;
    }
}

function autoFocus(el) {
    setTimeout(() => {
        let widget0 = el.querySelector('label > *[autofocus]') ||
                      el.querySelector(`label > input:not(:disabled),
                                        label > select:not(:disabled),
                                        label > textarea:not(:disabled)`);

        if (widget0 != null)
            widget0.focus();
    }, 0);
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

function popup(e, content) {
    closePopup();

    // Prevent default action such as label click if the trigger is inside a label
    e.preventDefault();

    if (!init_popups) {
        window.addEventListener('keydown', e => {
            if (e.keyCode == 27)
                closePopup();
        });

        document.addEventListener('click', e => {
            let popup = Util.findParent(e.target, el => el.classList.contains('popup'));

            if (popup == null)
                closePopup();
        }, { capture: true });

        init_popups = true;
    }

    let el = document.createElement('div');
    el.className = 'popup';

    // Prepare popup content
    let p = new Promise((resolve, reject) => {
        let popup = {
            resolve: resolve,
            reject: reject
        };

        if (typeof content == 'function')
            content = content(() => popup.reject());

        render(html`
            <form @submit=${wrap(e => popup.resolve())}>
                ${content}
            </form>
        `, el);

        current_popup = popup;
    });

    p.finally(() => {
        document.body.removeChild(el);
        current_popup = null;
    });

    // We need it in the DOM for proper size information
    document.body.appendChild(el);

    // Position properly
    {
        let pos = { x: null, y: null };
        let origin = { x: null, y: null };

        if (e.clientX && e.clientY) {
            origin.x = e.clientX - 1;
            origin.y = e.clientY - 1;
        } else {
            let rect = e.target.getBoundingClientRect();

            origin.x = (rect.left + rect.right) / 2;
            origin.y = (rect.top + rect.bottom) / 2;
        }

        pos.x = origin.x;
        pos.y = origin.y;

        if (pos.x > window.innerWidth - el.offsetWidth - 10) {
            pos.x = origin.x - el.offsetWidth;

            if (pos.x < 24) {
                pos.x = Math.min(origin.x, window.innerWidth - el.offsetWidth - 24);
                pos.x = Math.max(pos.x, 24);
            }
        }
        if (pos.y > window.innerHeight - el.offsetHeight - 24) {
            pos.y = origin.y - el.offsetHeight;

            if (pos.y < 24) {
                pos.y = Math.min(origin.y, window.innerHeight - el.offsetHeight - 24);
                pos.y = Math.max(pos.y, 24);
            }
        }

        el.style.left = pos.x + 'px';
        el.style.top = pos.y + 'px';
    }

    return p;
}

function closePopup() {
    if (current_popup != null)
        current_popup.reject();
}

class SelectDirective extends Directive {
    update(part, [value]) {
        window.requestAnimationFrame(() => { part.element.value = value; });
        return noChange;
    }
}

const selectValue = directive(SelectDirective);

class ReorderDirective extends Directive {
    installed = false;

    items = null;
    item = null;
    finalize = null;

    update(part, [items, item, enable, finalize]) {
        let el = part.element;
        let grab = el.querySelector('.grab') || el;

        if (enable == null)
            enable = true;

        if (!this.installed) {
            grab.addEventListener('dragstart', wrap(e => dragStart(e, this.items, this.item)));
            grab.addEventListener('dragover', wrap(e => dragOver(e, this.items, this.item)));
            grab.addEventListener('dragend', wrap(e => dragEnd(e, this.finalize)));

            this.installed = true;
        }

        this.items = items;
        this.item = item;
        this.finalize = finalize;

        grab.setAttribute('draggable', enable ? 'true' : 'false');

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

    e.dataTransfer.dropEffect = 'move';
    e.preventDefault();

    let idx1 = drag_items.indexOf(drag_src);
    let idx2 = drag_items.indexOf(dest);

    if (idx1 == idx2)
        return;

    reorder(drag_items, idx1, idx2);

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
}

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

function tableValues(key, values, default_by = null, default_ascending = null) {
    let order = table_orders[key];
    let filter = table_filters[key];

    if (order == null && default_by != null) {
        setOrder(key, default_by, default_ascending);
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
}

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
    init,

    wrap,
    insist,
    confirm,

    main,
    fullscreen as isFullscreen,
    toggleFullscreen,

    dialog,
    runDialog,
    closeDialog,
    isDialogOpen,

    popup,
    closePopup,

    selectValue,
    reorderItems,

    tableHeader,
    tableFilter,
    tableValues
}
