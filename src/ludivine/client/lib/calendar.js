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

import { Util, Log , LocalDate} from '../../../web/core/base.js';
import { render, html } from '../../../../vendor/lit-html/lit-html.bundle.js';
import * as UI from '../../../web/flat/ui.js';

import './calendar.css';

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
    let events = null;

    let event_func = (start, end) => ({});
    let click_handler = (e, offset, filter, sort_key) => {};

    let root_el;

    Object.defineProperties(this, {
        eventFunc: { get: () => event_func, set: func => { event_func = func; }, enumerable: true },

        year: { get: () => date.year, enumerable: true },
        month: { get: () => date.month, enumerable: true }
    })

    this.render = function() {
        if (!root_el) {
            root_el = document.createElement('div');
            root_el.className = 'scal';
        }

        if (events == null) {
            let first = new LocalDate(date.year, date.month, 1);
            let start = date.minus(first.getWeekDay() - 1);
            let end = first.plusMonths(1).plus(7);

            let ret = event_func(start, end);

            if (ret instanceof Promise) {
                let year = date.year;
                let month = date.month;

                ret.then(value => {
                    if (date.year != year || date.month != month)
                        return;

                    events = value;
                    self.render();
                });
                ret.catch(err => console.error(err));
            } else {
                events = ret;
            }
        }

        // We don't return VDOM, because if we did the next render() we do after user interaction
        // would use a new binding, and replace the widget.
        render(renderWidget(), root_el);

        return root_el;
    };

    function renderWidget() {
        let first = new LocalDate(date.year, date.month, 1);
        let last = first.plusMonths(1).minus(1);

        let before = first.getWeekDay() - 1;
        let after = 7 - last.getWeekDay() - 1;
        let count = LocalDate.daysInMonth(date.year, date.month);

        let days = Util.mapRange(1, 8, day => T.days[day].substr(0, 3));

        return html`
            <div class="scal_period">
                <div class="scal_button" @click=${UI.wrap(e => changeMonth(-1))}>&lt;</div>
                <div class="scal_month">${T.months[date.month]} ${date.year}</div>
                <div class="scal_button" @click=${UI.wrap(e => changeMonth(1))}>&gt;</div>
            </div>
            <div class="scal_days">
                ${days.map(day => html`<div class="scal_head">${day}</div>`)}
                ${Util.mapRange(0, before + 1, idx => {
                    let at = first.minus(before - idx + 1);
                    return renderDay(at, 'outside');
                })}
                ${Util.mapRange(1, count + 1, day => {
                    let at = new LocalDate(date.year, date.month, day);
                    return renderDay(at);
                })}
                ${Util.mapRange(0, after, idx => {
                    let at = last.plus(idx + 1);
                    return renderDay(at, 'outside');
                })}
            </div>
        `;
    }

    function renderDay(date, cls = '') {
        let texts = events?.[date] ?? [];
        let tooltip = texts.join('\n');

        if (tooltip)
            cls += ' events';

        return html`<div class=${cls} title=${tooltip}>${date.day}</div>`;
    }

    async function changeMonth(delta) {
        date = date.plusMonths(delta);
        events = null;

        self.render();
    }
}

export { SmallCalendar }
