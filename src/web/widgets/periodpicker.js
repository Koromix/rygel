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

import { Util, Log, LocalDate } from '../core/base.js';
import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';

import './periodpicker.css';

function PeriodPicker() {
    let self = this;

    this.clickHandler = (e, start, end) => {};

    let limit_dates = [new LocalDate(1900, 1, 1), LocalDate.today()];
    let current_dates = limit_dates;

    let grab_target;
    let grab_offset;
    let grab_start_date;
    let grab_end_date;

    let root_el;

    this.setRange = function(...dates) { limit_dates = dates; };
    this.getRange = function() { return limit_dates; };

    this.setDates = function(...dates) { current_dates = dates.map((date, idx) => date || limit_dates[idx]); };
    this.getDates = function() { return current_dates; };

    this.render = function() {
        if (!root_el) {
            root_el = document.createElement('div');
            root_el.className = 'ppik';
        }

        // We can't return VDOM, because if we did the next render() we do after user interaction
        // would use a new binding, and replace the widget (and break pointer capture).
        render(renderWidget(), root_el);

        return root_el;
    };

    function renderWidget() {
        let left_pos = dateToPosition(current_dates[0]);
        let right_pos = dateToPosition(current_dates[1]);

        // The dummy button catches click events that happen when a label encloses the widget
        return html`
            <div class="ppik_main">
                <button style="display: none;" @click=${e => e.preventDefault()}></button>

                <div class="ppik_line"></div>
                <div class="ppik_bar" style=${`left: ${left_pos}; width: calc(${right_pos} - ${left_pos});`}
                        @pointerdown=${handleBarDown} @pointerup=${handlePointerUp}
                        @mousedown=${handleBarDown}>
                </div>
                <div class="ppik_text" style=${`margin-left: calc((${right_pos} + ${left_pos}) / 2 - 50%);`}>
                    ${current_dates[0].toLocaleString()} — ${current_dates[1].toLocaleString()}
                </div>
                <div class="ppik_handle" style=${`left: ${left_pos};`}
                     @pointerdown=${e => handleHandleDown(e, 0)} @pointerup=${handlePointerUp}
                     @mousedown=${handleHandleDown}></div>
                <div class="ppik_handle" style=${`left: ${right_pos};`}
                     @pointerdown=${e => handleHandleDown(e, 1)} @pointerup=${handlePointerUp}
                     @mousedown=${handleHandleDown}></div>
            </div>
        `;
    }

    function handleHandleDown(e, idx) {
        if (Element.prototype.setPointerCapture) {
            if (e.type === 'pointerdown') {
                e.target.onpointermove = e => handleHandleMove(e, idx);
                e.target.setPointerCapture(e.pointerId);
            }
        } else if (!grab_target) {
            function forwardUp(e) {
                document.body.removeEventListener('mousemove', handleHandleMove);
                document.body.removeEventListener('mouseup', forwardUp);
                handlePointerUp(e);
            }

            document.body.addEventListener('mousemove', handleHandleMove);
            document.body.addEventListener('mouseup', forwardUp);
        }

        grab_target = e.target;
        e.target.classList.add('grabbed');
    }

    function handleHandleMove(e, idx) {
        if (grab_target) {
            let main_el = root_el.querySelector('.ppik_main');

            let date = positionToDate(main_el, e.clientX);
            current_dates[idx] = clampDate(date, idx);

            render(renderWidget(), root_el);
        }
    }

    function handleBarDown(e) {
        if (Element.prototype.setPointerCapture) {
            if (e.type === 'pointerdown') {
                e.target.onpointermove = handleBarMove;
                e.target.setPointerCapture(e.pointerId);
            }
        } else if (!grab_target) {
            function forwardUp(e) {
                document.body.removeEventListener('mousemove', handleBarMove);
                document.body.removeEventListener('mouseup', forwardUp);
                handlePointerUp(e);
            }

            document.body.addEventListener('mousemove', handleBarMove);
            document.body.addEventListener('mouseup', forwardUp);
        }

        grab_target = e.target;
        e.target.classList.add('grabbed');

        grab_offset = e.offsetX;
        grab_start_date = current_dates[0];
        grab_end_date = current_dates[1];
    }

    function handleBarMove(e) {
        if (grab_target) {
            let main_el = root_el.querySelector('.ppik_main');

            let delta = grab_end_date.diff(grab_start_date);
            let delta_months = (grab_end_date.year - grab_start_date.year) * 12 +
                               (grab_end_date.month - grab_start_date.month);

            let pos = positionToDate(main_el, e.clientX - grab_offset);
            let start_date = forceDay(pos, grab_start_date.day);
            let end_date = forceDay(start_date.plusMonths(delta_months), grab_end_date.day);

            if (start_date < limit_dates[0]) {
                current_dates[0] = limit_dates[0];
                if (grab_start_date.day !== limit_dates[0].day) {
                    current_dates[1] = limit_dates[0].plus(delta);
                } else {
                    current_dates[1] = forceDay(limit_dates[0].plusMonths(delta_months), grab_end_date.day);
                }
            } else if (end_date > limit_dates[1]) {
                if (grab_end_date.day !== limit_dates[1].day) {
                    current_dates[0] = limit_dates[1].minus(delta);
                } else {
                    current_dates[0] = forceDay(limit_dates[1].minusMonths(delta_months), grab_start_date.day);
                }
                current_dates[1] = limit_dates[1];
            } else {
                current_dates[0] = clampDate(start_date, 0);
                current_dates[1] = clampDate(end_date, 1);
            }

            render(renderWidget(), root_el);
        }
    }

    function handlePointerUp(e) {
        grab_target = null;
        e.target.classList.remove('grabbed');
        e.target.onpointermove = null;

        if (!self.clickHandler.call(self, e, current_dates[0], current_dates[1]))
            render(renderWidget(), root_el);
    }

    function dateToPosition(date) {
        let pos = 100 * date.diff(limit_dates[0]) / limit_dates[1].diff(limit_dates[0]);
        return `${pos}%`;
    }

    function positionToDate(main_el, pos) {
        let rect = main_el.getBoundingClientRect();

        let delta_days = Math.floor(limit_dates[1].diff(limit_dates[0]) * ((pos - rect.left) / rect.width));
        let date = limit_dates[0].plus(delta_days);

        return date;
    }

    function clampDate(date, idx) {
        switch (idx) {
            case 0: {
                let max = current_dates[1].minus(1);

                if (date <= limit_dates[0]) {
                    return limit_dates[0];
                } else if (date >= max) {
                    return max;
                } else {
                    return date;
                }
            } break;

            case 1: {
                let min = current_dates[0].plus(1);

                if (date >= limit_dates[1]) {
                    return limit_dates[1];
                } else if (date <= min) {
                    return min;
                } else {
                    return date;
                }
            } break;
        }
    }

    function forceDay(date, day) {
        day = Math.min(LocalDate.daysInMonth(date.year, date.month), day);
        date = new LocalDate(date.year, date.month, day);

        return date;
    }
}

export { PeriodPicker }
