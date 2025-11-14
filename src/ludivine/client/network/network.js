// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from 'src/core/web/base/base.js';
import { AppRunner } from 'src/core/web/base/runner.js';
import { deflate, inflate } from '../core/misc.js';
import { NetworkWidget } from './widget.js';
import { ASSETS } from '../../assets/assets.js';

import './network.css';

const DATA_VERSION = 1;
const UNDO_HISTORY = 50;
const EVENT_SIZE = 32;

function NetworkModule(app, study, page) {
    let self = this;

    const UI = app.UI;
    const db = app.db;

    // DOM nodes
    let wrapper = null;
    let canvas = null;
    let action_elements = {};

    // Platform
    let runner = null;
    let ctx = null;
    let mouse_state = null;
    let pressed_keys = null;

    // UI state
    let debug = false;
    let tooltip = null;

    // Data
    let world = null;

    // General state
    let widget = null;
    let now = null;

    // Undo and redo
    let undo_actions = [];
    let redo_actions = [];
    let save_timer = null;

    // Interactive guidance
    let known_guides = new Set;
    let overlay = null;
    let guide_count = 0;

    // Mode flags
    let anonymous = false;

    Object.defineProperties(this, {
        runner: { get: () => runner, enumerable: true },

        anonymous: { get: () => anonymous, set: value => { anonymous = value; }, enumerable: true }
    });

    this.run = async function() {
        if (runner != null)
            return;

        wrapper = document.createElement('div');
        wrapper.className = 'net_wrapper';
        canvas = document.createElement('canvas');
        canvas.className = 'net_canvas';

        wrapper.addEventListener('click', e => {
            if (overlay != null) {
                let ignore = !!Util.findParent(e.target, el => el.classList.contains('net_actions'));

                if (ignore)
                    e.stopPropagation();
            }
        }, { capture: true });

        runner = new AppRunner(canvas);
        ctx = canvas.getContext('2d');
        mouse_state = runner.mouseState;
        pressed_keys = runner.pressedKeys;

        ctx.imageSmoothingEnabled = true;
        ctx.imageSmoothingQuality = 'high';

        new ResizeObserver(syncSize).observe(wrapper);
        syncSize();

        // Don't burn CPU or battery
        runner.idleTimeout = 1000;

        runner.onUpdate = update;
        runner.onDraw = draw;
        runner.onContextMenu = () => {};

        // Load existing world
        {
            let payload = await db.pluck('SELECT payload FROM tests WHERE study = ? AND key = ?', study.id, page.key);

            if (payload != null) {
                try {
                    let buf = await inflate(payload);
                    let json = (new TextDecoder).decode(buf);
                    let obj = JSON.parse(json);

                    await load(obj);
                } catch (err) {
                    Log.error(err);
                    world = null;
                }
            }

            if (world == null)
                await reset();
        }

        self.render();

        runner.start();
    };

    this.stop = function() {
        runner.stop();
    };

    this.render = function(section) {
        runner.busy();

        render(html`
            ${overlay}
            ${canvas}
            ${Util.mapRange(1, 10, idx => self.actions(idx))}
        `, wrapper);

        return wrapper;
    };

    this.hasUnsavedData = function() {
        let unsafe = (save_timer != null);
        return unsafe;
    };

    function syncSize() {
        let rect = wrapper.getBoundingClientRect();
        if (!rect.width && !rect.height)
            return;
        runner.resize(rect.width, rect.height, window.devicePixelRatio);
    }

    async function reset() {
        world = null;

        let subject0 = {
            id: Util.makeULID(),

            name: '',
            birthdate: null,
            gender: 'H',

            last_kind: null
        };

        let person0 = {
            id: Util.makeULID(),

            subject: subject0.id,
            kind: 'family',
            quality: 0,

            x: 0,
            y: 0,
            proximity: 0
        };

        await load({
            subjects: [subject0],
            persons: [person0],
            links: [],
            levels: [
                { id: Util.makeULID(), radius: 0.1 },
                { id: Util.makeULID(), radius: 1.0 },
                { id: Util.makeULID(), radius: 0.7 },
                { id: Util.makeULID(), radius: 0.4 }
            ]
        });
    }

    async function load(data) {
        if (data.world == null) {
            data = {
                version: DATA_VERSION,

                world: data,

                undo: [],
                redo: [],
                guides: []
            };
        }

        if (data.version == null)
            throw new Error('Impossible de migrer les données existantes');

        data = migrateData(data);
        world = data.world;

        undo_actions = data.undo;
        redo_actions = data.redo;
        known_guides = new Set(data.guides);

        widget = new NetworkWidget(app, self, world);
        await widget.init();
    }

    function serialize() {
        let data = {
            version: DATA_VERSION,

            world: world,

            undo: undo_actions,
            redo: redo_actions,
            guides: Array.from(known_guides)
        };

        return data;
    }

    function update() {
        if (world == null)
            return;

        now = (new Date).valueOf();

        // Global shortcuts
        if (pressed_keys.tab == -1)
            debug = !debug;
        if (pressed_keys.ctrl > 0 && pressed_keys.z == -1)
            rewind(undo_actions, redo_actions);
        if (pressed_keys.ctrl > 0 && pressed_keys.y == -1)
            rewind(redo_actions, undo_actions);

        self.actions(9, html`
            <button type="button" class=${UI.isFullscreen ? 'secondary active' : 'secondary'}
                    @click=${UI.wrap(e => UI.toggleFullscreen())}>
                <img src=${ASSETS['ui/fullscreen']} alt="" />
                <span>${UI.isFullscreen ? 'Quitter le plein écran' : 'Plein écran'}</span>
            </button>
            <div style="height: 20px;"></div>
            <button type="button" class="secondary" title="Annuler la dernière modification"
                    ?disabled=${!undo_actions.length}
                    @click=${UI.wrap(e => rewind(undo_actions, redo_actions))}>
                <img src=${ASSETS['ui/undo']} alt="" />
                <span>Annuler</span>
            </button>
            <button type="button" class="secondary" title="Rétablir la dernière action annulée"
                    ?disabled=${!redo_actions.length}
                    @click=${UI.wrap(e => rewind(redo_actions, undo_actions))}>
                <img src=${ASSETS['ui/redo']} alt="" />
                <span>Rétablir</span>
            </button>
        `);

        self.actions(3, html`
            <button type="button" class="confirm" @click=${UI.wrap(finalize)}>
                <img src=${ASSETS['ui/confirm']} alt="" />
                <span style="display: inline;">Finaliser</span>
            </button>
        `);

        // Run widget code
        widget.update();

        // Handle tooltip
        if (tooltip != null) {
            tooltip.x = mouse_state.x;
            tooltip.y = mouse_state.y;

            if (runner.updateCounter > tooltip.update + 30) {
                tooltip.opacity -= 0.03;
                runner.busy();
            }
            if (tooltip.opacity < 0)
                tooltip = null;
        }
    }

    async function finalize() {
        let data = serialize();

        let results = Object.assign({}, data.world);

        // Anonymize subjects
        results.subjects = results.subjects.map(subject => {
            subject = Object.assign({}, subject);
            delete subject.name;
            return subject;
        });

        await app.finalizeTest(page, data, results);
    }

    this.registerPush = function(array, ref, auto = false) {
        let undo = {
            type: 'push',
            more: auto,

            array: findObjectPath(world, array),
            ref: ref.id
        };

        addUndo(undo);
    };

    this.registerRemove = function(array, idx, obj, auto = false) {
        let undo = {
            type: 'remove',
            more: auto,

            array: findObjectPath(world, array),
            idx: idx,
            copy: obj
        };

        addUndo(undo);
    };

    this.registerChange = function(objects, auto = false, aggregate = false) {
        objects = Array.isArray(objects) ? objects.slice() : [objects];

        if (aggregate && undo_actions.length) {
            let prev = undo_actions[undo_actions.length - 1];

            if (prev.type == 'change' && prev.aggregate) {
                let same = (prev.references.length == objects.length &&
                            objects.every((obj, idx) => obj.id == prev.references[idx]));

                if (same) {
                    redo_actions.length = 0;
                    autoSave();

                    return;
                }
            }
        }

        let undo = {
            type: 'change',
            more: auto,

            aggregate: aggregate,
            references: objects.map(obj => obj.id),
            copies: objects.map(obj => Object.assign({}, obj))
        };

        addUndo(undo);
    };

    function findObjectPath(root, obj, prefix = 'world') {
        if (obj == null)
            return null;

        for (let key in root) {
            let path = `${prefix}.${key}`;
            let value = root[key];

            if (obj === value) {
                return path;
            } else if (Util.isPodObject(value)) {
                let ret = findObjectPath(value, obj, path);
                if (ret != null)
                    return ret;
            } else if (Array.isArray(value)) {
                for (let i = 0; i < value.length; i++) {
                    let ret = findObjectPath(value[i], obj, `${path}[${i}]`);
                    if (ret != null)
                        return ret;
                }
            }
        }

        return null;
    }

    function addUndo(undo) {
        undo_actions.push(undo);

        while (undo_actions.length > UNDO_HISTORY) {
            let remove = 1;

            while (remove < undo_actions.length && undo_actions[remove].more)
                remove++;

            undo_actions.splice(0, remove);
        }

        redo_actions.length = 0;

        autoSave();
    }

    function rewind(from, to) {
        if (!from.length)
            return;

        let prev_len = from.length;

        do {
            let action = from.pop();

            switch (action.type) {
                case 'push': {
                    let array = resolveObjectPath(action.array);
                    let ref = resolveObjectID(world, action.ref);

                    let idx = array.indexOf(ref);
                    array.splice(idx, 1);

                    let reverse = {
                        type: 'remove',
                        more: false,

                        array: action.array,
                        idx: idx,
                        copy: ref
                    };

                    to.push(reverse);
                } break;

                case 'remove': {
                    let array = resolveObjectPath(action.array);

                    array.splice(action.idx, 0, action.copy);

                    let reverse = {
                        type: 'push',
                        more: false,

                        array: action.array,
                        ref: action.copy.id
                    };

                    to.push(reverse);
                } break;

                case 'change': {
                    let objects = action.references.map(id => resolveObjectID(world, id));

                    let reverse = {
                        type: 'change',
                        more: false,

                        aggregate: false,
                        references: action.references,
                        copies: objects.map(obj => Object.assign({}, obj))
                    };

                    for (let i = 0; i < objects.length; i++)
                        Object.assign(objects[i], action.copies[i]);

                    to.push(reverse);
                }
            }

            if (!action.more)
                break;
        } while (from.length);

        for (let i = 1; i < prev_len - from.length; i++)
            to[to.length - i].more = true;

        autoSave();
    }

    function resolveObjectPath(path) {
        if (typeof path != 'string' || !path.match(/^world(\.[a-zA-Z][a-zA-Z_0-9]+(\[[0-9]+\])?)+$/))
            throw new Error('Unsafe named reference in serialized history');

        let func = new Function('world', 'return ' + path);
        let value = func(world);

        if (value == null)
            throw new Error('Failed to restore named reference');

        return value;
    }

    function resolveObjectID(root, id) {
        if (Util.isPodObject(root)) {
            if (root.id == id)
                return root;

            for (let key in root) {
                let ret = resolveObjectID(root[key], id);
                if (ret != null)
                    return ret;
            }
        } else if (Array.isArray(root)) {
            for (let it of root) {
                let ret = resolveObjectID(it, id);
                if (ret != null)
                    return ret;
            }
        }

        return null;
    }

    function autoSave() {
        if (save_timer != null)
            clearTimeout(save_timer);

        save_timer = setTimeout(() => {
            save_timer = null;

            let data = serialize();
            app.saveTest(page, data);
        }, 1000);
    }

    this.actions = function(align, content = null) {
        let el = action_elements[align];

        if (el == null) {
            el = document.createElement('div');
            el.className = 'net_actions align' + align;

            action_elements[align] = el;
        }

        if (content != null)
            render(content, el);

        return el;
    };

    this.tooltip = function(text, options = {}) {
        if (!text)
            return;

        let id = options.id || text;

        if (tooltip == null || id != tooltip.id) {
            tooltip = {
                id: id,

                start: runner.updateCounter,
                text: text,
                opacity: 0,

                update: null,
                x: mouse_state.x,
                y: mouse_state.y
            };
        }

        tooltip.update = runner.updateCounter;

        if (tooltip.update > tooltip.start + 30) {
            tooltip.opacity = Math.min(1, tooltip.opacity + 0.03);
            runner.busy();
        }
    };

    this.showGuide = async function(key, text, options = {}) {
        if (overlay != null)
            return;
        if (known_guides.has(key))
            return;

        let highlight = options.highlight;

        if (typeof highlight == 'string')
            highlight = wrapper.querySelectorAll(highlight);
        if (highlight == null)
            highlight = [];

        for (let el of highlight)
            el.classList.add('highlight');
        wrapper.classList.add('guide');

        await new Promise((resolve, reject) => {
            let left = (guide_count % 2 == 0);

            if (left) {
                overlay = html`
                    <div class="help">
                        <img src=${ASSETS['pictures/help1']} alt="" />
                        <div>
                            ${text}
                            <div class="actions">
                                <button type="button" class="secondary small" @click=${resolve}>C'est compris</button>
                            </div>
                        </div>
                    </div>
                `;
            } else {
                overlay = html`
                    <div class="help right">
                        <div>
                            ${text}
                            <div class="actions">
                                <button type="button" class="secondary small" @click=${resolve}>C'est compris</button>
                            </div>
                        </div>
                        <img src=${ASSETS['pictures/help2']} alt="" />
                    </div>
                `;
            }

            self.render();
        });

        guide_count++;

        overlay = null;
        self.render();

        for (let el of highlight)
            el.classList.remove('highlight');
        wrapper.classList.remove('guide');

        known_guides.add(key);
    };

    this.isGuideNeeded = function(key) {
        let known = known_guides.has(key);
        return !known;
    };

    function draw() {
        if (world == null)
            return;

        ctx.clearRect(0, 0, canvas.width, canvas.height);

        // Draw widget
        widget.draw();

        if (debug)
            drawDebug();

        if (tooltip != null) {
            ctx.save();

            ctx.font = (14 * window.devicePixelRatio) + 'px Open Sans';
            ctx.globalAlpha = tooltip.opacity;

            let delta = (mouse_state.x < canvas.width - canvas.width / 4) ? 20 : -20;
            let align = (delta >= 0) ? 4 : 6;

            runner.text(tooltip.x + delta, tooltip.y, tooltip.text, {
                align: align,
                background: 'black',
                color: 'white'
            });

            ctx.restore();
        }
    }

    function drawDebug() {
        ctx.save();

        // FPS counter
        {
            let text = `FPS : ${(1000 / runner.frameTime).toFixed(0)} (${runner.frameTime.toFixed(1)} ms)` +
                       ` | Update : ${runner.updateTime.toFixed(1)} ms | Draw : ${runner.drawTime.toFixed(1)} ms`;

            ctx.font = (18 * window.devicePixelRatio) + 'px Open Sans';
            runner.text(canvas.width / 2, 10, text, { align: 8, color: 'red', background: 'white' });
        }

        ctx.restore();
    }
}

function computeDistance(p1, p2) {
    let dx = p1.x - p2.x;
    let dy = p1.y - p2.y;

    return Math.sqrt(dx * dx + dy * dy);
}

function isInsideRect(pos, rect) {
    let inside = (pos.x >= rect.left &&
                  pos.x <= rect.left + rect.width &&
                  pos.y >= rect.top &&
                  pos.y <= rect.top + rect.height);
    return inside;
}

function migrateData(data) {
    let world = data.world;

    data.world = world;
    if (data.version != DATA_VERSION) {
        data.undo = [];
        data.redo = [];
    }
    if (data.guides == null)
        data.guides = [];
    data.version = DATA_VERSION;

    return data;
}

export { NetworkModule }
