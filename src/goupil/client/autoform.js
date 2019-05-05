// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormBuilder(root, widgets) {
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
            <label for=${id}>${label || name}</label>
            <input id=${id} type="number" .value=${value}
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
            <label for=${id}>${label || name}</label>
            <select id=${id} @change=${e => self.changeHandler(e)}>
                <option value="null" .selected=${value == null}>-- Choisissez une option --</option>
                ${choices.filter(c => c != null).map(c =>
                    html`<option value=${stringifyValue(c[0])} .selected=${value === c[0]}>${c[1]}</option>`)}
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
            <label for=${id}>${label || name}</label>
            <div class="af_select" id=${id}>
                ${choices.filter(c => c != null).map(c =>
                    html`<button data-value=${stringifyValue(c[0])}
                                 .className=${value == c[0] ? 'af_button active' : 'af_button'}
                                 @click=${e => changeSelect(e, id, c[0])}>${c[1]}</button>`)}
            </div>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    this.boolean = function(name, label, options = {}) {
        return self.select(name, label, [[true, 'Oui'], [false, 'Non']], options);
    };

    function changeRadio(e, already_checked) {
        if (already_checked)
            e.target.checked = false;

        self.changeHandler(e);
    }

    this.radio = function(name, label, choices = [], options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value;
        if (prev) {
            let el = prev.querySelector('input:checked');
            if (el)
                value = parseValue(el.value);
        }

        let render = errors => wrapWidget(html`
            <label>${label || name}</label>
            <div class="af_radio" id=${id}>
                ${choices.filter(c => c != null).map((c, i) =>
                    html`<input type="radio" name=${id} id=${`${id}.${i}`} value=${stringifyValue(c[0])}
                                .checked=${value === c[0]}
                                @click=${e => changeRadio(e, value === c[0])}/>
                         <label for=${`${id}.${i}`}>${c[1]}</label><br/>`)}
            </div>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    this.multi = function(name, label, choices = [], options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value = [];
        if (prev) {
            let els = prev.querySelectorAll('input');
            for (let el of els) {
                if (el.checked)
                    value.push(parseValue(el.value));
            }
        }

        let render = errors => wrapWidget(html`
            <label>${label || name}</label>
            <div class="af_multi" id=${id}>
                ${choices.filter(c => c != null).map((c, i) =>
                    html`<input type="checkbox" name=${id} id=${`${id}.${i}`} value=${stringifyValue(c[0])}
                                .checked=${value.includes(c[0])}
                                @click=${e => self.changeHandler(e)}/>
                         <label for=${`${id}.${i}`}>${c[1]}</label><br/>`)}
            </div>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
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
            <label for=${id}>${label || name}</label>
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
                ${label ? html`<legend>${label}</legend>` : html``}
                ${widgets.map(w => w.render(w.errors))}
            </fieldset>
        `;

        addWidget(render);
    };

    this.link = function(label, dest, options = {}) {
        if (typeof dest === 'function') {
            let render = () => wrapWidget(html`
                <a class="af_link" href="#" @click=${e => { dest(e); e.preventDefault(); }}>${label}</a>
            `, options);

            addWidget(render);
        } else {
            let render = () => wrapWidget(html`
                <a class="af_link" href=${dest}>${label}</a>
            `, options);

            addWidget(render);
        }
    };

    this.button = function(label, func, options = {}) {
        let render = () => wrapWidget(html`
            <button class="af_button" @click=${func}>${label}</button>
        `, options);

        addWidget(render);
    };
}

function AutoForm(widget) {
    let self = this;

    let form;
    let log;

    let pages = new Map;
    let current_page_key = null;

    function page(key, title, func) {
        if (pages.has(key))
            throw new Error(`Page '${title}' already exists`);

        let page = {
            key: key,
            title: title,
            func: func
        };

        pages.set(key, page);
    }

    function go(key) {
        setTimeout(() => renderPage(key), 0);
    }

    function setError(line, msg) {
        form.classList.add('af_form_broken');

        log.textContent = `⚠ Line ${line || '?'}: ${msg}`;
        log.style.display = 'block';
    }

    function clearError() {
        form.classList.remove('af_form_broken');

        log.innerHTML = '';
        log.style.display = 'none';
    }

    function parseAnonymousErrorLine(err) {
        if (err.stack) {
            let m = err.stack.match(/ > Function:([0-9]+):[0-9]+/) ||
                    err.stack.match(/, <anonymous>:([0-9]+):[0-9]+/);

            // Can someone explain to me why do I have to offset by -2?
            let line = (m && m.length >= 2) ? (parseInt(m[1], 10) - 2) : null;
            return line;
        } else {
            return null;
        }
    }

    function renderPage(key) {
        let page = pages.get(key);
        if (!page) {
            setError(null, `Page '${key}' does not exist`);
            return false;
        }
        current_page_key = page.key;

        let widgets = [];
        let form_builder = new FormBuilder(form, widgets);
        form_builder.changeHandler = () => renderPage(key);

        try {
            page.func(form_builder);
        } catch (err) {
            let line = parseAnonymousErrorLine(err);

            setError(line, err.message);
            return false;
        }

        render(html`
            <h1 class="af_title">${page.title}</h1>
            ${widgets.map(w => w.render(w.errors))}
        `, form);

        clearError();
        return true;
    }

    self.update = function(script) {
        pages.clear();

        try {
            Function('page', 'go', script)(page, go);
        } catch (err) {
            let line;
            if (err instanceof SyntaxError) {
                // At least Firefox seems to do well in this case, it's better than nothing
                line = err.lineNumber - 2;
            } else if (err.stack) {
                line = parseAnonymousErrorLine(err);
            }

            setError(line, err.message);
            return false;
        }

        if (pages.size) {
            if (!current_page_key)
                current_page_key = pages.values().next().value.key;

            return renderPage(current_page_key);
        } else {
            current_page_key = null;

            render(html``, form);
            setError(null, 'No page defined');

            return false;
        }
    };

    render(html`
        <div class="af_form"></div>
        <div class="af_log" style="display: none;"></div>
    `, widget);
    form = widget.querySelector('.af_form');
    log = widget.querySelector('.af_log');

    widget.object = this;
}
