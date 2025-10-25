// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

function html(strings) { return strings.map(str => str.trim()).join('?').replace(/[ \n]+/g, ' '); }
function svg() { return strings.map(str => str.trim()).join('?').replace(/[ \n]+/g, ' '); }

function render() {}
function live() {}

export {
    html,
    svg,
    render,
    live
}
