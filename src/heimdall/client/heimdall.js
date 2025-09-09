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

import { render, html, live, unsafeHTML } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, HttpError } from '../../web/core/base.js';
import { AppRunner } from '../../web/core/runner.js';

import en from '../i18n/en.json';
import fr from '../i18n/fr.json';

import '../../../vendor/opensans/OpenSans.css';
import './heimdall.css';

const ROW_HEIGHT = 40;
const PLOT_HEIGHT = 80;
const EVENT_WIDTH = 36;
const EVENT_HEIGHT = 32;
const PERIOD_HEIGHT = 4;
const MERGE_TRESHOLD = 24;
const SPACE_BETWEEN_ENTITIES = 8;

const DEFAULT_SETTINGS = {
    tree: 240,
    interpolation: 'linear',
    view: ''
};

// Assets
let languages = {};

// DOM nodes
let dom = {
    main: null,
    config: null
};
let canvas = null;

// Render and input API
let runner = null;
let ctx = null;
let mouse_state = null;
let pressed_keys = null;

// User settings
let settings = Object.assign({}, DEFAULT_SETTINGS);
let show_debug = false;
let save_timer = null;

// State
let world = null;
let layout = {
    tree: null,
    main: null,
    time: null
};
let position = {
    entity: 0,
    x: 0,
    y: 0,
    zoom: 0
};
let interaction = null;

// Draw data
let rows = [];

// ------------------------------------------------------------------------
// Init
// ------------------------------------------------------------------------

async function load(prefix, lang, progress = null) {
    languages.en = en;
    languages.fr = fr;

    Util.initLocales(languages, lang);

    if (window.createImageBitmap == null)
        throw new Error('ImageBitmap support is not available (old browser?)');

    let project = window.location.pathname.substr(1);
    await fetchProject(project);

    // Keep this around for gesture emulation on desktop
    if (false) {
        let script = document.createElement('script');

        script.onload = () => TouchEmulator();
        script.src = BUNDLES['touch-emulator.js'];

        document.head.appendChild(script);
    }
}

async function fetchProject(project) {
    let url = Util.pasteURL('/api/data', { project: project });
    let json = await Net.get(url);

    world = {
        project: project,
        views: new Map(json.views.map(view => [view.name, view])),
        entities: json.entities,

        start: Number.MAX_SAFE_INTEGER,
        end: Number.MIN_SAFE_INTEGER
    };

    for (let view of json.views)
        view.expand = new Set;

    for (let entity of json.entities) {
        world.start = Math.min(world.start, entity.start);
        world.end = Math.max(world.end, entity.end);
    }
}

async function start(root) {
    render(html`
        <div class="hm_main">
            <canvas class="hm_canvas"></canvas>
            <div class="hm_config"></div>
        </div>
    `, root);
    dom.main = root.querySelector('.hm_main');
    dom.config = root.querySelector('.hm_config');
    canvas = root.querySelector('.hm_canvas');

    runner = new AppRunner(canvas);
    ctx = runner.ctx;
    mouse_state = runner.mouseState;
    pressed_keys = runner.pressedKeys;

    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';

    new ResizeObserver(syncSize).observe(dom.main);
    syncSize();

    revealTime(world.start, world.end);
    restoreState();

    runner.updateFrequency = 60;
    runner.idleTimeout = 5000;
    runner.onUpdate = update;
    runner.onDraw = draw;
    runner.start();

    render(html`
        ${world.views.size ? html`
            <select @change=${e => { settings.view = e.target.value; saveState(); }}>
                ${Array.from(world.views.values()).map(view => html`<option value=${view.name} ?selected=${view.name == settings.view}>${view.name}</option>`)}
            </select>
        ` : ''}
    `, dom.config);

    document.title = `${world.project} (Heimdall)`;
}

function syncSize() {
    // Our position info is sensitive to pixel ratio
    {
        let ratio = window.devicePixelRatio / (runner.pixelRatio || 1);

        position.x *= ratio;
        position.y *= ratio;
        position.zoom *= ratio;
    }

    // Resize canvas
    {
        let rect = dom.main.getBoundingClientRect();
        if (!rect.width && !rect.height)
            return;
        runner.resize(rect.width, rect.height, window.devicePixelRatio);
    }

    layout.tree = {
        left: 0,
        top: 0,
        width: settings.tree * window.devicePixelRatio,
        height: canvas.height
    };
    layout.main = {
        left: settings.tree * window.devicePixelRatio,
        top: 0,
        width: canvas.width - settings.tree * window.devicePixelRatio,
        height: canvas.height - 50 * window.devicePixelRatio
    };
    layout.time = {
        left: settings.tree * window.devicePixelRatio,
        top: canvas.height - 50 * window.devicePixelRatio,
        width: canvas.width - settings.tree * window.devicePixelRatio,
        height: 50 * window.devicePixelRatio
    };
}

// ------------------------------------------------------------------------
// Run
// ------------------------------------------------------------------------

function update() {
    if (pressed_keys.tab == 1)
        show_debug = !show_debug;

    let view = world.views.get(settings.view);
    let zone = null;

    if (view == null) {
        view = world.views.values().next().value;

        settings.view = view?.name;
        saveState();
    }

    if (isInside(mouse_state, layout.tree)) {
        zone = layout.tree;
    } else if (isInside(mouse_state, layout.time)) {
        zone = layout.time;
    } else {
        zone = layout.main;
    }

    let reset = (pressed_keys.r == -1);
    let reveal = (pressed_keys.c == -1);

    if (reset) {
        position.entity = 0;
        position.y = 0;

        reveal = true;
    }

    // Resize tree panel
    if (interaction == null && isInside(mouse_state, makeRect(layout.main.left - 5, layout.main.top, 10, layout.main.height))) {
        if (mouse_state.left > 0) {
            interaction = {
                type: 'tree',
                offset: mouse_state.x - settings.tree * window.devicePixelRatio
            };
        }

        runner.cursor = 'col-resize';
    } else if (interaction?.type == 'tree') {
        if (mouse_state.left > 0) {
            let min = 140 * window.devicePixelRatio;
            let split = Util.clamp(mouse_state.x - interaction.offset, min, canvas.width / 2);

            settings.tree = Math.floor(split / window.devicePixelRatio);
            syncSize();
        } else {
            interaction = null;
            saveState();
        }

        runner.cursor = 'col-resize';
    }

    // Handle canvas grab
    if (interaction == null) {
        if (mouse_state.left > 0 && mouse_state.moving) {
            interaction = {
                type: 'move',
                x: (zone == layout.main || zone == layout.time) ? mouse_state.x : null,
                y: (zone == layout.main || zone == layout.tree) ? mouse_state.y : null
            };

            if (interaction.x == null && interaction.y == null)
                interaction = null;
        }
    } else if (interaction?.type == 'move') {
        if (mouse_state.left > 0) {
            let factor = pressed_keys.ctrl ? 5 : 1;

            if (interaction.x != null) {
                position.x = Math.round(position.x + (interaction.x - mouse_state.x) * factor);
                interaction.x = mouse_state.x;
            }
            if (interaction.y != null) {
                position.y = Math.round(position.y + (interaction.y - mouse_state.y) * factor);
                interaction.y = mouse_state.y;
            }

            saveState();
        } else {
            interaction = null;
        }

        runner.cursor = 'grabbing';
    }

    // Handle deploy/collapse clicks
    {
        let cursor = { x: mouse_state.x - layout.tree.left, y: mouse_state.y - layout.tree.top };
        let idx = rows.findIndex(row => cursor.y >= row.top && cursor.y < row.top + row.height);

        if (idx >= 0) {
            let row = rows[idx];

            if (interaction?.type != 'move') {
                row.hover = true;

                for (let i = idx - row.index; i < rows.length && rows[i].entity == row.entity; i++)
                    rows[i].active = true;
                for (let i = idx + 1; i < rows.length && rows[i].depth > row.depth; i++)
                    rows[i].hover = true;
            }

            if (!row.leaf && cursor.x >= 0 && cursor.x <= layout.tree.width) {
                if (mouse_state.left == -1) {
                    position.entity = row.entity;
                    position.y = -rows[idx - row.index].top;

                    if (!view.expand.delete(row.path))
                        view.expand.add(row.path);

                    saveState();
                }

                runner.cursor = 'pointer';
            }
        }
    }

    // Handle wheel actions (scroll or zoom)
    if (mouse_state.wheel) {
        let zoom = (zone == layout.time || (pressed_keys.ctrl && zone == layout.main));

        if (zoom) {
            let factor = (zone == layout.time && pressed_keys.ctrl) ? 5 : 1;
            let delta = Util.clamp(-mouse_state.wheel, -1, 1) * factor;
            let at = position.x + mouse_state.x - zone.left;

            zoomTime(delta, at);
            saveState();
        } else {
            let factor = pressed_keys.ctrl ? 100 : 10;

            position.y += mouse_state.wheel * factor;
            saveState();
        }
    } else if (mouse_state.pinch) {
        let delta = 10 * mouse_state.pinch;
        let at = position.x + mouse_state.x - layout.main.left;

        zoomTime(delta, at);
        saveState();
    }

    // Transform view tree into exhaustive levels
    let levels = [];
    if (view != null) {
        let map = new Map;

        for (let key in view.items) {
            let concept = view.items[key];
            let parts = key.split('/');

            for (let i = 0; i < parts.length; i++) {
                let path = parts.slice(0, i + 1).join('/');
                let level = map.get(path);

                if (level == null) {
                    level = {
                        name: parts[i],
                        path: path,
                        depth: i,
                        leaf: i == (parts.length - 1),
                        expand: view.expand.has(path),
                        concepts: new Set
                    };

                    levels.push(level);
                    map.set(path, level);
                }

                level.concepts.add(concept);

                if (!level.expand)
                    break;
            }
        }

        levels.sort(Util.makeComparator(level => level.path));
    }

    // Combine view and entities into draw rows
    {
        rows.length = 0;

        let space = SPACE_BETWEEN_ENTITIES * window.devicePixelRatio;
        let first = null;
        let offset = 0;

        for (let i = position.entity - 1, top = -position.y - space; i >= 0; i--) {
            let units = combine(world.entities, i, levels);

            if (units.length) {
                for (let j = units.length - 1; j >= 0; j--) {
                    let row = units[j];

                    top -= row.height;
                    row.top = top;

                    rows.unshift(row);
                }

                if (top < 0)
                    break;

                first = i;
                offset = -top;

                top -= space;
            }
        }

        for (let i = position.entity, top = -position.y; i < world.entities.length; i++) {
            let units = combine(world.entities, i, levels);

            if (units.length) {
                if (top >= 0 && first == null) {
                    first = i;
                    offset = -top;
                }

                for (let row of units) {
                    row.top = top;
                    top += row.height;

                    rows.push(row);
                }

                top += space;
            }

            if (top > canvas.height)
                break;
        }

        if (first != null) {
            position.entity = first ?? 0;
            position.y = offset;
        }
    }

    // Recenter view when user needs it
    if (reveal) {
        let left = Math.min(...rows.map(row => row.left));
        let right = Math.max(...rows.map(row => row.right));
        let start = positionToTime(left, position.zoom);
        let end = positionToTime(right, position.zoom);

        revealTime(start, end);
        saveState();
    }
}

function zoomTime(delta, at) {
    if (position.zoom + delta < -200 || position.zoom + delta > 200)
        return;

    let scale1 = zoomToScale(position.zoom);
    let scale2 = zoomToScale(position.zoom + delta);
    let transformed = at / scale1;

    position.x = Math.round(position.x - transformed * scale1 + transformed * scale2);
    position.zoom += delta;
}

function revealTime(start, end) {
    let range = end - start;
    let time = start - range * 0.05;
    let zoom = range ? scaleToZoom(layout.main.width / range / 1.1) : 0;

    position.x = timeToPosition(time, zoom);
    position.zoom = zoom;
}

function combine(entities, idx, levels) {
    let entity = entities[idx];
    let rows = [];

    for (let level of levels) {
        let row = {
            entity: idx,
            index: rows.length,
            name: level.depth ? level.name : entity.name,
            path: level.path,
            depth: level.depth,
            leaf: level.leaf,
            expand: level.expand,
            top: 0,
            height: ROW_HEIGHT * window.devicePixelRatio,
            active: false,
            hover: false,
            events: [],
            periods: [],
            values: [],
            left: Number.MAX_SAFE_INTEGER,
            right: Number.MIN_SAFE_INTEGER
        };

        for (let evt of entity.events) {
            if (!level.concepts.has(evt.concept))
                continue;

            let draw = {
                x: timeToPosition(evt.time, position.zoom),
                width: 0,
                count: 1,
                warning: evt.warning
            };
            row.events.push(draw);

            row.left = Math.min(row.left, draw.x);
            row.right = Math.max(row.right, draw.x + draw.width);
        }

        for (let period of entity.periods) {
            if (!level.concepts.has(period.concept))
                continue;

            let draw = {
                x: timeToPosition(period.time, position.zoom),
                width: period.duration * zoomToScale(position.zoom),
                warning: period.warning,
                stack: 0
            };
            row.periods.push(draw);

            row.left = Math.min(row.left, draw.x);
            row.right = Math.max(row.right, draw.x + draw.width);
        }

        if (canPlot(level) && !row.events.length && !row.periods.length) {
            let concept = level.concepts.values().next().value;
            let values = entity.values.filter(value => value.concept == concept);

            if (values.length) {
                row.height = PLOT_HEIGHT * window.devicePixelRatio;

                let min = Math.min(...values.map(value => value.value));
                let max = Math.max(...values.map(value => value.value));
                let range = max - min;

                if (range) {
                    min -= range * 0.3;
                    max += range * 0.3;
                    range = max - min;
                } else {
                    min -= 1;
                    range = 2;
                }

                for (let value of values) {
                    let draw = {
                        x: timeToPosition(value.time, position.zoom),
                        y: row.height - (value.value - min) / range * row.height,
                        label: value.value,
                        warning: value.warning
                    };
                    row.values.push(draw);

                    row.left = Math.min(row.left, draw.x);
                    row.right = Math.max(row.right, draw.x);
                }
            }
        } else {
            for (let value of entity.values) {
                if (!level.concepts.has(value.concept))
                    continue;

                let draw = {
                    x: timeToPosition(value.time, position.zoom),
                    width: 0,
                    count: 1,
                    warning: value.warning
                };
                row.events.push(draw);

                row.left = Math.min(row.left, draw.x);
                row.right = Math.max(row.right, draw.x);
            }
        }

        if (!row.events.length && !row.periods.length && !row.values.length)
            continue;

        row.events.sort((evt1, evt2) => evt1.x - evt2.x);
        row.periods.sort((period1, period2) => period1.x - period2.x);
        row.values.sort((value1, value2) => value1.x - value2.x);

        // Make sure periods don't overlap vertically
        {
            let overlaps = [];

            for (let period of row.periods) {
                overlaps = overlaps.filter(prev => prev.x + prev.width >= period.x);
                period.stack = overlaps.length;
                overlaps.push(period);
            }
        }

        // Merge events
        if (row.events.length) {
            let treshold = MERGE_TRESHOLD * window.devicePixelRatio;

            let j = 1;
            for (let i = 1; i < row.events.length; i++) {
                let evt = row.events[i];
                let prev = row.events[j - 1];

                row.events[j] = evt;

                if (evt.x - prev.x - prev.width < treshold) {
                    prev.width = evt.x - prev.x;
                    prev.count++;
                    prev.warning ||= evt.warning;
                } else {
                    j++;
                }
            }
            row.events.length = j;
        }

        // Make sure event with warning flag come up on top
        row.events.sort((evt1, evt2) => evt1.warning - evt2.warning);

        rows.push(row);
    }

    return rows;
}

function timeToPosition(time, zoom) {
    return Math.round(time * zoomToScale(zoom));
}

function positionToTime(x, zoom) {
    return Math.round(x / zoomToScale(zoom));
}

function zoomToScale(zoom) {
    let scale = Math.pow(2, 5 + zoom / 5);
    return scale;
}

function scaleToZoom(scale) {
    let zoom = 5 * (Math.log2(scale) - 5);
    return zoom;
}

function makeRect(left, top, width, height) {
    return { left: left, top: top, width: width, height: height };
}

function isInside(pos, rect) {
    let inside = (pos.x >= rect.left &&
                  pos.x <= rect.left + rect.width &&
                  pos.y >= rect.top &&
                  pos.y <= rect.top + rect.height);
    return inside;
}

function canPlot(level) {
    if (level.concepts.size > 1)
        return false;
    if (level.expand)
        return false;

    return true;
}

function restoreState() {
    let key = 'heimdall:' + world.project;
    let user = {};

    try {
        let json = localStorage.getItem(key);

        if (json != null) {
            let obj = JSON.parse(json);

            if (Util.isPodObject(obj))
                user = obj;
        }
    } catch (err) {
        console.error(err);
        localStorage.removeItem(key);
    }

    if (!Util.isPodObject(user.position))
        user.position = {};
    if (!Util.isPodObject(user.settings))
        user.settings = {};
    if (!Util.isPodObject(user.views))
        user.views = {};

    // Try to restore position, give up if stored entity is gone
    if (user.position.entity != null) {
        let idx = world.entities.findIndex(entity => entity.name == user.position.entity);

        if (idx >= 0) {
            position.x = (user.position.x ?? 0) * window.devicePixelRatio;
            position.zoom = (user.position.zoom ?? 0) * window.devicePixelRatio;
            position.entity = idx;
            position.y = (user.position.y ?? 0) * window.devicePixelRatio;
        }
    }

    for (let key in user.settings) {
        if (!settings.hasOwnProperty(key))
            continue;
        if (typeof user.settings[key] != typeof settings[key])
            continue;

        settings[key] = user.settings[key];
    }

    for (let key in user.views) {
        let expand = user.views[key];
        let view = world.views.get(key);

        if (Array.isArray(expand) && view != null)
            view.expand = new Set(expand);
    }
}

function saveState() {
    if (save_timer != null)
        return;

    save_timer = setTimeout(() => {
        save_timer = null;

        let key = 'heimdall:' + world.project;

        let state = {
            position: {
                x: position.x / window.devicePixelRatio,
                zoom: position.zoom / window.devicePixelRatio,
                entity: world.entities[position.entity]?.name,
                y: position.y / window.devicePixelRatio
            },
            settings: settings,
            views: Object.fromEntries(Util.map(world.views.values(), view => [view.name, Array.from(view.expand)]))
        };
        let json = JSON.stringify(state);

        localStorage.setItem(key, json);
    }, 200);
}

// ------------------------------------------------------------------------
// Draw
// ------------------------------------------------------------------------

function draw() {
    ctx.setTransform(1, 0, 0, 1, 0, 0);

    // Draw background
    ctx.fillStyle = 'white';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = '#222222';
    ctx.fillRect(layout.tree.left, layout.tree.top, layout.tree.width, layout.tree.height);
    ctx.fillRect(layout.time.left, layout.time.top, layout.time.width, layout.time.height);

    // Highlight rows
    for (let row of rows) {
        if (row.hover) {
            ctx.fillStyle = '#555555';
            ctx.fillRect(layout.tree.left, layout.tree.top + row.top, layout.tree.width, row.height);
            ctx.fillStyle = '#dddddd';
            ctx.fillRect(layout.main.left, layout.main.top + row.top, layout.main.width, row.height);
        } else if (row.active) {
            ctx.fillStyle = '#333333';
            ctx.fillRect(layout.tree.left, layout.tree.top + row.top, layout.tree.width, row.height);
            ctx.fillStyle = '#eeeeee';
            ctx.fillRect(layout.main.left, layout.main.top + row.top, layout.main.width, row.height);
        }
    }
    // Highlight rows
    for (let row of rows) {
        if (row.hover) {
            ctx.fillStyle = '#555555';
            ctx.fillRect(layout.tree.left, layout.tree.top + row.top, layout.tree.width, row.height);
            ctx.fillStyle = '#dddddd';
            ctx.fillRect(layout.main.left, layout.main.top + row.top, layout.main.width, row.height);
        } else if (row.active) {
            ctx.fillStyle = '#333333';
            ctx.fillRect(layout.tree.left, layout.tree.top + row.top, layout.tree.width, row.height);
            ctx.fillStyle = '#eeeeee';
            ctx.fillRect(layout.main.left, layout.main.top + row.top, layout.main.width, row.height);
        }
    }

    // Draw entity tree
    {
        ctx.save();
        ctx.translate(layout.tree.left, layout.tree.top);

        ctx.beginPath();
        ctx.rect(0, 0, layout.tree.width, layout.tree.height);
        ctx.clip();

        ctx.font = `${18 * window.devicePixelRatio}px Open Sans`;

        for (let row of rows) {
            if (row.top + row.height < 0)
                continue;
            if (row.top > layout.tree.height)
                continue;

            ctx.fillStyle = 'white';

            let x = 10 + row.depth * 10;
            let offset = 20 * window.devicePixelRatio;

            if (!row.leaf) {
                ctx.beginPath();
                if (row.expand) {
                    ctx.moveTo(x, row.top + row.height / 2 - 3 * window.devicePixelRatio);
                    ctx.lineTo(x + 12 * window.devicePixelRatio, row.top + row.height / 2 - 3 * window.devicePixelRatio);
                    ctx.lineTo(x + 6 * window.devicePixelRatio, row.top + row.height / 2 + 5 * window.devicePixelRatio);
                } else {
                    ctx.moveTo(x + 2 * window.devicePixelRatio, row.top + row.height / 2 - 5 * window.devicePixelRatio);
                    ctx.lineTo(x + 10 * window.devicePixelRatio, row.top + row.height / 2 + 1 * window.devicePixelRatio);
                    ctx.lineTo(x + 2 * window.devicePixelRatio, row.top + row.height / 2 + 7 * window.devicePixelRatio);
                }
                ctx.closePath();
                ctx.fill();
            }

            runner.text(x + offset, row.top + row.height / 2, row.name, { align: 4 });

            if (!row.active) {
                ctx.fillStyle = '#22222288';
                ctx.fillRect(0, row.top, layout.tree.width, row.height);
            }
        }

        ctx.restore();
    }

    // Draw vertical guide line at cursor
    if (mouse_state.x > layout.main.left && mouse_state.x < layout.main.left + layout.main.width) {
        ctx.strokeStyle = '#cccccc';
        ctx.lineWidth = 1;

        let x = Math.round(mouse_state.x);

        ctx.beginPath();
        ctx.moveTo(x, layout.main.top);
        ctx.lineTo(x, layout.main.top + layout.main.height);
        ctx.stroke();
    }

    // Draw entity rows
    {
        ctx.save();
        ctx.translate(layout.main.left, layout.main.top);

        for (let row of rows) {
            if (row.top + row.height < 0)
                continue;
            if (row.top > layout.main.height)
                continue;

            ctx.save();
            ctx.translate(0, row.top);

            ctx.beginPath();
            ctx.rect(0, 0, layout.main.width, row.height);
            ctx.clip();

            // Draw periods
            for (let period of row.periods) {
                ctx.save();

                ctx.fillStyle = period.warning ? '#ff6600' : '#444444';

                let height = PERIOD_HEIGHT * window.devicePixelRatio
                let offset = (period.stack + 1) * height * 2;

                ctx.beginPath();
                ctx.roundRect(period.x - position.x, row.height - offset, period.width, height, height / 2);
                ctx.fill();

                ctx.restore();
            }

            // Draw events
            for (let evt of row.events) {
                ctx.save();

                ctx.fillStyle = evt.warning ? '#ff6600' : '#4444cc';

                let x = evt.x - position.x;
                let width = EVENT_WIDTH * window.devicePixelRatio;
                let height = EVENT_HEIGHT * window.devicePixelRatio;

                ctx.beginPath();
                ctx.moveTo(x, row.height - height);
                ctx.lineTo(x + evt.width, row.height - height);
                ctx.lineTo(x + evt.width + width / 2, row.height);
                ctx.lineTo(x - width / 2, row.height);
                ctx.closePath();
                ctx.fill();

                if (evt.count > 1) {
                    let size = Math.floor(height / 2) - 3 * Math.floor(Math.log10(evt.count));

                    ctx.font = `bold ${size}px Open Sans`;
                    ctx.fillStyle = 'white';

                    runner.text(x + evt.width / 2, row.height - height / 4, evt.count, { align: 2 });
                }

                ctx.restore();
            }

            // Draw value graph
            {
                let values = row.values;
                let idx = 0;

                while (idx < values.length) {
                    ctx.save();

                    ctx.strokeStyle = 'black';
                    ctx.lineWidth = 1;

                    ctx.beginPath();
                    ctx.moveTo(values[idx].x - position.x, values[idx].y);
                    while (++idx < values.length) {
                        if (values[idx].y == null)
                            break;

                        switch (settings.interpolation) {
                            case 'linear': {
                                ctx.lineTo(values[idx].x - position.x, values[idx].y);
                            } break;

                            case 'locf': {
                                ctx.lineTo(values[idx].x - position.x, values[idx - 1].y);
                                ctx.lineTo(values[idx].x - position.x, values[idx].y);
                            } break;
                        }
                    }
                    ctx.stroke();

                    ctx.restore();
                }
            }

            // Draw value labels
            for (let value of row.values) {
                if (value.label == null)
                    continue;

                ctx.save();

                let background = value.warning ? '#ff6600' : 'white';
                let color = value.warning ? 'white' : 'black';

                ctx.font = `${12 * window.devicePixelRatio}px Open Sans`;
                ctx.fillStyle = color;

                runner.text(value.x - position.x, value.y, value.label, { background: background, align: 5 });

                ctx.restore();
            }

            if (!row.active) {
                ctx.fillStyle = '#ffffff33';
                ctx.fillRect(0, 0, layout.main.width, row.height);
            }

            ctx.restore();
        }

        ctx.restore();
    }

    // Draw time scale
    {
        ctx.save();
        ctx.translate(layout.time.left, layout.time.top);

        ctx.fillStyle = '#222222';

        ctx.beginPath();
        ctx.rect(-5, 0, layout.time.width + 10, layout.time.height);
        ctx.clip();

        ctx.font = `${12 * window.devicePixelRatio}px Open Sans`;
        ctx.fillStyle = 'white';
        ctx.strokeStyle = 'white';
        ctx.lineWidth = 1;

        let scale = zoomToScale(position.zoom);
        let start = Math.floor(position.x / scale);
        let end = Math.ceil((position.x + layout.time.width) / scale);
        let chars = Math.max(countDigits(start), countDigits(end));
        let height = 8 * window.devicePixelRatio;

        let step1 = Math.ceil(height / scale);
        let step2 = Math.ceil(1.4 * height * chars / scale);

        // Stabilize chosen start values to go through 0
        let start1 = start - start % step1 - step1;
        let start2 = start - start % step2 - step2;
        end += step2;

        for (let time = start1; time < end; time += step1) {
            let x = time * scale - position.x;

            ctx.beginPath();
            ctx.moveTo(x, height);
            ctx.lineTo(x, height * 2);
            ctx.stroke();
        }

        for (let time = start2; time < end; time += step2) {
            let x = time * scale - position.x;
            runner.text(x, height * 3, time, { align: 8 });
        }

        ctx.restore();
    }

    // Draw separating lines
    {
        ctx.strokeStyle = '#888888';
        ctx.lineWidth = 1;

        ctx.beginPath();
        ctx.moveTo(layout.main.left, layout.main.top);
        ctx.lineTo(layout.main.left, layout.main.top + layout.main.height);
        ctx.lineTo(layout.main.left + layout.main.width, layout.main.top + layout.main.height);
        ctx.stroke();
    }

    if (show_debug) {
        ctx.font = `${18 * window.devicePixelRatio}px Open Sans`;
        ctx.fillStyle = 'black';

        // FPS counter
        {
            let text = `FPS : ${(1000 / runner.frameTime).toFixed(0)} (${runner.frameTime.toFixed(1)} ms)` +
                       ` | Update : ${runner.updateTime.toFixed(1)} ms | Draw : ${runner.drawTime.toFixed(1)} ms`;
            runner.text(canvas.width - 12, 12, text, { align: 9 });
        }
    }
}

function countDigits(value) {
    if (value) {
        let digits = Math.floor(Math.log10(Math.abs(value)));
        return (value < 0) + digits;
    } else {
        return 1;
    }
}

export {
    load,
    start
}
