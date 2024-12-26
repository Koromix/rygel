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

import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from '../../web/core/base.js';
import { AppRunner } from '../../web/core/runner.js';
import { loadAssets, assets } from './assets.js';
import * as rules from './rules.js';

import '../../../vendor/opensans/OpenSans.css';
import './game.css';

const KEYBOARD_SHORTCUTS = [
    ['P', 'Mettre en pause'],
    ['F', `Mode plein écran`],
    null,
    ['← / →', `Déplacement latéral`],
    ['↑', `Rotation horaire`],
    ['Control', `Rotation anti-horaire`],
    ['↓', `Descente rapide`],
    ['Espace', `Verrouillage immédiat`],
    null,
    ['C', `Mettre la pièce de côté (hold)`],
    null,
    ['G', `Afficher/cacher le fantôme`],
    ['B', `Modifier le fond d'écran`],
    ['M', 'Activer/désactiver le son'],
    null,
    ['H', `Afficher/cacher l'aide`]
];

const BUTTON_SIZE = 56;
const SQUARE_SIZE = 64;

const DEFAULT_SETTINGS = {
    background: 'aurora',
    sound: true,
    music: 'serious_corporate',
    ghost: true,
    help: true
};

// DOM nodes
let main = null;
let canvas = null;
let attributions = null;

// Render and input API
let runner = null;
let ctx = null;
let mouse_state = null;
let pressed_keys = null;

// Game state
let game = new Game;

// User settings
let settings = Object.assign({}, DEFAULT_SETTINGS);
let show_debug = false;

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
    ui: null
};
let buttons = new Map;
let prev_buttons = null;
let menu_lines = null;

// ------------------------------------------------------------------------
// Init
// ------------------------------------------------------------------------

async function load(prefix, progress = null) {
    await loadAssets(prefix, progress);

    loadSettings();

    for (let i = 0; i < rules.BLOCKS.length; i++) {
        let block = rules.BLOCKS[i];

        let trail = ctz(block.shape);
        let lead = clz(block.shape) - (32 - block.size * block.size);

        block.value = i;

        block.top = block.size - Math.floor(trail / block.size);
        block.bottom = Math.floor(lead / block.size);
    }
}

async function start(root) {
    render(html`
        <div class="stk_game">
            <div class="stk_attributions">
                Musiques par <a href="https://freesound.org/people/AudioCoffee/" target="_blank">AudioCoffee</a><br>
                Fonds d'écran par <a href="https://www.deviantart.com/psiipilehto/" target="_blank">psiipilehto</a>, <a href="https://fr.m.wikipedia.org/wiki/Fichier:Fomalhaut_planet.jpg" target=_"blank">NASA et ESA</a>
            </div>
            <canvas class="stk_canvas"></canvas>
        </div>
    `, root);
    main = root.querySelector('.stk_game');
    canvas = root.querySelector('.stk_canvas');
    attributions = root.querySelector('.stk_attributions');

    runner = new AppRunner(canvas);
    ctx = canvas.getContext('2d');
    mouse_state = runner.mouseState;
    pressed_keys = runner.pressedKeys;

    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';

    runner.updateFrequency = 120;
    runner.onUpdate = update;
    runner.onDraw = draw;
    runner.start();
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
        if (settings.hasOwnProperty(key))
            settings[key] = user[key];
    }

    if (!assets.backgrounds.hasOwnProperty(settings.background))
        settings.background = DEFAULT_SETTINGS.background;
    if (!assets.musics.hasOwnProperty(settings.music))
        settings.music = DEFAULT_SETTINGS.music;
}

function saveSettings() {
    let json = JSON.stringify(settings);
    localStorage.setItem('staks', json);
}

// ------------------------------------------------------------------------
// Main
// ------------------------------------------------------------------------

function update() {
    // Reset UI buttons
    prev_buttons = buttons;
    buttons = new Map;

    // Global layout
    {
        let padding = 16;

        let width = canvas.width - padding * 2;
        let height = canvas.height - padding * 2;

        // Account for touch controls
        if (runner.isTouch)
            height -= 100 * window.devicePixelRatio + padding;

        // Make sure things fit, and square size divides by two nicely
        let square = Math.floor(Math.min(
            Math.min(width / rules.COLUMNS, height / (rules.ROWS + rules.EXTRA_TOP)),
            SQUARE_SIZE * window.devicePixelRatio
        ) / 2) * 2;

        width = rules.COLUMNS * square;
        height = (rules.ROWS + rules.EXTRA_TOP) * square;

        let left = canvas.width / 2 - width / 2;
        let top = runner.isTouch ? padding : (canvas.height / 2 - height / 2);

        layout.padding = padding;
        layout.square = square;
        layout.button = BUTTON_SIZE * window.devicePixelRatio;

        layout.well = {
            left: left,
            top: top,
            width: width,
            height: height
        };
        layout.bag = {
            left: left + (rules.COLUMNS * layout.square) + padding,
            top: top,
            width: (runner.isTouch ? 7 : 5) / 2 * layout.square,
            height: (1 + rules.VISIBLE_BAG_SIZE * 4) / 2 * layout.square
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

        if (runner.isTouch) {
            let bottom = top + height;

            layout.hold = {
                left: layout.bag.left,
                top: bottom - 2.5 * layout.square,
                width: layout.bag.width,
                height: 2.5 * layout.square
            };

            layout.help = null;
            layout.ui = {
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
                height: 2.5 * layout.square
            };

            layout.help = {
                right: left - padding,
                bottom: top + height
            };
            layout.ui = null;
        }
    }

    // Global touch buttons
    if (runner.isTouch || game.isPlaying) {
        let sound_key = settings.sound ? 'sound' : 'silence';
        let pause_key = game.pause ? 'play' : 'pause';

        let size = 0.6 * layout.button;
        let x = runner.isTouch ? (20 + size / 2) : (layout.bag.left + size / 2);
        let y = runner.isTouch ? (20 + size / 2) : (layout.well.top + layout.well.height - size / 2);
        let delta = runner.isTouch ? (size + 20) : -(size + 20);

        if (button(sound_key, x, y, size).clicked) {
            settings.sound = !settings.sound;
            saveSettings();
        }
        y += delta;

        if (button('background', x, y, size).clicked)
            toggleBackground();
        y += delta;

        if (main.requestFullscreen != null) {
            if (button('fullscreen', x, y, size).clicked)
                toggleFullScreen();
            y += delta;
        }

        if (game.isPlaying && runner.isTouch) {
            if (button(pause_key, x, y, size).clicked)
                game.pause = !game.pause;
            y += delta;
        }

        if (game.isPlaying && !runner.isTouch && !settings.help) {
            let clicked = button('help', layout.well.left - layout.padding - size / 2,
                                         layout.well.top + layout.well.height - size / 2, size).clicked;

            if (clicked) {
                settings.help = true;
                game.pause = true;
            }
        }
    }

    // Simple shortcuts
    if (!runner.isTouch) {
        if (pressed_keys.p == 1 || pressed_keys.pause == 1)
            game.pause = !game.pause;
        if (pressed_keys.g == 1) {
            settings.ghost = !settings.ghost;
            saveSettings();
        }
        if (pressed_keys.b == 1)
            toggleBackground();
        if (pressed_keys.m == 1) {
            settings.sound = !settings.sound;
            saveSettings();
        }
        if (pressed_keys.h == 1) {
            settings.help = !settings.help;
            saveSettings();
        }
        if (pressed_keys.f == 1)
            toggleFullScreen();
        if (pressed_keys.tab == 1)
            show_debug = !show_debug;
    }

    runner.volume = settings.sound ? 1 : 0;

    // Play music
    if (game.hasStarted) {
        let sound = assets.musics[settings.music];
        let handle = runner.playOnce(sound);

        if (handle.ended) {
            let musics = Object.keys(assets.musics);
            let idx = musics.indexOf(settings.music);
            let next = (idx + 1) % musics.length;

            settings.music = musics[next] ?? null;
            saveSettings();
        }
    }

    if (!game.hasStarted) {
        menu_lines = [
            [0, '28pt Open Sans', runner.isTouch ? `Touchez l'écran pour commencer`
                                                 : `Appuyez sur la touche entrée ⏎\uFE0E pour commencer`]
        ];

        if (mouse_state.left == -1 || pressed_keys.return == -1) {
            play();
            pressed_keys.space = 0;
        }
    } else if (game.isOver) {
        menu_lines = [
            [80, '24pt Open Sans', `Bien joué !`],
            [38, '24pt Open Sans', `Niveau`],
            [20, 'bold 40pt Open Sans', game.level],
            [38, '24pt Open Sans', `Score`],
            [80, 'bold 40pt Open Sans', game.score]
        ];

        if (runner.isTouch) {
            if (button('retry', canvas.width / 2, 3 * canvas.height / 4, layout.button).clicked)
                play();
        } else {
            menu_lines.push([0, '24pt Open Sans', `Appuyez sur la touche entrée ⏎\uFE0E pour recommencer`]);

            if (pressed_keys.return == -1) {
                play();
                pressed_keys.space = 0;
            }
        }

        runner.playOnce(assets.sounds.gameover, false);
    }

    // Apply user settings
    game.showGhost = settings.ghost;

    let now = performance.now();
    game.update(now);

    canvas.focus();
}

function toggleFullScreen() {
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
    let backgrounds = Object.keys(assets.backgrounds);
    let idx = backgrounds.indexOf(settings.background);
    let next = (idx + 1) % backgrounds.length;

    settings.background = backgrounds[next];
    saveSettings();
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

    if (game.isPlaying) {
        attributions.classList.remove('active');
        game.draw();

        if (settings.help && layout.help != null)
            drawHelp();
    } else {
        attributions.classList.add('active');
        drawMenu();
    }

    drawButtons();

    if (show_debug) {
        ctx.save();

        ctx.font = '18px Open Sans';
        ctx.fillStyle = 'white';

        // FPS counter
        {
            let text = `FPS : ${(1000 / runner.frameTime).toFixed(0)} (${runner.frameTime.toFixed(1)} ms)` +
                       ` | Update : ${runner.updateTime.toFixed(1)} ms | Draw : ${runner.drawTime.toFixed(1)} ms`;
            runner.text(canvas.width - 12, 12, text, { align: 9 });
        }

        ctx.restore();
    }
}

function drawMenu() {
    let font = '20px Open Sans';
    ctx.fillStyle = 'white';

    let lines = menu_lines.map(line => {
        ctx.font = line[1];

        let m = runner.measure(line[2]);
        let obj = {
            font: line[1],
            text: line[2],
            height: m.height + line[0]
        };

        return obj;
    });

    let y = canvas.height / 2 - lines.reduce((acc, line) => acc + line.height, 0) / 2;

    for (let line of lines) {
        ctx.font = line.font;
        runner.text(canvas.width / 2, y, line.text, { align: 5 });

        y += line.height;
    }
}

function drawHelp() {
    ctx.save();

    ctx.font = '14px Open Sans';
    ctx.fillStyle = 'white';

    let x = layout.help.right;
    let y = layout.help.bottom;
    let width = 0;
    let height = 18;

    for (let shortcut of KEYBOARD_SHORTCUTS) {
        if (shortcut != null) {
            width = Math.max(width, 114 + runner.measure(shortcut[1]).width);
            height += 8;
        }

        height += 10;
    }

    drawArea(x - width, y - height, width, height);
    y -= 12;

    for (let i = KEYBOARD_SHORTCUTS.length - 1; i >= 0; i--) {
        let shortcut = KEYBOARD_SHORTCUTS[i];

        if (shortcut != null) {
            runner.text(x - 12, y, shortcut[0], { align: 3 });
            runner.text(x - 92, y, shortcut[1], { align: 3 });

            y -= 8;
        }

        y -= 10;
    }

    ctx.restore();
}

function drawArea(x, y, width, height) {
    ctx.beginPath();
    ctx.rect(x, y, width, height);

    ctx.save();

    ctx.fillStyle = 'black';
    ctx.globalAlpha = 0.75;
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

    // Well
    let grid = new Int8Array((rules.ROWS + rules.EXTRA_ROWS) * rules.COLUMNS);
    grid.fill(-1);

    // Pieces
    let bag = [];
    let piece = null;

    // Actions
    let das = null;
    let hold_block = null;
    let can_hold = true;
    let ghost = null;
    let gravity = null;
    let locking = null;

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

    this.update = function(now) {
        if (!started || gameover)
            return;

        // Increase level manually
        if (isInsideRect(mouse_state.x, mouse_state.y, layout.level)) {
            if (mouse_state.left == 1)
                level++;

            runner.cursor = 'pointer';
            mouse_state.left = 0;
        }

        if (pause)
            return;

        // Draw next pieces
        if (bag.length < rules.BLOCKS.length) {
            let draw = Util.shuffle(rules.BLOCKS);
            bag.push(...draw);
        }

        // Generate new piece if needed
        if (piece == null) {
            let block = bag.shift();

            if (!spawnPiece(block)) {
                gameover = true;
                return;
            }

            can_hold = true;
        }

        let move = 0;
        let rotate = false;
        let turbo = false;
        let drop = false;
        let hold = false;

        // Respond to user input
        if (runner.isTouch) {
            move = 0;
            move += button('right', layout.ui.left + 0.25 * layout.ui.width + 0.7 * layout.button,
                                    layout.ui.top + 0.3 * layout.ui.height, layout.button).pressed >= 1;
            move -= button('left', layout.ui.left + 0.25 * layout.ui.width - 0.7 * layout.button,
                                   layout.ui.top + 0.3 * layout.ui.height, layout.button).pressed >= 1;

            rotate = 0;
            rotate += button('clockwise', layout.ui.left + 0.75 * layout.ui.width + 0.7 * layout.button,
                                          layout.ui.top + 0.5 * layout.ui.height, layout.button).clicked;
            rotate -= button('counterclock', layout.ui.left + 0.75 * layout.ui.width - 0.7 * layout.button,
                                             layout.ui.top + 0.5 * layout.ui.height, layout.button).clicked;

            turbo = button('turbo', layout.ui.left + 0.25 * layout.ui.width,
                                    layout.ui.top + 0.85 * layout.ui.height, 0.7 * layout.button).pressed >= 1;

            if (isInsideRect(mouse_state.x, mouse_state.y, layout.hold)) {
                if (mouse_state.left == 1)
                    hold = true;
                mouse_state.left = 0;
            }

            if (isInsideRect(mouse_state.x, mouse_state.y, expandRect(layout.well, -40, -40))) {
                if (mouse_state.left == 1)
                    drop = true;
                mouse_state.left = 0;
            }
        } else {
            move = (pressed_keys.right >= 1) - (pressed_keys.left >= 1);
            rotate = (pressed_keys.up == 1 || pressed_keys.x == 1) - (pressed_keys.ctrl == 1 || pressed_keys.z == 1);
            turbo = (pressed_keys.down >= 1);
            drop = (pressed_keys.space == 1);
            hold = (pressed_keys.c == 1);
        }

        // Hold piece when requested
        if (hold && can_hold) {
            let block = hold_block ?? bag.shift();

            hold_block = piece.block;
            can_hold = false;

            if (!spawnPiece(block)) {
                gameover = true;
                return;
            }

            runner.playOnce(assets.sounds.hold);

            return;
        }

        // Perform rotation
        if (rotate && piece.actions < rules.MAX_ACTIONS) {
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

                if (locking != null)
                    locking.time = now;
                piece.actions++;

                if (piece.type == 'T') {
                    special = detectTSpin(piece, valid);
                } else {
                    special = null;
                }

                runner.playOnce(assets.sounds.move);
            }
        }

        // Perform lateral movement
        if (move) {
            let first = (das == null);

            if (first) {
                das = now + rules.DAS_DELAY;
            } else if (now >= das) {
                das = now + rules.DAS_PERIOD;
            } else {
                move = 0;
            }

            let edit = Object.assign({}, piece);
            edit.column += move;

            if (isPieceValid(edit, false)) {
                piece.column = edit.column;
                if (piece.actions < rules.MAX_ACTIONS && locking != null)
                    locking.time = now;
                piece.actions += first;

                special = null;

                runner.playOnce(assets.sounds.move);
            }
        } else {
            das = null;
        }

        // Compute ghost
        {
            ghost = Object.assign({}, piece);

            while (isPieceFloating(ghost))
                ghost.row--;
        }

        // Run gravity
        {
            if (gravity == null)
                gravity = now;

            let speed = Math.min(20, level);
            let delay = 1000 * Math.pow(0.8 - ((speed - 1) * 0.007), speed - 1);

            if (turbo && delay > rules.TURBO_DELAY)
                delay = rules.TURBO_DELAY;

            if (now >= gravity + delay) {
                if (isPieceFloating(piece)) {
                    piece.row--;
                    piece.actions = 0;

                    if (turbo)
                        score++;
                }

                gravity = now;
            }
        }

        // Lock piece?
        if (drop && !turbo) {
            let delta = piece.row - ghost.row;

            piece.row = ghost.row;

            if (!isPieceValid(piece, true)) {
                gameover = true;
                return;
            }

            lockPiece();

            score += 2 * delta;
        } else {
            if (isPieceFloating(piece)) {
                locking = null;
            } else if (locking == null) {
                locking = {
                    time: now,
                    frame: runner.drawCounter
                };
            }

            if (locking != null && now >= locking.time + rules.LOCK_DELAY) {
                if (!isPieceValid(piece, true)) {
                    gameover = true;
                    return;
                }

                lockPiece();
            }
        }

        // Clear grid and score
        if (piece == null) {
            let clears = clearStack();
            let perfect = grid.every(value => value < 0);

            let action = 0;

            switch (clears) {
                case 1: { action += 100 * level; } break;
                case 2: { action += 300 * level; } break;
                case 3: { action += 500 * level; } break;
                case 4: { action += 800 * level; } break;
            }

            if (clears) {
                combo++;
                action += 50 * combo * level;

                runner.playOnce(assets.sounds.clear);
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
                } break;
                case 'miniT': {
                    switch (clears) {
                        case 0: { action += 100; } break;
                        case 1: { action += 200; } break;
                        case 2: { action += 400; } break;
                        case 3: { /* Apparently impossible */ } break;
                    }
                } break;
            }

            if (perfect) {
                switch (clears) {
                    case 1: { action += 800 * level; } break;
                    case 2: { action += 1200 * level; } break;
                    case 3: { action += 1800 * level; } break;
                    case 4: { action += 2000 * level; } break;
                }
            }

            back2back &&= (special == 'T' || clears == 4);
            if (back2back)
                action *= 1.5;
            back2back = (special != null || clears == 4);

            lines += clears;
            score += action;

            let new_level = 1 + Math.floor(lines / 10);

            if (new_level > level) {
                level = new_level;
                runner.playOnce(assets.sounds.levelup);
            }
        }
    }

    function spawnPiece(block, now) {
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

            if (!isPieceValid(piece, false))
                return false;
        }

        das = null;
        gravity = now;
        locking = null;
        special = null;

        return true;
    }

    function lockPiece() {
        runner.playOnce(assets.sounds.lock);

        for (let i = 0; i < piece.size; i++) {
            for (let j = 0; j < piece.size; j++) {
                if (!testShape(piece.size, piece.shape, i, j))
                    continue;

                let row = piece.row + i;
                let column = piece.column + j;
                let idx = (row * rules.COLUMNS) + column;

                grid[idx] = piece.value;
            }
        }

        piece = null;
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
                if (!testShape(piece.size, piece.shape, i, j))
                    continue;

                let row = piece.row + i;
                let column = piece.column + j;
                let idx = (row * rules.COLUMNS) + column;

                if (grid[idx] >= 0)
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
        let rotated = 0;

        switch (size) {
            case 2: {
                rotated |= (shape & 0b10_00) ? 0b00_10 : 0;
                rotated |= (shape & 0b01_00) ? 0b10_00 : 0;
                rotated |= (shape & 0b00_10) ? 0b00_01 : 0;
                rotated |= (shape & 0b00_01) ? 0b01_00 : 0;
            } break;

            case 3: {
                rotated |= (shape & 0b100_000_000) ? 0b000_000_100 : 0;
                rotated |= (shape & 0b010_000_000) ? 0b000_100_000 : 0;
                rotated |= (shape & 0b001_000_000) ? 0b100_000_000 : 0;
                rotated |= (shape & 0b000_100_000) ? 0b000_000_010 : 0;
                rotated |= (shape & 0b000_010_000) ? 0b000_010_000 : 0;
                rotated |= (shape & 0b000_001_000) ? 0b010_000_000 : 0;
                rotated |= (shape & 0b000_000_100) ? 0b000_000_001 : 0;
                rotated |= (shape & 0b000_000_010) ? 0b000_001_000 : 0;
                rotated |= (shape & 0b000_000_001) ? 0b001_000_000 : 0;
            } break;

            case 4: {
                rotated |= (shape & 0b1000_0000_0000_0000) ? 0b0000_0000_0000_1000 : 0;
                rotated |= (shape & 0b0100_0000_0000_0000) ? 0b0000_0000_1000_0000 : 0;
                rotated |= (shape & 0b0010_0000_0000_0000) ? 0b0000_1000_0000_0000 : 0;
                rotated |= (shape & 0b0001_0000_0000_0000) ? 0b1000_0000_0000_0000 : 0;
                rotated |= (shape & 0b0000_1000_0000_0000) ? 0b0000_0000_0000_0100 : 0;
                rotated |= (shape & 0b0000_0100_0000_0000) ? 0b0000_0000_0100_0000 : 0;
                rotated |= (shape & 0b0000_0010_0000_0000) ? 0b0000_0100_0000_0000 : 0;
                rotated |= (shape & 0b0000_0001_0000_0000) ? 0b0100_0000_0000_0000 : 0;
                rotated |= (shape & 0b0000_0000_1000_0000) ? 0b0000_0000_0000_0010 : 0;
                rotated |= (shape & 0b0000_0000_0100_0000) ? 0b0000_0000_0010_0000 : 0;
                rotated |= (shape & 0b0000_0000_0010_0000) ? 0b0000_0010_0000_0000 : 0;
                rotated |= (shape & 0b0000_0000_0001_0000) ? 0b0010_0000_0000_0000 : 0;
                rotated |= (shape & 0b0000_0000_0000_1000) ? 0b0000_0000_0000_0001 : 0;
                rotated |= (shape & 0b0000_0000_0000_0100) ? 0b0000_0000_0001_0000 : 0;
                rotated |= (shape & 0b0000_0000_0000_0010) ? 0b0000_0001_0000_0000 : 0;
                rotated |= (shape & 0b0000_0000_0000_0001) ? 0b0001_0000_0000_0000 : 0;
            } break;
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
                let idx = (row * rules.COLUMNS) + column;

                major += testShape(3, front, i, j) && (grid[idx] >= 0);
                minor += testShape(3, back, i, j) && (grid[idx] >= 0);
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

    function clearStack() {
        let cleared = 0;

        for (let row = 0; row < rules.ROWS; row++) {
            let start = row * rules.COLUMNS;
            let end = start + rules.COLUMNS;

            let clear = grid.subarray(start, end).every(value => value >= 0);

            if (clear) {
                grid.copyWithin(start, end, grid.length);
                row--;

                cleared++;
            }
        }

        return cleared;
    }

    this.draw = function() {
        drawWell();
        drawBag();
        drawLevel();
        drawScore();
        drawHold();
    };

    function drawWell() {
        ctx.save();

        ctx.translate(layout.well.left, layout.well.top);
        drawArea(0, 0, layout.well.width, layout.well.height);

        if (pause) {
            let x = layout.well.width / 2;
            let y = layout.well.height / 2;

            ctx.font = '24pt Open Sans';
            ctx.fillStyle = 'white';
            runner.text(x, y, 'PAUSE', { align: 5 });

            ctx.restore();
            return;
        }

        // Draw grid
        {
            ctx.save();

            ctx.globalAlpha = 0.05;
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

        let adjust = Math.floor(rules.EXTRA_TOP * layout.square)
        ctx.translate(0, adjust);

        // Draw grid
        for (let i = 0; i < grid.length; i++) {
            let value = grid[i];

            if (value >= 0) {
                let row = Math.floor(i / rules.COLUMNS);
                let column = i % rules.COLUMNS;

                let x = column * layout.square;
                let y = (rules.ROWS - row - 1) * layout.square;

                let color = rules.BLOCKS[value].color;
                drawSquare(x, y, layout.square, color);
            }
        }

        // Draw ghost
        if (show_ghost && ghost != null) {
            ctx.save();

            ctx.globalAlpha = 0.1;
            drawPiece(ghost, 'white');

            ctx.restore();
        }

        // Draw piece
        if (piece != null) {
            ctx.save();

            if (locking != null) {
                let frames = runner.drawCounter - locking.frame;
                let alpha = 0.7 + Math.cos(frames / 2) * 0.3;

                ctx.globalAlpha = alpha;
            }
            drawPiece(piece);

            ctx.restore();
        }

        ctx.restore();
    }

    function drawBag() {
        ctx.save();

        ctx.translate(layout.bag.left, layout.bag.top);
        drawArea(0, 0, layout.bag.width, layout.bag.height);

        if (pause) {
            ctx.restore();
            return;
        }

        let square = layout.square * 0.5;

        for (let i = 0; i < rules.VISIBLE_BAG_SIZE; i++) {
            let block = bag[i];

            if (block != null) {
                let top = 0.5 + i * 4 + 2 - block.size / 2 - 0.5 * (block.size - block.top - block.bottom);
                let x = layout.bag.width / 2 - (block.size * square) / 2;
                let y = top * square;

                drawShape(x, y, block.size, block.shape, square, block.color);
            }
        }

        ctx.restore();
    }

    function drawLevel() {
        ctx.save();

        ctx.translate(layout.level.left, layout.level.top);
        drawArea(0, 0, layout.level.width, layout.level.height);

        ctx.font = '20pt Open Sans';
        ctx.fillStyle = 'white';
        runner.text(layout.level.width / 2, layout.level.height / 2, level, { align: 5 });

        ctx.restore();
    }

    function drawScore() {
        ctx.save();

        ctx.translate(layout.score.left, layout.score.top);
        drawArea(0, 0, layout.score.width, layout.score.height);

        ctx.font = '20pt Open Sans';
        ctx.fillStyle = 'white';
        runner.text(layout.score.width / 2, layout.score.height / 2, score, { align: 5 });

        ctx.restore();
    }

    function drawHold() {
        ctx.save();

        ctx.translate(layout.hold.left, layout.hold.top);
        drawArea(0, 0, layout.hold.width, layout.hold.height);

        if (pause) {
            ctx.restore();
            return;
        }

        let square = layout.square * 0.5;

        if (hold_block != null) {
            let block = hold_block;

            let top = 0.5 + 2 - block.size / 2 - 0.5 * (block.size - block.top - block.bottom);
            let x = layout.hold.width / 2 - (block.size * square) / 2;
            let y = top * square;

            let color = block.color;

            if (!can_hold) {
                ctx.globalAlpha = 0.1;
                color = 'white';
            }

            drawShape(x, y, block.size, block.shape, square, color);
        }

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
                if (!testShape(size, shape, i, j))
                    continue;

                let x2 = x + j * pixels;
                let y2 = y - i * pixels;

                drawSquare(x2, y2, pixels, color);
            }
        }
    }

    function drawSquare(x, y, pixels, color) {
        ctx.fillStyle = color;
        ctx.strokeStyle = '#00000022';
        ctx.lineWidth = 1;

        ctx.beginPath();
        ctx.rect(x + 0.5, y + 0.5, pixels, pixels);

        ctx.fill();
        ctx.stroke();
    }
}

// ------------------------------------------------------------------------
// Buttons
// ------------------------------------------------------------------------

function button(key, x, y, size) {
    let prev = prev_buttons.get(key);

    let img = assets.ui[key];

    let btn = {
        rect: {
            left: x - size / 2,
            top: y - size / 2,
            width: size,
            height: size,
        },
        pos: {
            x: x,
            y: y,
        },
        size: size,

        img: img,

        clicked: false,
        pressed: 0
    };

    if (isInsideRect(mouse_state.x, mouse_state.y, btn.rect)) {
        if (mouse_state.left == 1)
            btn.clicked = true;
        if (mouse_state.left >= 1)
            btn.pressed = (prev?.pressed ?? 0) + 1;

        runner.cursor = 'pointer';
        mouse_state.left = 0;
    }

    buttons.set(key, btn);

    let status = {
        clicked: prev?.clicked ?? false,
        pressed: prev?.pressed ?? 0
    };

    return status;
}

function drawButtons() {
    ctx.fillStyle = 'white';

    for (let btn of buttons.values()) {
        ctx.beginPath();
        ctx.arc(btn.pos.x, btn.pos.y, btn.size / 2, 0, 2 * Math.PI);
        ctx.fill();

        let size = 0.5 * btn.size;
        let left = btn.pos.x - size / 2;
        let top = btn.pos.y - size / 2;

        ctx.drawImage(btn.img, left, top, size, size);
    }
}

// ------------------------------------------------------------------------
// Utility
// ------------------------------------------------------------------------

function clz(i) {
    return Math.clz32(i);
}

function ctz(i) {
    i >>>= 0;

    if (i === 0)
        return 32;

    i &= -i;
    return 31 - clz(i);
}

function popcnt(i) {
    i = i - ((i >>> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >>> 2) & 0x33333333);
    i = (i + (i >>> 4)) & 0x0f0f0f0f;
    i = i + (i >>> 8);
    i = i + (i >>> 16);
    return i & 0x0000003f;
}

function testShape(size, shape, i, j) {
    let shift = size * (size - i) - j - 1;
    let mask = 1 << shift;

    let hit = !!(shape & mask);
    return hit;
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
