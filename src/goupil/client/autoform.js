// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function PageBuilder(root, widgets) {
    let self = this;

    this.changeHandler = e => {};

    let interfaces = {};

    function makeID(name) {
        return `af_var_${name}`;
    }

    function addWidget(name, id, func, value) {
        if (interfaces[name])
            throw new Error(`Variable '${name}' already exists`);

        let widget = {
            id: id,
            func: func,
            errors: []
        };
        let intf = {
            value: value,
            error: msg => {
                widget.errors.push(msg || '');
                return intf;
            }
        };

        widgets.push(widget);
        interfaces[name] = intf;

        return intf;
    }

    function wrapWidget(id, frag, options, errors) {
        return html`
            <div class=${errors.length ? 'af_var af_var_error' : 'af_var'} id=${id + '_div'}>
                ${frag}
                <span class="af_error">${errors.map(err => html`${err}<br/>`)}</span>
                ${options.help ? html`<p class="af_help">${options.help}</p>` : ''}
            </div>
        `;
    }

    this.find = name => interfaces[name];
    this.value = function(name) {
        let intf = interfaces[name];
        return intf ? intf.value : undefined;
    };
    this.error = (name, msg) => interfaces[name].error(msg);

    this.boolean = function(name, label, options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value = prev ? prev.checked : undefined;

        let func = errors => wrapWidget(id, html`
            <label for=${id}>${label}</label>
            <input id=${id} type="checkbox" checked=${value}
                   oninput=${e => self.changeHandler(e)}/>
        `, options, errors);

        return addWidget(name, id, func, value);
    };

    this.integer = function(name, label, options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value = (prev && prev.value != '') ? parseInt(prev.value, 10) : undefined;

        let func = errors => wrapWidget(id, html`
            <label for=${id}>${label}</label>
            <input id=${id} type="number" value=${value}
                   oninput=${e => self.changeHandler(e)}/>
        `, options, errors);

        let ret = addWidget(name, id, func, value);

        if (value != null) {
            if (options.min !== undefined && value < options.min)
                ret.error(`Valeur < ${options.min}`);
            if (options.max !== undefined && value > options.max)
                ret.error(`Valeur > ${options.max}`);
        }

        return ret;
    };

    this.dropdown = function(name, label, choices = [], options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value = prev ? prev.value : undefined;

        let func = errors => wrapWidget(id, html`
            <label for=${id}>${label}</label>
            <select id=${id} onchange=${e => self.changeHandler(e)}>
                <option value="" selected=${value == null}>-- Choisissez une option --</option>
                ${choices.map(c => html`<option value=${c[0]} selected=${value === c[0]}>${c[1]}</option>`)}
            </select>
        `, options, errors);

        return addWidget(name, id, func, value);
    };

    this.calc = function(name, label, value, options = {}) {
        let id = makeID(name);

        let text = value;
        if (!options.raw) {
            if (isNaN(value) || value == null) {
                text = '';
            } else if (isFinite(value)) {
                // This is a garbage way to round numbers
                let multiplicator = Math.pow(10, 2);
                let n = parseFloat((value * multiplicator).toFixed(11));
                text = Math.round(n) / multiplicator;
            }
        }

        let func = errors => wrapWidget(id, html`
            <label for=${id}>${label}</label>
            <span>${text}</span>
        `, options, errors);

        return addWidget(name, id, func, value);
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

            render(form, () => html`${widgets.map(w => w.func(w.errors))}`);
            log.innerHTML = '';

            return true;
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
    };

    render(widget, () => html`
        <div class="af_form"></div>
        <div class="af_log"></div>
    `);
    form = widget.childNodes[0];
    log = widget.childNodes[1];

    widget.object = this;
}
