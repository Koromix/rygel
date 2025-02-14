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

import { render, html, noChange, directive, Directive } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from '../../web/core/base.js';
import { deploy, sos } from '../assets/ludivine.js';
import { ASSETS } from '../assets/assets.js';
import * as app from './app.js';

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

let log_entries = [];

let root_el = null;
let body_el = null;
let fullscreen = false;

let init_dialogs = false;
let dialogs = [];

let drag_items = null;
let drag_src = null;
let drag_restore = null;

let table_orders = {};
let table_filters = {};

function init(run) {
    Log.pushHandler(notifyHandler);

    // Detect fullscreen exit
    document.body.addEventListener('fullscreenchange', () => {
        if (fullscreen && document.fullscreenElement != document.body) {
            fullscreen = false;
            run();
        }
    });

    run_func = run;
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
                <div class="tabbar">
                    <a class="active">${action}</a>
                </div>

                <div class="tab">
                    <div class="box">${T.confirm_not_reversible}</div>

                    <div class="actions">
                        <button type="button" class="secondary" @click=${wrap(close)}>${T.cancel}</button>
                        <button type="submit" class="danger">${T.confirm}</button>
                    </div>
                </div>
            `,

            submit: () => func()
        });
    }
}

function body(content = null) {
    if (root_el == null) {
        root_el = document.createElement('div');
        root_el.id = 'root';
        body_el = document.createElement('main');
        body_el.className = 'primary';

        document.body.appendChild(root_el);
    }

    let el = dialogs.length ? dialogs[dialogs.length - 1].el : body_el;

    if (fullscreen) {
        render(el, root_el);
    } else {
        render(html`
            <div id="deploy" @click=${deploy}></div>

            <nav id="top">
                <menu>
                    <a id="logo" href=${ENV.urls.static}><img src=${ASSETS['main/logo']} alt="Logo Lignes de Vie" /></a>
                    <li><a href=${ENV.urls.static} style="margin-left: 0em;">Accueil</a></li>
                    <li><a href=${ENV.urls.app} class="active" style="margin-left: 0em;">Participer</a></li>
                    <li><a href=${ENV.urls.static + '/etudes'} style="margin-left: 0em;">Études</a></li>
                    <li><a href=${ENV.urls.static + '/livres'} style="margin-left: 0em;">Ressources</a></li>
                    <li><a href=${ENV.urls.static + '/detente'} style="margin-left: 0em;">Détente</a></li>
                    <li><a href=${ENV.urls.static + '/equipe'} style="margin-left: 0em;">Qui sommes-nous ?</a></li>
                    <div style="flex: 1;"></div>
                    <img class="picture" src=${app.pictureURL()} alt="" />
                </menu>
            </nav>

            ${el}

            <footer>
                <div>Lignes de Vie © 2024</div>
                <img src=${ASSETS['main/footer']} alt="" width="79" height="64">
                <div style="font-size: 0.8em;">
                    <a href="mailto:lignesdevie@cn2r.fr" style="font-weight: bold; color: inherit;">contact@ldv-recherche.fr</a>
                </div>
            </footer>

            ${app.isLogged() ? html`<a id="sos" @click=${wrap(e => sos(event))}></a>` : ''}
        `, root_el);
    }

    body_el.classList.toggle('fullscreen', fullscreen);

    if (content != null)
        render(content, body_el);
}

async function toggleFullscreen() {
    fullscreen = !fullscreen;

    try {
        if (fullscreen) {
            document.body.requestFullscreen();
        } else {
            render('', root_el);
            document.exitFullscreen();
        }
    } catch (err) {
        console.error(err);
    }

    await run_func();
}

function dialog(options = {}) {
    options = Object.assign({}, options);

    if (options.resolve_on_submit == null)
        options.resolve_on_submit = true;
    if (options.can_be_closed == null)
        options.can_be_closed = true;

    let dlg = {
        el: null,

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
            if (e.keyCode == 27 && dialogs.length) {
                let dlg = dialogs[dialogs.length - 1];

                if (dlg.can_be_closed)
                    dlg.reject();
            }
        });

        init_dialogs = true;
    }

    dlg.el = document.createElement('main');
    dlg.el.className = 'dialog';

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

        window.requestAnimationFrame(() => {
            let widget0 = dlg.el.querySelector('label > *[autofocus]') ||
                          dlg.el.querySelector(`label > input:not(:disabled),
                                                label > select:not(:disabled),
                                                label > textarea:not(:disabled)`);

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

        dialogs = dialogs.filter(it => it != dlg);

        if (dialogs.length) {
            let dlg = dialogs[dialogs.length - 1];
            dlg.update();
        }

        setTimeout(run_func, 0);

        body();
    });

    dialogs.push(dlg);
    dlg.update();

    if (options.open != null)
        options.open(dlg.el.children[0]);
    if (options.can_be_closed)
        p.close = () => dlg.reject();

    body();

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

function tableHeader(key, title, by, func = null) {
    if (func == null) {
        if (typeof by == 'function') {
            func = by;
        } else {
            func = value => value[by];
        }
    }
    by = String(by);

    let info = table_orders[key];
    let ascending = (info != null && info.by == by) ? info.ascending : null;

    return html`
        <th class="item" @click=${e => { sortTable(key, by, func); run_func(); }}>
            ${title}
            <div class="arrows">
                <span class=${'up' + (ascending === false ? ' active' : '')}></span>
                <span class=${'down' + (ascending === true ? ' active' : '')}></span>
            </div>
        </th>
    `;
}

function sortTable(key, by, func, ascending = null) {
    if (ascending == null) {
        let order = table_orders[key];

        if (order != null && order.by == by) {
            ascending = !order.ascending;
        } else {
            ascending = true;
        }
    }

    let order = {
        by: by,
        func: func,
        ascending: ascending,

        comparator: null,
        language: null
    };

    order.language = document.documentElement.lang || 'en';
    order.comparator = makeComparator(order);

    table_orders[key] = order;
}

function tableFilter(key, ...keys) {
    let filter = table_filters[key];

    return html`<input type="search" placeholder=${T.filter + '...'} style="width: 10em;"
                       value=${(filter != null) ? filter.str : ''}
                       @input=${e => { filterTable(key, e.target.value, keys); run_func(); }} />`;
};

function filterTable(key, str, keys) {
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

function tableValues(key, values, by = null, func = null) {
    let order = table_orders[key];
    let filter = table_filters[key];

    if (order == null && by != null) {
        if (func == null) {
            if (typeof by == 'function') {
                func = by;
            } else {
                func = value => value[by];
            }
        }
        by = String(by);

        sortTable(key, by, func);
        order = table_orders[key];
    }
    if (order != null) {
        let lang = document.documentElement.lang || 'en';

        if (lang != order.language) {
            order.language = lang;
            order.comparator = makeComparator(order);
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

function makeComparator(order) {
    let collator = new Intl.Collator(order.language, {
        numeric: true,
        ignorePunctuation: true,
        sensitivity: 'base'
    });

    let comparator = (obj1, obj2) => {
        let value1 = order.func(obj1);
        let value2 = order.func(obj2);

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

    body,
    fullscreen as isFullscreen,
    toggleFullscreen,

    dialog,
    runDialog,
    closeDialog,
    isDialogOpen,

    selectValue,
    reorderItems,

    tableHeader,
    tableFilter,
    tableValues,
}
