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

import { Util, Log } from '../../../web/core/base.js';

const ROWS = 20;
const COLUMNS = 10;
const EXTRA_ROWS = 4;
const EXTRA_TOP = 0.12;

// Game updates/second
const UPDATE_FREQUENCY = 240;

// Delays relative to game speed above
const TURBO_DELAY = 2; // ~8.33 ms
const LOCK_DELAY = 0;
const DAS_DELAY = 40; // ~166.67 ms
const DAS_PERIOD = 8; // ~33.33 ms
const CLEAR_DELAY = 80; // ~333.33 ms

const HOLD = false;
const MAX_ACTIONS = 20;

const kicks = [
    [[0, 0]],
    [[0, 0]],
    [[0, 0]],
    [[0, 0]]
];

const BLOCKS = [
    { type: 'I', color: 0x22c3b1, shape: new Uint8Array([0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0]), kicks: kicks },
    { type: 'J', color: 0x3546d5, shape: new Uint8Array([0, 0, 0, 1, 1, 1, 1, 0, 0]), kicks: kicks },
    { type: 'L', color: 0xec6834, shape: new Uint8Array([0, 0, 0, 1, 1, 1, 0, 0, 1]), kicks: kicks },
    { type: 'O', color: 0xd9b73c, shape: new Uint8Array([1, 1, 1, 1]), kicks: kicks },
    { type: 'S', color: 0x60c233, shape: new Uint8Array([0, 0, 0, 1, 1, 0, 0, 1, 1]), kicks: kicks },
    { type: 'T', color: 0xcc42bf, shape: new Uint8Array([0, 0, 0, 1, 1, 1, 0, 1, 0]), kicks: kicks },
    { type: 'Z', color: 0xbf2c41, shape: new Uint8Array([0, 0, 0, 0, 1, 1, 1, 1, 0]), kicks: kicks }
];

function* BAG_GENERATOR() {
    for (;;) {
        let rnd = Util.getRandomInt(0, BLOCKS.length);
        yield BLOCKS[rnd];
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
