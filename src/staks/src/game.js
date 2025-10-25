// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, LruMap } from '../../web/core/base.js';
import { AppRunner } from '../../web/core/runner.js';
import * as rules from './rules/modern.js';
import { TouchInterface } from './touch.js';
import { ASSETS } from '../assets/assets.js';

import en from '../i18n/en.json';
import fr from '../i18n/fr.json';

import '../../../vendor/opensans/OpenSans.css';
import './game.css';

const KEYBOARD_SHORTCUTS = [
    ['← / →', () => T.input_slide],
    ['↑', () => T.input_rotate_cw],
    ['Ctrl', () => T.input_rotate_ccw],
    ['↓', () => T.input_turbo],
    null,
    ['Espace', () => T.input_lock],
    ['C', () => T.input_hold],
    null,
    ['P', () => T.input_pause],
    ['F', () => T.input_fullscreen],
    null,
    ['G', () => T.input_ghost],
    ['B', () => T.input_wallpaper],
    ['S', () => T.input_mute],
    ['M', () => T.input_music],
    null,
    ['H', () => T.input_help]
];

const AREA_RADIUS = 4;
const BUTTON_SIZE = 56;
const SQUARE_SIZE = 40;

const DEFAULT_SETTINGS = {
    background: 'aurora',
    music: true,
    sound: true,
    ghost: true,
    help: true
};

// Assets
let languages = {};
let assets = null;

// DOM nodes
let main = null;
let canvas = null;
let attributions = null;

// Render and input API
let runner = null;
let ctx = null;
let mouse_state = null;
let pressed_keys = null;
let touch = null;

// Game state
let game = new Game;

// User settings
let settings = Object.assign({}, DEFAULT_SETTINGS);
let show_debug = false;

// Sound
let music = null;
let sfx = null;
let music_buffers = new LruMap(2);
let music_idx = 0;

// UI state
let layout = {
    padding: null,
    square: null,
    button: null,

    well: null,
    bag: null,
    level: null,
    score: null,
    hold: null,
    help: null,
    input: null
};

// ------------------------------------------------------------------------
// Init
// ------------------------------------------------------------------------

async function load(prefix, lang, progress = null) {
    languages.en = en;
    languages.fr = fr;

    Util.initLocales(languages, lang);

    if (window.createImageBitmap == null)
        throw new Error('ImageBitmap support is not available (old browser?)');

    assets = await loadAssets(prefix, progress);

    loadSettings();

    for (let i = 0; i < rules.BLOCKS.length; i++) {
        let block = rules.BLOCKS[i];

        let trail = 0;
        let lead = 0;

        for (let j = 0; j < block.shape.length; j++) {
            if (block.shape[j])
                break;
            lead++;
        }
        for (let j = block.shape.length - 1; j >= 0; j--) {
            if (block.shape[j])
                break;
            trail++;
        }

        block.value = i;
        block.size = Math.sqrt(block.shape.length);
        block.top = block.size - Math.floor(trail / block.size);
        block.bottom = Math.floor(lead / block.size);
        block.middle = (block.top + block.bottom) / 2;
    }
}

async function loadAssets(prefix, progress) {
    prefix = prefix.replace(/\/+$/g, '') + '/';

    if (progress == null)
        progress = (value, total) => {};

    let mp3 = detectOpusSupport();

    let assets = {
        backgrounds: filterAssets('backgrounds'),
        musics: filterAssets('musics/' + (mp3 ? 'mp3' : 'opus')),
        sounds: filterAssets('sounds/' + (mp3 ? 'mp3' : 'opus')),
        ui: filterAssets('ui')
    };

    // Shuffle musics
    assets.musics = Util.shuffle(Object.keys(assets.musics)).reduce((obj, key) => { obj[key] = assets.musics[key]; return obj; }, {});

    let categories = Object.keys(assets);
    let resources = categories.flatMap(category => {
        let obj = assets[category];
        return Object.keys(obj).map(key => ({ category, obj, key }));
    });
    let downloaded = 0;

    resources = Util.shuffle(resources);

    await Promise.all(Util.mapRange(0, 3, async start => {
        for (let i = start; i < resources.length; i += 3) {
            let res = resources[i];
            let url = prefix + res.obj[res.key];

            if (res.category == 'musics') {
                // Lazy load musics
                res.obj[res.key] = url;
            } else if (url.endsWith('.png') || url.endsWith('.webp')) {
                res.obj[res.key] = await loadTexture(url);
            } else if (url.endsWith('.webm') || url.endsWith('.mp3')) {
                res.obj[res.key] = await loadSound(url);
            } else {
                throw new Error(`Unknown resource type for '${url}'`);
            }

            downloaded++;
            progress(downloaded, resources.length);
        }
    }));

    return assets;
}

function filterAssets(path) {
    let prefix = path + '/';

    let obj = {};

    for (let key in ASSETS) {
        let url = ASSETS[key];

        if (!key.startsWith(prefix))
            continue;
        key = key.substr(prefix.length);

        obj[key] = url;
    }

    return obj;
}

function detectOpusSupport() {
    let audio = document.createElement('audio');
    let support = audio.canPlayType?.('audio/webm;codecs=opus');

    return (support == 'probably');
}

async function loadTexture(url) {
    let response = await self.fetch(url);

    let blob = await response.blob();
    let texture = await createImageBitmap(blob);

    return texture;
}

async function loadSound(url) {
    let response = await fetch(url);
    let buf = await response.arrayBuffer();

    return buf;
}

function loadSettings() {
    let user = {};

    try {
        let json = localStorage.getItem('staks');

        if (json != null) {
            let obj = JSON.parse(json);

            if (Util.isPodObject(obj))
                user = obj;
        }
    } catch (err) {
        console.error(err);
        localStorage.removeItem('staks');
    }

    for (let key in user) {
        if (!settings.hasOwnProperty(key))
            continue;
        if (typeof user[key] != typeof settings[key])
            continue;

        settings[key] = user[key];
    }

    if (!assets.backgrounds.hasOwnProperty(settings.background))
        settings.background = DEFAULT_SETTINGS.background;
}

function saveSettings() {
    let json = JSON.stringify(settings);
    localStorage.setItem('staks', json);
}

async function start(root) {
    render(html`
        <div class="stk_game">
            <div class="stk_attributions">
                ${T.music_by} <a href="https://freemusicarchive.org/music/audiocoffee/" target="_blank">AudioCoffee</a><br>
                ${T.wallpapers_by} <a href="https://www.deviantart.com/psiipilehto/" target="_blank">psiipilehto</a>
            </div>
            <canvas class="stk_canvas"></canvas>
        </div>
    `, root);
    main = root.querySelector('.stk_game');
    canvas = root.querySelector('.stk_canvas');
    attributions = root.querySelector('.stk_attributions');

    runner = new AppRunner(canvas);
    ctx = runner.ctx;
    mouse_state = runner.mouseState;
    pressed_keys = runner.pressedKeys;

    sfx = runner.createTrack(32);
    music = runner.createTrack(1);

    touch = new TouchInterface(runner, assets);

    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';

    new ResizeObserver(syncSize).observe(main);
    syncSize();

    runner.updateFrequency = rules.UPDATE_FREQUENCY;
    runner.idleTimeout = 5000;
    runner.onUpdate = update;
    runner.onDraw = draw;
    runner.start();
}

function syncSize() {
    let rect = main.getBoundingClientRect();
    if (!rect.width && !rect.height)
        return;
    runner.resize(rect.width, rect.height, window.devicePixelRatio);
}

// ------------------------------------------------------------------------
// Main
// ------------------------------------------------------------------------

function update() {
    touch.begin();

    // Global layout
    {
        let padding = runner.isTouch ? 16 : 8;

        let width = canvas.width - padding * 2;
        let height = canvas.height - padding * 2;

        // Account for touch controls
        if (runner.isTouch && runner.isPortrait) {
            width -= 200 + padding;
            height -= 100 * window.devicePixelRatio + padding;
        }

        // Make sure things fit, and square size divides by two nicely
        let square = Math.round(Math.min(
            Math.min(width / rules.COLUMNS, height / (rules.ROWS + rules.EXTRA_TOP)),
            SQUARE_SIZE * window.devicePixelRatio
        ) / 2) * 2;

        width = rules.COLUMNS * square;
        height = Math.ceil((rules.ROWS + rules.EXTRA_TOP) * square);

        let left = canvas.width / 2 - width / 2;
        let top = (runner.isTouch && runner.isPortrait) ? padding : (canvas.height / 2 - height / 2);
        let bag = Math.max(120, 2.5 * layout.square);

        if (runner.isTouch) {
            let factor = runner.isPortrait ? 0.25 : 0.5;
            left -= factor * (padding + bag);
        }

        layout.padding = padding;
        layout.square = square;
        layout.button = Math.round(BUTTON_SIZE * window.devicePixelRatio);

        if (runner.isTouch && !runner.isPortrait)
            layout.button = Math.round(layout.button * 1.2);

        layout.well = {
            left: left,
            top: top,
            width: width,
            height: height
        };
        layout.bag = {
            left: left + (rules.COLUMNS * layout.square) + padding,
            top: top,
            width: bag,
            height: (1 + rules.BAG_SIZE * 4) / 2 * layout.square
        };
        layout.level = {
            left: layout.bag.left,
            top: layout.bag.top + layout.bag.height + padding,
            width: layout.bag.width,
            height: Math.max(40, 30 * window.devicePixelRatio)
        };
        layout.score = {
            left: layout.bag.left,
            top: layout.level.top + layout.level.height + padding,
            width: layout.bag.width,
            height: Math.max(40, 30 * window.devicePixelRatio)
        };

        if (runner.isTouch && runner.isPortrait) {
            let bottom = top + height;

            layout.hold = {
                left: layout.bag.left,
                top: bottom - 2.5 * layout.square,
                width: layout.bag.width,
                height: Math.max(100, 2.5 * layout.square)
            };

            layout.help = null;
            layout.input = {
                left: padding,
                top: bottom + padding,
                width: canvas.width - padding * 2,
                height: canvas.height - bottom - padding * 2
            };
        } else {
            layout.hold = {
                left: left - padding - layout.bag.width,
                top: top,
                width: layout.bag.width,
                height: Math.max(100, 2.5 * layout.square)
            };

            if (runner.isTouch) {
                layout.help = null;
                layout.input = {
                    left: padding,
                    top: canvas.height / 2 - 80,
                    width: canvas.width - padding * 2,
                    height: 300
                };
            } else {
                layout.help = {
                    right: left - padding,
                    bottom: top + height
                };
                layout.input = null;
            }
        }
    }

    // Global touch buttons
    if (runner.isTouch || game.isPlaying) {
        let size = (runner.isTouch ? 0.5 : 0.7) * layout.button;
        let x = 0, y = 0;
        let dx = 0, dy = 0;

        if (runner.isTouch && runner.isPortrait) {
            x = 18 + size / 2;
            y = 18 + size / 2;
            dy = size + 20;
        } else {
            x = canvas.width - 18 - size / 2;
            y = 18 + size / 2;
            dx = -size - 20;
        }

        if (touch.button('sound' + (0 + settings.sound), x, y, size).clicked) {
            settings.sound = !settings.sound;
            saveSettings();
        }
        x += dx; y += dy;

        if (touch.button('music' + (0 + settings.music), x, y, size).clicked) {
            settings.music = !settings.music;
            saveSettings();
        }
        x += dx; y += dy;

        if (touch.button('background', x, y, size).clicked)
            toggleBackground();
        x += dx; y += dy;

        if (main.requestFullscreen != null) {
            if (touch.button('fullscreen', x, y, size).clicked)
                toggleFullscreen();
            x += dx; y += dy;
        }

        if (game.isPlaying && runner.isTouch) {
            if (touch.button('ghost', x, y, size).clicked) {
                settings.ghost = !settings.ghost;
                saveSettings();
            }
            x += dx; y += dy;
        }
    }
    if (game.isPlaying && runner.isTouch) {
        if (runner.isPortrait) {
            let size = 0.7 * layout.button;
            let x = layout.score.left + layout.score.width / 2;
            let y = (layout.score.top + layout.score.height + layout.hold.top) / 2;

            if (touch.button('pause' + (0 + game.pause), x, y, size).clicked)
                game.pause = !game.pause;
        } else {
            let size = 0.7 * layout.button;
            let x = 18 + size / 2;
            let y = 18 + size / 2;

            if (touch.button('pause' + (0 + game.pause), x, y, size).clicked)
                game.pause = !game.pause;
        }
    }
    if (game.hasStarted && !runner.isTouch && !settings.help) {
        let size = 0.7 * layout.button;
        let x = layout.well.left - layout.padding - size / 2;
        let y = layout.well.top + layout.well.height - size / 2;

        let clicked = touch.button('help', x, y, size).clicked;

        if (clicked) {
            settings.help = true;
            game.pause = true;
        }
    }

    // Simple shortcuts
    if (pressed_keys.p == 1 || pressed_keys.pause == 1)
        game.pause = !game.pause;
    if (pressed_keys.g == 1) {
        settings.ghost = !settings.ghost;
        saveSettings();
    }
    if (pressed_keys.b == 1)
        toggleBackground();
    if (pressed_keys.s == 1) {
        settings.sound = !settings.sound;
        saveSettings();
    }
    if (pressed_keys.m == 1) {
        settings.music = !settings.music;
        saveSettings();
    }
    if (pressed_keys.h == 1) {
        settings.help = !settings.help;
        saveSettings();
    }
    if (pressed_keys.f == 1)
        toggleFullscreen();
    if (pressed_keys.j == 1)
        toggleMusic();
    if (pressed_keys.tab == 1)
        show_debug = !show_debug;

    sfx.volume = 0 + settings.sound;
    music.volume = 0 + (settings.sound && settings.music);

    // Play music
    {
        if (music.volume)
            loadMusics(game.hasStarted);

        let asset = music_buffers.get(music_idx);

        if (asset instanceof ArrayBuffer) {
            let handle = music.playOnce(asset, false);

            if (handle.ended)
                toggleMusic();
        }
    }

    if (!game.isPlaying) {
        if (runner.isTouch) {
            if (game.hasStarted) {
                let x = layout.well.left + layout.well.width / 2;
                let y = layout.well.top + layout.well.height / 2 + 150;

                if (touch.button('start', x, y, 1.2 * layout.button).clicked)
                    play();
            } else {
                let x = canvas.width / 2;
                let y = canvas.height / 2 + 150;

                if (touch.button('start', x, y, 1.2 * layout.button).clicked)
                    play();
            }
        } else {
            if (mouse_state.left == -1 || pressed_keys.return == -1) {
                play();
                pressed_keys.space = 0;
            }
        }
    }

    game.showGhost = settings.ghost;
    game.update();

    canvas.focus();
}

function toggleFullscreen() {
    let fullscreen = (document.fullscreenElement == main);

    if (!fullscreen) {
        try {
            main.requestFullscreen();
        } catch (err) {
            console.error(err);
        }
    } else {
        document.exitFullscreen();
    }
}

function toggleBackground() {
    let keys = Object.keys(assets.backgrounds);
    let idx = keys.indexOf(settings.background);
    let next = (idx + 1) % keys.length;

    settings.background = keys[next];
    saveSettings();
}

function loadMusics(preload) {
    let keys = Object.keys(assets.musics);
    let indices = [music_idx];

    if (preload) {
        let next = (music_idx + 1) % keys.length;
        indices.push(next);
    }

    for (let idx of indices) {
        let buf = music_buffers.get(idx);

        if (buf == null) {
            let url = assets.musics[keys[idx]];

            let p = Net.fetch(url);

            p = p.then(response => response.arrayBuffer())
                 .then(buf => { music_buffers.set(idx, buf); });
            p.catch(err => {
                music_buffers.delete(idx);
                console.error(err);
            });

            music_buffers.set(idx, p);
        }
    }
}

function toggleMusic() {
    let keys = Object.keys(assets.musics);
    music_idx = (music_idx + 1) % keys.length;
}

function play() {
    if (game.isOver)
        game = new Game;
    game.start();
}

function draw() {
    ctx.setTransform(1, 0, 0, 1, 0, 0);

    // Draw background with parallax effect
    {
        let img = assets.backgrounds[settings.background];

        let ratio1 = canvas.width / canvas.height;
        let ratio2 = img.width / img.height;
        let factor = (ratio1 > ratio2) ? (canvas.width / img.width) : (canvas.height / img.height);

        let origx = -(img.width * factor - canvas.width) / 2;
        let origy = -(img.height * factor - canvas.height) / 2;

        ctx.drawImage(img, origx, origy, img.width * factor, img.height * factor);
    }

    if (game.hasStarted) {
        attributions.classList.remove('active');
        game.draw();

        if (settings.help && layout.help != null)
            drawHelp();
    } else {
        attributions.classList.add('active');
        drawStart();
    }

    touch.draw();

    if (show_debug) {
        ctx.save();

        ctx.font = font(18);
        ctx.fillStyle = 'white';

        // FPS counter
        {
            let text = `FPS : ${(1000 / runner.frameTime).toFixed(0)} (${runner.frameTime.toFixed(1)} ms)` +
                       ` | Update : ${runner.updateTime.toFixed(1)} ms | Draw : ${runner.drawTime.toFixed(1)} ms`;
            runner.text(12, 12, text, { align: 7 });
        }

        ctx.restore();
    }
}

function drawStart() {
    ctx.font = font(32);
    ctx.fillStyle = 'white';

    let text = runner.isTouch ? T.start_button : T.start_return;
    runner.text(canvas.width / 2, canvas.height / 2, text, { align: 5 });

    if (!runner.isTouch) {
        ctx.font = 'italic ' + font(18);

        let help = [
            T.help_slide,
            T.help_pivot,
            '',
            T.help_shortcuts
        ];

        for (let i = 0; i < help.length; i++) {
            let text = help[i];
            let adjust = 100 + 30 * i;

            runner.text(canvas.width / 2, canvas.height / 2 + adjust, text, { align: 5 });
        }
    }
}

function drawHelp() {
    ctx.save();

    ctx.font = font(14);
    ctx.fillStyle = 'white';

    let step = 10 * window.devicePixelRatio;

    let x = layout.help.right;
    let y = layout.help.bottom;
    let width = 0;
    let height = 18;

    let shortcuts = KEYBOARD_SHORTCUTS.map(shortcut => shortcut ? [shortcut[0], shortcut[1]()] : null);

    for (let shortcut of shortcuts) {
        if (shortcut != null) {
            width = Math.max(width, 24 + 80 * window.devicePixelRatio + runner.measure(shortcut[1]).width);
            height += step;
        }

        height += step;
    }

    drawArea(x - width, y - height, width, height);
    y -= step;

    for (let i = shortcuts.length - 1; i >= 0; i--) {
        let shortcut = shortcuts[i];

        if (shortcut != null) {
            ctx.font = 'bold ' + font(14);
            runner.text(x - 12, y, shortcut[0], { align: 3 });

            ctx.font = font(14);
            runner.text(x - 12 - 80 * window.devicePixelRatio, y, shortcut[1], { align: 3 });

            y -= step;
        }

        y -= step;
    }

    ctx.restore();
}

function drawArea(x, y, width, height) {
    if (ctx.roundRect != null) {
        ctx.beginPath();
        ctx.roundRect(x, y, width, height, AREA_RADIUS);
    } else {
        ctx.beginPath();
        ctx.rect(x, y, width, height);
    }

    ctx.save();

    ctx.globalAlpha = 0.75;
    ctx.fillStyle = 'black';

    ctx.fill();

    ctx.restore();

    ctx.clip();
}

// ------------------------------------------------------------------------
// Game
// ------------------------------------------------------------------------

function Game() {
    // Main state
    let started = false;
    let gameover = false;
    let pause = false;
    let counter = 0;

    // Bag
    let bag_generator = rules.BAG_GENERATOR();
    let bag_draw = [];

    // Well
    let grid = new Int8Array((rules.ROWS + rules.EXTRA_ROWS) * rules.COLUMNS);
    grid.fill(-1);

    // Pieces
    let piece = null;
    let ghost = null;

    // Actions
    let das_start = null;
    let das_left = false;
    let das_count = null;
    let hold_block = null;
    let can_hold = rules.HOLD;
    let gravity_start = null;
    let lock_start = null;
    let lock_since = null;
    let clear_start = null;
    let clear_rows = null;

    // Animations
    let particles = [];
    let hit_force = 0;
    let hit_value = 0;

    // Scoring
    let level = 1;
    let lines = 0;
    let score = 0;
    let combo = -1;
    let special = null;
    let back2back = false;

    // Settings
    let show_ghost = true;

    Object.defineProperties(this, {
        hasStarted: { get: () => started, enumerable: true },
        isPlaying: { get: () => started && !gameover, enumerable: true },
        isOver: { get: () => gameover, enumerable: true },

        pause: { get: () => pause, set: value => { pause = value; }, enumerable: true },
        showGhost: { get: () => show_ghost, set: value => { show_ghost = value; }, enumerable: true },

        score: { get: () => score, enumerable: true },
        level: { get: () => level, enumerable: true },
        lines: { get: () => lines, enumerable: true }
    });

    this.start = function() {
        started = true;
        gameover = false;
        pause = false;
    };

    this.update = function() {
        if (!started)
            return;

        if (gameover)
            pause = false;

        // Run game simulation
        if (!pause) {
            counter++;

            if (!gameover) {
                updatePlay();
                runner.busy();
            } else {
                sfx.playOnce(assets.sounds.gameover);
            }
        }

        // Update fast fall animation
        {
            hit_value += hit_force / 10;
            hit_force -= 100 / rules.UPDATE_FREQUENCY;

            if (hit_value < 0) {
                hit_value = 0;
                hit_force = 0;
            }
        }
    }

    function updatePlay() {
        // Draw next pieces
        while (bag_draw.length < rules.BLOCKS.length) {
            let block = bag_generator.next().value;
            bag_draw.push(block);
        }

        // User actions
        let left = false;
        let right = false;
        let rotate = 0;
        let turbo = false;
        let drop = false;
        let hold = false;

        // Respond to user input
        if (runner.isTouch) {
            let rect = layout.input;

            left = touch.button('left', rect.left + 0.8 * layout.button,
                                        rect.top + 0.3 * rect.height, layout.button).pressed >= 1;
            right = touch.button('right', rect.left + 2.2 * layout.button,
                                          rect.top + 0.3 * rect.height, layout.button).pressed >= 1;
            turbo = touch.button('turbo', rect.left + 1.5 * layout.button,
                                          rect.top + 0.8 * rect.height, 0.7 * layout.button).pressed >= 1;

            rotate += touch.button('clockwise', rect.left + rect.width - 0.8 * layout.button,
                                                rect.top + 0.3 * rect.height, layout.button).clicked;
            rotate -= touch.button('counterclock', rect.left + rect.width - 2.2 * layout.button,
                                                   rect.top + 0.3 * rect.height, layout.button).clicked;
            if (!runner.isPortrait)
                drop = touch.button('drop', rect.left + rect.width - 1.5 * layout.button,
                                            rect.top + 0.8 * rect.height, 0.7 * layout.button).clicked;

            if (isInsideRect(mouse_state.x, mouse_state.y, expandRect(layout.well, -40, -40))) {
                if (mouse_state.left == 1)
                    drop = true;
                mouse_state.left = 0;
            }

            if (isInsideRect(mouse_state.x, mouse_state.y, layout.hold)) {
                if (mouse_state.left == 1)
                    hold = true;
                mouse_state.left = 0;
            }
        } else {
            left = (pressed_keys.left >= 1);
            right = (pressed_keys.right >= 1);
            rotate = (pressed_keys.up == 1 || pressed_keys.x == 1) - (pressed_keys.ctrl == 1 || pressed_keys.z == 1);
            turbo = (pressed_keys.down >= 1);
            drop = (pressed_keys.space == 1);
            hold = (pressed_keys.c == 1);
        }

        // Perform lateral movement and handle DAS
        for (;;) {
            let first = (das_start == null);
            let move = 0;

            if (left != right) {
                if (das_start == null) {
                    move = right - left;

                    das_start = counter;
                    das_left = left;
                    das_count = 0;
                } else if (counter >= das_start + rules.DAS_DELAY) {
                    if (das_left != left)
                        das_count = 0;
                    das_left = left;

                    das_count++;

                    if (das_count >= rules.DAS_PERIOD)
                        move += right - left;
                }
            } else if (!left) { // Implies !right given previous condition
                das_start = null;
            }

            if (piece == null)
                break;
            if (!move)
                break;

            let edit = Object.assign({}, piece);
            edit.column += move;

            if (!isPieceValid(edit, false))
                break;

            // Confirm move
            {
                piece.column = edit.column;

                if (piece.actions < rules.MAX_ACTIONS)
                    lock_start = counter;
                piece.actions += first;

                das_count -= rules.DAS_PERIOD;

                special = null;

                sfx.playFull(assets.sounds.move);
            }
        }

        // Perform rotation
        if (piece != null && rotate && piece.actions < rules.MAX_ACTIONS) {
            let edit = Object.assign({}, piece);

            let kicks = null;
            let valid = null;

            if (rotate > 0) {
                rotatePiece(edit);
                kicks = edit.kicks[piece.angle];
            } else {
                rotatePiece(edit);
                rotatePiece(edit);
                rotatePiece(edit);

                kicks = edit.kicks[piece.angle].slice();

                for (let i = 0; i < kicks.length; i++) {
                    let kick = kicks[i];
                    kicks[i] = [-kick[0], -kick[1]];
                }
            }

            for (let kick of kicks) {
                edit.row = piece.row + kick[0];
                edit.column = piece.column + kick[1];

                if (isPieceValid(edit, false)) {
                    valid = kick;
                    break;
                }
            }

            if (valid != null) {
                piece.row = edit.row;
                piece.column = edit.column;
                piece.shape = edit.shape;
                piece.angle = edit.angle;

                lock_start = counter;
                piece.actions++;

                if (piece.type == 'T') {
                    special = detectTSpin(piece, valid);
                } else {
                    special = null;
                }

                sfx.playFull(assets.sounds.move);
            }
        }

        // Hold piece when requested
        if (piece != null && hold && can_hold) {
            let block = hold_block ?? bag_draw.shift();

            hold_block = piece.block;
            can_hold = false;

            if (!spawnPiece(block)) {
                gameover = true;
                return;
            }

            sfx.playFull(assets.sounds.hold);
        }

        // Run gravity
        if (piece != null) {
            if (gravity_start == null)
                gravity_start = counter;

            let delay = computeGravity(level);

            // Don't add bonus points if turbo is slow
            turbo &&= (delay >= rules.TURBO_DELAY);

            if (turbo)
                delay = rules.TURBO_DELAY;

            while (counter >= gravity_start + delay) {
                gravity_start = counter;

                if (!isPieceFloating(piece))
                    break;

                piece.row--;
                piece.actions = 0;

                if (turbo)
                    score++;
            }
        }

        // Compute ghost
        if (piece != null) {
            ghost = Object.assign({}, piece);

            while (isPieceFloating(ghost))
                ghost.row--;
        } else {
            ghost = null;
        }

        // Lock piece?
        if (piece != null) {
            if (drop) {
                let delta = piece.row - ghost.row;

                piece.row = ghost.row;

                if (!isPieceValid(piece, true)) {
                    gameover = true;
                    return;
                }

                lockAndScore();

                hit_force += 4 + Math.min(3, delta / 2);
                sfx.playFull(assets.sounds.lock);

                score += 2 * delta;
            } else {
                if (isPieceFloating(piece)) {
                    lock_start = null;
                } else if (lock_start == null) {
                    lock_start = counter;
                    lock_since = counter;
                }

                if (lock_start != null && counter >= lock_start + rules.LOCK_DELAY) {
                    if (!isPieceValid(piece, true)) {
                        gameover = true;
                        return;
                    }

                    lockAndScore();

                    hit_force = 4;
                    sfx.playFull(assets.sounds.lock);
                }
            }
        }

        // Clear stack after clear delay
        if (clear_start != null && counter >= clear_start + rules.CLEAR_DELAY) {
            clearStack(clear_rows);
            clear_start = null;
        }

        // Generate new piece if needed
        if (piece == null && clear_start == null) {
            let block = bag_draw.shift();

            if (!spawnPiece(block)) {
                gameover = true;
                return;
            }

            can_hold = rules.HOLD;
        }
    }

    function computeGravity(level) {
        if (level < 20) {
            let speed = level - 1;
            let delay = rules.UPDATE_FREQUENCY * Math.pow(0.8 - (speed * 0.007), speed);

            return delay;
        } else {
            return 0;
        }
    }

    function spawnPiece(block) {
        piece = {
            row: rules.ROWS - block.top,
            column: Math.floor(rules.COLUMNS / 2 - block.size / 2),

            block: block,
            ...block,
            angle: 0,

            actions: 0
        };

        if (!isPieceValid(piece, false)) {
            for (let i = 0; i < 2; i++) {
                piece.row++;

                if (isPieceValid(piece, false))
                    break;
            }

            if (!isPieceValid(piece, false)) {
                piece = null;
                return false;
            }
        }

        ghost = null;
        gravity_start = counter;
        lock_start = null;
        special = null;

        return true;
    }

    function isPieceFloating(piece) {
        let edit = Object.assign({}, piece);
        edit.row--;;

        return isPieceValid(edit, false);
    }

    function isPieceValid(piece, strict) {
        let bottom = rules.ROWS;
        let left = rules.COLUMNS;
        let right = 0;

        for (let i = 0; i < piece.size; i++) {
            for (let j = 0; j < piece.size; j++) {
                if (!piece.shape[i * piece.size + j])
                    continue;

                let row = piece.row + i;
                let column = piece.column + j;

                if (grid[row * rules.COLUMNS + column] >= 0)
                    return false;

                bottom = Math.min(bottom, row);
                left = Math.min(left, column);
                right = Math.max(right, column + 1);
            }
        }

        if (bottom < 0)
            return false;
        if (strict && bottom >= rules.ROWS)
            return false;
        if (left < 0)
            return false;
        if (right > rules.COLUMNS)
            return false;

        return true;
    }

    function rotatePiece(piece) {
        piece.shape = rotateShape(piece.size, piece.shape);
        piece.angle = (piece.angle + 1) % 4;
    }

    function rotateShape(size, shape) {
        let rotated = new Uint8Array(size * size);

        for (let i = 0; i < size; i++) {
            for (let j = 0; j < size; j++) {
                let i2 = size - 1 - j;
                let j2 = i;

                rotated[i2 * size + j2] = shape[i * size + j];
            }
        }

        return rotated;
    }

    function detectTSpin(piece, kick) {
        if (kick[0] || kick[1])
            return null;

        let front = 0b000_000_101;
        let back = 0b101_000_000;

        for (let i = 0; i < piece.angle; i++) {
            front = rotateShape(3, front);
            back = rotateShape(3, back);
        }

        let major = 0;
        let minor = 0;

        for (let i = 0; i < 3; i++) {
            for (let j = 0; j < 3; j++) {
                let row = piece.row + i;
                let column = piece.column + j;

                major += front[i * 3 + j] && (grid[row * rules.COLUMNS + column] >= 0);
                minor += back[i * 3 + j] && (grid[row * rules.COLUMNS + column] >= 0);
            }
        }

        if (major == 2 && minor) {
            return 'T';
        } else if (minor == 2 && major) {
            return 'miniT';
        } else {
            return null;
        }
    }

    function lockAndScore() {
        for (let i = 0; i < piece.size; i++) {
            for (let j = 0; j < piece.size; j++) {
                if (!piece.shape[i * piece.size + j])
                    continue;

                let row = piece.row + i;
                let column = piece.column + j;

                grid[row * rules.COLUMNS + column] = piece.value;
            }
        }

        let [clears, perfect] = listClears();

        if (clears.length) {
            clear_start = counter;
            clear_rows = clears;
        } else {
            clear_start = null;
        }
        clears = clears.length;

        let action = 0;

        switch (clears) {
            case 1: {
                action += 100 * level;
                logAction(T.bonus_single);
            } break;
            case 2: {
                action += 300 * level;
                logAction(T.bonus_double);
            } break;
            case 3: {
                action += 500 * level;
                logAction(T.bonus_triple);
            } break;
            case 4: {
                action += 800 * level;
                logAction(T.bonus_quad);
            } break;
        }

        if (clears) {
            combo++;
            action += 50 * combo * level;

            sfx.playFull(assets.sounds.clear);

            if (combo)
                logAction(T.format(T.bonus_combo_x, combo));
        } else {
            combo = -1;
        }

        switch (special) {
            case 'T': {
                switch (clears) {
                    case 0: { action += 400; } break;
                    case 1: { action += 800; } break;
                    case 2: { action += 1200; } break;
                    case 3: { action += 1600; } break;
                }

                logAction(T.bonus_t);
            } break;
            case 'miniT': {
                switch (clears) {
                    case 0: { action += 100; } break;
                    case 1: { action += 200; } break;
                    case 2: { action += 400; } break;
                    case 3: { /* Apparently impossible */ } break;
                }

                logAction(T.bonus_minit);
            } break;
        }

        if (perfect) {
            switch (clears) {
                case 1: { action += 800 * level; } break;
                case 2: { action += 1200 * level; } break;
                case 3: { action += 1800 * level; } break;
                case 4: { action += 2000 * level; } break;
            }

            logAction(T.bonus_perfect);
        }

        back2back &&= (special == 'T' || clears == 4);
        if (back2back) {
            action *= 1.5;
            logAction(T.bonus_b2b);
        }
        back2back = (special != null || clears == 4);

        lines += clears;
        score += action;

        let new_level = 1 + Math.floor(lines / 10);

        if (new_level > level) {
            level = new_level;

            sfx.playFull(assets.sounds.levelup);
            logAction(T.bonus_levelup);
        }

        piece = null;
    }

    function listClears() {
        let clears = [];
        let perfect = true;

        for (let row = 0; row < rules.ROWS; row++) {
            let start = row * rules.COLUMNS;
            let end = start + rules.COLUMNS;
            let cells = grid.subarray(start, end);

            let clear = cells.every(value => value >= 0);
            let empty = cells.every(value => value < 0);

            if (clear) {
                let start = Math.max(counter, counter + rules.CLEAR_DELAY - 40);

                for (let column = 0; column < rules.COLUMNS; column++) {
                    let value = cells[column];

                    if (value >= 0) {
                        let color = rules.BLOCKS[value].color;
                        emitParticles(row, column, start, 100, color);
                    }
                }

                clears.push(row);
            } else if (!empty) {
                perfect = false;
            }
        }

        return [clears, perfect];
    }

    function clearStack(rows) {
        for (let i = rows.length - 1; i >= 0; i--) {
            let row = rows[i];

            let start = row * rules.COLUMNS;
            let end = start + rules.COLUMNS;

            grid.copyWithin(start, end, grid.length);
        }
    }

    function emitParticles(row, column, start, count, color) {
        color = '#' + color.toString(16).padStart(6, '0');

        for (let i = 0; i < count; i++) {
            let particle = {
                column: column,
                row: row,
                color: color,

                start: start,
                health: Util.getRandomFloat(0.4, 2),
                size: Util.getRandomInt(4, 8),
                vx: Util.getRandomFloat(-1.5, 1.5),
                vy: Util.getRandomFloat(-1, 1)
            };

            particles.push(particle);
        }
    }

    function logAction(action) {
        console.log(`${action}!`);
    }

    this.draw = function() {
        drawWell();
        drawParticles();
        drawBag();
        drawLevel();
        drawScore();
        drawHold();

        if (gameover || pause) {
            ctx.save();

            ctx.translate(layout.well.left, layout.well.top);

            let x = layout.well.width / 2;
            let y = layout.well.height / 2;
            let text = gameover ? 'GAME OVER' : 'PAUSE';

            ctx.font = 'bold ' + font(32);
            ctx.fillStyle = 'white';
            ctx.globalAlpha = 1;
            runner.text(x, y, text, { align: 5 });

            ctx.restore();
        }
    };

    function drawWell() {
        ctx.save();

        ctx.translate(layout.well.left, layout.well.top + hit_value);
        drawArea(0, 0, layout.well.width, layout.well.height);

        if (pause) {
            ctx.restore();
            return;
        }

        if (gameover)
            ctx.globalAlpha = 0.1;

        // Draw grid
        if (!gameover) {
            ctx.save();

            ctx.globalAlpha *= 0.03;
            ctx.strokeStyle = 'white';

            for (let y = layout.well.height - layout.square; y > 0; y -= layout.square) {
                ctx.beginPath();
                ctx.moveTo(0, y + 0.5);
                ctx.lineTo(layout.well.width, y + 0.5);

                ctx.stroke();
            }

            for (let x = layout.square; x < layout.well.width; x += layout.square) {
                ctx.beginPath();
                ctx.moveTo(x + 0.5, 0);
                ctx.lineTo(x + 0.5, layout.well.height);

                ctx.stroke();
            }

            ctx.restore();
        }

        let adjust = layout.well.height - rules.ROWS * layout.square;
        ctx.translate(0, adjust);

        // Draw stack
        for (let i = 0; i < grid.length; i++) {
            let value = grid[i];

            if (value >= 0) {
                ctx.save();

                let row = Math.floor(i / rules.COLUMNS);
                let column = i % rules.COLUMNS;

                let x = column * layout.square;
                let y = (rules.ROWS - row - 1) * layout.square;

                let color = rules.BLOCKS[value].color;

                if (clear_start != null && clear_rows.includes(row)) {
                    let progress = (counter - clear_start) / rules.CLEAR_DELAY;
                    let factor = progress < 0.5 ? (8 * Math.pow(progress, 4))
                                                : (1 - Math.pow(-2 * progress + 2, 4) / 2);

                    color = mixColors(0xFFFFFF, color, factor);
                }

                drawSquare(x, y, layout.square, color);

                ctx.restore();
            }
        }

        // Draw ghost
        if (show_ghost && ghost != null) {
            ctx.save();

            ctx.globalAlpha *= 0.1;
            drawPiece(ghost, 0xFFFFFF);

            ctx.restore();
        }

        // Draw piece
        if (piece != null) {
            ctx.save();

            if (lock_start != null) {
                let delay = counter - lock_since;
                let alpha = 0.7 + Math.cos(delay / 24) * 0.3;

                ctx.globalAlpha *= alpha;
            }
            drawPiece(piece);

            ctx.restore();
        }

        ctx.restore();
    }

    function drawBag() {
        ctx.save();

        ctx.translate(layout.bag.left, layout.bag.top + hit_value);
        drawArea(0, 0, layout.bag.width, layout.bag.height);

        if (pause) {
            ctx.restore();
            return;
        }

        let square = layout.square * 0.5;

        for (let i = 0; i < rules.BAG_SIZE; i++) {
            let block = bag_draw[i];

            if (block != null) {
                let x = layout.bag.width / 2 - (block.size * square) / 2;
                let y = (2.5 + i * 4 + block.middle - block.size) * square;

                drawShape(x, y, block.size, block.shape, square, block.color);
            }
        }

        ctx.restore();
    }

    function drawLevel() {
        ctx.save();

        ctx.translate(layout.level.left, layout.level.top + hit_value);
        drawArea(0, 0, layout.level.width, layout.level.height);

        ctx.font = font(runner.isTouch ? 24 : 16);
        ctx.fillStyle = 'white';
        runner.text(layout.level.width / 2, layout.level.height / 2, level, { align: 5 });

        ctx.restore();
    }

    function drawScore() {
        ctx.save();

        ctx.translate(layout.score.left, layout.score.top + hit_value);
        drawArea(0, 0, layout.score.width, layout.score.height);

        ctx.font = font(runner.isTouch ? 24 : 16);
        ctx.fillStyle = 'white';
        runner.text(layout.score.width / 2, layout.score.height / 2, score, { align: 5 });

        ctx.restore();
    }

    function drawHold() {
        if (!rules.HOLD)
            return;

        ctx.save();

        ctx.translate(layout.hold.left, layout.hold.top + hit_value);
        drawArea(0, 0, layout.hold.width, layout.hold.height);

        if (pause) {
            ctx.restore();
            return;
        }

        let square = layout.square * 0.5;

        if (hold_block != null) {
            let block = hold_block;

            let x = layout.hold.width / 2 - (block.size / 2) * square;
            let y = layout.hold.height / 2 + (block.middle - block.size) * square;

            if (can_hold) {
                drawShape(x, y, block.size, block.shape, square, block.color);
            } else {
                ctx.globalAlpha = 0.1;
                drawShape(x, y, block.size, block.shape, square, 0xFFFFFF);
            }
        }

        ctx.restore();
    }

    // Also cleans them up
    function drawParticles() {
        ctx.save();

        ctx.translate(layout.well.left, layout.well.top);

        let j = 0;
        for (let i = 0; i < particles.length; i++) {
            let particle = particles[i];

            let delay = counter - particle.start;
            let alpha = Math.max(0, 1 - delay / particle.health / rules.UPDATE_FREQUENCY);

            particles[j] = particle;
            j += (alpha > 0);

            if (delay >= 0) {
                let x = particle.column * layout.square + layout.square / 2;
                let y = (rules.ROWS - particle.row - 1) * layout.square + layout.square / 2;

                x += delay * particle.vx;
                y += delay * (particle.vy + 0.008 * delay);

                ctx.globalAlpha = alpha;
                ctx.fillStyle = particle.color;
                ctx.fillRect(x - particle.size / 2, y - particle.size / 2, particle.size, particle.size);
            }
        }
        particles.length = j;

        ctx.restore();
    }

    function drawPiece(piece, color = null) {
        if (color == null)
            color = piece.color;

        let x = piece.column * layout.square;
        let y = (rules.ROWS - piece.row - piece.size) * layout.square;

        drawShape(x, y, piece.size, piece.shape, layout.square, color);
    }

    function drawShape(x, y, size, shape, pixels, color) {
        y += (size - 1) * pixels;

        for (let i = 0; i < size; i++) {
            for (let j = 0; j < size; j++) {
                if (!shape[i * size + j])
                    continue;

                let x2 = x + j * pixels;
                let y2 = y - i * pixels;

                drawSquare(x2, y2, pixels, color);
            }
        }
    }

    function drawSquare(x, y, pixels, color) {
        ctx.beginPath();
        ctx.rect(x + 0.5, y + 0.5, pixels - 0.5, pixels - 0.5);

        ctx.fillStyle = '#' + color.toString(16).padStart(6, '0');
        ctx.fill();
    }

    function mixColors(color1, color2, factor) {
        let r1 = (color1 >> 16) & 0xFF;
        let g1 = (color1 >> 8) & 0xFF;
        let b1 = (color1 >> 0) & 0xFF;
        let r2 = (color2 >> 16) & 0xFF;
        let g2 = (color2 >> 8) & 0xFF;
        let b2 = (color2 >> 0) & 0xFF;

        let r = r1 * factor + r2 * (1 - factor);
        let g = g1 * factor + g2 * (1 - factor);
        let b = b1 * factor + b2 * (1 - factor);
        let color = (r << 16) | (g << 8) | b;

        return color;
    }
}

// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

function font(size) {
    if (!runner.isTouch)
        size *= window.devicePixelRatio;

    return size + 'px Open Sans';
}

function isInsideRect(x, y, rect) {
    let inside = (x >= rect.left &&
                  x <= rect.left + rect.width &&
                  y >= rect.top &&
                  y <= rect.top + rect.height);
    return inside;
}

function expandRect(rect, sx, sy) {
    let expanded = {
        left: rect.left - sx,
        top: rect.top - sy,
        width: rect.width + sx * 2,
        height: rect.height + sy * 2
    };

    return expanded;
}

export {
    load,
    start
}
