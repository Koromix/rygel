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

import { Util, Log } from '../../web/core/common.js';
import { AppRunner } from '../../web/core/runner.js';
import { loadAssets, assets } from './assets.js';
import * as rules from './rules.js';

import '../../../vendor/opensans/OpenSans.css';

const KEYBOARD_SHORTCUTS = [
    ['← / →', `Déplacement latéral`],
    ['↑', `Rotation horaire`],
    ['Control', `Rotation anti-horaire`],
    ['↓', `Descente rapide`],
    ['Espace', `Verrouillage immédiat`],
    null,
    ['C', `Mettre la pièce de côté (hold)`],
    null,
    ['P', 'Mettre en pause'],
    ['G', `Afficher/cacher le fantôme`],
    ['B', `Modifier le fond d'écran`],
    ['M', 'Activer/désactiver le son'],
    null,
    ['H', `Afficher/cacher l'aide`],
    ['F', `Mode plein écran`]
];

const BUTTON_SIZE = 56;
const SQUARE_SIZE = 48;

const DEFAULT_SETTINGS = {
    background: 'aurora',
    sound: true,
    music: 'serious_corporate',
    ghost: true,
    help: true
};

// Application
let canvas = null;
let runner = null;
let ctx = null;
let mouse_state = null;
let pressed_keys = null;

// Global state
let game_mode = 'start';
let now = null;
let started = false;

// Game state
let pause = false;
let stack = new Uint8Array((rules.ROWS + rules.EXTRA_ROWS) * rules.COLUMNS);
let bag = null;
let piece = null;
let hold_block = null;
let can_hold = null;
let ghost = null;
let gravity = null;
let locking = null;

// Scoring
let level = null;
let clears = null;
let score = null;
let combo = null;

// User settings
let settings = Object.assign({}, DEFAULT_SETTINGS);
let show_debug = false;

// UI state
let ui_mode = null;
let layout = {
    block: null,
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
let text_lines = null;

// ------------------------------------------------------------------------
// Init
// ------------------------------------------------------------------------

async function start(root) {
    canvas = root;

    runner = new AppRunner(canvas);
    ctx = canvas.getContext('2d');
    mouse_state = runner.mouseState;
    pressed_keys = runner.pressedKeys;

    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';

    await loadAssets();
    loadSettings();

    init();

    runner.updateFrequency = 120;
    runner.onUpdate = update;
    runner.onDraw = draw;
    runner.start();

    document.body.classList.remove('loading');
}

function init() {
    for (let block of rules.BLOCKS) {
        let trail = ctz(block.shape);
        let lead = clz(block.shape) - (32 - block.size * block.size);

        block.top = block.size - Math.floor(trail / block.size);
        block.bottom = Math.floor(lead / block.size);
    }
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

function play() {
    game_mode = 'play';

    pause = false;
    stack.fill(0);
    bag = [];
    piece = null;
    hold_block = null;
    can_hold = true;
    ghost = null;
    gravity = performance.now();
    locking = null;

    level = 1;
    clears = 0;
    score = 0;
    combo = 0;

    started = true;
}

// ------------------------------------------------------------------------
// Update
// ------------------------------------------------------------------------

function update() {
    // Reset UI buttons
    prev_buttons = buttons;
    buttons = new Map;

    // Global layout
    {
        let padding = runner.isTouch ? 8 : 16;

        let width = canvas.width - padding * 2;
        let height = canvas.height - padding * 2;

        // Account for touch controls
        if (runner.isTouch)
            height -= 100 + padding;

        // Make sure things fit, and square size divides by two nicely
        let square = Math.floor(Math.min(
            Math.min(width / rules.COLUMNS, height / (rules.ROWS + rules.EXTRA_TOP)),
            SQUARE_SIZE
        ) / 2) * 2;

        width = rules.COLUMNS * square;
        height = (rules.ROWS + rules.EXTRA_TOP) * square;

        let left = canvas.width / 2 - width / 2;
        let top = runner.isTouch ? padding : (canvas.height / 2 - height / 2);

        layout.square = square;
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
            height: 6.5 * layout.square
        };
        layout.level = {
            left: layout.bag.left,
            top: layout.bag.top + layout.bag.height + padding,
            width: layout.bag.width,
            height: 40
        };
        layout.score = {
            left: layout.bag.left,
            top: layout.level.top + layout.level.height + padding,
            width: layout.bag.width,
            height: 40
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
    if (runner.isTouch || game_mode == 'play') {
        let sound_key = settings.sound ? 'sound' : 'silence';
        let pause_key = pause ? 'play' : 'pause';

        let size = 0.6 * BUTTON_SIZE;
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

        if (canvas.requestFullscreen != null) {
            if (button('fullscreen', x, y, size).clicked)
                toggleFullScreen();
            y += delta;
        }

        if (runner.isTouch && game_mode == 'play') {
            if (button(pause_key, x, y, size).clicked)
                pause = !pause;
            y += delta;
        }
    }

    // Simple shortcuts
    if (!runner.isTouch) {
        if (pressed_keys.p == 1)
            pause = !pause;
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
    if (started) {
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

    // Should only run once unless the game mode changes, in which case rerun
    // to make sure state is consistent before next screen draw
    for (let mode = null; mode != game_mode;) {
        now = performance.now();
        mode = game_mode;

        switch (mode) {
            case 'start': {
                ui_mode = 'text';

                text_lines = [
                    [0, '20px Open Sans', runner.isTouch ? `Touchez l'écran pour commencer`
                                                         : `Appuyez sur la touche entrée ⏎\uFE0E pour commencer`]
                ];

                if (mouse_state.left == -1 || pressed_keys.return == -1) {
                    play();
                    pressed_keys.space = 0;
                }
            } break;

            case 'play': {
                ui_mode = 'game';
                updateGame();
            } break;

            case 'gameover': {
                ui_mode = 'text';

                text_lines = [
                    [68, '20px Open Sans', `Bien joué !`],
                    [40, '20px Open Sans', `Niveau`],
                    [48, 'bold 34px Open Sans', level],
                    [40, '20px Open Sans', `Score`],
                    [68, 'bold 34px Open Sans', score],
                    [0, '20px Open Sans', runner.isTouch ? `Touchez l'écran pour recommencer`
                                                         : `Appuyez sur la touche entrée ⏎\uFE0E pour recommencer`]
                ];

                if (mouse_state.left == -1 || pressed_keys.return == -1) {
                    play();
                    pressed_keys.space = 0;
                }

                runner.playOnce(assets.sounds.gameover, false);
            } break;
        }
    }

    canvas.focus();
}

function toggleFullScreen() {
    let fullscreen = (document.fullscreenElement == canvas);

    if (!fullscreen) {
        try {
            canvas.requestFullscreen();
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

// ------------------------------------------------------------------------
// Game
// ------------------------------------------------------------------------

function updateGame() {
    // Increase level manually
    if (isInsideRect(mouse_state.x, mouse_state.y, layout.level)) {
        if (mouse_state.left == 1)
            level++;
        runner.cursor = 'pointer';
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
            game_mode = 'gameover';
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
        move += button('right', layout.ui.left + 0.25 * layout.ui.width + 0.7 * BUTTON_SIZE,
                                layout.ui.top + 0.3 * layout.ui.height, BUTTON_SIZE).clicked;
        move -= button('left', layout.ui.left + 0.25 * layout.ui.width - 0.7 * BUTTON_SIZE,
                               layout.ui.top + 0.3 * layout.ui.height, BUTTON_SIZE).clicked;

        rotate = 0;
        rotate += button('clockwise', layout.ui.left + 0.75 * layout.ui.width + 0.7 * BUTTON_SIZE,
                                      layout.ui.top + 0.5 * layout.ui.height, BUTTON_SIZE).clicked;
        rotate -= button('counterclock', layout.ui.left + 0.75 * layout.ui.width - 0.7 * BUTTON_SIZE,
                                         layout.ui.top + 0.5 * layout.ui.height, BUTTON_SIZE).clicked;

        turbo = button('turbo', layout.ui.left + 0.25 * layout.ui.width,
                                layout.ui.top + 0.85 * layout.ui.height, 0.7 * BUTTON_SIZE).pressed >= 1;

        if (isInsideRect(mouse_state.x, mouse_state.y, layout.hold)) {
            if (mouse_state.left == 1)
                hold = true;
            runner.cursor = 'pointer';
        }
    } else {
        move = (pressed_keys.right == 1) - (pressed_keys.left == 1);
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
            game_mode = 'gameover';
            return;
        }

        runner.playOnce(assets.sounds.hold);

        return;
    }

    // Perform rotation
    if (rotate && piece.actions < rules.MAX_ACTIONS) {
        let edit = Object.assign({}, piece);

        let kicks = null;

        if (rotate > 0) {
            rotateRight(edit);
            kicks = edit.kicks[piece.angle];
        } else {
            rotateRight(edit);
            rotateRight(edit);
            rotateRight(edit);

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
                piece.row = edit.row;
                piece.column = edit.column;
                piece.shape = edit.shape;
                piece.angle = edit.angle;

                if (locking != null)
                    locking.time = now;
                piece.actions++;

                runner.playOnce(assets.sounds.move);

                break;
            }
        }
    }

    // Perform lateral movement
    if (move) {
        let edit = Object.assign({}, piece);
        edit.column += move;

        if (isPieceValid(edit, false)) {
            piece.column = edit.column;
            if (piece.actions < rules.MAX_ACTIONS && locking != null)
                locking.time = now;
            piece.actions++;

            runner.playOnce(assets.sounds.move);
        }
    }

    // Compute ghost
    {
        ghost = Object.assign({}, piece);

        while (isPieceFloating(ghost))
            ghost.row--;
    }

    // Run gravity
    {
        let delay = 725 * Math.pow(0.85, level) + level;

        if (turbo)
            delay = Math.min(rules.TURBO_DELAY, delay);

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
    if (drop) {
        let delta = piece.row - ghost.row;

        piece.row = ghost.row;

        if (!isPieceValid(piece, true)) {
            game_mode = 'gameover';
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
                game_mode = 'gameover';
                return;
            }

            lockPiece();
        }
    }

    // Clear stack and score
    if (piece == null) {
        let lines = clearStack();

        clears += lines;

        switch (lines) {
            case 0: { combo = -1; } break;
            case 1: { score += 100 * level; } break;
            case 2: { score += 300 * level; } break;
            case 3: { score += 500 * level; } break;
            case 4: { score += 800 * level; } break;
        }

        if (lines) {
            combo++;
            score += 50 * combo * level;

            runner.playOnce(assets.sounds.clear);
        }

        let perfect = stack.every(value => !value);

        if (perfect) {
            switch (lines) {
                case 1: { score += 800 * level; } break;
                case 2: { score += 1200 * level; } break;
                case 3: { score += 1800 * level; } break;
                case 4: { score += 2000 * level; } break;
            }
        }

        let new_level = 1 + Math.floor(clears / 10);

        if (new_level > level) {
            level = new_level;
            runner.playOnce(assets.sounds.levelup);
        }
    }
}

function spawnPiece(block) {
    piece = {
        row: rules.ROWS - block.top,
        column: Math.floor(rules.COLUMNS / 2 - block.size / 2),

        block: block,
        color: block.color,
        kicks: block.kicks,
        size: block.size,
        shape: block.shape,
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

    gravity = now;
    locking = null;

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

            stack[idx] = piece.color;
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

            if (stack[idx])
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

function rotateRight(piece) {
    let rotated = 0;

    switch (piece.size) {
        case 2: {
            rotated |= (piece.shape & 0b10_00) ? 0b00_10 : 0;
            rotated |= (piece.shape & 0b01_00) ? 0b10_00 : 0;
            rotated |= (piece.shape & 0b00_10) ? 0b00_01 : 0;
            rotated |= (piece.shape & 0b00_01) ? 0b01_00 : 0;
        } break;

        case 3: {
            rotated |= (piece.shape & 0b100_000_000) ? 0b000_000_100 : 0;
            rotated |= (piece.shape & 0b010_000_000) ? 0b000_100_000 : 0;
            rotated |= (piece.shape & 0b001_000_000) ? 0b100_000_000 : 0;
            rotated |= (piece.shape & 0b000_100_000) ? 0b000_000_010 : 0;
            rotated |= (piece.shape & 0b000_010_000) ? 0b000_010_000 : 0;
            rotated |= (piece.shape & 0b000_001_000) ? 0b010_000_000 : 0;
            rotated |= (piece.shape & 0b000_000_100) ? 0b000_000_001 : 0;
            rotated |= (piece.shape & 0b000_000_010) ? 0b000_001_000 : 0;
            rotated |= (piece.shape & 0b000_000_001) ? 0b001_000_000 : 0;
        } break;

        case 4: {
            rotated |= (piece.shape & 0b1000_0000_0000_0000) ? 0b0000_0000_0000_1000 : 0;
            rotated |= (piece.shape & 0b0100_0000_0000_0000) ? 0b0000_0000_1000_0000 : 0;
            rotated |= (piece.shape & 0b0010_0000_0000_0000) ? 0b0000_1000_0000_0000 : 0;
            rotated |= (piece.shape & 0b0001_0000_0000_0000) ? 0b1000_0000_0000_0000 : 0;
            rotated |= (piece.shape & 0b0000_1000_0000_0000) ? 0b0000_0000_0000_0100 : 0;
            rotated |= (piece.shape & 0b0000_0100_0000_0000) ? 0b0000_0000_0100_0000 : 0;
            rotated |= (piece.shape & 0b0000_0010_0000_0000) ? 0b0000_0100_0000_0000 : 0;
            rotated |= (piece.shape & 0b0000_0001_0000_0000) ? 0b0100_0000_0000_0000 : 0;
            rotated |= (piece.shape & 0b0000_0000_1000_0000) ? 0b0000_0000_0000_0010 : 0;
            rotated |= (piece.shape & 0b0000_0000_0100_0000) ? 0b0000_0000_0010_0000 : 0;
            rotated |= (piece.shape & 0b0000_0000_0010_0000) ? 0b0000_0010_0000_0000 : 0;
            rotated |= (piece.shape & 0b0000_0000_0001_0000) ? 0b0010_0000_0000_0000 : 0;
            rotated |= (piece.shape & 0b0000_0000_0000_1000) ? 0b0000_0000_0000_0001 : 0;
            rotated |= (piece.shape & 0b0000_0000_0000_0100) ? 0b0000_0000_0001_0000 : 0;
            rotated |= (piece.shape & 0b0000_0000_0000_0010) ? 0b0000_0001_0000_0000 : 0;
            rotated |= (piece.shape & 0b0000_0000_0000_0001) ? 0b0001_0000_0000_0000 : 0;
        } break;
    }

    piece.shape = rotated;
    piece.angle = (piece.angle + 1) % 4;
}

function clearStack() {
    let lines = 0;

    for (let row = 0; row < rules.ROWS; row++) {
        let clear = isRowFull(row);

        if (clear) {
            let dest = row * rules.COLUMNS;
            let src = dest + rules.COLUMNS;

            stack.copyWithin(dest, src, stack.length);
            row--;

            lines++;
        }
    }

    return lines;
}

function isRowFull(row) {
    for (let column = 0; column < rules.COLUMNS; column++) {
        let idx = (row * rules.COLUMNS) + column;

        if (!stack[idx])
            return false;
    }

    return true;
}

// ------------------------------------------------------------------------
// Draw
// ------------------------------------------------------------------------

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

    switch (ui_mode) {
        case 'text': { drawText(); } break;
        case 'game': { drawGame(); } break;
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

function drawText() {
    let font = '20px Open Sans';
    ctx.fillStyle = 'white';

    let y = canvas.height / 2 - text_lines.reduce((acc, line) => acc + line[0], 0) / 2;

    for (let line of text_lines) {
        ctx.font = line[1];
        runner.text(canvas.width / 2, y, line[2], { align: 5 });
        y += line[0];
    }
}

function drawGame() {
    drawWell();
    drawBag();
    drawLevel();
    drawScore();
    drawHold();

    if (settings.help && layout.help != null)
        drawShortcuts();
}

function drawWell() {
    ctx.save();

    ctx.translate(layout.well.left, layout.well.top);
    drawArea(0, 0, layout.well.width, layout.well.height);

    if (pause) {
        let x = layout.well.width / 2;
        let y = layout.well.height / 2;

        ctx.font = '24px Open Sans';
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

    // Draw stack
    for (let i = 0; i < stack.length; i++) {
        let value = stack[i];

        if (value) {
            let row = Math.floor(i / rules.COLUMNS);
            let column = i % rules.COLUMNS;

            let x = column * layout.square;
            let y = (rules.ROWS - row - 1) * layout.square;

            let color = rules.COLORS[value];
            drawSquare(x, y, layout.square, color);
        }
    }

    // Draw ghost
    if (settings.ghost && ghost != null) {
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

    for (let i = 0; i < 3; i++) {
        let block = bag[i];

        if (block != null) {
            let top = 0.5 + i * 4 + 2 - block.size / 2 - 0.5 * (block.size - block.top - block.bottom);
            let x = layout.bag.width / 2 - (block.size * square) / 2;
            let y = top * square;

            let color = rules.COLORS[block.color];
            drawShape(x, y, block.size, block.shape, square, color);
        }
    }

    ctx.restore();
}

function drawLevel() {
    ctx.save();

    ctx.translate(layout.level.left, layout.level.top);
    drawArea(0, 0, layout.level.width, layout.level.height);

    ctx.font = '22px Open Sans';
    ctx.fillStyle = 'white';
    runner.text(layout.level.width / 2, layout.level.height / 2, level, { align: 5 });

    ctx.restore();
}

function drawScore() {
    ctx.save();

    ctx.translate(layout.score.left, layout.score.top);
    drawArea(0, 0, layout.score.width, layout.score.height);

    if (!runner.isTouch && score < 100000) {
        ctx.font = '24px Open Sans';
    } else {
        ctx.font = '14px Open Sans';
    }
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

        let color = rules.COLORS[block.color];
        drawShape(x, y, block.size, block.shape, square, color);
    }

    ctx.restore();
}

function drawShortcuts() {
    ctx.save();

    ctx.font = '14px Open Sans';
    ctx.fillStyle = 'white';

    let x = layout.help.right;
    let y = layout.help.bottom;
    let width = 0;
    let height = 18;

    for (let shortcut of KEYBOARD_SHORTCUTS) {
        if (shortcut != null) {
            width = Math.max(width, 114 + ctx.measureText(shortcut[1]).width);
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

function drawPiece(piece, color = null) {
    if (color == null)
        color = rules.COLORS[piece.color];

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

function isInsideRect(x, y, rect) {
    let inside = (x >= rect.left &&
                  x <= rect.left + rect.width &&
                  y >= rect.top &&
                  y <= rect.top + rect.height);
    return inside;
}

function testShape(size, shape, i, j) {
    let shift = size * (size - i) - j - 1;
    let mask = 1 << shift;

    let hit = !!(shape & mask);
    return hit;
}

export { start }
