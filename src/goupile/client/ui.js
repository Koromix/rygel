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

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, LruMap, Mutex } from '../../web/core/base.js';
import { FormState, FormModel, FormBuilder } from './form.js';

import './ui.css';

let small_threshold = 900;
let mobile_threshold = 580;

let menu_render;
let panels = new Map;
let active_panels = [null, null];

let expanded = {};
let new_expanded;

let dialogs = {
    prev: null,
    next: null
};
dialogs.prev = dialogs;
dialogs.next = dialogs;

let log_el;
let log_entries = [];

function init() {
    log_el = document.createElement('div');
    log_el.id = 'ui_log';
    document.body.appendChild(log_el);

    Log.pushHandler(notifyHandler);

    window.addEventListener('resize', () => {
        adaptToViewport();
        if (!document.body.classList.contains('gp_loading'))
            draw();
    });
    adaptToViewport();

    document.addEventListener('click', closeOldDialogs);
}

function adaptToViewport(keep_panel = null) {
    let small = window.innerWidth < small_threshold;
    let mobile = window.innerWidth < mobile_threshold;

    document.documentElement.classList.toggle('small', small);
    document.documentElement.classList.toggle('mobile', mobile);
}

async function draw() {
    let render_main = true;

    // Run dialog functions
    {
        let it = dialogs.next;

        while (it !== dialogs) {
            it.render();

            if (it.type === 'screen') {
                render_main = false;
                break;
            }

            it = it.next;
        }
    }

    // Render main screen
    if (render_main) {
        new_expanded = {};

        if (active_panels[0] == null && active_panels[1] == null) {
            let panel = panels.values().next().value;
            active_panels[panel.position] = panel;
        }

        render(html`
            ${menu_render != null ? menu_render() : ''}
            <main id="ui_panels">
                ${isPanelActive(0) ? await active_panels[0].render() : ''}
                ${isPanelActive(1) ? await active_panels[1].render() : ''}
            </main>
        `, document.querySelector('#ui_main'));

        expanded = new_expanded;
    } else {
        render('', document.querySelector('#ui_main'));
    }

    adjustLogLayer();
    adjustLoadingSpinner();
}

function setMenu(func) {
    menu_render = func;
}

function createPanel(key, position, func) {
    let panel = {
        key: key,
        position: position,
        render: func
    };

    panels.set(key, panel);
}

function togglePanel(key, enable = null) {
    if (typeof key == 'number') {
        if (enable == null)
            enable = (active_panels[key] == null);

        if (enable) {
            if (active_panels[key] == null) {
                for (let panel of panels.values()) {
                    if (panel.position === key) {
                        active_panels[key] = panel;
                        break;
                    }
                }
            }
        } else {
            active_panels[key] = null;
        }
    } else {
        if (enable == null)
            enable = !isPanelActive(key);

        let panel = panels.get(key);

        active_panels[panel.position] = enable ? panel : null;
    }
}

function getPanels() {
    let keys = active_panels.filter(panel => panel != null).map(panel => panel.key);
    return keys;
}

function setPanels(keys) {
    keys = keys.filter(key => panels.has(key));

    if (!keys.length)
        return;

    active_panels = [null, null];

    for (let key of keys) {
        let panel = panels.get(key);
        active_panels[panel.position] = panel;
    }
}

function isPanelActive(key) {
    let panel = (typeof key == 'number') ? active_panels[key] : panels.get(key);

    if (panel != null) {
        if (panel == active_panels[0])
            return true;
        if (active_panels[0] == null || allowTwoPanels()) {
            if (panel == active_panels[1])
                return true;
        }
    }

    return false;
}

function allowTwoPanels() {
    let allow = window.innerWidth >= small_threshold;
    return allow;
}

function hasTwoPanels() {
    if (!allowTwoPanels())
        return false;

    let dual = (active_panels[0] != null && active_panels[1] != null);
    return dual;
}

function dialog(e, title, options, func) {
    if (e != null) {
        e.stopPropagation();

        if (options.type == null) {
            if (document.documentElement.classList.contains('small')) {
                options.type = 'modal';
            } else {
                options.type = 'popup';
            }
        }
    } else if (options.type == null) {
        options.type = (title != null) ? 'modal' : 'screen';
    }

    return runDialog(e, title, options, func);
}

function runDialog(e, title, options, func) {
    if (e != null)
        closeOldDialogs(e);

    return new Promise((resolve, reject) => {
        let dialog = {
            prev: dialogs.prev,
            next: dialogs,

            title: title,
            type: options.type,
            fixed: !!options.fixed,
            el: document.createElement('div'),
            state: new FormState,
            render: () => buildDialog(dialog, e, func),
            closeable: options.type !== 'screen',

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
        dialog.el.className = `ui_dialog ${options.type}`;
        if (options.type !== 'screen') {
            dialog.el.addEventListener('keydown', e => {
                if (e.keyCode == 27)
                    dialog.reject(null);
            });
        }

        dialog.el.style.zIndex = 9999999;
        adjustLogLayer();

        // Show it!
        document.querySelector('#ui_root').appendChild(dialog.el);
        dialog.render();

        if (options.type === 'screen')
            render('', document.querySelector('#ui_main'));

        document.body.classList.remove('gp_loading');
    });
}

function closeOldDialogs(e) {
    // Close dialogs
    {
        let it = dialogs.next;

        while (it !== dialogs) {
            if (it.closeable) {
                if (it.type === 'popup') {
                    let target = e.target.parentNode;

                    for (;;) {
                        if (target == null || target.classList == null) {
                            it.reject(null);
                            break;
                        } else if (target.classList.contains('ui_dialog') && target.classList.contains('popup')) {
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

    // Close dropdown menus
    {
        let els = document.querySelectorAll('.ui_toolbar > .drop.active');
        let target = Util.findParent(e.target, el => el.classList.contains('drop'));

        for (let el of els) {
            if (el !== target)
                el.classList.remove('active');
        }
    }

    adjustLogLayer();
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
        builder.action(model.actions.length ? T.cancel : T.close, {}, () => dialog.reject(null));

    render(html`
        <div>
            ${dialog.title ? html`<div class="ui_legend">${dialog.title}</div>` : ''}
            <form @submit=${e => e.preventDefault()}>
                ${model.render()}
            </form>
        </div>
    `, dialog.el);

    // We need to know popup width and height
    let give_focus = !dialog.el.classList.contains('active');
    dialog.el.style.visibility = 'hidden';
    dialog.el.classList.add('active');
    if (dialog.fixed)
        dialog.el.classList.add('fixed');

    // Try different popup positions
    if (dialog.type === 'popup' && e != null) {
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

    document.querySelector('#ui_root').removeChild(dialog.el);
    dialog.el = null;

    adjustLoadingSpinner();
}

function isDialogOpen() {
    return (dialogs.prev != dialogs);
}

function refreshDialog() {
    if (dialogs.prev == dialogs)
        return;

    let dlg = dialogs.prev;
    dlg.render();
}

function adjustLoadingSpinner() {
    let empty = (dialogs.next === dialogs && active_panels[0] == null
                                          && active_panels[1] == null);
    document.body.classList.toggle('gp_loading', empty);
}

function wrap(func) {
    return async e => {
        let target = e.target;

        if (target.disabled || target.classList.contains('disabled') ||
                               target.classList.contains('ui_busy'))
            return;

        try {
            target.classList.add('ui_busy');
            if (target.disabled != null)
                target.disabled = true;

            await func(e);
        } catch (err) {
            if (err != null) {
                Log.error(err);
                throw err;
            }
        } finally {
            target.classList.remove('ui_busy');
            if (target.disabled != null)
                target.disabled = false;
        }
    };
}

function confirm(e, msg, action, func = null) {
    let title = `${action} (${T.confirmation.toLowerCase()})`;

    return dialog(e, title, {}, (d, resolve, reject) => {
        d.output(msg);

        d.action(action, { disabled: !d.isValid() }, async e => {
            try {
                if (func != null)
                    await func(e);
                resolve();
            } catch (err) {
                Log.error(err);
                d.refresh();
            }
        });
    });
}

function deployMenu(e) {
    let el = Util.findParent(e.target, el => el.classList.contains('drop'));

    if (el.classList.contains('active')) {
        if (!e.ctrlKey)
            el.classList.remove('active');
    } else {
        closeOldDialogs(e);
        el.classList.add('active');
    }

    let expands = el.querySelectorAll('.expand');
    for (let i = 0; i < expands.length; i++) {
        let exp = expands[i];
        exp.classList.toggle('active', i === expands.length - 1);
    }

    e.stopPropagation();
}

function expandMenu(key, title, active, content) {
    if (expanded.hasOwnProperty(key))
        active = expanded[key];
    new_expanded[key] = active;

    return html`
        <div class=${active ? 'expand active' : 'expand'} data-key=${key}>
            <button @click=${handleMenuClick}>${title}</button>
            <div>${content}</div>
        <div>
    `;
}

function handleMenuClick(e) {
    let el = Util.findParent(e.target, el => el.classList.contains('expand'));
    let key = el.dataset.key;

    expanded[key] = el.classList.toggle('active');

    e.stopPropagation();
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
    render(log_entries.map(entry => {
        let msg = (entry.msg instanceof Error) ? entry.msg.message : entry.msg;

        if (typeof msg === 'string')
            msg = msg.split('\n').map(line => [line, html`<br/>`]);

        if (entry.type === 'progress') {
            return html`<div class="progress">
                <div class="ui_log_spin"></div>
                ${msg}
            </div>`;
        } else {
            return html`<div class=${entry.type} @click=${e => entry.close()}>
                <button class="ui_log_close">X</button>
                ${msg}
            </div>`;
        }
    }), log_el);
}

function adjustLogLayer() {
    // Adjust log z-index. We need to do it dynamically because we want it to show
    // above screen and modal dialogs (which show up above eveyerthing else), but
    // below menu bar dropdown menus.
    log_el.style.zIndex = (dialogs.next != dialogs) ? 999999999 : 99999;
}

export {
    init,
    draw,

    setMenu,
    createPanel,
    togglePanel,
    getPanels,
    setPanels,
    isPanelActive,
    allowTwoPanels,
    hasTwoPanels,

    dialog,
    isDialogOpen,
    refreshDialog,

    wrap,
    confirm,

    deployMenu,
    expandMenu
}
