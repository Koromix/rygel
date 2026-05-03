// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { Util, Log, Net } from 'lib/web/base/base.js';

const DAYS = ['monday', 'tuesday', 'wednesday', 'thursday', 'friday', 'saturday', 'sunday'];

function formatSize(size) {
    if (size >= 999950000) {
        let value = size / 1000000000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' ' + T.size_units.gb;
    } else if (size >= 999950) {
        let value = size / 1000000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' ' + T.size_units.mb;
    } else if (size >= 999.95) {
        let value = size / 1000;
        let prec = 1 + (value < 9.9995) + (value < 99.995);

        return value.toFixed(prec) + ' ' + T.size_units.kb;
    } else if (size > 1) {
        return T.format(T.size_units.x_b, size);
    } else {
        return size ? T.size_units.one : T.size_units.zero;
    }
}

function formatDays(days) {
    let parts = [];

    for (let i = 0; i < DAYS.length; i++) {
        if (days & (1 << i)) {
            let prefix = T.day_names[DAYS[i]].substr(0, 3);
            parts.push(prefix);
        }
    }

    return parts.join(', ');
}

function formatClock(clock) {
    let hh = Math.floor(clock / 100).toString().padStart(2, '0');
    let mm = Math.floor(clock % 100).toString().padStart(2, '0');

    return `${hh}:${mm}`;
}

function parseClock(clock) {
    let [hh, mm] = (clock ?? '').split(':').map(value => parseInt(value, 10));
    return hh * 100 + mm;
}

async function writeClipboard(label, text) {
    await navigator.clipboard.writeText(text);

    let msg = T.message(`{1} copied to clipboard`, label);
    Log.info(msg);
}

export {
    DAYS,

    formatSize,
    formatDays,
    formatClock,
    parseClock,

    writeClipboard
}
