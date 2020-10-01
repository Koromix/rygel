// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Stub lithtml functions
var render = () => {};
var html = () => {};
var svg = () => {};

// Stub exposed goupile methods
var goupile = new function() {
    let self = this;

    this.isConnected = function() { return false; };
    this.isTablet = function() { return false; };
    this.isStandalone = function() { return false; };
    this.isLocked = function() { return false; };
};

var server = new function() {
    let self = this;

    // C++ functions
    this.readCode = null;

    this.validateFragments = function(table, json, fragments) {
        let values = JSON.parse(json);

        let fragments2 = fragments.map(frag => {
            if (frag.page != null) {
                values = Object.assign(values, frag.values);

                // We don't care about PageState (single execution)
                let model = new PageModel;
                let builder = new PageBuilder(new PageState, model);
                builder.getValue = (key, default_value) => getValue(values, key, default_value);

                // Execute user script
                // XXX: We should fail when data types don't match (string given to number widget)
                let code = self.readCode(frag.page);
                let func = Function('shared', 'route', 'go', 'form', 'page', 'scratch', code);
                func({}, {}, () => {}, builder, builder, {});

                let frag2 = {
                    mtime: frag.mtime,
                    version: frag.version,
                    page: frag.page,

                    // Make it easy for the C++ caller to store in database with stringified JSON
                    columns: expandColumns(table, frag.page, model.variables),
                    json: JSON.stringify(filterValues(values, model.variables))
                };

                return frag2;
            } else {
                let frag2 = {
                    mtime: frag.mtime,
                    version: frag.version
                };

                return frag2;
            }
        });

        let ret = {
            fragments: fragments2,
            json: JSON.stringify(values)
        };
        return ret;
    };

    function getValue(values, key, default_value) {
        if (!values.hasOwnProperty(key)) {
            values[key] = default_value;
            return default_value;
        }

        return values[key];
    }

    function expandColumns(table, page, variables) {
        let columns = variables.flatMap((variable, idx) => {
            let key = variable.key.toString();

            if (variable.multi) {
                let ret = variable.props.map(prop => ({
                    key: makeColumnKeyMulti(table, page, key, prop.value),
                    variable: key,
                    type: variable.type,
                    prop: JSON.stringify(prop.value)
                }));

                return ret;
            } else {
                let ret = {
                    key: makeColumnKey(table, page, key),
                    variable: key,
                    type: variable.type
                };

                return ret;
            }
        });

        return columns;
    }

    // Duplicated in client/virt_rec.js, keep in sync
    function makeColumnKey(table, page, variable) {
        return `${table}@${page}.${variable}`;
    }
    function makeColumnKeyMulti(table, page, variable, prop) {
        return `${table}@${page}.${variable}@${prop}`;
    }

    function filterValues(values, variables) {
        let values2 = {};

        let set = new Set;
        for (let variable of variables) {
            let key = variable.key.toString();

            set.add(key);
            values2[key] = values[key];
        }

        for (let key in values) {
            if (!set.has(key))
                throw new Error(`Unexpected variable '${key}'`);
        }

        return values2;
    }
};
