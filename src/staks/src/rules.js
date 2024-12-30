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

import { Util, Log } from '../../web/core/base.js';

const ROWS = 20;
const COLUMNS = 10;
const EXTRA_ROWS = 4;
const EXTRA_TOP = 0.12;

// Delays relative to game speed of 480 updates/second
const TURBO_DELAY = 4; // ~8.33 ms
const LOCK_DELAY = 240; // 500 ms
const DAS_DELAY = 80; // ~166.67 ms
const DAS_PERIOD = 16; // ~33.33 ms
const CLEAR_DELAY = 160; // ~333.33 ms

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

    TURBO_DELAY,
    LOCK_DELAY,
    DAS_DELAY,
    DAS_PERIOD,
    CLEAR_DELAY,

    MAX_ACTIONS,

    BLOCKS,

    BAG_GENERATOR,
    BAG_SIZE
};
