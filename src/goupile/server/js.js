// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Stub lithtml functions
var render = () => {};
var html = () => {};
var svg = () => {};

var server = new function() {
    let self = this;

    let cache = new LruMap(4);

    // C++ functions
    this.readFile = null;

    this.changeProfile = function(username, zone) {
        throw new Error('Not implemented');
    };

    this.validateFragments = function(table, json, fragments) {
        throw new Error('Not implemented');
    };

    this.recompute = function(table, json, page) {
        throw new Error('Not implemented');
    };
};
