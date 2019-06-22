// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let schedule = (function() {
    let self = this;

    let init = false;
    let schedule;

    // FIXME: Replace with better libweb multi-fetch thingy
    function FetchQueue(func) {
        let self = this;

        let missing = new Set;
        let data = {};
        let errors = [];

        let controller;
        let timer_id;

        function clearFetch(key) {
            missing.delete(key);

            if (!missing.size) {
                clearTimeout(timer_id);
                func(data, errors);
            }
        }

        function handleResponse(key, response) {
            if (missing.has(key)) {
                if (response.status == 200) {
                    response.json().then(json => {
                        data[key] = json;
                        clearFetch(key);
                    });
                } else {
                    errors.push(response.statusText);
                    clearFetch(key);
                }
            }
        }

        function handleError(key, err) {
            if (missing.has(key)) {
                errors.push(err.message);
                clearFetch(key);
            }
        }

        function handleTimeout() {
            if (missing.size) {
                controller.abort();

                errors.push(`${missing.size} network ${missing.size > 1 ? 'requests have' : 'request has'} timed out`);
                missing.clear();

                func(data, errors);
            }
        }

        this.fetch = function(key, url, options = {}) {
            if (!controller) {
                controller = new AbortController();
                timer_id = setTimeout(handleTimeout, 10000);
            }

            options = Object.assign({}, options);
            options.signal = controller.signal;

            missing.add(key);
            fetch(url, options).then(response => handleResponse(key, response))
                               .catch(error => handleError(key, error));
        }
    }

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
        document.title = `${settings.project_key} — goupil schedule`;

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
