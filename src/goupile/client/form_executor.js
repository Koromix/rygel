// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormExecutor(form, record) {
    let self = this;

    let page_state;
    let page_scratch;
    let page_key;

    this.runPage = async function(info, el) {
        let path = `pages/${info.key}.js`;
        let file = await virt_fs.load(path);

        self.runPageScript(page, await file.data.text(), el);
    };

    this.runPageScript = function(info, script, el) {
        if (info.key !== page_key) {
            page_state = new PageState;
            page_scratch = {};
            page_key = info.key;
        }

        let page = new Page;

        // Make page builder
        let page_builder = new PageBuilder(page_state, page);
        page_builder.decodeKey = decodeKey;
        page_builder.setValue = setValue;
        page_builder.getValue = getValue;
        page_builder.changeHandler = () => {
            self.runPageScript(info, script, el);
            window.history.replaceState(null, null, app.makeURL());
        };
        page_builder.submitHandler = saveRecordAndReset;

        // Execute user script
        let func = Function('data', 'route', 'go', 'form', 'page', 'scratch', script);
        func(app.data, app.route, app.go, page_builder, page_builder, page_scratch);

        page.render(el);
    };

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
        if (!record.values.hasOwnProperty(key)) {
            record.values[key] = default_value;
            return default_value;
        }

        return record.values[key];
    }

    async function saveRecordAndReset(page) {
        let entry = new log.Entry();
        entry.progress('Enregistrement en cours');

        await virt_data.save(record, page.variables);
        entry.success('Données enregistrées !');

        goupile.run(null, {id: null});
        // XXX: Give focus to first widget
        window.scrollTo(0, 0);
    }
}
