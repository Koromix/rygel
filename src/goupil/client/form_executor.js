// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormExecutor(values = {}) {
    let self = this;

    // Storage for user scripts
    this.memory = {};

    // Page state
    this.missing_errors = new Set;
    this.changed_variables = new Set;
    this.sections_state = {};
    this.file_lists = new Map;
    this.pressed_buttons = new Set;
    this.clicked_buttons = new Set;

    // Key handling
    this.decodeKey = function(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');

        key = {
            variable: key,
            toString: () => key.variable
        };

        return key;
    };

    // Value handling
    this.setValue = function(key, value) {
        values[key] = value;

        self.missing_errors.delete(key.toString());
        self.changed_variables.add(key.toString());
    };
    this.getValue = function(key, default_value) {
        if (values.hasOwnProperty(key)) {
            return values[key];
        } else {
            return default_value;
        }
    };
}
