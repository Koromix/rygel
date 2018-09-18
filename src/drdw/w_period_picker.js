// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function PeriodPicker(min_date, max_date, start_date, end_date)
{
    this.changeHandler = null;

    let self = this;
    let widget = null;
    let main = null;
    let handles = null;
    let bar = null;

    let grab_offset;
    let grab_diff_year;
    let grab_diff_month;
    let grab_start_day;
    let grab_end_day;

    let parser = createElement('input', {type: 'date'});
    function strToDate(str)
    {
        // Put a few hours into the day to avoid DST problems
        parser.value = str;
        let date = parser.valueAsDate;
        date.setHours(2);

        return date;
    }

    function syncHandle(handle)
    {
        let new_pos = 100 * (handle.lastChild.valueAsDate - min_date) / (max_date - min_date);
        handle.style.left = '' + new_pos + '%';
    }

    function syncBar()
    {
        bar.style.left = handles[0].style.left;
        bar.style.width = 'calc(' + handles[1].style.left + ' - ' + handles[0].style.left + ')';
    }

    function clampHandleDate(handle, date)
    {
        let min = min_date;
        let max = max_date;
        if (handle !== handles[0]) {
            min = handles[0].lastChild.valueAsDate;
            min.setDate(min.getDate() + 1);
            min.setHours(2);
        }
        if (handle !== handles[1]) {
            max = handles[1].lastChild.valueAsDate;
            max.setDate(min.getDate() - 1);
            max.setHours(2);
        }

        if (date < min) {
            return min;
        } else if (date > max) {
            return max;
        } else {
            return date;
        }
    }

    function positionToDate(pos)
    {
        let date_delta;
        {
            let min_pos = 0;
            let max_pos = main.offsetWidth;
            if (pos < min_pos) {
                pos = min_pos;
            } else if (pos > max_pos) {
                pos = max_pos;
            }

            date_delta = ((max_date - min_date) * ((pos - min_pos) / (max_pos - min_pos)));
        }

        // Is there any part of the standard JS library that isn't complete crap?
        let date = new Date(min_date);
        date.setTime(date.getTime() + date_delta + 2 * 86400000);
        date.setDate(1);
        date.setHours(2);

        return date;
    }

    function handleHandleDown(e)
    {
        this.setPointerCapture(e.pointerId);
    }

    function handleHandleMove(e)
    {
        if (this.hasPointerCapture(e.pointerId)) {
            let handle = this.parentNode;
            let input = handle.lastChild;

            let new_date = positionToDate(e.clientX - main.offsetLeft);
            new_date = clampHandleDate(handle, new_date, handle);
            input.valueAsDate = new_date;

            syncHandle(handle);
            syncBar();
        }
    }

    function handleDateChange(e)
    {
        let handle = this.parentNode;

        if (this.valueAsDate) {
            this.valueAsDate = clampHandleDate(handle, this.valueAsDate);
        } else {
            this.valueAsDate = (handle === handles[0]) ? min_date : max_date;
        }

        syncHandle(this.parentNode);
        syncBar();

        if (self.changeHandler)
            setTimeout(function() { self.changeHandler.call(widget); }, 0);
    }

    function handleBarDown(e)
    {
        this.setPointerCapture(e.pointerId);

        let start_date = handles[0].lastChild.valueAsDate;
        let end_date = handles[1].lastChild.valueAsDate;

        grab_offset = e.offsetX;
        grab_diff_year = end_date.getFullYear() - start_date.getFullYear();
        grab_diff_month = end_date.getMonth() - start_date.getMonth();
        grab_start_day = start_date.getDate();
        grab_end_day = end_date.getDate();
    }

    function handleBarMove(e)
    {
        if (this.hasPointerCapture(e.pointerId)) {
            let new_start_date = positionToDate(e.clientX - main.offsetLeft - grab_offset);
            let new_end_date = new Date(new_start_date.getFullYear() + grab_diff_year,
                                        new_start_date.getMonth() + grab_diff_month,
                                        grab_end_day, 2);
            while (new_end_date > max_date) {
                new_start_date.setMonth(new_start_date.getMonth() - 1);
                new_end_date.setMonth(new_end_date.getMonth() - 1);
            }
            new_start_date.setDate(grab_start_day);

            handles[0].lastChild.valueAsDate = new_start_date;
            handles[1].lastChild.valueAsDate = new_end_date;

            syncHandle(handles[0]);
            syncHandle(handles[1]);
            syncBar();
        }
    }

    function handlePointerUp(e)
    {
        if (self.changeHandler)
            setTimeout(function() { self.changeHandler.call(widget); }, 0);
    }

    this.getValues = function() {
        return [handles[0].lastChild.value, handles[1].lastChild.value];
    }

    this.getWidget = function() {
        return widget;
    }

    widget = createElement('div', {class: 'ppik'},
        // This dummy button catches click events that happen when a label encloses the widget
        createElement('button', {style: 'display: none;', click: function(e) { e.preventDefault(); }}),

        createElement('div', {class: 'ppik_main'},
            createElement('div', {class: 'ppik_line'}),
            createElement('div', {class: 'ppik_bar', pointerdown: handleBarDown,
                                  pointermove: handleBarMove, pointerup: handlePointerUp}),
            createElement('div', {class: 'ppik_handle'},
                createElement('div', {pointerdown: handleHandleDown, pointermove: handleHandleMove,
                                      pointerup: handlePointerUp}),
                createElement('input', {type: 'date', style: 'bottom: 4px;', change: handleDateChange})
            ),
            createElement('div', {class: 'ppik_handle'},
                createElement('div', {pointerdown: handleHandleDown, pointermove: handleHandleMove,
                                      pointerup: handlePointerUp}),
                createElement('input', {type: 'date', style: 'top: 18px;', change: handleDateChange})
            )
        )
    );
    main = widget.querySelector('.ppik_main');
    handles = widget.querySelectorAll('.ppik_handle');
    bar = widget.querySelector('.ppik_bar');

    min_date = strToDate(min_date) || new Date(1900, 1, 1);
    max_date = strToDate(max_date) || new Date(2100, 1, 1);
    start_date = start_date ? strToDate(start_date) : min_date;
    end_date = end_date ? strToDate(end_date) : max_date;

    handles[0].lastChild.valueAsDate = start_date;
    handles[1].lastChild.valueAsDate = end_date;

    syncHandle(handles[0]);
    syncHandle(handles[1]);
    syncBar();

    widget.object = this;
}
