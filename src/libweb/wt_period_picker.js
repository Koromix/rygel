// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let wt_period_picker = (function() {
    function PeriodPicker() {
        let self = this;

        this.changeHandler = null;

        let root_el;

        let limit_dates = [dates.create(1900, 1, 1), dates.create(2100, 1, 1)];
        let current_dates = limit_dates;

        let main_el;
        let handle_els;
        let bar_el;

        let grab_target;
        let grab_offset;
        let grab_start_date;
        let grab_end_date;

        this.getRootElement = function() { return root_el; };

        this.setLimitDates = function(dates) { limit_dates = dates; };
        this.getLimitDates = function() { return limit_dates; };
        this.setDates = function(dates) { current_dates = dates.map((date, idx) => date || limit_dates[idx]); };
        this.getDates = function() { return current_dates; };

        function syncHandle(handle, date) {
            let input = handle.querySelector('input');

            let new_pos = 100 * date.diff(limit_dates[0]) / limit_dates[1].diff(limit_dates[0]);
            handle.style.left = '' + new_pos + '%';
            input.value = date.toString();
        }

        function syncView() {
            syncHandle(handle_els[0], current_dates[0]);
            syncHandle(handle_els[1], current_dates[1]);

            bar_el.style.left = handle_els[0].style.left;
            bar_el.style.width = 'calc(' + handle_els[1].style.left + ' - ' + handle_els[0].style.left + ')';
        }

        function positionToDate(pos) {
            let delta_days = Math.floor(limit_dates[1].diff(limit_dates[0]) * (pos / main_el.offsetWidth));
            let date = limit_dates[0].plus(delta_days);

            return date;
        }

        function forceDay(date, day) {
            day = Math.min(dates.daysInMonth(date.year, date.month), day);
            date = dates.create(date.year, date.month, day);

            return date;
        }

        function clampStartDate(date) {
            let max = current_dates[1].minus(1);

            if (date <= limit_dates[0]) {
                return limit_dates[0];
            } else if (date >= max) {
                return max;
            } else {
                return date;
            }
        }

        function clampEndDate(date) {
            let min = current_dates[0].plus(1);

            if (date >= limit_dates[1]) {
                return limit_dates[1];
            } else if (date <= min) {
                return min;
            } else {
                return date;
            }
        }

        function handleHandleDown(e) {
            if (Element.prototype.setPointerCapture) {
                e.target.setPointerCapture(e.pointerId);
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
        }

        function handleHandleMove(e) {
            if (grab_target) {
                let handle = grab_target.parentNode;
                let date = positionToDate(e.clientX - main_el.offsetLeft);

                if (handle === handle_els[0]) {
                    current_dates[0] = clampStartDate(date);
                } else {
                    current_dates[1] = clampEndDate(date);
                }

                syncView();
            }
        }

        function handleDateChange(e) {
            let handle = e.target.parentNode;
            let input = handle.querySelector('input');

            if (input.value) {
                let date = dates.fromString(input.value);

                if (handle === handle_els[0]) {
                    current_dates[0] = clampStartDate(date);
                } else {
                    current_dates[1] = clampEndDate(date);
                }
            } else {
                if (handle === handle_els[0]) {
                    current_dates[0] = limit_dates[0];
                } else {
                    current_dates[1] = limit_dates[1];
                }

                if (self.changeHandler)
                    setTimeout(() => self.changeHandler.call(self, e), 0);
            }

            syncView();
        }

        function handleDateFocusOut(e) {
            if (self.changeHandler)
                setTimeout(() => self.changeHandler.call(self, e), 0);
        }

        function handleBarDown(e) {
            if (Element.prototype.setPointerCapture) {
                e.target.setPointerCapture(e.pointerId);
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

            grab_offset = e.offsetX;
            grab_start_date = current_dates[0];
            grab_end_date = current_dates[1];
        }

        function handleBarMove(e) {
            if (grab_target) {
                let delta = grab_end_date.diff(grab_start_date);
                let delta_months = (grab_end_date.year - grab_start_date.year) * 12 +
                                   (grab_end_date.month - grab_start_date.month);

                let start_date = forceDay(positionToDate(e.clientX - main_el.offsetLeft - grab_offset), grab_start_date.day);
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
                    current_dates[0] = clampStartDate(start_date);
                    current_dates[1] = clampEndDate(end_date);
                }

                syncView();
            }
        }

        function handlePointerUp(e) {
            grab_target = null;

            if (self.changeHandler)
                setTimeout(() => self.changeHandler.call(self, e), 0);
        }

        function makeMouseElement(cls, down, move) {
            if (Element.prototype.setPointerCapture) {
                return html`<div class=${cls} @pointerdown=${down} @pointermove=${move}
                                 @pointerup=${handlePointerUp}></div>`;
            } else {
                return html`<div class=${cls} @mousedown=${down}</div>`;
            }
        }

        this.render = function(new_root_el) {
            root_el = new_root_el;

            // The dummy button catches click events that happen when a label encloses the widget
            render(html`
                <div class="ppik">
                    <button style="display: none;" @click=${e => e.preventDefault()}></button>

                    <div class="ppik_main">
                        <div class="ppik_line"></div>
                        ${makeMouseElement('ppik_bar', handleBarDown, handleBarMove)}
                        <div class="ppik_handle">
                            ${makeMouseElement('', handleHandleDown, handleHandleMove)}
                            <input type="date" style="bottom: 4px;" @change=${handleDateChange}
                                   @focusout=${handleDateFocusOut}/>
                        </div>
                        <div class="ppik_handle">
                            ${makeMouseElement('', handleHandleDown, handleHandleMove)}
                            <input type="date" style="top: 18px;" @change=${handleDateChange}
                                   @focusout=${handleDateFocusOut}/>
                        </div>
                    </div>
                </div>
            `, root_el);
            main_el = root_el.querySelector('.ppik_main');
            handle_els = root_el.querySelectorAll('.ppik_handle');
            bar_el = root_el.querySelector('.ppik_bar');

            syncView();
        };
    }

    this.create = function() { return new PeriodPicker(); };

    return this;
}).call({});
