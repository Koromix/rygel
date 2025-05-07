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

import { render, html, svg } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, LocalDate } from '../../../web/core/base.js';

function computeAge(from, to = null) {
    if (!(from instanceof Date))
        from = new Date(from);
    if (to == null) {
        to = new Date;
    } else if (!(to instanceof Date)) {
        to = new Date(to);
    }

    let age = (to.getFullYear() - from.getFullYear());

    if (to.getMonth() < from.getMonth()) {
        age--;
    } else if (to.getMonth() == from.getMonth() && to.getDate() < from.getDate()) {
        age--;
    }

    return age;
}

function computeAgeMonths(from, to) {
    if (!(from instanceof Date))
        from = new Date(from);
    if (!(to instanceof Date))
        to = new Date(to);

    if (to.getMonth() > from.getMonth()) {
        let delta = to.getMonth() - from.getMonth();

        if (delta && to.getDate() < from.getDate())
            delta--;

        return delta;
    } else if (to.getMonth() == from.getMonth() && to.getDate() < from.getDate()) {
        return 11;
    } else {
        return 12 - from.getMonth() + to.getMonth();
    }
}

function dateToString(date) {
    if (date == null)
        return '';
    if (!(date instanceof Date))
        date = new Date(date);

    let year = '' + date.getFullYear();
    let month = '' + (date.getMonth() + 1);
    let day = '' + date.getDate();

    let str = `${year.padStart(2, '0')}-${month.padStart(2, '0')}-${day.padStart(2, '0')}`;
    return str;
}

function niceDate(date, full = true) {
    if (date == null)
        return '';

    if (full) {
        let today = LocalDate.today();
        let str = `${date.day} ${T.months[date.month]}${date.year != today.year ? ' ' + date.year : ''}`;
        return str;
    } else {
        let str = `${date.day} ${T.months[date.month].substr(0, 3)}`;
        return str;
    }
}

function loadImage(url) {
    if (typeof url == 'string') {
        return new Promise((resolve, reject) => {
            let img = new Image();

            img.addEventListener('load', () => resolve(img));
            img.addEventListener('error', e => { console.error(e); reject(new Error('Failed to load image')); });

            img.src = url;
        });
    } else if (url instanceof Blob) {
        return new Promise((resolve, reject) => {
            let reader = new FileReader();

            reader.onload = async () => {
                try {
                    let img = await loadImage(reader.result);
                    resolve(img);
                } catch (err) {
                    reject(err);
                }
            };
            reader.onerror = () => reject(new Error('Failed to load image'));

            reader.readAsDataURL(url);
        });
    } else {
        throw new Error('Cannot load image from value of type ' + typeof url);
    }
}

async function loadTexture(url) {
    let response = await Net.fetch(url);

    let blob = await response.blob();
    let texture = await createImageBitmap(blob);

    return texture;
}

function progressBar(value, total, cls = null) {
    if (!total)
        return '';

    let ratio = (value / total);
    let progress = ratio * 100;

    // Round up unless high for user motivation
    progress = (progress < 99.5) ? Math.ceil(progress) : Math.floor(progress);

    cls = 'bar' + (cls ? ' ' + cls : '');

    return html`
        <div class=${cls} style=${'--progress: ' + progress}>
            <div></div>
            <span>${value}/${total}</span>
        </div>
    `;
}

function progressCircle(value, total, cls = null) {
    if (!total) {
        value = 0;
        total = 100;
    }

    let ratio = (value / total);
    let progress = ratio * 100;

    // Round up unless high for user motivation
    progress = (progress < 99.5) ? Math.ceil(progress) : Math.floor(progress);

    let array = 314.1;
    let offset = array - ratio * array;

    cls = 'circle' + (cls ? ' ' + cls : '');

    return svg`
        <svg class=${cls} width="100" height="100" viewBox="-12.5 -12.5 125 125" style="transform: rotate(-90deg)">
            <circle r="50" cx="50" cy="50" fill="transparent" stroke-width="16" stroke-dasharray=${array + 'px'} stroke-dashoffset="0"></circle>
            <circle r="50" cx="50" cy="50" stroke-width="16" stroke-linecap="butt" stroke-dasharray=${array + 'px'} stroke-dashoffset=${offset + 'px'} fill="transparent"></circle>
            <text x="50px" y="53px" font-size="19px" fill="black" font-weight="bold" text-anchor="middle" style="transform: rotate(90deg) translate(0px, -96px)">${progress}%</text>
        </svg>
    `;
}

async function deflate(bytes) {
    if (bytes instanceof ArrayBuffer)
        bytes = new Uint8Array(bytes);

    let cs = new CompressionStream('deflate');

    let input = new Blob([bytes]);
    let stream = input.stream().pipeThrough(cs);
    let output = await new Response(stream).blob();
    let buf = await output.arrayBuffer();

    return new Uint8Array(buf);
}

async function inflate(bytes) {
    if (bytes instanceof ArrayBuffer)
        bytes = new Uint8Array(bytes);

    let ds = new DecompressionStream('deflate');

    let input = new Blob([bytes]);
    let stream = input.stream().pipeThrough(ds);
    let output = await new Response(stream).blob();
    let buf = await output.arrayBuffer();

    return new Uint8Array(buf);
}

const EventProviders = [
    { key: 'apple', title: 'Apple', icon: 'calendars/apple' },
    { key: 'google', title: 'Google', icon: 'calendars/google' },
    { key: 'outlook', title: 'outlook', icon: 'calendars/outlook' },
    { key: 'office365', title: 'office365', icon: 'calendars/office365' },
    { key: 'yahoo', title: 'Yahoo', icon: 'calendars/yahoo' },
    { key: 'ical', title: 'iCalendar', icon: 'calendars/ical' },
];

function createEvent(provider, date, title, description = '') {
    let [html, text] = formatDescription(description);

    switch (provider) {
        case 'ical':
        case 'apple': {
            let now = new Date;

            let lines = [
                'BEGIN:VCALENDAR',
                'VERSION:2.0',
                'CALSCALE:GREGORIAN',
                'METHOD:PUBLISH',
                'BEGIN:VEVENT',
                'DTSTAMP:' + now.toISOString().replace(/\.[0-9]{3}/, '').replace(/[\-:\.]/g, ''),
                'DTSTART;VALUE=DATE:' + formatDate(date, ''),
                'SUMMARY:' + title,
                'DESCRIPTION:' + text.replace(/\r?\n/g, '\\n'),
                'X-ALT-DESC;FMTTYPE=text/html:' + html.replace(/\r?\n/g, '\\n'),
                'END:VEVENT',
                'END:VCALENDAR'
            ];

            // Clean up missing values
            lines = lines.filter(line => !line.endsWith(':'));

            let ical = lines.join('\n');
            let blob = new Blob([ical], { type: 'text/calendar' });

            Util.saveFile(blob, 'events.ics');
            return;
        } break;

        case 'google': {
            let url = 'https://calendar.google.com/calendar/r/eventedit';

            url += '?' + new URLSearchParams({
                dates: formatDate(date, '') + '/' + formatDate(date.plus(1), ''),
                text: title,
                details: html
            });

            window.open(url, '_blank');
            return;
        } break;

        case 'outlook': {
            let url = 'https://outlook.live.com/calendar/action/compose';

            url += '?' + new URLSearchParams({
                rru: 'addevent',
                startdt: formatDate(date, '-'),
                enddt: formatDate(date.plus(1), '-'),
                allday: 'true',
                subject: title,
                body: html
            });

            window.open(url, '_blank');
            return;
        } break;

        case 'office365': {
            let url = 'https://outlook.office.com/calendar/action/compose';

            url += '?' + new URLSearchParams({
                rru: 'addevent',
                startdt: formatDate(date, '-'),
                enddt: formatDate(date.plus(1), '-'),
                allday: 'true',
                subject: title,
                body: html
            });

            window.open(url, '_blank');
            return;
        } break;

        case 'yahoo': {
            let url = 'https://calendar.yahoo.com/';

            url += '?' + new URLSearchParams({
                v: 60,
                ST: formatDate(date, ''),
                dur: 'allday',
                TITLE: title,
                DESC: text
            });

            window.open(url, '_blank');
            return;
        } break;
    }

    throw new Error(`Invalid calendar provider '${provider}'`);
}

function formatDate(date, separator) {
    let str = date.toString();
    return str.replaceAll('-', separator);
}

function formatDescription(str) {
    let div = document.createElement('div');

    render(str, div);

    let html = div.innerHTML;
    let anchors = div.querySelectorAll('a');

    for (let a of anchors) {
        if (a.href != null)
            a.textContent = a.href;
    }

    let lines = div.textContent.trim().split('\n').map(line => line.trim());
    let text = lines.join('\n');

    return [html, text];
}

export {
    computeAge,
    computeAgeMonths,
    dateToString,
    niceDate,

    loadImage,
    loadTexture,

    progressBar,
    progressCircle,

    deflate,
    inflate,

    EventProviders,
    createEvent
}
