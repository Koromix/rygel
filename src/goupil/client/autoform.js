// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function PageBuilder(root, widgets) {
    let self = this;

    this.changeHandler = e => {};

    let interfaces = {};
    let widgets_ref = widgets;

    function makeID(name) { return `af_var_${name}`; }

    function addWidget(render) {
        let widget = {
            render: render,
            errors: []
        };

        widgets_ref.push(widget);

        return widget;
    }

    function wrapWidget(frag, options, errors = []) {
        return html`
            <div class=${errors.length ? 'af_widget af_widget_error' : 'af_widget'}>
                ${frag}
                ${errors.length ? html`<span class="af_error">${errors.map(err => html`${err}<br/>`)}</span>` : html``}
                ${options.help ? html`<p class="af_help">${options.help}</p>` : ''}
            </div>
        `;
    }

    function addVariableWidget(name, id, render, value) {
        if (interfaces[name])
            throw new Error(`Variable '${name}' already exists`);

        let widget = addWidget(render);

        let intf = {
            value: value,
            error: msg => {
                widget.errors.push(msg || '');
                return intf;
            }
        };
        interfaces[name] = intf;

        return intf;
    }

    this.find = name => interfaces[name];
    this.value = function(name) {
        let intf = interfaces[name];
        return intf ? intf.value : undefined;
    };
    this.error = (name, msg) => interfaces[name].error(msg);

    this.integer = function(name, label, options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value = (prev && prev.value != '') ? parseInt(prev.value, 10) : undefined;

        let render = errors => wrapWidget(html`
            <label for=${id}>${label}</label>
            <input id=${id} type="number" value=${value}
                   @input=${e => self.changeHandler(e)}/>
        `, options, errors);

        let intf = addVariableWidget(name, id, render, value);

        if (value != null) {
            if (options.min !== undefined && value < options.min)
                intf.error(`Valeur < ${options.min}`);
            if (options.max !== undefined && value > options.max)
                intf.error(`Valeur > ${options.max}`);
        }

        return intf;
    };

    function parseValue(str) { return (str && str !== 'undefined') ? JSON.parse(str) : undefined; }
    function stringifyValue(value) { return JSON.stringify(value); }

    this.dropdown = function(name, label, choices = [], options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value = prev ? parseValue(prev.value) : undefined;

        let render = errors => wrapWidget(html`
            <label for=${id}>${label}</label>
            <select id=${id} @change=${e => self.changeHandler(e)}>
                <option value="null" ?selected=${value == null}>-- Choisissez une option --</option>
                ${choices.map(c => html`<option value=${stringifyValue(c[0])} ?selected=${value === c[0]}>${c[1]}</option>`)}
            </select>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    function changeSelect(e, id, value) {
        let json = stringifyValue(value);

        let els = document.querySelectorAll(`#${id} button`);
        for (let el of els)
            el.classList.toggle('active', el.dataset.value == json && !el.classList.contains('active'));

        self.changeHandler(e);
    }

    this.select = function(name, label, choices = [], options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value;
        if (prev) {
            let els = prev.querySelectorAll('button');
            for (let el of els) {
                if (el.classList.contains('active')) {
                    value = parseValue(el.dataset.value);
                    break;
                }
            }
        }

        let render = errors => wrapWidget(html`
            <label for=${id}>${label}</label>
            <div class="af_select" id=${id}>
                ${choices.map(c => html`<button data-value=${stringifyValue(c[0])} class=${value == c[0] ? 'active' : ''}
                                                @click=${e => changeSelect(e, id, c[0])}>${c[1]}</button>`)}
            </div>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    this.boolean = function(name, label, options = {}) {
        return self.select(name, label, [[true, 'Oui'], [false, 'Non']], options);
    };

    this.calc = function(name, label, value, options = {}) {
        let id = makeID(name);

        let text = value;
        if (!options.raw && typeof value !== 'string') {
            if (isNaN(value) || value == null) {
                text = '';
            } else if (isFinite(value)) {
                // This is a garbage way to round numbers
                let multiplicator = Math.pow(10, 2);
                let n = parseFloat((value * multiplicator).toFixed(11));
                text = Math.round(n) / multiplicator;
            }
        }

        let render = errors => wrapWidget(html`
            <label for=${id}>${label}</label>
            <span>${text}</span>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    this.output = function(content, options = {}) {
        // Don't output function content, helps avoid garbage output when the
        // user types 'page.oupt(html);'.
        if (!content || typeof content === 'function')
            return;

        let render = () => wrapWidget(content, options);
        addWidget(render);
    };

    this.section = function(label, func = () => {}) {
        let widgets = [];
        let prev_widgets = widgets_ref;

        widgets_ref = widgets;
        func();
        widgets_ref = prev_widgets;

        let render = () => html`
            <fieldset class="af_section">
                <legend>${label}</legend>
                ${widgets.map(w => w.render(w.errors))}
            </fieldset>
        `;

        addWidget(render);
    };
}

function AutoForm(widget) {
    let self = this;

    let form;
    let log;

    self.render = function(script) {
        let widgets = [];

        let page = new PageBuilder(form, widgets);
        page.changeHandler = () => self.render(script);

        try {
            Function('page', script)(page);
        } catch (err) {
            let line;
            if (err.stack) {
                let m = err.stack.match(/ > Function:([0-9]+):[0-9]+/) ||
                        err.stack.match(/, <anonymous>:([0-9]+):[0-9]+/);

                // Can someone explain to me why do I have to offset by -2?
                line = (m && m.length >= 2) ? (parseInt(m[1], 10) - 2) : null;
            }

            log.textContent = `⚠ Line ${line || '?'}: ${err.message}`;
            return false;
        }

        render(html`${widgets.map(w => w.render(w.errors))}`, form);
        log.innerHTML = '';

        return true;
    };

    render(html`
        <div class="af_form"></div>
        <div class="af_log"></div>
    `, widget);
    form = widget.querySelector('.af_form');
    log = widget.querySelector('.af_log');

    widget.object = this;
}
