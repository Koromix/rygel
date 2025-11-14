// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log } from 'src/core/web/base/base.js';

const ROWS = 20;
const COLUMNS = 10;
const EXTRA_ROWS = 4;
const EXTRA_TOP = 0.12;

// Game updates/second
const UPDATE_FREQUENCY = 240;

// Delays relative to game speed above
const TURBO_DELAY = 2; // ~8.33 ms
const LOCK_DELAY = 120; // 500 ms
const DAS_DELAY = 40; // ~166.67 ms
const DAS_PERIOD = 8; // ~33.33 ms
const CLEAR_DELAY = 80; // ~333.33 ms

const HOLD = true;
const MAX_ACTIONS = 20;

const kicks_I = [
    [[0, 0], [0, -2], [0,  1], [-1, -2], [ 2,  1]],
    [[0, 0], [0, -1], [0,  2], [ 2, -1], [-1,  2]],
    [[0, 0], [0,  2], [0, -1], [ 1,  2], [-2, -1]],
    [[0, 0], [0,  1], [0, -2], [-2,  1], [ 1, -2]]
];
const kicks_JLTSZ = [
    [[0, 0], [0, -1], [ 1, -1], [-2, 0], [-2, -1]],
    [[0, 0], [0,  1], [-1,  1], [ 2, 0], [ 2,  1]],
    [[0, 0], [0,  1], [ 1,  1], [-2, 0], [-2,  1]],
    [[0, 0], [0, -1], [-1, -1], [ 2, 0], [ 2, -1]]
];
const kicks_O = [
    [[0, 0]],
    [[0, 0]],
    [[0, 0]],
    [[0, 0]]
];

const BLOCKS = [
    { type: 'I', color: 0x22c3b1, shape: new Uint8Array([0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0]), kicks: kicks_I },
    { type: 'J', color: 0x3546d5, shape: new Uint8Array([0, 0, 0, 1, 1, 1, 1, 0, 0]), kicks: kicks_JLTSZ },
    { type: 'L', color: 0xec6834, shape: new Uint8Array([0, 0, 0, 1, 1, 1, 0, 0, 1]), kicks: kicks_JLTSZ },
    { type: 'O', color: 0xd9b73c, shape: new Uint8Array([1, 1, 1, 1]), kicks: kicks_O },
    { type: 'S', color: 0x60c233, shape: new Uint8Array([0, 0, 0, 1, 1, 0, 0, 1, 1]), kicks: kicks_JLTSZ },
    { type: 'T', color: 0xcc42bf, shape: new Uint8Array([0, 0, 0, 1, 1, 1, 0, 1, 0]), kicks: kicks_JLTSZ },
    { type: 'Z', color: 0xbf2c41, shape: new Uint8Array([0, 0, 0, 0, 1, 1, 1, 1, 0]), kicks: kicks_JLTSZ }
];

function* BAG_GENERATOR() {
    for (;;) {
        let draw = Util.shuffle(BLOCKS);

        for (let block of draw)
            yield block;
    }
}
const BAG_SIZE = 3;

export {
    ROWS,
    COLUMNS,
    EXTRA_ROWS,
    EXTRA_TOP,

    UPDATE_FREQUENCY,

    TURBO_DELAY,
    LOCK_DELAY,
    DAS_DELAY,
    DAS_PERIOD,
    CLEAR_DELAY,

    HOLD,
    MAX_ACTIONS,

    BLOCKS,

    BAG_GENERATOR,
    BAG_SIZE
};
