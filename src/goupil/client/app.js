// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function Application() {
    let self = this;

    this.forms = [];
    this.schedules = [];

    // Used for user globals
    this.data = {};

    this.go = (url = null, push_history = true) => {};
}

function FormInfo(key) {
    this.key = key;
    this.pages = [];
    this.links = [];
}
