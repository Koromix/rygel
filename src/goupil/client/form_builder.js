// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormBuilder(state, widgets, variables = []) {
    let self = this;

    // Prevent DOM ID conflicts
    let unique_key = ++FormBuilder.current_unique_key;

    let variables_map = {};
    let widgets_ref = widgets;
    let options_stack = [{
        deploy: true,
        untoggle: true
    }];

    let missing_set = new Set;
    let missing_block = false;

    this.errors = [];

    this.changeHandler = form => {};
    this.validateHandler = form => {
        let problems = [];
        if (missing_block)
            problems.push('Informations obligatoires manquantes');
        if (self.errors.length)
            problems.push('Présence d\'erreurs sur le formulaire');

        return problems;
    };
    this.submitHandler = null;

    this.isValid = function() { return !self.validateHandler(self).length; };

    function makeID(key) {
        return `af_var_${unique_key}_${key}`;
    }

    function renderWrappedWidget(intf, frag) {
        let cls = 'af_widget';
        if (intf.options.large)
            cls += ' af_widget_large';
        if (intf.errors.length)
            cls += ' af_widget_error';
        if (intf.options.disable)
            cls += ' af_widget_disable';
        if (intf.options.mandatory)
            cls += ' af_widget_mandatory';

        return html`
            <div class=${cls}>
                ${frag}
                ${intf.errors.length && intf.errors.every(err => err) ?
                    html`<div class="af_error">${intf.errors.map(err => html`${err}<br/>`)}</div>` : html``}
                ${intf.options.help ? html`<p class="af_help">${intf.options.help}</p>` : ''}
            </div>
        `;
    }

    function addWidget(render, options = {}) {
        let intf = {
            render: render,
            options: options,
            errors: []
        };

        widgets_ref.push(intf);

        return intf;
    }

    function fillVariableInfo(intf, key, type, label, value, missing) {
        Object.assign(intf, {
            key: key,
            type: type,
            label: label,
            value: value,
            missing: missing || intf.options.missing,
            error: msg => {
                if (!intf.errors.length)
                    self.errors.push(intf);
                intf.errors.push(msg || '');

                return intf;
            }
        });

        if (intf.options.mandatory && intf.missing) {
            missing_set.add(key);
            if (intf.options.missingMode === 'error' || state.missing_errors.has(key))
                intf.error('Donnée obligatoire manquante');
            if (intf.options.missingMode === 'disable')
                missing_block |= true;
        }

        variables.push(intf);
        variables_map[key] = intf;

        return intf;
    }

    function makePrefixOrSuffix(cls, text, value) {
        if (typeof text === 'function') {
            return html`<span class="${cls}">${text(value)}</span>`;
        } else if (text) {
            return html`<span class="${cls}">${text}</span>`;
        } else {
            return html``;
        }
    }

    function expandOptions(options) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);
        return options;
    }

    function decodeKey(key, options) {
        if (key.startsWith('*')) {
            key = key.substr(1);
            options.mandatory = true;
        }

        if (variables_map[key])
            throw new Error(`Variable '${key}' already exists`);
        if (!key)
            throw new Error('Empty variable keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed variable key characters: a-z, _ and 0-9 (not as first character)');

        return key;
    }

    this.pushOptions = function(options = {}) {
        options = expandOptions(options);
        options_stack.push(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error('Too many popOptions() operations');

        options_stack.pop();
    }

    function parseValue(str) { return (str && str !== 'undefined') ? JSON.parse(str) : undefined; }
    function stringifyValue(value) { return JSON.stringify(value); }

    this.find = key => variables_map[key];
    this.value = key => {
        let intf = variables_map[key];
        return intf ? intf.value : undefined;
    };
    this.missing = key => variables_map[key].missing;
    this.error = (key, msg) => variables_map[key].error(msg);

    function handleTextInput(e, key) {
        let value = e.target.value;
        state.values[key] = value || null;
        state.missing_errors.delete(key);

        self.changeHandler(self);
    }

    this.text = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let id = makeID(key);
        let value = state.values.hasOwnProperty(key) ? state.values[key] : options.value;

        let render = intf => renderWrappedWidget(intf, html`
            <label for=${id}>${label || key}</label>
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="text" size="${options.size || 30}" .value=${value || ''}
                   placeholder=${options.placeholder || ''} ?disabled=${options.disable}
                   @input=${e => handleTextInput(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, 'string', label, value, value == null);

        return intf;
    };

    this.password = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let id = makeID(key);
        let value = state.values.hasOwnProperty(key) ? state.values[key] : options.value;

        let render = intf => renderWrappedWidget(intf, html`
            <label for=${id}>${label || key}</label>
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="password" size="${options.size || 30}" .value=${value || ''}
                   ?disabled=${options.disable} @input=${e => handleTextInput(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, 'string', label, value, value == null);

        return intf;
    };

    function handleNumberChange(e, key) {
        let value = parseFloat(e.target.value);

        // Hack to accept incomplete values, mainly in the case of a '-' being typed first,
        // in which case we don't want to clear the field immediately.
        if (!isNaN(value) || !isNaN(state.values[key])) {
            state.values[key] = value;
            state.missing_errors.delete(key);

            self.changeHandler(self);
        }
    }

    this.number = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let id = makeID(key);
        let value = parseFloat(state.values.hasOwnProperty(key) ? state.values[key] : options.value);

        let render = intf => renderWrappedWidget(intf, html`
            <label for=${id}>${label || key}</label>
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="number" .value=${value}
                   step=${1 / Math.pow(10, options.decimals || 0)}
                   placeholder=${options.placeholder || ''} ?disabled=${options.disable}
                   @input=${e => handleNumberChange(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, 'number', label, value, Number.isNaN(value));

        if (value != null &&
                (options.min !== undefined && value < options.min) ||
                (options.max !== undefined && value > options.max)) {
            if (options.min !== undefined && options.max !== undefined) {
                intf.error(`Doit être entre ${options.min} et ${options.max}`);
            } else if (options.min !== undefined) {
                intf.error(`Doit être ≥ ${options.min}`);
            } else {
                intf.error(`Doit être ≤ ${options.max}`);
            }
        }

        return intf;
    };

    function normalizePropositions(props) {
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

    this.proposition = function(value, label) {
        return {value: value, label: label || value};
    };

    function handleDropdownChange(e, key) {
        let value = parseValue(e.target.value);
        state.values[key] = value;
        state.missing_errors.delete(key);

        self.changeHandler(self);
    }

    this.dropdown = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let id = makeID(key);
        let value = state.values.hasOwnProperty(key) ? state.values[key] : options.value;

        let render = intf => renderWrappedWidget(intf, html`
            <label for=${id}>${label || key}</label>
            <select id=${id} ?disabled=${options.disable}
                    @change=${e => handleDropdownChange(e, key)}>
                ${options.untoggle || !props.some(p => p != null && value === p.value) ?
                    html`<option value="null" .selected=${value == null}>-- Choisissez une option --</option>` : html``}
                ${props.map(p =>
                    html`<option value=${stringifyValue(p.value)} .selected=${value === p.value}>${p.label}</option>`)}
            </select>
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, 'enum', label, value, value == null);

        return intf;
    };

    function handleChoiceChange(e, key, allow_untoggle) {
        let json = e.target.dataset.value;
        if (e.target.classList.contains('active') && allow_untoggle) {
            state.values[key] = undefined;
        } else {
            state.values[key] = parseValue(json);
        }
        state.missing_errors.delete(key);

        // This is useless in most cases because the new form will incorporate
        // this change, but who knows. Do it like other browser-native widgets.
        let els = e.target.parentNode.querySelectorAll('button');
        for (let el of els)
            el.classList.toggle('active', el.dataset.value === json &&
                                          (!el.classList.contains('active') || !allow_untoggle));

        self.changeHandler(self);
    }

    this.choice = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let id = makeID(key);
        let value = state.values.hasOwnProperty(key) ? state.values[key] : options.value;

        let render = intf => renderWrappedWidget(intf, html`
            <label for=${id}>${label || key}</label>
            <div class="af_select" id=${id}>
                ${props.map(p =>
                    html`<button data-value=${stringifyValue(p.value)}
                                 ?disabled=${options.disable} .className=${value === p.value ? 'af_button active' : 'af_button'}
                                 @click=${e => handleChoiceChange(e, key, options.untoggle)}>${p.label}</button>`)}
            </div>
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, 'enum', label, value, value == null);

        return intf;
    };

    this.binary = function(key, label, options = {}) {
        return self.choice(key, label, [[1, 'Oui'], [0, 'Non']], options);
    };
    this.boolean = function(key, label, options = {}) {
        return self.choice(key, label, [[true, 'Oui'], [false, 'Non']], options);
    };

    function handleRadioChange(e, key, already_checked) {
        let value = parseValue(e.target.value);

        if (already_checked) {
            e.target.checked = false;
            state.values[key] = undefined;
        } else {
            state.values[key] = value;
        }
        state.missing_errors.delete(key);

        self.changeHandler(self);
    }

    this.radio = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let id = makeID(key);
        let value = state.values.hasOwnProperty(key) ? state.values[key] : options.value;

        let render = intf => renderWrappedWidget(intf, html`
            <label>${label || key}</label>
            <div class="af_radio" id=${id}>
                ${props.map((p, i) =>
                    html`<input type="radio" name=${id} id=${`${id}.${i}`} value=${stringifyValue(p.value)}
                                ?disabled=${options.disable} .checked=${value === p.value}
                                @click=${e => handleRadioChange(e, key, options.untoggle && value === p.value)}/>
                         <label for=${`${id}.${i}`}>${p.label}</label><br/>`)}
            </div>
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, 'enum', label, value, value == null);

        return intf;
    };

    function handleMultiChange(e, key) {
        let els = e.target.parentNode.querySelectorAll('input');

        let nullify = (e.target.checked && e.target.value === 'null');
        let value = [];
        for (let el of els) {
            if ((el.value === 'null') != nullify)
                el.checked = false;
            if (el.checked)
                value.push(parseValue(el.value));
        }
        state.values[key] = value;
        state.missing_errors.delete(key);

        self.changeHandler(self);
    }

    this.multi = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let id = makeID(key);
        let value;
        {
            let candidate = state.values.hasOwnProperty(key) ? state.values[key] : options.value;
            value = Array.isArray(candidate) ? candidate : [];
        }

        let render = intf => renderWrappedWidget(intf, html`
            <label>${label || key}</label>
            <div class="af_multi" id=${id}>
                ${props.map((p, idx) =>
                    html`<input type="checkbox" id=${`${id}.${idx}`} value=${stringifyValue(p.value)}
                                ?disabled=${options.disable} .checked=${value.includes(p.value)}
                                @click=${e => handleMultiChange(e, key)}/>
                         <label for=${`${id}.${idx}`}>${p.label}</label><br/>`)}
            </div>
        `);

        let intf = addWidget(render, options);
        let missing = !value.length && props.some(p => p.value == null);
        fillVariableInfo(intf, key, 'multi', label, value, missing);

        return intf;
    };

    this.calc = function(key, label, value, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let id = makeID(key);

        let text = value;
        if (!options.raw && typeof value !== 'string') {
            if (value == null || Number.isNaN(value)) {
                text = '';
            } else if (isFinite(value)) {
                // This is a garbage way to round numbers
                let multiplicator = Math.pow(10, 2);
                let n = parseFloat((value * multiplicator).toFixed(11));
                text = Math.round(n) / multiplicator;
            }
        }

        let render = intf => renderWrappedWidget(intf, html`
            <label for=${id}>${label || key}</label>
            <span>${text}</span>
        `);

        let intf = addWidget(render, options);
        let type = options.type || 'string';
        fillVariableInfo(intf, key, type, label, value);

        return intf;
    };

    this.output = function(content, options = {}) {
        options = expandOptions(options);

        // Don't output function content, helps avoid garbage output when the
        // user types 'page.oupt(html);'.
        if (!content || typeof content === 'function')
            return;

        let render = intf => renderWrappedWidget(intf, content);

        addWidget(render, options);
    };

    function handleSectionClick(e, label) {
        state.sections_state[label] = !state.sections_state[label];
        self.changeHandler(self);
    }

    this.section = function(label, func, options = {}) {
        options = expandOptions(options);

        if (!func)
            throw new Error(`Section call must contain a function.

Make sure you did not use this syntax by accident:
    f.section("Title"), () => { /* Do stuff here */ };
instead of:
    f.section("Title", () => { /* Do stuff here */ });`);

        let widgets = [];
        let prev_widgets = widgets_ref;

        widgets_ref = widgets;
        func();
        widgets_ref = prev_widgets;

        let deploy = state.sections_state[label];
        if (deploy == null) {
            state.sections_state[label] = options.deploy;
            deploy = options.deploy;
        }

        let render = intf => html`
            <fieldset class="af_section">
                ${label ? html`<legend @click=${e => handleSectionClick(e, label)}>${label}</legend>` : html``}
                ${deploy ?
                    widgets.map(intf => intf.render(intf)) :
                    html`<a class="af_deploy" href="#"
                            @click=${e => { handleSectionClick(e, label); e.preventDefault(); }}>(ouvrir la section)</a>`}
            </fieldset>
        `;

        addWidget(render);
    };

    this.buttons = function(buttons, options = {}) {
        options = expandOptions(options);

        if (typeof buttons === 'string') {
            let type = buttons;
            let func = self.buttons.std[type];
            if (!func)
                throw new Error(`Standard button list '${type}' does not exist.

Valid choices include:
    ${Object.keys(self.buttons.std).join(', ')}`);

            buttons = func();
        }

        let render = intf => renderWrappedWidget(intf, html`
            <div class="af_buttons">
                ${buttons.map(button =>
                    html`<button class="af_button" ?disabled=${!button[1]} title=${button[2] || ''}
                                 @click=${button[1]}>${button[0]}</button>`)}
            </div>
        `);

        addWidget(render);
    };
    this.buttons.std = {
        save: (label, options = {}) => {
            let problems = self.validateHandler(self);

            return [
                [label || 'Enregistrer', self.submitHandler && !problems.length ? self.submit : null, problems.join('\n')]
            ];
        },
        ok_cancel: (label, options = {}) => {
            let problems = self.validateHandler(self);
            return [
                [label || 'OK', self.submitHandler && !problems.length ? self.submit : null, problems.join('\n')],
                ['Annuler', self.close]
            ];
        }
    };

    this.errorList = function(options = {}) {
        options = expandOptions(options);

        let render = intf => {
            if (self.errors.length || options.force) {
                return html`
                    <fieldset class="af_section af_section_error">
                        <legend>${options.label || 'Liste des erreurs'}</legend>
                        ${!self.errors.length ? 'Aucune erreur' : html``}
                        ${self.errors.map(intf =>
                            html`${intf.errors.length} ${intf.errors.length > 1 ? 'erreurs' : 'erreur'} sur :
                                 <a href=${'#' + makeID(intf.key)}>${intf.label}</a><br/>`)}
                    </fieldset>
                `;
            } else {
                return html``;
            }
        };

        addWidget(render);
    };

    this.submit = function() {
        if (self.submitHandler && self.isValid()) {
            if (missing_set.size) {
                log.error('Impossible d\'enregistrer : données manquantes');

                state.missing_errors.clear();
                for (let key of missing_set)
                    state.missing_errors.add(key);

                self.changeHandler(self);
                return;
            }

            self.submitHandler(self, state.values);
        }
    };
}
FormBuilder.current_unique_key = 0;
