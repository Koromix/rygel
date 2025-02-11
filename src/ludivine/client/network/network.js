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

import { render, html } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from '../../../web/core/base.js';
import * as UI from '../../../web/flat/ui.js';
import { AppRunner } from '../../../web/core/runner.js';
import { computeAge, computeAgeMonths, dateToString } from '../lib/util.js';
import { ASSETS } from '../../assets/assets.js';
import * as app from '../app.js';
import { NetworkWidget } from './widget.js';

import './network.css';

const DATA_VERSION = 1;
const UNDO_HISTORY = 50;
const EVENT_SIZE = 32;

function NetworkModule(db, test) {
    let self = this;

    // DOM nodes
    let action_elements = {};
    let canvas = null;

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

    // Mode flags
    let anonymous = false;

    Object.defineProperties(this, {
        runner: { get: () => runner, enumerable: true },

        anonymous: { get: () => anonymous, set: value => { anonymous = value; }, enumerable: true }
    });

    this.start = async function() {
        canvas = document.createElement('canvas');
        canvas.className = 'net_canvas';

        runner = new AppRunner(canvas);

        ctx = canvas.getContext('2d');
        mouse_state = runner.mouseState;
        pressed_keys = runner.pressedKeys;

        // Don't burn CPU or battery
        runner.idleTimeout = 1000;

        runner.onUpdate = update;
        runner.onDraw = draw;
        runner.onContextMenu = () => {};
        UI.setRunFunction(runner.busy);

        ctx.imageSmoothingEnabled = true;
        ctx.imageSmoothingQuality = 'high';

        // Load existing world
        {
            let payload = await db.pluck('SELECT payload FROM tests WHERE id = ?', test.id);

            if (payload != null) {
                try {
                    let json = JSON.parse(payload);
                    await load(json);
                } catch (err) {
                    Log.error(err);
                    world = null;
                }
            }

            if (world == null)
                await reset();
        }

        runner.start();
    };

    this.stop = function() {
        runner.stop();
    };

    this.render = function() {
        return html`
            <div class="net_wrapper">
                ${self.actions(1)}
                ${self.actions(3)}
                ${self.actions(9)}
                ${self.actions(7)}

                ${canvas}
            </div>
        `;
    };

    this.hasUnsavedData = function() {
        let unsafe = (save_timer != null);
        return unsafe;
    };

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

            text: 'Sujet',
            subject: subject0.id,
            kind: 'family',

            x: 0,
            y: 0,
            proximity: 0
        };

        await load({
            subjects: [],
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
                redo: []
            };
        }

        if (data.version == null)
            throw new Error('Impossible de migrer les données existantes');

        data = migrateData(data);
        world = data.world;

        undo_actions = data.undo;
        redo_actions = data.redo;

        widget = new NetworkWidget(self, world);
        await widget.init();

        autoSave();
    }

    function serialize() {
        let data = {
            version: DATA_VERSION,

            world: world,

            undo: undo_actions,
            redo: redo_actions
        };

        return data;
    }

    function update() {
        if (world == null)
            return;

        now = (new Date).valueOf();

        // Global shortcuts
        if (pressed_keys.debug == -1)
            debug = !debug;
        if (pressed_keys.ctrl > 0 && pressed_keys.z == -1)
            rewind(undo_actions, redo_actions);
        if (pressed_keys.ctrl > 0 && pressed_keys.y == -1)
            rewind(redo_actions, undo_actions);

        self.actions(9, html`
            <button type="button" title="Annuler la dernière modification"
                    ?disabled=${!undo_actions.length}
                    @click=${UI.wrap(e => rewind(undo_actions, redo_actions))}>
                <img src=${ASSETS['ui/undo']} alt="" />
                <span>Annuler</span>
            </button>
            <button type="button" title="Rétablir la dernière action annulée"
                    ?disabled=${!redo_actions.length}
                    @click=${UI.wrap(e => rewind(redo_actions, undo_actions))}>
                <img src=${ASSETS['ui/redo']} alt="" />
                <span>Rétablir</span>
            </button>
        `);

        self.actions(3, html`
            <button type="button" title="Mode plein écran"
                    @click=${UI.wrap(app.toggleFullScreen)}>
                <img src=${ASSETS['ui/fullscreen']} alt="" />
                <span>${app.isFullScreen ? 'Quitter le plein écran' : 'Plein écran'}</span>
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

    function saveFile() {
        let data = serialize();
        let json = JSON.stringify(data, null, 4);

        let blob = new Blob([json]);

        Util.saveFile(blob, 'world.json');
    }

    async function loadFile() {
        let file = await Util.loadFile();
        let json = await file.text();

        let data = JSON.parse(json);

        await load(data);
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

            let mtime = (new Date).valueOf();
            let data = serialize();
            let json = JSON.stringify(data);

            db.exec(`UPDATE tests SET status = 'draft', mtime = ?, payload = ?
                     WHERE id = ?`, mtime, json, test.id);
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
    }

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

            ctx.font = '14px Open Sans';
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

            ctx.font = '12px Open Sans';
            runner.text(-10, -10, text, { color: 'red', background: 'white' });
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
    data.version = DATA_VERSION;

    return data;
}

export { NetworkModule }
