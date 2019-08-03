// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function Schedule(resources_map, meetings_map) {
    this.changeResourcesHandler = (key, resources) => {};
    this.changeMeetingsHandler = (key, meetings) => {};

    let self = this;

    let month_names = ['Janvier', 'Février', 'Mars', 'Avril', 'Mai', 'Juin', 'Juillet',
                       'Août', 'Septembre', 'Octobre', 'Novembre', 'Décembre'];
    let week_day_names = ['Lundi', 'Mardi', 'Mercredi', 'Jeudi', 'Vendredi', 'Samedi', 'Dimanche'];

    let days_el;
    let footer_el;

    let current_month;
    let current_year;
    let current_mode;

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
        str = str || '';

        let hours;
        let min;
        if (str.match(/^[0-9]{1,2}h(|[0-9]{2})$/)) {
            [hours, min] = str.split('h').map(str => parseInt(str, 10) || 0);
        } else if (str.match(/^[0-9]{1,2}:[0-9]{2}$/)) {
            [hours, min] = str.split(':').map(str => parseInt(str, 10) || 0);
        } else {
            return null;
        }
        if (hours > 23 || min > 59)
            return null;

        return hours * 100 + min;
    }

    function getMonthDays(year, month, add_skip_days = false) {
        let start_date = dates.create(year, month, 1);
        let start_week_day = start_date.getWeekDay();

        let days = [];
        if (add_skip_days) {
            for (let i = 0; i < start_week_day; i++)
                days.push(null);
        }
        for (let date = start_date; date.year === start_date.year &&
                                    date.month === start_date.month; date = date.plus(1)) {
            let day = {
                key: date.toString(),
                date: date.toLocaleString(),
                week_day: week_day_names[date.getWeekDay() - 1]
            };

            days.push(day);
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

    function createMeeting(slot_ref, identity) {
        slot_ref.meetings.splice(slot_ref.splice_idx, 0, {
            time: slot_ref.time,
            identity: identity
        });
        self.changeMeetingsHandler(slot_ref.day.key, slot_ref.meetings);

        renderAll();
    }

    function deleteMeeting(slot_ref) {
        slot_ref.meetings.splice(slot_ref.splice_idx, 1);
        self.changeMeetingsHandler(slot_ref.day.key, slot_ref.meetings);

        renderAll();
    }

    function moveMeeting(src_ref, dest_ref) {
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

    function showCreateMeetingDialog(e, slot_ref) {
        goupil.popup(e, form => {
            let name = form.text('name', 'Nom :', {mandatory: true});

            form.submitHandler = () => {
                createMeeting(slot_ref, name.value);
                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Créer'));
        });
    }

    function showDeleteMeetingDialog(e, slot_ref) {
        goupil.popup(e, form => {
            form.output('Voulez-vous vraiment supprimer ce rendez-vous ?');

            form.submitHandler = () => {
                deleteMeeting(slot_ref);
                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function renderMeetings() {
        let days = getMonthDays(current_year, current_month, true);

        render(days.map(day => {
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
                                    e.dataTransfer.setData('application/x.goupil.meeting', '');
                                }
                            }
                            function dragOverSlot(e) {
                                if (e.dataTransfer.types.includes('application/x.goupil.meeting'))
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
                                        html`<a href="#" @click=${e => { showDeleteMeetingDialog(e, slot_ref); e.preventDefault(); }}>x</a>` :
                                        html`<a href="#" @click=${e => { showCreateMeetingDialog(e, slot_ref); e.preventDefault(); }}>+</a>`
                                    }
                                </td>
                            </tr>`;
                        })}</table>
                    </div>
                `;
            } else {
                return html`<div class="sc_skip"></div>`;
            }
        }), days_el);
    }

    function createResource(day, time) {
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

    function deleteResource(day, res_idx) {
        let resources = resources_map[day.key];

        resources.splice(res_idx, 1);
        self.changeResourcesHandler(day.key, resources);

        renderAll();
    }

    function closeDay(day) {
        let resources = resources_map[day.key];

        resources_map[day.key].length = 0;
        self.changeResourcesHandler(day.key, resources);

        renderAll();
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

    function showCreateResourceDialog(e, day) {
        goupil.popup(e, form => {
            let time = form.text('time', 'Horaire :', {mandatory: true});

            // Check value
            let time2 = parseTime(time.value);
            if (time.value && time2 == null)
                time.error('Non valide (ex : 15h30, 7:30)');

            form.submitHandler = () => {
                createResource(day, time2);
                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Créer'));
        });
    }

    function showDeleteResourceDialog(e, day, res_idx) {
        goupil.popup(e, form => {
            form.output('Voulez-vous vraiment supprimer ces créneaux ?');

            form.submitHandler = () => {
                deleteResource(day, res_idx);
                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Supprimer'));
        });
    }

    function showCloseDayDialog(e, day) {
        goupil.popup(e, form => {
            form.output('Voulez-vous vraiment fermer cette journée ?',
                        {help: 'Ceci supprime tous les créneaux'});

            form.submitHandler = () => {
                closeDay(day);
                form.close();
            };
            form.buttons(form.buttons.std.ok_cancel('Fermer'));
        });
    }

    function renderSettings() {
        let days = getMonthDays(current_year, current_month, true);

        render(days.map(day => {
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
                                <td class="sc_slot_edit"><a href="#" @click=${e => { showDeleteResourceDialog(e, day, res_idx); e.preventDefault(); }}>x</a></td>
                            </tr>`;
                        })}
                    </table>` : ''}

                    <div class="sc_actions">
                        <a href="#" @click=${e => { showCreateResourceDialog(e, day); e.preventDefault(); }}>Nouveau</a> |
                        <a href="#" @click=${e => { startCopy(day); e.preventDefault(); }}>Copier</a> |
                        <a href="#" @click=${e => { showCloseDayDialog(e, day); e.preventDefault(); }}>Fermer</a>
                    </div>
                </div>`;
            } else {
                return html`<div class="sc_skip"></div>`;
            }
        }), days_el);
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

        render(days.map(day => {
            if (day !== null) {
                let resources = resources_map[day.key] || [];

                let normal_count = resources.reduce((acc, res) => acc + res.slots, 0);
                let overbook_count = resources.reduce((acc, res) => acc + res.overbook, 0);

                let cls = 'sc_day sc_day_copy';
                if (normal_count + overbook_count) {
                    cls += ' sc_opt_some';
                } else {
                    cls += ' sc_opt_empty';
                }

                return html`<div class=${cls}>
                    <div class="sc_head">
                        <div class="sc_head_week_day">${day.week_day}</div>
                        <div class="sc_head_date">${day.date}</div>
                        <div class="sc_head_count">${(normal_count + overbook_count) ? `${normal_count}+${overbook_count}` : 'Fermé'}</div>
                    </div>

                    <a href="#" style=${copy_ignore.has(day.key) ? 'opacity: 0.1' : ''}
                       @click=${e => { executeCopy(day); e.preventDefault(); }}>+</a>
                    <a href="#" style=${copy_ignore.has(day.key) ? 'opacity: 0.1' : ''}
                       @click=${e => { executeCopyAndEnd(day); e.preventDefault(); }}>⇳</a>
                </div>`;
            } else {
                return html`<div class="sc_skip"></div>`;
            }
        }), days_el);
    }

    function switchToPreviousMonth() {
        if (--current_month < 1) {
            current_year--;
            current_month = 12;
        }

        renderAll();
    }

    function switchToNextMonth() {
        if (++current_month > 12) {
            current_year++;
            current_month = 1;
        }

        renderAll();
    }

    function switchToMonth(month) {
        if (month !== current_month) {
            current_month = month;
            renderAll();
        }
    }

    function switchToPreviousYear() {
        current_year--;
        renderAll();
    }

    function switchToNextYear() {
        current_year++;
        renderAll();
    }

    function renderFooter() {
        render(html`
            <div class="sc_selector">
                <button @click=${switchToPreviousMonth}
                        @dragover=${slowDownEvents(300, switchToPreviousMonth)}>≪</button>
                <b>${month_names[current_month - 1]}</b>
                <button @click=${switchToNextMonth}
                        @dragover=${slowDownEvents(300, switchToNextMonth)}>≫</button>
            </div>

            ${month_names.map((name, idx) => {
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

                return html`<button class=${cls}
                                    @click=${e => switchToMonth(month)}
                                    @dragover=${e => switchToMonth(month)}>${name}</button>`;
            })}

            <div class="sc_selector">
                <button @click=${switchToPreviousYear}
                        @dragover=${slowDownEvents(300, switchToPreviousYear)}>≪</button>
                <b>${current_year}</b>
                <button @click=${switchToNextYear}
                        @dragover=${slowDownEvents(300, switchToNextYear)}>≫</button>
            </div>
        `, footer_el);
    }

    function renderAll() {
        switch (current_mode) {
            case 'meetings': { renderMeetings(); } break;
            case 'settings': { renderSettings(); } break;
            case 'copy': { renderCopy(); } break;
        }
        renderFooter();
    }

    this.render = function(year, month, mode, root_el) {
        current_year = year;
        current_month = month;
        current_mode = mode;

        render(html`
            <div id="sc_header">${week_day_names.map(name => html`<div>${name}</div>`)}</div>
            <div id="sc_days"></div>
            <div id="sc_footer" class="gp_toolbar"></div>
        `, root_el);

        days_el = root_el.querySelector('#sc_days');
        footer_el = root_el.querySelector('#sc_footer');

        renderAll();
    };
}
