// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function Page() {
    let self = this;

    this.widgets = [];
    this.variables = [];

    this.render = function(el) {
        render(self.widgets.map(intf => intf.render(intf)), el);
    };
}

function PageState() {
    this.values = {};

    this.sections_state = {};
    this.tabs_state = {};
    this.file_lists = new Map;
    this.pressed_buttons = new Set;
    this.clicked_buttons = new Set;

    this.missing_errors = new Set;
    this.changed_variables = new Set;
}
