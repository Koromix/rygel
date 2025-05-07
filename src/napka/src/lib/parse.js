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

function cleanURL(url) {
    let m = url ? url.match(/(https?:\/\/)?(?:www\.)?(?:[a-zA-Z0-9-]+\.)+[a-z]+(?:[^ ]+\/)*/) : null;

    if (m != null) {
        url = m[0];

        if (!url.endsWith('/'))
            url += '/';
        if (m[1] == null)
            url = 'https://' + url;

        return url;
    } else {
        return null;
    }
}

function cleanMail(mail) {
    let m = mail ? mail.match(/[a-zA-Z0-9\.\-_]+@(?:[a-zA-Z0-9-]+\.)+[a-z]+/) : null;
    return m?.[0];
}

function cleanPhoneNumber(tel) {
    tel = tel ? tel.replace(/[\. ]/g, '').match(/(.{1,2})/g).join('.') : null;
    return tel;
}

function parseSchedule(str) {
    let horaires = [];

    // Simplify and fix common typos
    str = str || '';
    str = str.toLowerCase();
    str = str.replaceAll('dio', 'di');
    str = str.replaceAll('e:', 'e');
    str = str.replaceAll('-h', 'h');
    str = str.replaceAll('à', 'a');
    str = str.replaceAll(/([a-z]) *- *([a-z])/g, '$1 a $2');
    str = str.replaceAll(/([0-9])a/g, '$1 a');
    str = str.replaceAll(/a([0-9])/g, 'a $1');

    let tokens = str.split(/[\r\n\t, \-:/;]+/);
    let offset = 0;

    function consume(...expected) {
        for (let i = 0; i < expected.length; i++) {
            let token = tokens[offset + i];
            let expect = expected[i];

            if (token != expect)
                return false;
        }

        offset += expected.length;
        return true;
    }
    function consumeDay() {
        if (consume('lundi') || consume('lun')) {
            return 1;
        } else if (consume('mardi') || consume('mar')) {
            return 2;
        } else if (consume('mercredi') || consume('mer')) {
            return 3;
        } else if (consume('jeudi') || consume('jeu')) {
            return 4;
        } else if (consume('vendredi') || consume('ven')) {
            return 5;
        } else if (consume('samedi') || consume('sam')) {
            return 6;
        } else if (consume('dimanche') || consume('dim')) {
            return 7;
        } else {
            return null;
        }
    }
    function consumeTime() {
        let token = tokens[offset] || '';
        let m = token.match(/^([0-9]+)(?:h([0-9]+)?)?$/);

        if (m != null) {
            let hour = parseInt(m[1], 10);
            let min = parseInt(m[2] || '0', 10);

            offset++;
            if (m[2] == null && offset < tokens.length && tokens[offset].startsWith('heure'))
                offset++;

            let time = hour * 100 + min;
            return time;
        } else {
            return null;
        }
    }

    while (offset < tokens.length) {
        let days = new Set;
        let spans = [];
        let min_time = 0;

        if (consume('tous', 'les', 'jours', 'de', 'la', 'semaine')) {
            for (let i = 1; i <= 5; i++)
                days.add(i);
        } else if (tokens[offset] === '7j') {
            offset++;
            consume('7');

            for (let i = 1; i <= 7; i++)
                days.add(i);
        } else {
            while (offset < tokens.length) {
                let day = consumeDay();

                if (day != null) {
                    if (consume('au') || consume('a')) {
                        let to = consumeDay();

                        if (to != null) {
                            for (let j = day; j <= to; j++)
                                days.add(j);
                        } else {
                            // console.log(`Token 'au' without time after`);
                            offset++;

                            days.add(day);
                        }
                    } else {
                        days.add(day);
                    }
                } else {
                    if (days.size)
                        break;

                    // console.log(`Skipping token '${tokens[offset]}'`);
                    offset++;
                }

                consume('et');
                consume('ou');
            }
        }

        while (offset < tokens.length) {
            let prev_offset = offset;

            if (consumeTime()) {
                offset = prev_offset;
                break;
            }

            offset++;
        }

        while (offset < tokens.length) {
            let from = consumeTime();
            consume('a');
            let to = consumeTime();

            if (from === 2400 && to === 2400) {
                spans.push(['0000', '2400']);
            } else if (from != null && to != null) {
                if (from < 959 && min_time < 1359 && from < min_time) {
                    from += 1200;
                    if (to < from && to < 959)
                        to += 1200;
                }
                min_time = to;

                from = ('' + from).padStart(4, '0');
                to = ('' + to).padStart(4, '0');

                spans.push([from, to]);
            } else {
                if (spans.length) {
                    break;
                } else {
                    // console.log(`Skipping token '${tokens[offset]}'`);
                    offset++;
                }
            }

            consume('et');
            consume('de');
        }

        for (let day of days.values()) {
            for (let span of spans)
                horaires.push({ jour: day, horaires: span });
        }
    }

    if (!horaires.length)
        return null;

    return horaires;
}

export {
    cleanMail,
    cleanURL,
    cleanPhoneNumber,
    parseSchedule
}
