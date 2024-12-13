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
const EXTRA_TOP = 0.3;

const DEFAULT_DELAY = 150;
const TURBO_DELAY = 5;
const LOCK_DELAY = 500;
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

const COLORS = [
    null, // Color 0 is invalid

    '#2df0f0',
    '#2d2df0',
    '#f0a32d',
    '#f0f02d',
    '#2df02d',
    '#a32df0',
    '#f02d2d'
];

const BLOCKS = [
    { color: 1, size: 4, shape: 0b0000000011110000, kicks: kicks_I }, // I
    { color: 2, size: 3, shape: 0b000111100, kicks: kicks_JLTSZ },    // J
    { color: 3, size: 3, shape: 0b000111001, kicks: kicks_JLTSZ },    // L
    { color: 4, size: 2, shape: 0b1111, kicks: kicks_O },             // O
    { color: 5, size: 3, shape: 0b000110011, kicks: kicks_JLTSZ },    // S
    { color: 6, size: 3, shape: 0b000111010, kicks: kicks_JLTSZ },    // T
    { color: 7, size: 3, shape: 0b000011110, kicks: kicks_JLTSZ }     // Z
];

export {
    ROWS,
    COLUMNS,
    EXTRA_ROWS,
    EXTRA_TOP,

    DEFAULT_DELAY,
    TURBO_DELAY,
    LOCK_DELAY,
    MAX_ACTIONS,

    COLORS,
    BLOCKS
};
