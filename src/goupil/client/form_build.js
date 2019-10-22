// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormBuilder(pages) {
    let self = this;

    let keys_set = new Set;

    this.page = function(key) {
        if (!key)
            throw new Error('Empty keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');
        if (keys_set.has(key))
            throw new Error(`Page '${key}' is already used in this form`);

        let page = {
            key: key
        };

        pages.push(page);
        keys_set.add(key);
    };
}

function PageBuilder(data, widgets, variables = []) {
    let self = this;

    // Prevent DOM ID conflicts
    let unique_key = ++PageBuilder.current_unique_key;

    let variables_map = {};
    let widgets_ref = widgets;
    let options_stack = [{
        deploy: true,
        untoggle: true
    }];

    let missing_set = new Set;
    let missing_block = false;

    this.errors = [];

    // Key and value handling
    this.decodeKey = function(key) {
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
    this.setValue = function(key, value) { data.values[key] = value; };
    this.getValue = function(key, default_value) {
        if (data.values.hasOwnProperty(key)) {
            return data.values[key];
        } else {
            return default_value;
        }
    };

    // Change and submission handling
    this.changeHandler = page => {};
    this.submitHandler = null;

    this.isValid = function() { return !listProblems().length; };

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
    this.value = key => {
        let intf = variables_map[key];
        return intf ? intf.value : undefined;
    };
    this.missing = key => variables_map[key].missing;
    this.error = (key, msg) => variables_map[key].error(msg);

    this.text = function(key, label, options = {}) {
        key = self.decodeKey(key);
        options = expandOptions(options);

        let value = self.getValue(key, options.value);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="text" size="${options.size || 30}" .value=${value || ''}
                   placeholder=${options.placeholder || ''} ?disabled=${options.disable}
                   @input=${e => handleTextInput(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, label, value, value == null);

        return intf;
    };

    this.password = function(key, label, options = {}) {
        key = self.decodeKey(key);
        options = expandOptions(options);

        let value = self.getValue(key, options.value);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="password" size="${options.size || 30}" .value=${value || ''}
                   ?disabled=${options.disable} @input=${e => handleTextInput(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, label, value, value == null);

        return intf;
    };

    function handleTextInput(e, key) {
        self.setValue(key, e.target.value || null);
        data.missing_errors.delete(key.toString());

        self.changeHandler(self);
    }

    this.number = function(key, label, options = {}) {
        key = self.decodeKey(key);
        options = expandOptions(options);

        let value = parseFloat(self.getValue(key, options.value));

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="number" .value=${value}
                   step=${1 / Math.pow(10, options.decimals || 0)}
                   placeholder=${options.placeholder || ''} ?disabled=${options.disable}
                   @input=${e => handleNumberChange(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, label, value, Number.isNaN(value));

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

    function handleNumberChange(e, key) {
        // Hack to accept incomplete values, mainly in the case of a '-' being typed first,
        // in which case we don't want to clear the field immediately.
        if (e.target.validity.valid) {
            self.setValue(key, parseFloat(e.target.value));
            data.missing_errors.delete(key.toString());

            self.changeHandler(self);
        }
    }

    this.dropdown = function(key, label, props = [], options = {}) {
        key = self.decodeKey(key);
        options = expandOptions(options);
        props = normalizePropositions(props);

        let value = self.getValue(key, options.value);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <select id=${id} ?disabled=${options.disable}
                    @change=${e => handleDropdownChange(e, key)}>
                ${options.untoggle || !props.some(p => p != null && value === p.value) ?
                    html`<option value="null" .selected=${value == null}>-- Choisissez une option --</option>` : ''}
                ${props.map(p =>
                    html`<option value=${util.valueToStr(p.value)} .selected=${value === p.value}>${p.label}</option>`)}
            </select>
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, label, value, value == null);

        return intf;
    };

    function handleDropdownChange(e, key) {
        self.setValue(key, util.strToValue(e.target.value));
        data.missing_errors.delete(key.toString());

        self.changeHandler(self);
    }

    this.choice = function(key, label, props = [], options = {}) {
        key = self.decodeKey(key);
        options = expandOptions(options);
        props = normalizePropositions(props);

        let value = self.getValue(key, options.value);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class="af_select" id=${id}>
                ${props.map(p =>
                    html`<button data-value=${util.valueToStr(p.value)}
                                 ?disabled=${options.disable} .className=${value === p.value ? 'af_button active' : 'af_button'}
                                 @click=${e => handleChoiceChange(e, key, options.untoggle)}>${p.label}</button>`)}
            </div>
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, label, value, value == null);

        return intf;
    };

    function handleChoiceChange(e, key, allow_untoggle) {
        let json = e.target.dataset.value;

        if (e.target.classList.contains('active') && allow_untoggle) {
            self.setValue(key, null);
        } else {
            self.setValue(key, util.strToValue(json));
        }
        data.missing_errors.delete(key.toString());

        // This is useless in most cases because the new form will incorporate
        // this change, but who knows. Do it like other browser-native widgets.
        let els = e.target.parentNode.querySelectorAll('button');
        for (let el of els)
            el.classList.toggle('active', el.dataset.value === json &&
                                          (!el.classList.contains('active') || !allow_untoggle));

        self.changeHandler(self);
    }

    this.binary = function(key, label, options = {}) {
        return self.choice(key, label, [[1, 'Oui'], [0, 'Non']], options);
    };
    this.boolean = function(key, label, options = {}) {
        return self.choice(key, label, [[true, 'Oui'], [false, 'Non']], options);
    };

    this.radio = function(key, label, props = [], options = {}) {
        key = self.decodeKey(key);
        options = expandOptions(options);
        props = normalizePropositions(props);

        let value = self.getValue(key, options.value);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class="af_radio" id=${id}>
                ${props.map((p, i) =>
                    html`<input type="radio" name=${id} id=${`${id}.${i}`} value=${util.valueToStr(p.value)}
                                ?disabled=${options.disable} .checked=${value === p.value}
                                @click=${e => handleRadioChange(e, key, options.untoggle && value === p.value)}/>
                         <label for=${`${id}.${i}`}>${p.label}</label><br/>`)}
            </div>
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, label, value, value == null);

        return intf;
    };

    function handleRadioChange(e, key, already_checked) {
        if (already_checked) {
            e.target.checked = false;
            self.setValue(key, null);
        } else {
            self.setValue(key, util.strToValue(e.target.value));
        }
        data.missing_errors.delete(key.toString());

        self.changeHandler(self);
    }

    this.multi = function(key, label, props = [], options = {}) {
        key = self.decodeKey(key);
        options = expandOptions(options);
        props = normalizePropositions(props);

        let value = self.getValue(key, options.value);
        if (!Array.isArray(value))
            value = [];

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class="af_multi" id=${id}>
                ${props.map((p, idx) =>
                    html`<input type="checkbox" id=${`${id}.${idx}`} value=${util.valueToStr(p.value)}
                                ?disabled=${options.disable} .checked=${value.includes(p.value)}
                                @click=${e => handleMultiChange(e, key)}/>
                         <label for=${`${id}.${idx}`}>${p.label}</label><br/>`)}
            </div>
        `);

        let intf = addWidget(render, options);
        let missing = !value.length && props.some(p => p.value == null);
        fillVariableInfo(intf, key, label, value, missing);

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
                value.push(util.strToValue(el.value));
        }

        self.setValue(key, value);
        data.missing_errors.delete(key.toString());

        self.changeHandler(self);
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

    this.file = function(key, label, options = {}) {
        key = self.decodeKey(key);
        options = expandOptions(options);

        let value = self.getValue(key, options.value);
        if (!(value instanceof File))
            value = null;

        // Setting files on input file elements is fragile. At least on Firefox, assigning
        // its own value to the property results in an empty FileList for some reason.
        let set_files = lithtml.directive(() => part => {
            let file_list = data.file_lists.get(key);

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
                   placeholder=${options.placeholder || ''} ?disabled=${options.disable}
                   @input=${e => handleFileInput(e, key)}/>
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, label, value, value == null);

        return intf;
    };

    function handleFileInput(e, key) {
        self.setValue(key, e.target.files[0] || null);
        data.missing_errors.delete(key.toString());
        data.file_lists.set(key, e.target.files);

        self.changeHandler(self);
    }

    this.calc = function(key, label, value, options = {}) {
        key = self.decodeKey(key);
        options = expandOptions(options);

        self.setValue(key, value);

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

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            <label for=${id}>${label || key}</label>
            <span id="${id}" class="af_calc">${text}</span>
        `);

        let intf = addWidget(render, options);
        fillVariableInfo(intf, key, label, value, value == null || Number.isNaN(value));

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

        let deploy = data.sections_state[label];
        if (deploy == null) {
            data.sections_state[label] = options.deploy;
            deploy = options.deploy;
        }

        let render = intf => html`
            <fieldset class="af_section">
                ${label != null ? html`<legend @click=${e => handleSectionClick(e, label)}>${label}</legend>` : ''}
                ${deploy ?
                    widgets.map(intf => intf.render(intf)) :
                    html`<a class="af_deploy" href="#"
                            @click=${e => { handleSectionClick(e, label); e.preventDefault(); }}>(ouvrir la section)</a>`}
            </fieldset>
        `;

        addWidget(render);
    };

    function handleSectionClick(e, label) {
        data.sections_state[label] = !data.sections_state[label];
        self.changeHandler(self);
    }

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
            let problems = listProblems();

            return [
                [label || 'Enregistrer', self.submitHandler && !problems.length ? self.submit : null, problems.join('\n')]
            ];
        },
        ok_cancel: (label, options = {}) => {
            let problems = listProblems();

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
                        ${!self.errors.length ? 'Aucune erreur' : ''}
                        ${self.errors.map(intf =>
                            html`${intf.errors.length} ${intf.errors.length > 1 ? 'erreurs' : 'erreur'} sur :
                                 <a href=${'#' + makeID(intf.key)}>${intf.label}</a><br/>`)}
                    </fieldset>
                `;
            } else {
                return '';
            }
        };

        addWidget(render);
    };

    this.submit = function() {
        if (self.submitHandler && self.isValid()) {
            if (missing_set.size) {
                log.error('Impossible d\'enregistrer : données manquantes');

                data.missing_errors.clear();
                for (let key of missing_set)
                    data.missing_errors.add(key);

                self.changeHandler(self);
                return;
            }

            self.submitHandler.call(self, data.values, variables);
        }
    };

    function expandOptions(options) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);
        return options;
    }

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
                    html`<div class="af_error">${intf.errors.map(err => html`${err}<br/>`)}</div>` : ''}
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

    function fillVariableInfo(intf, key, label, value, missing) {
        if (variables_map[key])
            throw new Error(`Variable '${key}' already exists`);

        Object.assign(intf, {
            key: key,
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
            missing_set.add(key.toString());
            if (intf.options.missingMode === 'error' || data.missing_errors.has(key.toString()))
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
            return '';
        }
    }

    function listProblems() {
        let problems = [];

        if (missing_block)
            problems.push('Informations obligatoires manquantes');
        if (self.errors.length)
            problems.push('Présence d\'erreurs sur le formulaire');

        return problems;
    }
}
PageBuilder.current_unique_key = 0;
