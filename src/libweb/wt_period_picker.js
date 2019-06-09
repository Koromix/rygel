// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let wt_period_picker = (function() {
    function PeriodPicker()
    {
        let self = this;

        this.changeHandler = null;

        let limit_dates = [new Date(1900, 1, 1), new Date(2100, 1, 1)];
        let current_dates = limit_dates.map(date => new Date(date));

        let main_el;
        let handle_els;
        let bar_el;

        let grab_capture = false;
        let grab_offset;
        let grab_diff_year;
        let grab_diff_month;
        let grab_start_day;
        let grab_end_day;

        // Used by dateToStr() and strToDate()
        let date_el = document.createElement('input');
        date_el.setAttribute('type', 'date');

        this.getRootElement = function() { return root_el; };

        this.setLimitDates = function(dates) { limit_dates = dates.map(strToDate); };
        this.getLimitDates = function() { return limit_dates.map(dateToStr); };
        this.setDates = function(dates) { current_dates = dates.map((str, idx) => str ? strToDate(str) : limit_dates[idx]); };
        this.getDates = function() { return current_dates.map(dateToStr); };

        function dateToStr(date)
        {
            date_el.valueAsDate = date;
            return date_el.value;
        }

        function strToDate(str)
        {
            date_el.value = str;

            let date = date_el.valueAsDate;
            // Put a few hours into the day to avoid DST problems
            date.setHours(2);

            return date;
        }

        function syncHandle(handle, date)
        {
            let input = handle.querySelector('input');

            let new_pos = 100 * (date - limit_dates[0]) / (limit_dates[1] - limit_dates[0]);
            handle.style.left = '' + new_pos + '%';
            input.valueAsDate = date;
        }

        function syncView()
        {
            syncHandle(handle_els[0], current_dates[0]);
            syncHandle(handle_els[1], current_dates[1]);

            bar_el.style.left = handle_els[0].style.left;
            bar_el.style.width = 'calc(' + handle_els[1].style.left + ' - ' + handle_els[0].style.left + ')';
        }

        function positionToDate(pos)
        {
            let date_delta;
            {
                let min_pos = 0;
                let max_pos = main_el.offsetWidth;
                if (pos < min_pos) {
                    pos = min_pos;
                } else if (pos > max_pos) {
                    pos = max_pos;
                }

                date_delta = ((limit_dates[1] - limit_dates[0]) * ((pos - min_pos) / (max_pos - min_pos)));
            }

            // Is there any part of the standard JS library that isn't complete crap?
            let date = new Date(limit_dates[0]);
            date.setTime(date.getTime() + date_delta + 2 * 86400000);
            date.setDate(1);
            date.setHours(2);

            return date;
        }

        function clampStartDate(date)
        {
            let max = new Date(current_dates[1]);
            max.setDate(max.getDate() - 1);
            max.setHours(2);

            if (date <= limit_dates[0]) {
                return limit_dates[0];
            } else if (date >= max) {
                return max;
            } else {
                return date;
            }
        }

        function clampEndDate(date)
        {
            let min = new Date(current_dates[0]);
            min.setDate(min.getDate() + 1);
            min.setHours(2);

            if (date >= limit_dates[1]) {
                return limit_dates[1];
            } else if (date <= min) {
                return min;
            } else {
                return date;
            }
        }

        function handleHandleDown(e)
        {
            e.target.setPointerCapture(e.pointerId);
            grab_capture = true;
        }

        function handleHandleMove(e)
        {
            if (grab_capture) {
                let handle = e.target.parentNode;
                let date = positionToDate(e.clientX - main_el.offsetLeft);

                if (handle === handle_els[0]) {
                    current_dates[0] = clampStartDate(date);
                } else {
                    current_dates[1] = clampEndDate(date);
                }

                syncView();
            }
        }

        function handleDateChange(e)
        {
            let handle = e.target.parentNode;
            let input = handle.querySelector('input');

            if (input.valueAsDate) {
                if (handle === handle_els[0]) {
                    current_dates[0] = clampStartDate(input.valueAsDate);
                } else {
                    current_dates[1] = clampEndDate(input.valueAsDate);
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

        function handleDateFocusOut(e)
        {
            if (self.changeHandler)
                setTimeout(() => self.changeHandler.call(self, e), 0);
        }

        function handleBarDown(e)
        {
            e.target.setPointerCapture(e.pointerId);
            grab_capture = true;

            grab_offset = e.offsetX;
            grab_diff_year = current_dates[1].getFullYear() - current_dates[0].getFullYear();
            grab_diff_month = current_dates[1].getMonth() - current_dates[0].getMonth();
            grab_start_day = current_dates[0].getDate();
            grab_end_day = current_dates[1].getDate();
        }

        function handleBarMove(e)
        {
            if (grab_capture) {
                current_dates[0] = positionToDate(e.clientX - main_el.offsetLeft - grab_offset);
                current_dates[1] = new Date(current_dates[0].getFullYear() + grab_diff_year,
                                            current_dates[0].getMonth() + grab_diff_month,
                                            grab_end_day, 2);
                while (current_dates[1] > limit_dates[1]) {
                    current_dates[0].setMonth(current_dates[0].getMonth() - 1);
                    current_dates[1].setMonth(current_dates[1].getMonth() - 1);
                }
                current_dates[0].setDate(grab_start_day);

                syncView();
            }
        }

        function handlePointerUp(e)
        {
            grab_capture = false;

            if (self.changeHandler)
                setTimeout(() => self.changeHandler.call(self, e), 0);
        }

        function makeMouseElement(cls, down, move)
        {
            return html`<div class=${cls} @pointerdown=${down} @pointermove=${move}
                             @pointerup=${handlePointerUp}></div>`;
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
            main_el = root_el.query('.ppik_main');
            handle_els = root_el.queryAll('.ppik_handle');
            bar_el = root_el.query('.ppik_bar');

            syncView();
        };
    }

    this.create = function() { return new PeriodPicker(); };

    return this;
}).call({});
