// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev_schedule = new function() {
    let self = this;

    let init = false;
    let view;

    this.run = async function(schedule) {
        if (!init) {
            goupil.listenToServerEvent('schedule', e => {
                schedule = null;
                self.run();
            });

            init = true;
        }

        // This is test code, won't stay that way (obviously)
        let today = dates.today();

        if (!view) {
            let [resources, meetings] = await loadSchedule(today.year, today.month);
            view = new ScheduleView(resources, meetings);
        }

        // Find panels
        let view_el = document.querySelector('main');
        view.render(today.year, today.month, view_el);
    };

    async function loadSchedule(year, month) {
        let [resources, meetings] = await Promise.all([
            fetch(`${env.base_url}api/schedule/resources.json?schedule=pl&year=${year}&month=${month}`).then(response => response.json()),
            fetch(`${env.base_url}api/schedule/meetings.json?schedule=pl&year=${year}&month=${month}`).then(response => response.json())
        ]);

        return [resources, meetings];
    }
};
