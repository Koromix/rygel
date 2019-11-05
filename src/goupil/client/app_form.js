// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function ApplicationForm(record) {
    let self = this;

    let page_state;
    let page_scratch;
    let page_key;

    this.runPage = async function(key, el) {
        let path = `pages/${key}.js`;
        let script = await file_manager.load(path);

        self.runPageScript(key, script, el);
    }

    this.runPageScript = function(key, script, el) {
        if (key !== page_key) {
            page_state = new PageState;
            page_scratch = {};
            page_key = key;
        }

        let page = new Page;

        // Make page builder
        let page_builder = new PageBuilder(page_state, page);
        page_builder.decodeKey = decodeKey;
        page_builder.setValue = setValue;
        page_builder.getValue = getValue;
        page_builder.changeHandler = () => self.runPageScript(key, script, el);
        page_builder.submitHandler = saveRecordAndReset;

        // Execute user script
        let func = Function('data', 'route', 'go', 'form', 'page', 'scratch', script);
        func(app.data, app.route, app.go, page_builder, page_builder, page_scratch);

        page.render(el);
    }

    function decodeKey(key) {
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

    function setValue(key, value) {
        record.values[key] = value;
    }

    function getValue(key, default_value) {
        return record.values.hasOwnProperty(key) ? record.values[key] : default_value;
    }

    async function saveRecordAndReset(page) {
        let entry = new log.Entry();
        entry.progress('Enregistrement en cours');

        await record_manager.save(record, page.variables);
        entry.success('Données enregistrées !');

        dev.run(null, {id: null});
        // TODO: Give focus to first widget
        window.scrollTo(0, 0);
    }
}
