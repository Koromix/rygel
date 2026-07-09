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

function formatDuration(duration) {
    duration = Math.round(duration / 1000);

    if (duration < 5) {
        return T.a_few_seconds;
    } else if (duration < 60) {
        return T.count(T.second_units, duration);
    } else if (duration < 300) {
        let minutes = Math.floor(duration / 60);
        let seconds = duration % 60;

        return T.count(T.minute_units, minutes) + ' ' + T.count(T.second_units, seconds);
    } else if (duration < 3600) {
        let minutes = Math.floor(duration / 60);
        return T.count(T.minute_units, minutes);
    } else if (duration < 5 * 3600) {
        let hours = Math.floor(duration / 3600);
        let minutes = Math.floor((duration % 3600) / 60);

        return T.count(T.hour_units, hours) + ' ' + T.count(T.minute_units, minutes);
    } else {
        let hours = Math.floor(duration / 3600);
        return T.count(T.hour_units, hours);
    }
}

function ProgressMeter(max) {
    let self = this;

    let duration = 10000;
    let interval = 1000;

    let points = [];

    let stat = {
        time: null,
        value: null,

        max: max,
        rate: null,
        remaining: null
    };

    let next_update = 0;

    Object.defineProperties(this, {
        duration: { get: () => duration, set: value => { duration = value }, enumerable: true },
        interval: { get: () => interval, set: value => { interval = value }, enumerable: true }
    });

    this.add = function (value) {
        let now = performance.now();

        points.push({
            value: value,
            time: now
        });

        collect(now);
    };

    this.measure = function () {
        let now = performance.now();

        collect(now);

        let first = points[0];
        let last = points[points.length - 1];

        if (now >= next_update) {
            if (last?.time - first?.time >= 500) {
                stat.rate = (last.value - first.value) / (now - first.time);
                stat.remaining = (max - last.value) / stat.rate;
            } else {
                stat.rate = null;
                stat.remaining = null;
            }

            next_update = now + interval;
        }

        stat.time = now;
        stat.value = last?.value ?? null;

        return stat;
    }

    function collect(now) {
        let j = 0;
        for (let i = 0; i < points.length; i++) {
            points[j] = points[i];
            j += (points[i].time >= now - duration);
        }
        points.length = j;
    }
}

export {
    DAYS,

    formatSize,
    formatDays,
    formatClock,
    parseClock,
    formatDuration,

    ProgressMeter
}
