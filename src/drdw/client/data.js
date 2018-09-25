// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

var data = {};
(function() {
    'use strict';

    this.idleHandler = null;

    let self = this;
    let errors = [];
    let queue = new Set();
    let busy = 0;

    function get(url, proceed, fail)
    {
        if (queue.has(url))
            return;
        queue.add(url);
        busy++;

        function handleFinishedRequest(status, response)
        {
            busy--;
            callRequestHandlers(url, proceed, fail, status, response);
            callIdleHandler();
        }

        var xhr = new XMLHttpRequest();
        xhr.responseType = 'json';
        xhr.timeout = 14000;
        xhr.onload = function(e) { handleFinishedRequest(this.status, xhr.response); };
        xhr.onerror = function(e) { handleFinishedRequest(503); };
        xhr.ontimeout = function(e) { handleFinishedRequest(504); };
        xhr.open('GET', url, true);

        xhr.send();
    }
    this.get = get;

    function post(url, parameters, proceed, fail)
    {
        if (queue.has(url))
            return;
        queue.add(url);
        busy++;

        function handleFinishedRequest(status, response)
        {
            busy--;
            callRequestHandlers(url, proceed, fail, status, response);
            callIdleHandler();
        }

        var xhr = new XMLHttpRequest();
        xhr.responseType = 'json';
        xhr.timeout = 14000;
        xhr.onload = function(e) { handleFinishedRequest(this.status, xhr.response); };
        xhr.onerror = function(e) { handleFinishedRequest(503); };
        xhr.ontimeout = function(e) { handleFinishedRequest(504); };
        xhr.open('POST', url, true);

        let encoded_parameters = buildUrl('', parameters).substr(1);
        xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
        xhr.send(encoded_parameters || null);
    }
    this.post = post;

    function callRequestHandlers(url, proceed, fail, status, response)
    {
        let error = null;
        switch (status) {
            case 200: { /* Success */ } break;
            case 404: { error = 'Adresse \'' + url + '\' introuvable'; } break;
            case 422: { error = 'Paramètres incorrects'; } break;
            case 502:
            case 503: { error = 'Service non accessible'; } break;
            case 504: { error = 'Délai d\'attente dépassé, réessayez'; } break;
            default: { error = 'Erreur inconnue ' + status; } break;
        }

        if (!error) {
            proceed(response);
        } else {
            errors.push(error);
            if (fail)
                fail(error);
        }
    }

    function callIdleHandler()
    {
        if (!busy) {
            if (self.idleHandler) {
                setTimeout(function() {
                    self.idleHandler();
                    if (!busy)
                        queue.clear();
                }, 0);
            } else {
                queue.clear();
            }
        }
    }

    this.isBusy = function() { return !!busy; }
    this.getErrors = function() { return errors; }
    this.clearErrors = function() { errors = []; }
}).call(data);
