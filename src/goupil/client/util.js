// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const {render, html, svg} = lithtml;

let util = (function() {
    this.pluralEN = function(value, single_text, plural_text) {
        if (value === 1) {
            return single_text;
        } else {
            return plural_text;
        }
    };
    this.pluralFR = function(value, single_text, plural_text) {
        if (value < 2) {
            return single_text;
        } else {
            return plural_text;
        }
    };

    this.compareStrings = function(str1, str2) {
        if (str1 > str2) {
            return 1;
        } else if (str1 < str2) {
            return -1;
        } else {
            return 0;
        }
    };

    return this;
}).call({});

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
