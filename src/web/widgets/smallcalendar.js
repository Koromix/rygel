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

import { Util, Log , LocalDate} from '../core/common.js';
import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';

import './smallcalendar.css';

if (typeof window.T == 'undefined')
    window.T = {};

Object.assign(T, {
    days: {
        1: 'Monday',
        2: 'Tuesday',
        3: 'Wednesday',
        4: 'Thursday',
        5: 'Friday',
        6: 'Saturday',
        7: 'Sunday'
    },
    months: {
        1: 'January',
        2: 'February',
        3: 'March',
        4: 'April',
        5: 'May',
        6: 'June',
        7: 'July',
        8: 'August',
        9: 'September',
        10: 'October',
        11: 'November',
        12: 'December'
    }
});

function SmallCalendar() {
    let self = this;

    let date = LocalDate.today();

    let root_el;

    this.render = function() {
        if (!root_el) {
            root_el = document.createElement('div');
            root_el.className = 'scal';
        }

        // We don't return VDOM, because if we did the next render() we do after user interaction
        // would use a new binding, and replace the widget.
        render(renderWidget(), root_el);

        return root_el;
    };

    function renderWidget() {
        let first = new LocalDate(date.year, date.month, 1);

        let skip = first.getWeekDay() - 1;
        let count = LocalDate.daysInMonth(date.year, date.month);

        let days = Util.mapRange(1, 8, day => T.days[day].substr(0, 3));

        return html`
            <div class="scal_period">${T.months[date.month]} ${date.year}</div>
            <div class="scal_days">
                ${days.map(day => html`<div class="scal_head">${day}</div>`)}
                ${Util.mapRange(0, skip, () => html`<div></div>`)}
                ${Util.mapRange(1, count + 1, day => html`<div class="cal_day">${day}</div>`)}
            </div>
        `;
    }
}

export { SmallCalendar }
