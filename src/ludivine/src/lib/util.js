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

import { Util, Log, Net } from '../../../web/core/base.js';

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

async function loadTexture(url) {
    let response = await Net.fetch(url);

    let blob = await response.blob();
    let texture = await createImageBitmap(blob);

    return texture;
}

export {
    computeAge,
    computeAgeMonths,
    dateToString,
    loadTexture
}
