// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormState(values = {}) {
    let self = this;

    // Interoperate
    this.decodeKey = key => key;
    this.setValue = (key, value) => {};
    this.getValue = (key, default_value) => default_value;
    this.changeHandler = model => {};

    // Stored values
    this.values = Object.assign({}, values);

    // Internal state
    this.unique_id = FormState.next_unique_id++;
    this.sections_state = {};
    this.tabs_state = {};
    this.file_lists = new Map;
    this.click_events = new Set;
    this.take_delayed = new Set;

    this.cached_values = Object.assign({}, values);
    this.changed_variables = new Set(Object.keys(values));
    this.updated_variables = new Set;

    this.hasChanged = function() { return !!self.changed_variables.size; };
}
FormState.next_unique_id = 0;

function FormModel() {
    let self = this;

    this.widgets = [];
    this.widgets0 = [];
    this.actions = [];

    this.values = {};
    this.variables = [];

    this.errors = 0;
    this.valid = true;

    this.render = function() {
        return html`
            <div class="fm_main">${self.renderWidgets()}</div>
            <div class="ui_actions">${self.renderActions()}</div>
        `;
    };
    this.renderWidgets = function() { return self.widgets0.map(intf => intf.render()); };
    this.renderActions = function() { return self.actions.map(intf => intf.render()); };
}

function FormBuilder(state, model, readonly = false) {
    let self = this;

    // XXX: Temporary workaround for lack of date and time inputs in Safari
    let has_input_date = (() => {
        if (typeof document !== 'undefined') {
            let el = document.createElement('input');
            el.setAttribute('type', 'date');
            el.value = '1900-01-01';
            return el.valueAsDate != null;
        } else {
            return false;
        }
    })();

    let variables_map = {};
    let options_stack = [{
        deploy: true,
        untoggle: true
    }];
    let widgets_ref = model.widgets0;

    let inline_next = false;
    let inline_widgets;

    let tabs_keys = new Set;
    let tabs_ref;
    let selected_tab;

    let restart = false;

    this.hasChanged = function() { return state.hasChanged(); };
    this.isValid = function() { return model.valid; };
    this.hasErrors = function() { return !!model.errors; };
    this.triggerErrors = function() {
        if (self.isValid()) {
            return true;
        } else {
            log.error('Corrigez les erreurs avant de valider');

            state.take_delayed = new Set(model.variables.map(variable => variable.key.toString()));
            self.restart();

            return false;
        }
    };

    this.pushOptions = function(options = {}) {
        options = expandOptions(options);
        options_stack.push(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error('Too many popOptions() operations');

        options_stack.pop();
    }

    this.find = key => variables_map[key];
    this.value = (key, default_value = undefined) => {
        let intf = variables_map[key];
        return (intf && !intf.missing) ? intf.value : default_value;
    };
    this.missing = key => variables_map[key].missing;
    this.error = (key, msg, delay = false) => variables_map[key].error(msg, delay);

    this.text = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options.value, value => (value != null) ? String(value) : undefined);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type="text" class="fm_input" style=${makeInputStyle(options)}
                   placeholder=${options.placeholder || ''}
                   .value=${value || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleTextInput(e, key)} />
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('text', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        return intf;
    };

    this.textArea = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options.value, value => (value != null) ? String(value) : undefined);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <textarea id=${id} class="fm_input" style=${makeInputStyle(options)}
                   rows=${options.rows || 5} cols=${options.cols || 40}
                   placeholder=${options.placeholder || ''}
                   .value=${value || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleTextInput(e, key)}></textarea>
        `);

        let intf = makeWidget('text', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        return intf;
    };

    this.password = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options.value, value => (value != null) ? String(value) : undefined);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type="password" class="fm_input" style=${makeInputStyle(options)}
                   .value=${value || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleTextInput(e, key)} />
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('password', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        return intf;
    };

    function handleTextInput(e, key) {
        if (!isModifiable(key))
            return;

        updateValue(key, e.target.value || undefined);
    }

    this.number = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options.value, value => {
            value = parseFloat(value);
            if (Number.isNaN(value))
                value = undefined;
            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type="number" class="fm_input" style=${makeInputStyle(options)}
                   step=${1 / Math.pow(10, options.decimals || 0)} .value=${value}
                   placeholder=${options.placeholder || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleNumberChange(e, key)}/>
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('number', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf);

        return intf;
    };

    function handleNumberChange(e, key) {
        if (!isModifiable(key))
            return;

        // Hack to accept incomplete values, mainly in the case of a '-' being typed first,
        // in which case we don't want to clear the field immediately.
        if (!e.target.validity || e.target.validity.valid) {
            let value = parseFloat(e.target.value);
            if (Number.isNaN(value))
                value = undefined;

            updateValue(key, value);
        }
    }

    this.slider = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        // Default options
        options.decimals = options.decimals || 0;
        options.min = options.min || 0;
        options.max = (options.max != null) ? options.max : 10;
        options.prefix = (options.prefix != null) ? options.prefix : options.min;
        options.suffix = (options.suffix != null) ? options.suffix : options.max;

        let range = options.max - options.min;
        if (range <= 0)
            throw new Error('Range (options.max - options.min) must be positive');

        let value = readValue(key, options.value, value => {
            value = parseFloat(value);
            if (Number.isNaN(value))
                value = undefined;
            return value;
        });

        let thumb_value = (value != null) ? value : ((options.max + options.min) / 2);
        let fix_value = (value != null) ? util.clamp(value, options.min, options.max)
                                        : ((options.min + options.max) / 2);

        // WebKit and Blink don't have anything like ::-moz-range-progress...
        // We use a gradient background set from a CSS property. Yuck.
        let webkit_progress = (value != null) ? ((fix_value - options.min) / range) : 0;

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class=${'fm_slider' + (value == null ? ' missing' : '') + (options.readonly ? ' readonly' : '')}
                 style=${makeInputStyle(options)}>
                <span style=${options.untoggle ? ' cursor: pointer;': ''}
                      @click=${e => handleSliderClick(e, key, value, options.min, options.max)}>${value != null ? value.toFixed(options.decimals) : '?'}</span>
                <div>
                    ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
                    <input id=${id} type="range" style=${`--webkit_progress: ${webkit_progress * 100}%`}
                           min=${options.min} max=${options.max} step=${1 / Math.pow(10, options.decimals)}
                           .value=${thumb_value} data-value=${thumb_value}
                           placeholder=${options.placeholder || ''}
                           ?disabled=${options.disabled}
                           @click=${e => { e.target.value = fix_value; handleSliderChange(e, key); }}
                           @dblclick=${e => handleSliderClick(e, key, value, options.min, options.max)}
                           @input=${e => handleSliderChange(e, key)}/>
                    ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
                </div>
            </div>
        `);

        let intf = makeWidget('slider', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf);

        return intf;
    }

    function handleSliderChange(e, key) {
        if (!isModifiable(key)) {
            e.target.value = e.target.dataset.value;
            return;
        }

        let value = parseFloat(e.target.value);

        if (value == null || Number.isNaN(value)) {
            value = undefined;

            e.target.style.setProperty('--webkit_progress', '0%');
        } else {
            let min = parseFloat(e.target.min);
            let max = parseFloat(e.target.max);

            let webkit_progress = (value - min) / (max - min);
            e.target.style.setProperty('--webkit_progress', `${webkit_progress * 100}%`);
        }

        updateValue(key, value);
    }

    function handleSliderClick(e, key, value, min, max) {
        if (!isModifiable(key))
            return;

        return ui.runDialog(e, (d, resolve, reject) => {
            let number = d.number('number', 'Valeur :', {min: min, max: max, value: value});

            d.action('Modifier', {disabled: !d.isValid()}, () => {
                updateValue(key, number.value);
                resolve(number.value);
            });
            d.action('Annuler', {}, () => reject(new Error('Action annulée')));
        });
    }

    this.enum = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options.value, value => {
            if (Array.isArray(value))
                value = (value.length === 1) ? value[0] : undefined;

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class=${options.readonly ? 'fm_enum readonly' : 'fm_enum'} id=${id}>
                ${props.map((p, i) =>
                    html`<button type="button" data-value=${util.valueToStr(p.value)}
                                 .className=${value === p.value ? 'active' : ''}
                                 ?disabled=${options.disabled}
                                 @click=${e => handleEnumChange(e, key, options.untoggle)}
                                 @keydown=${handleEnumOrMultiKey} tabindex=${i ? -1 : 0}>${p.label}</button>`)}
            </div>
        `);

        let intf = makeWidget('enum', label, render, options);
        fillVariableInfo(intf, key, value, props, false);
        addWidget(intf);

        return intf;
    };

    function handleEnumChange(e, key, allow_untoggle) {
        if (!isModifiable(key))
            return;

        let json = e.target.dataset.value;
        let activate = !e.target.classList.contains('active');

        if (activate || allow_untoggle) {
            // This is useless in most cases because the new form will incorporate
            // this change, but who knows. Do it like other browser-native widgets.
            let els = e.target.parentNode.querySelectorAll('button');
            for (let el of els)
                el.classList.toggle('active', el.dataset.value === json && activate);

            if (activate) {
                updateValue(key, util.strToValue(json));
            } else {
                updateValue(key, undefined);
            }
        }
    }

    this.binary = function(key, label, options = {}) {
        return self.enum(key, label, [[1, 'Oui'], [0, 'Non']], options);
    };
    this.boolean = function(key, label, options = {}) {
        return self.enum(key, label, [[true, 'Oui'], [false, 'Non']], options);
    };

    this.enumDrop = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options.value, value => {
            if (Array.isArray(value))
                value = (value.length === 1) ? value[0] : undefined;

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div style=${'display: inline-block; max-width: 100%; ' + makeInputStyle(options)}>
                <select id=${id} class="fm_select" ?disabled=${options.disabled}
                        @change=${e => handleEnumDropChange(e, key)}>
                    ${options.untoggle || !props.some(p => p != null && value === p.value) ?
                        html`<option value="undefined" .selected=${value == null}
                                     ?disabled=${options.readonly && value != null}>-- Choisissez une option --</option>` : ''}
                    ${props.map(p =>
                        html`<option value=${util.valueToStr(p.value)} .selected=${value === p.value}
                                     ?disabled=${options.readonly && value !== p.value}>${p.label}</option>`)}
                </select>
            </div>
        `);

        let intf = makeWidget('enumDrop', label, render, options);
        fillVariableInfo(intf, key, value, props, false);
        addWidget(intf);

        return intf;
    };

    function handleEnumDropChange(e, key) {
        if (!isModifiable(key))
            return;

        updateValue(key, util.strToValue(e.target.value));
    }

    this.enumRadio = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options.value, value => {
            if (Array.isArray(value))
                value = (value.length === 1) ? value[0] : undefined;

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class=${options.readonly ? 'fm_radio readonly' : 'fm_radio'} id=${id}>
                ${props.map((p, i) =>
                    html`<input type="radio" name=${id} id=${`${id}.${i}`} value=${util.valueToStr(p.value)}
                                ?disabled=${options.disabled} .checked=${value === p.value}
                                @click=${e => handleEnumRadioChange(e, key, options.untoggle && value === p.value)}
                                @keydown=${handleRadioOrCheckKey} tabindex=${i ? -1 : 0} />
                         <label for=${`${id}.${i}`}>${p.label}</label><br/>`)}
            </div>
        `);

        let intf = makeWidget('enumRadio', label, render, options);
        fillVariableInfo(intf, key, value, props, false);
        addWidget(intf);

        return intf;
    };

    function handleEnumRadioChange(e, key, already_checked) {
        if (!isModifiable(key))
            return;

        if (already_checked) {
            e.target.checked = false;
            updateValue(key, undefined);
        } else {
            updateValue(key, util.strToValue(e.target.value));
        }
    }

    this.multi = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options.value, value => {
            if (!Array.isArray(value)) {
                if (value != null) {
                    value = [value];
                } else {
                    let nullable = props.some(p => p.value == null);
                    value = nullable ? undefined : [];
                }
            }

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class=${options.readonly ? 'fm_enum readonly' : 'fm_enum'} id=${id}>
                ${props.map((p, i) =>
                    html`<button type="button" data-value=${util.valueToStr(p.value)}
                                 .className=${value != null && value.includes(p.value) ? 'active' : ''}
                                 ?disabled=${options.disabled}
                                 @click=${e => handleMultiChange(e, key)}
                                 @keydown=${handleEnumOrMultiKey} tabindex=${i ? -1 : 0}>${p.label}</button>`)}
            </div>
        `);

        let intf = makeWidget('multi', label, render, options);
        fillVariableInfo(intf, key, value, props, true);
        addWidget(intf);

        return intf;
    };

    function handleMultiChange(e, key) {
        if (!isModifiable(key))
            return;

        e.target.classList.toggle('active');

        let els = e.target.parentNode.querySelectorAll('button');

        let nullify = (e.target.classList.contains('active') && e.target.dataset.value === 'null');
        let value = [];
        for (let el of els) {
            if ((el.dataset.value === 'null') != nullify)
                el.classList.remove('active');
            if (el.classList.contains('active'))
                value.push(util.strToValue(el.dataset.value));
        }

        updateValue(key, value);
    }

    function handleEnumOrMultiKey(e) {
        switch (e.keyCode) {
            case 37: { // left
                let prev = e.target.previousElementSibling;
                if (prev)
                    prev.focus();
                e.preventDefault();
            } break;
            case 39: { // right
                let next = e.target.nextElementSibling;
                if (next)
                    next.focus();
                e.preventDefault();
            } break;
        }
    }

    this.multiCheck = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options.value, value => {
            if (!Array.isArray(value)) {
                if (value != null) {
                    value = [value];
                } else {
                    let nullable = props.some(p => p.value == null);
                    value = nullable ? undefined : [];
                }
            }

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class=${options.readonly ? 'fm_multi readonly' : 'fm_multi'} id=${id}>
                ${props.map((p, i) =>
                    html`<input type="checkbox" id=${`${id}.${i}`} value=${util.valueToStr(p.value)}
                                ?disabled=${options.disabled} .checked=${value != null && value.includes(p.value)}
                                @click=${e => handleMultiCheckChange(e, key)}
                                @keydown=${handleRadioOrCheckKey} tabindex=${i ? -1 : 0} />
                         <label for=${`${id}.${i}`}>${p.label}</label><br/>`)}
            </div>
        `);

        let intf = makeWidget('multiCheck', label, render, options);
        fillVariableInfo(intf, key, value, props, true);
        addWidget(intf);

        return intf;
    };

    function handleMultiCheckChange(e, key) {
        if (!isModifiable(key))
            return;

        let els = e.target.parentNode.querySelectorAll('input');

        let nullify = (e.target.checked && e.target.value === 'null');
        let value = [];
        for (let el of els) {
            if ((el.value === 'null') != nullify)
                el.checked = false;
            if (el.checked)
                value.push(util.strToValue(el.value));
        }

        updateValue(key, value);
    }

    function handleRadioOrCheckKey(e) {
        switch (e.keyCode) {
            case 38: { // up
                let prev = e.target.previousElementSibling;
                while (prev && prev.tagName !== 'INPUT')
                    prev = prev.previousElementSibling;
                if (prev)
                    prev.focus();
                e.preventDefault();
            } break;
            case 40: { // down
                let next = e.target.nextElementSibling;
                while (next && next.tagName !== 'INPUT')
                    next = next.nextElementSibling;
                if (next)
                    next.focus();
                e.preventDefault();
            } break;
        }
    }

    this.proposition = function(value, label) {
        return {value: value, label: label || value};
    };

    function normalizePropositions(props) {
        if (!Array.isArray(props))
            props = Array.from(props);

        props = props.filter(c => c != null).map(c => {
            if (Array.isArray(c)) {
                return {value: c[0], label: c[1] || c[0]};
            } else if (typeof c === 'string') {
                let sep_pos = c.indexOf(':::');
                if (sep_pos >= 0) {
                    let value = c.substr(0, sep_pos);
                    let label = c.substr(sep_pos + 3);
                    return {value: value, label: label || value};
                } else {
                    return {value: c, label: c};
                }
            } else if (typeof c === 'number') {
                return {value: c, label: c};
            } else {
                return c;
            }
        });

        return props;
    }

    this.date = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        if (options.value != null)
            options.value = options.value.toString();

        let value = readValue(key, options.value, value => {
            if (typeof value === 'string') {
                value = dates.parseSafe(value);
            } else if (value != null && value.constructor.name !== 'LocalDate') {
                value = undefined;
            }

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type=${has_input_date ? 'date' : 'text'}
                   class="fm_input" style=${makeInputStyle(options)}
                   .value=${value ? (has_input_date ? value.toString() : value.toLocaleString()) : ''}
                   placeholder=${!has_input_date ? 'DD/MM/YYYY' : ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleDateInput(e, key)}/>
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('date', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf, dates.parse);

        return intf;
    };

    function handleDateInput(e, key) {
        if (!isModifiable(key))
            return;

        if (has_input_date) {
            let date = dates.parse(e.target.value || null);
            updateValue(key, date || undefined);
        } else {
            try {
                let date = dates.parse(e.target.value || null);

                e.target.setCustomValidity('');
                updateValue(key, date || undefined);
            } catch (err) {
                e.target.setCustomValidity('Date malformée ou ambiguë');
            }
        }
    }

    this.time = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        if (options.value != null)
            options.value = options.value.toString();

        let value = readValue(key, options.value, value => {
            if (typeof value === 'string') {
                value = times.parseSafe(value);
            } else if (value != null && value.constructor.name !== 'LocalTime') {
                value = undefined;
            }

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type=${has_input_date ? 'time' : 'text'} step
                   class="fm_input" style=${makeInputStyle(options)}
                   .value=${value ? value.toString().substr(0, options.seconds ? 8 : 5) : ''}
                   placeholder=${!has_input_date ? 'HH:MM:SS'.substr(0, options.seconds ? 8 : 5) : ''}
                   step=${options.seconds ? 1 : 60}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleTimeInput(e, key)} />
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('date', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf, times.parse);

        return intf;
    };

    function handleTimeInput(e, key) {
        if (!isModifiable(key))
            return;

        if (has_input_date) {
            let time = times.parse(e.target.value || null);
            updateValue(key, time || undefined);
        } else {
            try {
                let time = times.parse(e.target.value || null);

                e.target.setCustomValidity('');
                updateValue(key, time || undefined);
            } catch (err) {
                e.target.setCustomValidity('Horaire malformé');
            }
        }
    }

    this.file = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options.value, value => {
            if (!(value instanceof File))
                value = undefined;

            return value;
        });

        // Setting files on input file elements is fragile. At least on Firefox, assigning
        // its own value to the property results in an empty FileList for some reason.
        let set_files = lithtml.directive(() => part => {
            let file_list = state.file_lists.get(key.toString());

            if (file_list == null) {
                part.committer.element.value = '';
            } else if (file_list !== part.committer.element.files) {
                part.committer.element.files = file_list;
            }
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <input id=${id} type="file" size="${options.size || 30}" .files=${set_files()}
                   placeholder=${options.placeholder || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleFileInput(e, key)}/>
        `);

        let intf = makeWidget('file', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        return intf;
    };

    function handleFileInput(e, key) {
        if (!isModifiable(key))
            return;

        state.file_lists.set(key.toString(), e.target.files);
        updateValue(key, e.target.files[0] || undefined);
    }

    this.calc = function(key, label, value, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        if (value != null && typeof value !== 'string' &&
                             typeof value !== 'number' &&
                             value.constructor.name !== 'LocalDate' &&
                             value.constructor.name !== 'LocalTime')
            throw new Error('Calculated value must be a string, a number, a date or a time');
        if (Number.isNaN(value))
            value = undefined;

        let text = value;
        if (!options.raw && typeof value !== 'string') {
            if (value == null) {
                value = undefined;
                text = 'Non calculable';
            } else if (isFinite(value)) {
                if (options.decimals != null) {
                    text = value.toFixed(options.decimals);
                } else {
                    // This is a garbage way to round numbers
                    let multiplicator = Math.pow(10, 2);
                    let n = parseFloat((value * multiplicator).toFixed(11));
                    text = Math.round(n) / multiplicator;
                }
            }
        }

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            <label for=${id}>${label || key}</label>
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <span id="${id}" class="fm_calc">${text}</span>
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('calc', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        readValue(key, undefined, value => value);
        updateValue(key, value, false);

        return intf;
    };

    this.output = function(content, options = {}) {
        options = expandOptions(options);

        // This helps avoid garbage output when the user types 'page.output(html);'
        if (content != null && content !== html && content !== svg) {
            let render = intf => html`<div class="fm_wrap">${content}</div>`;

            let intf = makeWidget('output', null, render, options);
            addWidget(intf);

            return intf;
        }
    };

    this.block = function(func, options = {}) {
        options = expandOptions(options);

        let widgets = [];
        let render = intf => widgets.map(intf => intf.render());

        let intf = makeWidget('block', '', render, options);
        addWidget(intf);

        captureWidgets(widgets, 'block', func);

        return intf;
    };
    this.scope = this.block;

    this.section = function(label, func, options = {}) {
        options = expandOptions(options);

        let deploy = state.sections_state[label];
        if (deploy == null) {
            deploy = options.deploy;
            state.sections_state[label] = deploy;
        }

        let widgets = [];
        let render = intf => html`
            <fieldset class="fm_container fm_section">
                ${label ? html`<div class="fm_legend" @click=${e => handleSectionClick(e, label)}>${label}</div>` : ''}
                ${deploy ?
                    widgets.map(intf => intf.render()) :
                    html`<a class="fm_deploy"
                            @click=${e => handleSectionClick(e, label)}>(ouvrir la section)</a>`}
            </fieldset>
        `;

        let intf = makeWidget('section', label, render, options);
        addWidget(intf);

        captureWidgets(widgets, 'section', func);

        return intf;
    };

    function handleSectionClick(e, label) {
        state.sections_state[label] = !state.sections_state[label];
        self.restart();
    }

    this.errorList = function(options = {}) {
        options = expandOptions(options);

        let render = intf => {
            if (self.hasErrors() || options.force) {
                return html`
                    <fieldset class="fm_container fm_section error">
                        <div class="fm_legend">${options.label || 'Liste des erreurs'}</div>
                        ${!self.hasErrors() ? 'Aucune erreur' : ''}
                        ${model.widgets.map(intf => {
                            if (intf.errors.length) {
                                return html`${intf.errors.length} ${intf.errors.length > 1 ? 'erreurs' : 'erreur'} sur :
                                            <a href=${'#' + makeID(intf.key)}>${intf.label}</a><br/>`;
                             } else {
                                return '';
                             }
                        })}
                    </fieldset>
                `;
            } else {
                return '';
            }
        };

        let intf = makeWidget('errorList', null, render, options);
        addWidget(intf);

        return intf;
    };

    this.tabs = function(key, func, options = {}) {
        options = expandOptions(options);

        if (!key)
            throw new Error('Key parameter must be specified');
        if (tabs_keys.has(key))
            throw new Error(`Tabs key '${key}' is already used`);
        tabs_keys.add(key);

        let tabs = [];
        let tab_idx;
        let widgets = [];

        let render = intf => tabs.length ? html`
            <div class="fm_container fm_section tabs">
                <div class="fm_tabs">
                    ${tabs.map((tab, idx) =>
                        html`<button type="button" class=${idx === tab_idx ? 'active' : ''}
                                     ?disabled=${tab.disable}
                                     @click=${e => handleTabClick(e, key, idx)}>${tab.label}</button>`)}
                </div>

                ${widgets.map(intf => intf.render())}
            </div>
        ` : '';

        let intf = makeWidget('tabs', null, render, options);
        addWidget(intf);

        // Capture tabs and widgets
        {
            let prev_tabs = tabs_ref;
            let prev_select = selected_tab;

            tabs_ref = tabs;
            selected_tab = (state.tabs_state.hasOwnProperty(key) ? state.tabs_state[key] : options.tab) || 0;
            captureWidgets(widgets, 'tabs', func);

            // form.tab() can change selected_tab
            if (typeof selected_tab === 'string') {
                tab_idx = tabs.findIndex(tab => tab.label === selected_tab);
            } else {
                tab_idx = selected_tab;
            }
            if (!tabs[tab_idx] || tabs[tab_idx].disable)
                tab_idx = tabs.findIndex(tab => !tab.disable);
            state.tabs_state[key] = tab_idx;

            selected_tab = prev_select;
            tabs_ref = prev_tabs;
        }

        return intf;
    };

    this.tab = function(label, func, options = {}) {
        options = expandOptions(options);

        if (!tabs_ref)
            throw new Error('form.tab must be called from inside form.tabs');
        if (!label)
            throw new Error('Label must be specified');
        if (tabs_ref.find(tab => tab.label === label))
            throw new Error(`Tab label '${label}' is already used`);

        let active;
        if (selected_tab === tabs_ref.length || selected_tab === label) {
            if (!options.disabled) {
                active = true;
            } else {
                selected_tab = tabs_ref.length + 1;
                active = false;
            }
        } else {
            active = false;
        }

        let tab = {
            label: label,
            disable: options.disabled
        };
        tabs_ref.push(tab);

        // We don't want to show actions created inside inactive tabs
        let prev_actions_len = model.actions.length;

        let widgets = captureWidgets([], 'tab', () => func(active));
        if (active) {
            widgets_ref.push(...widgets);
        } else {
            model.actions.length = prev_actions_len;
        }
        model.widgets.push(...widgets);
    };

    function handleTabClick(e, key, label) {
        state.tabs_state[key] = label;
        self.restart();
    }

    this.columns = function(func, options) {
        options = expandOptions(options);

        let widgets = [];
        let render = intf => html`
            <div class="fm_container fm_columns">
                ${widgets.map(intf => html`
                    <div style=${options.wide ? 'flex: 1;' : ''}>
                        ${intf.render()}
                    </div>
                `)}
            </div>
        `;

        let intf = makeWidget('columns', undefined, render, options);
        addWidget(intf);

        captureWidgets(widgets, 'columns', func, {compact: false});

        return intf;
    };

    this.sameLine = function(wide = false) {
        if (inline_widgets == null) {
            let prev_widget = widgets_ref.pop();

            let widgets;
            self.columns(() => {
                if (prev_widget != null)
                    widgets_ref.push(prev_widget);
                widgets = widgets_ref;
            }, {wide: wide});

            inline_widgets = widgets;
            inline_next = true;
        } else {
            inline_next = true;
        }
    };

    function captureWidgets(widgets, type, func, options = {}) {
        if (!func) {
            throw new Error(`This call does not contain a function.

Make sure you did not use this syntax by accident:
    page.${type}("Title"), () => { /* Do stuff here */ };
instead of:
    page.${type}("Title", () => { /* Do stuff here */ });`);
        }

        let prev_widgets = widgets_ref;
        let prev_options = options_stack;
        let prev_inline = inline_widgets;

        try {
            widgets_ref = widgets;
            options_stack = [expandOptions(options)];

            inline_next = false;
            inline_widgets = null;

            func();
        } finally {
            inline_next = false;
            inline_widgets = prev_inline;

            options_stack = prev_options;
            widgets_ref = prev_widgets;
        }

        return widgets;
    }

    this.action = function(label, options = {}, func = null) {
        options = expandOptions(options);

        if (func == null)
            func = e => handleActionClick(e, label);

        let clicked = state.click_events.has(label);
        state.click_events.delete(label);

        let render;
        if (typeof label === 'string' && label.match(/^\-+$/)) {
            render = intf => html`<hr/>`;
        } else {
            let type = model.actions.length ? 'button' : 'submit';

            render = intf => html`<button type=${type} ?disabled=${options.disabled}
                                          title=${options.tooltip || ''}
                                          @click=${ui.wrapAction(func)}>${label}</button>`;
        }

        let intf = makeWidget('action', label, render, options);
        intf.clicked = clicked;
        model.actions.push(intf);

        return intf;
    };

    function handleActionClick(e, label) {
        state.click_events.add(label, e);
        self.restart();
    }

    this.restart = function() {
        if (!restart) {
            setTimeout(() => state.changeHandler(model), 0);
            restart = true;
        }
    };

    function decodeKey(key, options) {
        if (typeof key === 'string') {
            for (;;) {
                if (key[0] === '*') {
                    options.mandatory = true;
                } else {
                    break;
                }

                key = key.substr(1);
            }
        }

        return state.decodeKey(key);
    }

    function expandOptions(options) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);
        if (readonly)
            options.readonly = true;
        return options;
    }

    function makeID(key) {
        return `fm_var_${state.unique_id}_${key}`;
    }

    function renderWrappedWidget(intf, frag) {
        let cls = 'fm_widget';
        if (intf.errors.length)
            cls += ' error';
        if (intf.options.mandatory)
            cls += ' mandatory';
        if (intf.options.compact)
            cls += ' compact';

        return html`
            <div class=${intf.options.disabled ? 'fm_wrap disabled' : 'fm_wrap'}>
                <div class=${cls}>
                    ${frag}
                    ${intf.errors.length ?
                        html`<div class="fm_error">${intf.errors.map(err => html`${err}<br/>`)}</div>` : ''}
                </div>
                ${intf.options.help ? html`<p class="fm_help">${intf.options.help}</p>` : ''}
            </div>
        `;
    }

    function makeInputStyle(options) {
        return options.wide ? 'width: 100%;' : '';
    }

    function makeWidget(type, label, func, options = {}) {
        if (label != null) {
            // Users are allowed to use complex HTML as label. Turn it into text for storage!
            if (typeof label === 'string' || typeof label === 'number') {
                label = '' + label;
            } else if (typeof lithtml !== 'undefined') {
                let el = document.createElement('span');
                render(label, el);
                label = el.textContent;
            } else {
                label = '????'; // QuickJS might hit this
            }
            label = label.trim();

            // Keep only first line of label
            let newline_idx = label.indexOf('\n');
            label = (newline_idx >= 0) ? label.substr(0, newline_idx).trim() : label;
        }

        let intf = {
            type: type,
            label: label,
            options: options,

            errors: [],

            render: () => {
                intf.render = () => '';

                if (!intf.options.hidden) {
                    return func(intf);
                } else {
                    return '';
                }
            },
            toHTML: () => intf.render()
        };

        return intf;
    }

    function addWidget(intf) {
        if (inline_next) {
            inline_widgets.push(intf);
            inline_next = false;
        } else {
            inline_widgets = null;
            widgets_ref.push(intf);
        }

        model.widgets.push(intf);
    }

    function fillVariableInfo(intf, key, value, props = null, multi = false) {
        if (variables_map[key])
            throw new Error(`Variable '${key}' already exists`);

        Object.assign(intf, {
            key: key,
            value: value,

            missing: value == null,
            changed: state.changed_variables.has(key.toString()),
            updated: state.updated_variables.has(key.toString()),

            error: (msg, delay = false) => {
                if (!delay || state.take_delayed.has(key.toString())) {
                    intf.errors.push(msg || '');
                    model.errors++;
                }

                model.valid = false;

                return intf;
            }
        });
        state.updated_variables.delete(key.toString());

        if (props != null) {
            intf.props = props;
            intf.props_map = util.arrayToObject(props, prop => prop.value, prop => prop.label);
            intf.multi = multi;
        }

        if (intf.options.mandatory && intf.missing) {
            intf.error('Obligatoire !', intf.options.missing_mode !== 'error');
            if (intf.options.missing_mode === 'disable')
                valid = false;
        }

        model.variables.push(intf);
        variables_map[key] = intf;

        model.values[key] = value;

        return intf;
    }

    function validateMinMax(intf, transform = value => value) {
        let value = intf.value;
        let options = intf.options;

        if (value != null) {
            let min = transform(options.min);
            let max = transform(options.max);

            if ((min != null && value < min) ||
                    (max != null && value > max)) {
                if (min != null && max != null) {
                    intf.error(`Doit être entre ${min.toLocaleString()} et ${max.toLocaleString()}`);
                } else if (min != null) {
                    intf.error(`Doit être ≥ ${min.toLocaleString()}`);
                } else {
                    intf.error(`Doit être ≤ ${max.toLocaleString()}`);
                }
            }
        }
    }

    function makePrefixOrSuffix(cls, text, value) {
        if (typeof text === 'function') {
            return html`<span class="${cls}">${text(value)}</span>`;
        } else if (text != null && text !== '') {
            return html`<span class="${cls}">${text}</span>`;
        } else {
            return '';
        }
    }

    function readValue(key, default_value, func) {
        let value;
        if (!state.changed_variables.has(key.toString())) {
            value = state.getValue(key, default_value);

            state.cached_values[key] = value;
            state.values[key] = value;
        } else {
            value = state.values[key];
        }

        value = func(value);
        if (value == null)
            value = undefined;
        if (value !== state.values[key]) {
            if (value !== state.cached_values[key]) {
                state.changed_variables.add(key.toString());
            } else {
                state.changed_variables.delete(key.toString());
            }
            state.values[key] = value;
        }

        return value;
    }

    function isModifiable(key) {
        let intf = variables_map[key];
        return !intf.options.disabled && !intf.options.readonly;
    }

    function updateValue(key, value, refresh = true) {
        if (value !== state.values[key]) {
            let intf = variables_map[key];

            state.values[key] = value;

            state.take_delayed.delete(key.toString());
            if (value !== state.cached_values[key]) {
                state.changed_variables.add(key.toString());
            } else {
                state.changed_variables.delete(key.toString());
            }
            state.updated_variables.add(key.toString());

            state.setValue(key, value);
            if (refresh)
                self.restart();
        }
    }
}
