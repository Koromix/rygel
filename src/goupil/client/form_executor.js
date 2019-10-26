// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormExecutor(values = {}) {
    // Storage for user scripts
    this.memory = {};

    // Page state
    this.missing_errors = new Set;
    this.sections_state = {};
    this.file_lists = new Map;

    // Key and value handling
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
    this.setValue = function(key, value) { values[key] = value; };
    this.getValue = function(key, default_value) {
        if (values.hasOwnProperty(key)) {
            return values[key];
        } else {
            return default_value;
        }
    };
}
