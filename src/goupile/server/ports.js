// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Stub lithtml functions
var render = () => {};
var html = () => {};
var svg = () => {};

let profile = {
    username: null,
    zone: null
};

// Stub exposed goupile methods
var goupile = new function() {
    let self = this;

    this.isConnected = function() { return true; };
    this.isTablet = function() { return false; };
    this.isStandalone = function() { return false; };
    this.isLocked = function() { return false; };
};
var user = new function() {
    let self = this;

    this.isConnected = function() { return true; };
    this.isConnectedOnline = function() { return true; }
    this.getUserName = function() { return profile.username; };
    this.getZone = function() { return profile.zone; };
};
var nav = new function() {
    let self = this;

    this.isConnected = function() { return goupile.isConnected(); };
    this.isTablet = function() { return goupile.isTablet(); };
    this.isStandalone = function() { return goupile.isStandalone(); };
    this.isLocked = function() { return goupile.isLocked(); };
};

var server = new function() {
    let self = this;

    let cache = new LruMap(4);

    // C++ functions
    this.readFile = null;

    this.changeProfile = function(username, zone) {
        profile.username = username;
        profile.zone = zone;

        cache.clear();
    };

    this.validateFragments = function(table, json, fragments) {
        let values = JSON.parse(json);
        let values2 = JSON.parse(json);

        let fragments2 = fragments.map(frag => {
            if (frag.page != null) {
                values = Object.assign(values, frag.values);

                // We don't care about PageState (single execution)
                let model = new PageModel;
                let builder = new PageBuilder(new PageState, model);
                builder.getValue = (key, default_value) => getValue(values, key, default_value);

                // Execute user script
                // XXX: We should fail when data types don't match (string given to number widget)
                let code = readPage(frag.page);
                let func = Function('shared', 'route', 'go', 'form', 'page', 'scratch', 'values', code);
                func({}, {}, () => {}, builder, builder, {}, values);

                let frag2_values = gatherValues(model.variables);
                let frag2 = {
                    mtime: frag.mtime,
                    version: frag.version,
                    page: frag.page,
                    complete: frag.complete,

                    // Make it easy for the C++ caller to store in database with stringified JSON
                    columns: expandColumns(table, frag.page, model.variables),
                    json: JSON.stringify(frag2_values)
                };

                values2 = Object.assign(values2, frag2_values);
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
            json: JSON.stringify(values2)
        };
        return ret;
    };

    function readPage(page) {
        let path = `/files/pages/${page}.js`;
        let code = cache.get(path);

        if (!code) {
            code = self.readFile(path);
            cache.set(path, code);
        }

        return code;
    }

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

    function gatherValues(variables) {
        let values = util.arrayToObject(variables, variable => variable.key.toString(),
                                        variable => !variable.missing ? variable.value : null);
        return values;
    }
};
