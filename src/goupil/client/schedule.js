// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function Schedule(widget, resources_map, meetings_map) {
    this.changeResourcesHandler = (key, resources) => {};
    this.changeMeetingsHandler = (key, meetings) => {};

    let self = this;

    let month_names = ['Janvier', 'Février', 'Mars', 'Avril', 'Mai', 'Juin', 'Juillet',
                       'Août', 'Septembre', 'Octobre', 'Novembre', 'Décembre'];
    let week_day_names = ['Lundi', 'Mardi', 'Mercredi', 'Jeudi', 'Vendredi', 'Samedi', 'Dimanche'];

    let main;
    let footer;

    let current_mode = 'meetings';
    let current_month;
    let current_year;

    let drag_slot_ref;

    let prev_event_time = 0;

    let copy_resources;
    let copy_ignore = new Set;

    function slowDownEvents(delay, func) {
        return e => {
            let now = performance.now();
            if (now - prev_event_time >= delay) {
                func(e);
                prev_event_time = now;
            }
        };
    }

    function formatTime(time) {
        let hour = Math.floor(time / 100);
        let min = time % 100;

        return `${hour}h${min < 10 ? ('0' + min) : min}`;
    }

    function parseTime(str) {
        if (!/^[0-9]{1,2}h[0-9]{0,2}$/.test(str))
            return null;

        let [hours, min] = str.split('h').map(str => parseInt(str, 10) || 0);
        if (hours > 23 || min > 59)
            return null;

        return hours * 100 + min;
    }

    function isLeapYear(year) {
        return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    }

    function daysInMonth(year, month) {
        let days_per_month = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
        return days_per_month[month - 1] + (month == 2 && isLeapYear(year));
    }

    function getWeekDay(year, month, day) {
        if (month < 3) {
            year--;
            month += 12;
        }

        let century = Math.floor(year / 100);
        year %= 100;

        // Zeller's congruence:
        // https://en.wikipedia.org/wiki/Zeller%27s_congruence
        return (day + Math.floor(13 * (month + 1) / 5) + year + Math.floor(year / 4) + Math.floor(century / 4) + 5 * century + 5) % 7;
    }

    function getMonthDays(year, month, add_skip_days = false) {
        let days_count = daysInMonth(year, month);
        let start_week_day = getWeekDay(year, month, 1);

        let days = [];
        if (add_skip_days) {
            for (let i = 0; i < start_week_day; i++)
                days.push(null);
        }
        for (let i = 1; i <= days_count; i++) {
            let month_str = (month < 10 ? '0' : '') + month;
            let day_str = (i < 10 ? '0' : '') + i;

            days.push({
                key: `${year}-${month_str}-${day_str}`,
                date: `${day_str}/${month_str}/${year}`,
                week_day: week_day_names[(start_week_day + i - 1) % 7]
            });
        }

        return days;
    }

    function expandSlots(resources) {
        let slots = [];
        for (let res of resources) {
            for (let j = 0; j < res.slots; j++) {
                slots.push({
                    time: res.time,
                    overbook: false
                });
            }
            for (let j = 0; j < res.overbook; j++) {
                slots.push({
                    time: res.time,
                    overbook: true
                });
            }
        }

        return slots;
    }

    function createMeeting(slot_ref) {
        let name = prompt('Name?');
        if (name !== null) {
            slot_ref.meetings.splice(slot_ref.splice_idx, 0, {
                time: slot_ref.time,
                identity: name
            });
            self.changeMeetingsHandler(slot_ref.day.key, slot_ref.meetings);

            renderAll();
        }
    }

    function deleteMeeting(slot_ref) {
        if (confirm('Are you sure?')) {
            slot_ref.meetings.splice(slot_ref.splice_idx, 1);
            self.changeMeetingsHandler(slot_ref.day.key, slot_ref.meetings);

            renderAll();
        }
    }

    function moveMeeting(src_ref, dest_ref)
    {
        if (dest_ref.identity) {
            // Exchange
            dest_ref.meetings.splice(dest_ref.splice_idx, 1, {
                time: dest_ref.time,
                identity: src_ref.identity
            });
            src_ref.meetings.splice(src_ref.splice_idx, 1, {
                time: src_ref.time,
                identity: dest_ref.identity
            });
        } else if (dest_ref.time < src_ref.time) {
            // Move to earlier time of day
            src_ref.meetings.splice(src_ref.splice_idx, 1);
            dest_ref.meetings.splice(dest_ref.splice_idx, 0, {
                time: dest_ref.time,
                identity: src_ref.identity
            });
        } else {
            // Move to later time of day (distinction only matters for same day moves)
            dest_ref.meetings.splice(dest_ref.splice_idx, 0, {
                time: dest_ref.time,
                identity: src_ref.identity
            });
            src_ref.meetings.splice(src_ref.splice_idx, 1);
        }

        self.changeMeetingsHandler(src_ref.day.key, src_ref.meetings);
        self.changeMeetingsHandler(dest_ref.day.key, dest_ref.meetings);

        renderAll();
    }

    function renderMeetings() {
        let days = getMonthDays(current_year, current_month, true);

        render(html`<div class="sc_days">${days.map(day => {
            if (day !== null) {
                if (!meetings_map[day.key])
                    meetings_map[day.key] = [];
                if (!resources_map[day.key])
                    resources_map[day.key] = [];

                let meetings = meetings_map[day.key];
                let resources = resources_map[day.key];
                let slots = expandSlots(resources);

                // Match slots and meetings
                let slot_refs = [];
                let normal_slots = 0;
                let used_slots = 0;
                let warn_slots = 0;
                for (let j = 0, k = 0; j < meetings.length || k < slots.length;) {
                    let min_time = Math.min(j < meetings.length ? meetings[j].time : 2400,
                                            k < slots.length ? slots[k].time : 2400);

                    // Meetings
                    while (j < meetings.length && meetings[j].time == min_time) {
                        let meeting = meetings[j];

                        let slot_ref = {
                            day: day,
                            meetings: meetings,
                            time: meeting.time,
                            identity: meeting.identity,
                            cls: 'sc_slot_used',
                            splice_idx: j,
                        };
                        slot_refs.push(slot_ref);

                        if (k < slots.length && slots[k].time == min_time) {
                            normal_slots++;
                            used_slots++;

                            if (slots[k].overbook)
                                slot_ref.cls += ' sc_slot_overbook';

                            k++;
                        } else {
                            warn_slots++;

                            console.log(`No slot for ${meeting.identity} (${day.date} ${formatTime(meeting.time)})`);
                            slot_ref.cls += ' sc_slot_warn';
                        }

                        j++;
                    }

                    // Empty slots
                    let first_empty_slot = true;
                    while (k < slots.length && slots[k].time == min_time) {
                        let slot = slots[k];

                        if (!slot.overbook || first_empty_slot) {
                            let slot_ref = {
                                day: day,
                                meetings: meetings,
                                time: slot.time,
                                identity: null,
                                cls: slot.overbook ? 'sc_slot_overbook' : '',
                                splice_idx: j
                            };
                            slot_refs.push(slot_ref);

                            normal_slots++;
                        }
                        first_empty_slot = false;

                        k++;
                    }
                }

                // Select day CSS class
                let cls;
                if (warn_slots) {
                    cls = 'sc_day sc_day_warn';
                } else if (!slot_refs.length) {
                    cls = 'sc_day sc_day_disabled';
                } else if (!used_slots) {
                    cls = 'sc_day sc_day_empty';
                } else if (used_slots < slot_refs.length) {
                    cls = 'sc_day sc_day_some';
                } else {
                    cls = 'sc_day sc_day_full';
                }

                // Create day DOM element
                return html`
                    <div class=${cls}>
                        <div class="sc_head">
                            <div class="sc_head_week_day">${day.week_day}</div>
                            <div class="sc_head_date">${day.date}</div>
                            <div class="sc_head_count">${slot_refs.length ? (`${used_slots}/${normal_slots}` + (warn_slots ? ` + ${warn_slots}` : '')) : 'Fermé'}</div>
                        </div>
                        <table class="sc_slots">${slot_refs.map(slot_ref => {
                            function dragStart(e) {
                                if (slot_ref.identity) {
                                    drag_slot_ref = slot_ref;
                                    e.dataTransfer.setData('application/x-meeting', '');
                                }
                            }
                            function dragOverSlot(e) {
                                if (e.dataTransfer.types.includes('application/x-meeting'))
                                    e.preventDefault();
                            }
                            function dropSlot(e) {
                                moveMeeting(drag_slot_ref, slot_ref);
                                e.preventDefault();
                            }

                            return html`<tr class=${slot_ref.cls} @dragover=${dragOverSlot} @drop=${dropSlot}>
                                <td class="sc_slot_time" draggable="true" @dragstart=${dragStart}>${formatTime(slot_ref.time)}</td>
                                <td class="sc_slot_identity">${slot_ref.identity || ''}</td>
                                <td class="sc_slot_edit">
                                    ${slot_ref.identity ?
                                        html`<a href="#" @click=${e => { deleteMeeting(slot_ref); e.preventDefault(); }}>x</a>` :
                                        html`<a href="#" @click=${e => { createMeeting(slot_ref); e.preventDefault(); }}>+</a>`
                                    }
                                </td>
                            </tr>`;
                        })}</table>
                    </div>
                `;
            } else {
                return html`<div class="sc_skip"></div>`;
            }
        })}</div>`, main);
    }

    function createResource(day) {
        let time = parseTime(prompt('Time?'));
        if (time !== null) {
            let resources = resources_map[day.key];

            let prev_res = resources.find(res => res.time === time);
            if (prev_res) {
                prev_res.slots++;
            } else {
                resources.push({
                    time: time,
                    slots: 1,
                    overbook: 0
                });
                resources.sort((res1, res2) => res1.time - res2.time);
            }

            self.changeResourcesHandler(day.key, resources);

            renderAll();
        }
    }

    function deleteResource(day, res_idx) {
        if (confirm('Are you sure?')) {
            let resources = resources_map[day.key];

            resources.splice(res_idx, 1);
            self.changeResourcesHandler(day.key, resources);

            renderAll();
        }
    }

    function closeDay(day) {
        if (confirm('Are you sure?')) {
            let resources = resources_map[day.key];

            resources_map[day.key].length = 0;
            self.changeResourcesHandler(day.key, resources);

            renderAll();
        }
    }

    function startCopy(day) {
        copy_resources = resources_map[day.key];

        // Ignore days with same configuration
        {
            let json = JSON.stringify(copy_resources);
            let days = getMonthDays(current_year, current_month);

            copy_ignore.clear();
            for (let day of days) {
                // Slow, but does the job
                if (JSON.stringify(resources_map[day.key]) === json)
                    copy_ignore.add(day.key);
            }
        }

        current_mode = 'copy';
        renderCopy();
    }

    function renderSettings() {
        let days = getMonthDays(current_year, current_month, true);

        render(html`<div class="sc_days">${days.map(day => {
            if (day !== null) {
                let resources = resources_map[day.key] || [];

                let normal_count = resources.reduce((acc, res) => acc + res.slots, 0);
                let overbook_count = resources.reduce((acc, res) => acc + res.overbook, 0);

                let cls;
                if (normal_count + overbook_count) {
                    cls = 'sc_day sc_opt_some';
                } else {
                    cls = 'sc_day sc_opt_empty';
                }

                return html`<div class="${cls}">
                    <div class="sc_head">
                        <div class="sc_head_week_day">${day.week_day}</div>
                        <div class="sc_head_date">${day.date}</div>
                        <div class="sc_head_count">${(normal_count + overbook_count) ? `${normal_count}+${overbook_count}` : 'Fermé'}</div>
                    </div>

                    ${resources.length ? html`<table class="sc_slots">
                        <tr>
                            <th style="width: 40px;"></th>
                            <th>places</th>
                            <th>surbooking</th>
                            <th style="width: 10px;"></th>
                        </tr>
                        ${resources.map((res, res_idx) => {
                            function changeSlots(delta) {
                                return e => {
                                    res.slots = Math.max(0, res.slots + delta);
                                    self.changeResourcesHandler(day.key, resources);

                                    renderAll();
                                    e.preventDefault();
                                };
                            }
                            function changeOverbook(delta) {
                                return e => {
                                    res.overbook = Math.max(0, res.overbook + delta);
                                    self.changeResourcesHandler(day.key, resources);

                                    renderAll();
                                    e.preventDefault();
                                };
                            }

                            return html`<tr>
                                <td class="sc_slot_time">${formatTime(res.time)}</td>
                                <td class="sc_slot_option">${res.slots} <a href="#" @click=${changeSlots(1)}>▲</a><a href="#" @click=${changeSlots(-1)}>▼</a></td>
                                <td class="sc_slot_option">${res.overbook} <a href="#" @click=${changeOverbook(1)}>▲</a><a href="#" @click=${changeOverbook(-1)}>▼</a></td>
                                <td class="sc_slot_edit"><a href="#" @click=${e => { deleteResource(day, res_idx); e.preventDefault(); }}>x</a></td>
                            </tr>`;
                        })}
                    </table>` : ''}

                    <div class="sc_actions">
                        <a href="#" @click=${e => { createResource(day); e.preventDefault(); }}>Nouveau</a> |
                        <a href="#" @click=${e => { startCopy(day); e.preventDefault(); }}>Copier</a> |
                        <a href="#" @click=${e => { closeDay(day); e.preventDefault(); }}>Fermer</a>
                    </div>
                </div>`;
            } else {
                return html`<div class="sc_skip"></div>`;
            }
        })}</div>`, main);
    }

    function executeCopy(dest_day) {
        copy_ignore.add(dest_day.key);

        resources_map[dest_day.key] = copy_resources.map(res => Object.assign({}, res));
        self.changeResourcesHandler(dest_day.key, resources_map[dest_day.key]);

        renderAll();
    }

    function executeCopyAndEnd(dest_day) {
        copy_ignore.add(dest_day.key);

        resources_map[dest_day.key] = copy_resources.map(res => Object.assign({}, res));
        self.changeResourcesHandler(dest_day.key, resources_map[dest_day.key]);

        current_mode = 'settings';
        renderAll();
    }

    function renderCopy() {
        let days = getMonthDays(current_year, current_month, true);

        render(html`<div class="sc_days">${days.map(day => {
            if (day !== null) {
                let resources = resources_map[day.key] || [];

                let normal_count = resources.reduce((acc, res) => acc + res.slots, 0);
                let overbook_count = resources.reduce((acc, res) => acc + res.overbook, 0);

                let cls;
                if (normal_count + overbook_count) {
                    cls = 'sc_day sc_opt_some';
                } else {
                    cls = 'sc_day sc_opt_empty';
                }

                return html`<div class="${cls}">
                    <div class="sc_head">
                        <div class="sc_head_week_day">${day.week_day}</div>
                        <div class="sc_head_date">${day.date}</div>
                        <div class="sc_head_count">${(normal_count + overbook_count) ? `${normal_count}+${overbook_count}` : 'Fermé'}</div>
                    </div>

                    <div class="sc_copy" style=${copy_ignore.has(day.key) ? 'opacity: 0.1' : ''}>
                        <a href="#" @click=${e => { executeCopy(day); e.preventDefault(); }}>+</a>
                        <a href="#" @click=${e => { executeCopyAndEnd(day); e.preventDefault(); }}>⇳</a>
                    </div>
                </div>`;
            } else {
                return html`<div class="sc_skip"></div>`;
            }
        })}</div>`, main);
    }

    function switchToPreviousMonth() {
        if (current_month > 1) {
            self.render(current_year, current_month - 1);
        } else {
            self.render(current_year - 1, 12);
        }
    }

    function switchToNextMonth() {
        if (current_month < 12) {
            self.render(current_year, current_month + 1);
        } else {
            self.render(current_year + 1, 1);
        }
    }

    function switchToMonth(month) {
        if (month !== current_month)
            self.render(current_year, month);
    }

    function switchToPreviousYear() {
        self.render(current_year - 1, current_month);
    }

    function switchToNextYear() {
        self.render(current_year + 1, current_month);
    }

    function toggleMode() {
        switch (current_mode) {
            case 'meetings': { current_mode = 'settings'; } break;
            case 'settings': { current_mode = 'meetings'; } break;
            case 'copy': { current_mode = 'settings'; } break;
        }

        renderAll();
    }

    function renderFooter() {
        render(html`<nav class="sc_footer">
            <a class="sc_deploy" href="#"
               @click=${e => { e.target.parentNode.querySelector('.sc_months').classList.toggle('active'); e.preventDefault(); }}></a>

            <div class="sc_selector">
                <a href="#"
                   @click=${e => { switchToPreviousMonth(); e.preventDefault(); }}
                   @dragover=${slowDownEvents(300, switchToPreviousMonth)}>≪</a>
                <b>${month_names[current_month - 1]}</b>
                <a href="#"
                   @click=${e => { switchToNextMonth(); e.preventDefault(); }}
                   @dragover=${slowDownEvents(300, switchToNextMonth)}>≫</a>
            </div>

            <div class="sc_months">${month_names.map((name, idx) => {
                let month = idx + 1;

                let warn = false;
                {
                    let days = getMonthDays(current_year, month);

                    for (let i = 0; !warn && i < days.length; i++) {
                        let day = days[i];

                        let meetings = meetings_map[day.key] || [];
                        let resources = resources_map[day.key] || [];
                        let slots = expandSlots(resources);

                        for (let j = 0, k = 0; j < meetings.length; j++) {
                            while (k < slots.length && slots[k].time < meetings[j].time) {
                                k++;
                            }

                            if (k >= slots.length || slots[k++].time !== meetings[j].time) {
                                warn = true;
                                break;
                            }
                        }
                    }
                }

                let cls = '';
                if (warn)
                    cls += ' sc_month_warn';
                if (month === current_month)
                    cls += ' active';

                return html`<a class=${cls} href="#"
                               @click=${e => { switchToMonth(month); e.preventDefault(); }}
                               @dragover=${e => switchToMonth(month)}>${name}</a>`;
            })}</div>

            <div class="sc_selector">
                <a href="#"
                   @click=${e => { switchToPreviousYear(); e.preventDefault(); }}
                   @dragover=${slowDownEvents(300, switchToPreviousYear)}>≪</a>
                <b>${current_year}</b>
                <a href="#"
                   @click=${e => { switchToNextYear(); e.preventDefault(); }}
                   @dragover=${slowDownEvents(300, switchToNextYear)}>≫</a>
            </div>

            <a class=${current_mode === 'meetings' ? 'sc_mode' : 'sc_mode active'} href="#"
               @click=${e => { toggleMode(); e.preventDefault(); }}>⚙</a>
        </nav>`, footer);
    }

    function renderAll()
    {
        // FIXME: Can we replace a node with render, instead of replacing its content?
        // Right now, render functions create content inside these two divs instead of replacing them.
        render(html`
            <div class="sc_header">${week_day_names.map(name => html`<div>${name}</div>`)}</div>
            <div class="sc_main"></div>
            <div class="sc_footer"></div>
        `, widget);
        main = widget.querySelector('.sc_main');
        footer = widget.querySelector('.sc_footer');

        switch (current_mode) {
            case 'meetings': { renderMeetings(); } break;
            case 'settings': { renderSettings(); } break;
            case 'copy': { renderCopy(); } break;
        }

        renderFooter();
    }

    this.render = function(year, month) {
        if (year !== undefined) {
            current_year = year;
            current_month = month;
        }

        renderAll();
    };
}

let schedule = (function() {
    let self = this;

    let init = false;
    let schedule;

    function updateSchedule(year, month) {
        let queue = new FetchQueue((data, errors) => {
            if (!errors.length) {
                let main = document.querySelector('main');

                // FIXME: Use merge algorithm instead of mass replace
                schedule = new Schedule(main, data.resources, data.meetings);
                schedule.render(year, month);
            } else {
                for (let err of errors)
                    console.log(`Fetch error: ${err}`);
            }
        });

        queue.fetch('resources', `${settings.base_url}schedule/resources.json?schedule=pl&year=${year}&month=${month}`);
        queue.fetch('meetings', `${settings.base_url}schedule/meetings.json?schedule=pl&year=${year}&month=${month}`);
    }

    this.activate = function() {
        document.title = 'goupil schedule';

        if (!init) {
            goupil.listenToServerEvent('schedule', e => updateSchedule(year, month));
            init = true;
        }

        let year;
        let month;
        {
            let date = new Date();

            year = date.getFullYear();
            month = date.getMonth() + 1;
        }

        if (schedule) {
            schedule.render(year, month);
        } else {
            updateSchedule(year, month);
        }
    };

    return this;
}).call({});
