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

const ROWS = 20;
const COLUMNS = 10;
const EXTRA_ROWS = 4;
const EXTRA_TOP = 0.12;

// Delays relative to game speed of 480 updates/second
const TURBO_DELAY = 4; // ~8.33 ms
const LOCK_DELAY = 240; // 500 ms
const DAS_DELAY = 80; // ~166.67 ms
const DAS_PERIOD = 8; // ~16.67 ms
const MAX_ACTIONS = 20;

const VISIBLE_BAG_SIZE = 3;

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
    { type: 'I', color: '#2df0f0', size: 4, shape: 0b0000_0000_1111_0000, kicks: kicks_I },
    { type: 'J', color: '#2d2df0', size: 3, shape: 0b000_111_100, kicks: kicks_JLTSZ },
    { type: 'L', color: '#f0a32d', size: 3, shape: 0b000_111_001, kicks: kicks_JLTSZ },
    { type: 'O', color: '#f0f02d', size: 2, shape: 0b11_11, kicks: kicks_O },
    { type: 'S', color: '#2df02d', size: 3, shape: 0b000_110_011, kicks: kicks_JLTSZ },
    { type: 'T', color: '#a32df0', size: 3, shape: 0b000_111_010, kicks: kicks_JLTSZ },
    { type: 'Z', color: '#f02d2d', size: 3, shape: 0b000_011_110, kicks: kicks_JLTSZ }
];

export {
    ROWS,
    COLUMNS,
    EXTRA_ROWS,
    EXTRA_TOP,

    TURBO_DELAY,
    LOCK_DELAY,
    DAS_DELAY,
    DAS_PERIOD,
    MAX_ACTIONS,

    VISIBLE_BAG_SIZE,

    BLOCKS
};
