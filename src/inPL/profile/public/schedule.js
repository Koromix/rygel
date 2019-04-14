// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function Schedule(widget, resources_map, meetings_map) {
    let self = this;

    let month_names = ['Janvier', 'Février', 'Mars', 'Avril', 'Mai', 'Juin', 'Juillet',
                       'Août', 'Septembre', 'Octobre', 'Novembre', 'Décembre'];
    let week_day_names = ['Lundi', 'Mardi', 'Mercredi', 'Jeudi', 'Vendredi', 'Samedi', 'Dimanche'];

    let widget_months;
    let widget_days;

    let current_month;
    let current_year;
    let drag_slot_ref;

    function formatTime(time) {
        let hour = Math.floor(time / 100);
        let min = time % 100;

        return `${hour}h${min}`;
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

    function getMonthDays(year, month) {
        let days_count = daysInMonth(year, month);
        let start_week_day = getWeekDay(year, month, 1);

        let days = [];
        for (let i = 0; i < start_week_day; i++)
            days.push(null);
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

    function createMeeting(slot_ref) {
        let name = prompt('Name?');
        if (name !== null) {
            slot_ref.meetings.splice(slot_ref.splice_idx, 0, {
                time: slot_ref.time,
                identity: name
            });

            renderDays(current_year, current_month);
        }
    }

    function deleteMeeting(slot_ref) {
        if (confirm('Are you sure?')) {
            slot_ref.meetings.splice(slot_ref.splice_idx, 1);
            renderDays(current_year, current_month);
        }
    }

    function moveSlot(src_ref, dest_ref)
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

        renderDays(current_year, current_month);
    }

    function renderDays() {
        let days = getMonthDays(current_year, current_month);

        render(widget.childNodes[1], () => html`<div class="sc_days">${days.map(day => {
            if (day !== null) {
                if (!settings.resources[day.key])
                    settings.resources[day.key] = [];
                if (!meetings_map[day.key])
                    meetings_map[day.key] = [];

                let resources = settings.resources[day.key];
                let meetings = meetings_map[day.key];

                // Create slots
                let slots = [];
                for (let res of resources) {
                    for (let j = 0; j < res.doctors.length * settings.slots_per_doctor; j++) {
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
                    cls = 'sc_day sc_day_full';
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
                        <table class="sc_slots">
                            ${slot_refs.map(slot_ref => {
                                function dragStart(e) {
                                    if (slot_ref.identity) {
                                        drag_slot_ref = slot_ref;
                                        e.dataTransfer.setData('application/x-slot', '');
                                    }
                                }
                                function dragOverSlot(e) {
                                    if (e.dataTransfer.types.includes('application/x-slot'))
                                        e.preventDefault();
                                }
                                function dropSlot(e) {
                                    moveSlot(drag_slot_ref, slot_ref);
                                    e.preventDefault();
                                }

                                return html`<tr class=${slot_ref.cls} ondragover=${dragOverSlot} ondrop=${dropSlot}>
                                    <td class="sc_slot_time" draggable="true" ondragstart=${dragStart}>${formatTime(slot_ref.time)}</td>
                                    <td class="sc_slot_identity">
                                        ${slot_ref.identity ?
                                            html`
                                                ${slot_ref.identity}
                                                <a class="sc_slot_edit" href="#"
                                                   onclick=${e => { deleteMeeting(slot_ref); e.preventDefault(); }}>x</a>
                                            ` :
                                            html`
                                                <a class="sc_slot_edit" href="#"
                                                   onclick=${e => { createMeeting(slot_ref); e.preventDefault(); }}>+</a>
                                            `
                                        }
                                    </td>
                                </tr>`;
                            })}
                        </table>
                    </div>
                `;
            } else {
                return html`<div class="sc_skip"></div>`;
            }
        })}</div>`);
    }

    function renderMonths() {
        render(widget.childNodes[2], () => html`<nav class="sc_footer">
            <div class="sc_selector">
                <a href="#" onclick="${e => { current_month > 1 ? schedule.render(current_year, current_month - 1)
                                                                : schedule.render(current_year - 1, 12); e.preventDefault(); }}">≪</a>
                <b>${month_names[current_month - 1]}</b>
                <a href="#" onclick="${e => { current_month < 12 ? schedule.render(current_year, current_month + 1)
                                                                 : schedule.render(current_year + 1, 1); e.preventDefault(); }}">≫</a>
            </div>

            <div class="sc_months">${month_names.map((name, idx) => {
                return html`<a class=${idx + 1 == current_month ? 'sc_month active' : 'sc_month'}
                               href="#" onclick=${e => { schedule.render(current_year, idx + 1); e.preventDefault(); }}>${name}</a>`;
            })}</div>

            <div class="sc_selector">
                <a href="#" onclick="${e => { schedule.render(current_year - 1, current_month); e.preventDefault(); }}">≪</a>
                <b>${current_year}</b>
                <a href="#" onclick="${e => { schedule.render(current_year + 1, current_month); e.preventDefault(); }}">≫</a>
            </div>

            <a class="sc_deploy" href="#" onclick=${e => { e.target.parentNode.querySelector('.sc_months').classList.toggle('active'); e.preventDefault(); }}></a>
        </nav>`);
    }

    this.render = function(year, month) {
        current_year = year;
        current_month = month;

        renderDays();
        renderMonths();
    };

    // FIXME: Can we replace a node with render, instead of replacing its content?
    // Right now, renderDays() and renderMonths() create content inside these two divs
    // instead of replacing them.
    render(widget, () => html`
        <div class="sc_header">${week_day_names.map(name => html`<div>${name}</div>`)}</div>
        <div></div>
        <div></div>
    `);

    widget.object = this;
}
