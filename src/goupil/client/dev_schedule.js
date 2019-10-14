// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev_schedule = new function() {
    let self = this;

    let current_mode = 'meetings';

    let modes_el;
    let view_el;

    let init = false;
    let schedule;

    this.run = async function(asset = null, args = {}) {
        if (!init) {
            goupil.listenToServerEvent('schedule', e => {
                schedule = null;
                self.run();
            });

            init = true;
        }

        // This is test code, won't stay that way (obviously)
        let today = dates.today();

        if (!schedule) {
            let [resources, meetings] = await loadSchedule(today.year, today.month);
            schedule = new Schedule(resources, meetings);
        }

        // Find panels
        modes_el = document.querySelector('#dev_modes');
        view_el = document.querySelector('main');

        renderModes();
        schedule.render(today.year, today.month, current_mode, view_el);
    };

    async function loadSchedule(year, month) {
        let [resources, meetings] = await Promise.all([
            fetch(`${env.base_url}api/schedule/resources?schedule=pl&year=${year}&month=${month}`).then(response => response.json()),
            fetch(`${env.base_url}api/schedule/meetings?schedule=pl&year=${year}&month=${month}`).then(response => response.json())
        ]);

        return [resources, meetings];
    }

    function renderModes() {
        render(html`
            <button class=${current_mode === 'meetings' ? 'active' : ''}
                    @click=${toggleMode}>Agenda</button>
            <button class=${current_mode === 'meetings' ? '' : 'active'}
                    @click=${toggleMode}>Créneaux</button>
        `, modes_el);
    }

    function toggleMode() {
        switch (current_mode) {
            case 'meetings': { current_mode = 'settings'; } break;
            case 'settings': { current_mode = 'meetings'; } break;
            case 'copy': { current_mode = 'settings'; } break;
        }

        self.run();
    }
};
