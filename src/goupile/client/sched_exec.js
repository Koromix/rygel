// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let sched_exec = new function() {
    let self = this;

    let view;

    this.runMeetings = async function(schedule, view_el) {
        // This is test code, won't stay that way (obviously)
        let today = dates.today();

        if (!view) {
            let [resources, meetings] = await loadSchedule(today.year, today.month);
            view = new ScheduleView(resources, meetings);
        }

        // Find panels
        view.renderMeetings(today.year, today.month, view_el);
    };

    this.runSettings = async function(schedule, view_el) {
        // This is test code, won't stay that way (obviously)
        let today = dates.today();

        if (!view) {
            let [resources, meetings] = await loadSchedule(today.year, today.month);
            view = new ScheduleView(resources, meetings);
        }

        // Find panels
        view.renderSettings(today.year, today.month, view_el);
    };

    async function loadSchedule(year, month) {
        let [resources, meetings] = await Promise.all([
            net.fetch(`${env.base_url}api/schedule/resources.json?schedule=pl&year=${year}&month=${month}`).then(response => response.json()),
            net.fetch(`${env.base_url}api/schedule/meetings.json?schedule=pl&year=${year}&month=${month}`).then(response => response.json())
        ]);

        return [resources, meetings];
    }
};
