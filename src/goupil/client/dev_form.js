// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev_form = new function() {
    let self = this;

    let current_record;
    let form_data;

    this.runPageScript = async function(script, record) {
        if (record !== current_record) {
            form_data = new FormData(record.values);
            current_record = record;
        }

        // Render
        runScript(script);
    };

    function runScript(script) {
        let widgets = [];

        let page_builder = new PageBuilder(form_data, widgets);
        page_builder.decodeKey = decodeFormKey;
        page_builder.changeHandler = () => runScript(script);
        page_builder.submitHandler = saveRecordAndReset;

        // Execute user script
        let func = Function('page', 'form', 'memory', script);
        func(page_builder, page_builder, form_data.memory);

        // Render widgets (even if overview is disabled)
        let page_el = document.querySelector('#dev_overview') || document.createElement('div');
        render(widgets.map(intf => intf.render(intf)), page_el);
    }

    function decodeFormKey(key) {
        let record_ids = ['foo'];
        let table_names = ['bar'];

        if (typeof key === 'string') {
            let split_idx = key.indexOf('.');

            if (split_idx >= 0) {
                key = {
                    record_id: null,
                    table: key.substr(0, key_idx),
                    variable: key.substr(key_idx + 1),

                    toString: null
                }
            } else {
                key = {
                    record_id: null,
                    table: null,
                    variable: key,

                    toString: null
                };
            }
        }

        if (!key.record_id) {
            if (record_ids.length > 1)
                throw new Error('Ambiguous key (multiple record IDs are available)');

            key.record_id = record_ids[0];
        }
        if (!key.table)
            key.table = table_names[0];

        if (!key.variable)
            throw new Error('Empty keys are not allowed');
        if (!key.table.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/) ||
                !key.variable.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');

        let str = `${key.record_id}+${key.table}.${key.variable}`;
        key.toString = () => str;

        return Object.freeze(key);
    }

    async function saveRecordAndReset(values, variables) {
        let entry = new log.Entry();
        entry.progress('Enregistrement en cours');

        await record_manager.save(current_record, variables);
        entry.success('Données enregistrées !');

        dev.go(null, {id: null});
        // TODO: Give focus to first widget
        window.scrollTo(0, 0);
    }
};
