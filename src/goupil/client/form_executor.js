// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormExecutor() {
    let self = this;

    this.goHandler = (key, args) => {};
    this.submitHandler = (values, variables) => {};

    let af_page;
    let af_log;

    let variables = [];
    let state = new FormState();

    this.setData = function(values) { state = new FormState(values); };
    this.getData = function() { return state.values; };

    this.setError = function(line, msg) {
        af_page.classList.add('af_page_broken');

        af_log.textContent = `⚠\uFE0E Line ${line || '?'}: ${msg}`;
        af_log.style.display = 'block';
    };

    this.clearError = function() {
        af_page.classList.remove('af_page_broken');

        af_log.innerHTML = '';
        af_log.style.display = 'none';
    };

    this.render = function(root_el, page_key, script) {
        render(html`
            <div class="af_page"></div>
            <div class="af_log" style="display: none;"></div>
        `, root_el);
        af_page = root_el.querySelector('.af_page');
        af_log = root_el.querySelector('.af_log');

        if (script !== undefined) {
            return renderForm(page_key, script);
        } else {
            return true;
        }
    };

    function renderForm(page_key, script) {
        let widgets = [];
        variables.length = 0;

        let builder = new FormBuilder(state, widgets, variables);
        builder.changeHandler = () => renderForm(page_key, script);
        builder.submitHandler = submitForm;

        // Prevent go() call from working if called during script eval
        let prev_go_handler = self.goHandler;
        let prev_submit_handler = self.submitHandler;
        self.goHandler = () => {
            throw new Error(`Navigation functions (go, page.submit, etc.) must be called from a callback (button click, etc.).

If you are using it for events, make sure you did not use this syntax by accident:
    go('page_key')
instead of:
    () => go('page_key')`);
        };
        self.submitHandler = self.goHandler;

        try {
            let func = Function('page', 'form', 'go', script);
            func(builder, builder, (key, args = {}) => self.goHandler(key, args));

            render(widgets.map(intf => intf.render(intf)), af_page);
            self.clearError();

            return true;
        } catch (err) {
            let line;
            if (err instanceof SyntaxError) {
                // At least Firefox seems to do well in this case, it's better than nothing
                line = err.lineNumber - 2;
            } else if (err.stack) {
                line = parseAnonymousErrorLine(err);
            }

            self.setError(line, err.message);

            return false;
        } finally {
            self.goHandler = prev_go_handler;
            self.submitHandler = prev_submit_handler;
        }
    }

    function parseAnonymousErrorLine(err) {
        if (err.stack) {
            let m;
            if (m = err.stack.match(/ > Function:([0-9]+):[0-9]+/) ||
                    err.stack.match(/, <anonymous>:([0-9]+):[0-9]+/)) {
                // Can someone explain to me why do I have to offset by -2?
                let line = parseInt(m[1], 10) - 2;
                return line;
            } else if (m = err.stack.match(/Function code:([0-9]+):[0-9]+/)) {
                let line = parseInt(m[1], 10);
                return line;
            }
        }

        return null;
    }

    function submitForm() {
        let variables2 = variables.map(variable => ({
            key: variable.key,
            type: variable.type
        }));

        self.submitHandler(state.values, variables2);
    }
}
