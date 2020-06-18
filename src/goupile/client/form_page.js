// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function Page(key) {
    let self = this;

    this.key = key;

    this.widgets = [];
    this.widgets0 = [];
    this.variables = [];

    this.errors = 0;
    this.valid = true;

    this.render = function() { return self.widgets0.map(intf => intf.render()); };
}

function PageState() {
    this.values = {};

    // Use this to track changes!
    this.changed = false;

    this.sections_state = {};
    this.tabs_state = {};
    this.file_lists = new Map;
    this.pressed_buttons = new Set;
    this.clicked_buttons = new Set;

    this.take_delayed = new Set;
    this.changed_variables = new Set;

    // Used for "real forms" by FormExecutor
    this.scratch = {};
}

function PageBuilder(state, page) {
    let self = this;

    let variables_map = {};
    let options_stack = [{
        deploy: true,
        untoggle: true
    }];
    let widgets_ref = page.widgets0;

    let tabs_keys = new Set;
    let tabs_ref;
    let selected_tab;

    let restart = false;

    // Key and value handling
    this.decodeKey = key => key;
    this.setValue = (key, value) => {};
    this.getValue = (key, default_value) => default_value;

    // Change and submission handling
    this.changeHandler = page => {};
    this.submitHandler = null;

    this.hasChanged = function() { return state.changed; };
    this.isValid = function() { return page.valid; };
    this.hasErrors = function() { return !!page.errors; };

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

        let value = readValue(key, options.value);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="text" class="af_input" style=${makeInputStyle(options)}
                   placeholder=${options.placeholder || ''}
                   .value=${value || ''} ?disabled=${options.disable}
                   @input=${e => handleTextInput(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget('text', label, render, options);
        fillVariableInfo(intf, key, value, value == null);

        return intf;
    };

    this.password = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options.value);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="password" class="af_input" style=${makeInputStyle(options)}
                   .value=${value || ''} ?disabled=${options.disable}
                   @input=${e => handleTextInput(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget('password', label, render, options);
        fillVariableInfo(intf, key, value, value == null);

        return intf;
    };

    this.pin = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options.value);
        if (value != null)
            value = '' + value;

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="text" class="af_input" style=${makeInputStyle(options)}
                   inputmode="none" .value=${value || ''}
                   placeholder=${options.placeholder || ''} ?disabled=${options.disable}
                   @input=${e => handleTextInput(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
            <div class="af_pin">
                ${[7, 8, 9, 4, 5, 6, 1, 2, 3].map(i =>
                    html`<button type="button" class="af_button"
                                 @click=${e => handlePinButton(e, key, value)}>${i}</button>`)}
                <button type="button" class="af_button" style="visibility: hidden;">?</button>
                <button type="button" class="af_button" @click=${e => handlePinButton(e, key, value)}>0</button>
                <button type="button" class="af_button clear" @click=${e => handlePinClear(e, key)}>C</button>
            </div>
        `);

        let intf = addWidget('pin', label, render, options);
        fillVariableInfo(intf, key, value, value == null);

        if (value && !value.match(/^[0-9]*$/))
            intf.error('Le code doit comporter uniquement des chiffres');

        return intf;
    };

    function handlePinButton(e, key, value) {
        updateValue(key, (value || '') + e.target.textContent);
        self.restart();
    }

    function handlePinClear(e, key) {
        updateValue(key, undefined);
        self.restart();
    }

    function handleTextInput(e, key) {
        updateValue(key, e.target.value || undefined);
        self.restart();
    }

    this.number = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = parseFloat(readValue(key, options.value));
        let missing = (value == null || Number.isNaN(value));

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="number" class="af_input" style=${makeInputStyle(options)}
                   step=${1 / Math.pow(10, options.decimals || 0)} .value=${value}
                   placeholder=${options.placeholder || ''} ?disabled=${options.disable}
                   @input=${e => handleNumberChange(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget('number', label, render, options);
        fillVariableInfo(intf, key, value, missing);

        validateNumber(intf);

        return intf;
    };

    this.slider = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        // Default options
        options.decimals = options.decimals || 0;
        options.min = options.min || 0;
        options.max = (options.max != null) ? options.max : 10;

        let range = options.max - options.min;
        if (range <= 0)
            throw new Error('Range (options.max - options.min) must be positive');

        let value = parseFloat(readValue(key, options.value));
        let missing = (value == null || Number.isNaN(value));

        // Used for rendering value box
        let thumb_value = !missing ? util.clamp(value, options.min, options.max)
                                   : ((options.min + options.max) / 2);
        let thumb_pos = (thumb_value - options.min) / range;

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class=${missing ? 'af_slider missing' : 'af_slider'} style=${makeInputStyle(options)}>
                <div>
                    <span style=${`left: ${thumb_pos * 100}%;${options.untoggle ? ' cursor: pointer;': ''}`}
                          @click=${e => handleSliderClick(e, key, value, options.min, options.max)}>${value.toFixed(options.decimals)}</span>
                </div>
                <input id=${id} type="range"
                       min=${options.min} max=${options.max} step=${1 / Math.pow(10, options.decimals)}
                       .value=${!missing ? value : ((options.max + options.min) / 2)}
                       placeholder=${options.placeholder || ''} ?disabled=${options.disable}
                       @click=${e => { e.target.value = thumb_value; handleNumberChange(e, key); }}
                       @dblclick=${e => handleSliderClick(e, key, value, options.min, options.max)}
                       @input=${e => handleNumberChange(e, key)}/>
            </div>
        `);

        let intf = addWidget('slider', label, render, options);
        fillVariableInfo(intf, key, value, missing);

        validateNumber(intf);

        return intf;
    }

    function handleNumberChange(e, key) {
        // Hack to accept incomplete values, mainly in the case of a '-' being typed first,
        // in which case we don't want to clear the field immediately.
        if (!e.target.validity || e.target.validity.valid) {
            let value = parseFloat(e.target.value);
            if (Number.isNaN(value))
                value = undefined;

            updateValue(key, value);
            self.restart();
        }
    }

    function handleSliderClick(e, key, value, min, max) {
        ui.popup(e, page => {
            let number = page.number('number', 'Valeur :', {min: min, max: max, value: value});

            page.submitHandler = () => {
                updateValue(key, number.value);

                self.restart();
                page.close();
            };
            page.buttons(page.buttons.std.ok_cancel('Modifier'));
        });
    }

    function validateNumber(intf) {
        let value = intf.value;
        let options = intf.options;

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
    }

    this.enum = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options.value);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class="af_select" id=${id}>
                ${props.map(p =>
                    html`<button type="button" data-value=${util.valueToStr(p.value)}
                                 ?disabled=${options.disable} .className=${value === p.value ? 'af_button active' : 'af_button'}
                                 @click=${e => handleEnumChange(e, key, options.untoggle)}>${p.label}</button>`)}
            </div>
        `);

        let intf = addWidget('enum', label, render, options);
        fillVariableInfo(intf, key, value, value == null);

        return intf;
    };

    function handleEnumChange(e, key, allow_untoggle) {
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

        let value = readValue(key, options.value);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <select id=${id} style=${makeInputStyle(options)}
                    ?disabled=${options.disable} @change=${e => handleEnumDropChange(e, key)}>
                ${options.untoggle || !props.some(p => p != null && value === p.value) ?
                    html`<option value="undefined" .selected=${value == null}>-- Choisissez une option --</option>` : ''}
                ${props.map(p =>
                    html`<option value=${util.valueToStr(p.value)} .selected=${value === p.value}>${p.label}</option>`)}
            </select>
        `);

        let intf = addWidget('enumDrop', label, render, options);
        fillVariableInfo(intf, key, value, value == null);

        return intf;
    };

    function handleEnumDropChange(e, key) {
        updateValue(key, util.strToValue(e.target.value));
    }

    this.enumRadio = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options.value);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class="af_radio" id=${id}>
                ${props.map((p, i) =>
                    html`<input type="radio" name=${id} id=${`${id}.${i}`} value=${util.valueToStr(p.value)}
                                ?disabled=${options.disable} .checked=${value === p.value}
                                @click=${e => handleEnumRadioChange(e, key, options.untoggle && value === p.value)}/>
                         <label for=${`${id}.${i}`}>${p.label}</label><br/>`)}
            </div>
        `);

        let intf = addWidget('enumRadio', label, render, options);
        fillVariableInfo(intf, key, value, value == null);

        return intf;
    };

    function handleEnumRadioChange(e, key, already_checked) {
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

        let value = readValue(key, options.value);
        if (!Array.isArray(value))
            value = [];

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class="af_select" id=${id}>
                ${props.map(p =>
                    html`<button type="button" data-value=${util.valueToStr(p.value)}
                                 ?disabled=${options.disable} .className=${value.includes(p.value) ? 'af_button active' : 'af_button'}
                                 @click=${e => handleMultiChange(e, key)}>${p.label}</button>`)}
            </div>
        `);

        let intf = addWidget('multi', label, render, options);
        let missing = !value.length && props.some(p => p.value == null);
        fillVariableInfo(intf, key, value, missing);

        return intf;
    };

    function handleMultiChange(e, key) {
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

    this.multiCheck = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options.value);
        if (!Array.isArray(value))
            value = [];

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            <div class="af_multi" id=${id}>
                ${props.map((p, idx) =>
                    html`<input type="checkbox" id=${`${id}.${idx}`} value=${util.valueToStr(p.value)}
                                ?disabled=${options.disable} .checked=${value.includes(p.value)}
                                @click=${e => handleMultiCheckChange(e, key)}/>
                         <label for=${`${id}.${idx}`}>${p.label}</label><br/>`)}
            </div>
        `);

        let intf = addWidget('multiCheck', label, render, options);
        let missing = !value.length && props.some(p => p.value == null);
        fillVariableInfo(intf, key, value, missing);

        return intf;
    };

    function handleMultiCheckChange(e, key) {
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

        let value = readValue(key, options.value);
        if (typeof value === 'string') {
            value = dates.parse(value);
        } else if (value == null || value.constructor.name !== 'LocalDate') {
            value = null;
        }

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="date" class="af_input" style=${makeInputStyle(options)}
                   .value=${value ? value.toString() : ''} ?disabled=${options.disable}
                   @input=${e => handleDateTimeInput(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget('date', label, render, options);
        fillVariableInfo(intf, key, value, value == null);

        return intf;
    };

    this.time = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options.value);
        if (typeof value === 'string') {
            value = times.parse(value);
        } else if (value == null || value.constructor.name !== 'LocalTime') {
            value = null;
        }

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<label for=${id}>${label}</label>` : ''}
            ${makePrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="time" step class="af_input" style=${makeInputStyle(options)}
                   .value=${value ? value.toString().substr(0, options.seconds ? 8 : 5) : ''}
                   step=${options.seconds ? 1 : 60} ?disabled=${options.disable}
                   @input=${e => handleDateTimeInput(e, key)}/>
            ${makePrefixOrSuffix('af_suffix', options.suffix, value)}
        `);

        let intf = addWidget('date', label, render, options);
        fillVariableInfo(intf, key, value, value == null);

        return intf;
    };

    function handleDateTimeInput(e, key) {
        // Store as string, for serialization purposes
        updateValue(key, e.target.value || undefined);
    }

    this.file = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options.value);
        if (!(value instanceof File))
            value = null;

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
                   placeholder=${options.placeholder || ''} ?disabled=${options.disable}
                   @input=${e => handleFileInput(e, key)}/>
        `);

        let intf = addWidget('file', label, render, options);
        fillVariableInfo(intf, key, value, value == null);

        return intf;
    };

    function handleFileInput(e, key) {
        state.file_lists.set(key.toString(), e.target.files);
        updateValue(key, e.target.files[0] || undefined);
    }

    this.calc = function(key, label, value, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        if (value !== state.values[key])
            updateValue(key, value, false);

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

        let intf = addWidget('calc', label, render, options);
        fillVariableInfo(intf, key, value, value == null || Number.isNaN(value));

        return intf;
    };

    this.output = function(content, options = {}) {
        options = expandOptions(options);

        // Don't output function content, helps avoid garbage output when the
        // user types 'page.oupt(html);'.
        if (!content || typeof content === 'function')
            return;

        let render = intf => html`<div class="af_wrap">${content}</div>`;

        return addWidget('output', null, render, options);
    };

    this.scope = function(func) {
        let widgets = captureWidgets('section', func);
        widgets_ref.push(...widgets);
    };

    this.section = function(label, func, options = {}) {
        options = expandOptions(options);

        let deploy = state.sections_state[label];
        if (deploy == null) {
            deploy = options.deploy;
            state.sections_state[label] = deploy;
        }

        let widgets = captureWidgets('section', func);

        let render = intf => html`
            <fieldset class="af_container af_section">
                ${label != null ? html`<legend @click=${e => handleSectionClick(e, label)}>${label}</legend>` : ''}
                ${deploy ?
                    widgets.map(intf => intf.render()) :
                    html`<a class="af_deploy" href="#"
                            @click=${e => { handleSectionClick(e, label); e.preventDefault(); }}>(ouvrir la section)</a>`}
            </fieldset>
        `;

        return addWidget('section', label, render);
    };

    function handleSectionClick(e, label) {
        state.sections_state[label] = !state.sections_state[label];
        self.restart();
    }

    this.tabs = function(key, func, options = {}) {
        options = expandOptions(options);

        if (!key)
            throw new Error('Key parameter must be specified');
        if (tabs_keys.has(key))
            throw new Error(`Tabs key '${key}' is already used`);
        tabs_keys.add(key);

        let tabs = [];
        let tab_idx;
        let widgets;
        {
            let prev_tabs = tabs_ref;
            let prev_select = selected_tab;

            tabs_ref = tabs;
            selected_tab = (state.tabs_state.hasOwnProperty(key) ? state.tabs_state[key] : options.tab) || 0;
            widgets = captureWidgets('tabs', func);

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

        let render = intf => html`
            <div class="af_container af_tabs">
                ${tabs.map((tab, idx) => {
                    let cls = 'af_button';
                    if (idx === tab_idx)
                        cls += ' active';
                    //if (tab.disable)
                      //  cls += ' disabled';

                    return html`<button type="button" class=${cls} ?disabled=${tab.disable}
                                        @click=${e => handleTabClick(e, key, idx)}>${tab.label}</button>`;
                })}

                <div class="af_section">
                    ${widgets.map(intf => intf.render())}
                </div>
            </div>
        `;

        return addWidget('tabs', null, render, options);
    };

    this.tab = function(label, options = {}) {
        options = expandOptions(options);

        if (!tabs_ref)
            throw new Error('form.tab must be called from inside form.tabs');
        if (!label)
            throw new Error('Label must be specified');
        if (tabs_ref.find(tab => tab.label === label))
            throw new Error(`Tab label '${label}' is already used`);

        let active;
        if (selected_tab === tabs_ref.length || selected_tab === label) {
            if (!options.disable) {
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
            disable: options.disable
        };
        tabs_ref.push(tab);

        return active;
    };

    function handleTabClick(e, key, label) {
        state.tabs_state[key] = label;
        self.restart();
    }

    function captureWidgets(type, func) {
        let widgets = [];

        if (!func) {
            throw new Error(`This call does not contain a function.

Make sure you did not use this syntax by accident:
    page.${type}("Title"), () => { /* Do stuff here */ };
instead of:
    page.${type}("Title", () => { /* Do stuff here */ });`);
        }

        let prev_widgets = widgets_ref;
        let prev_options = options_stack;

        try {
            widgets_ref = widgets;
            options_stack = [options_stack[options_stack.length - 1]];

            func();
        } finally {
            options_stack = prev_options;
            widgets_ref = prev_widgets;
        }

        return widgets;
    }

    this.button = function(label, options = {}) {
        options = expandOptions(options);

        let pressed = state.pressed_buttons.has(label);
        let clicked = state.clicked_buttons.has(label);
        state.clicked_buttons.delete(label);

        let render = intf => renderWrappedWidget(intf, html`
            <button type="button" class="af_button"
                    @click=${e => handleButtonClick(e, label)}>${label}</button>
        `);

        let intf = addWidget('button', label, render, options);
        intf.pressed = pressed;
        intf.clicked = clicked;

        return intf;
    };

    function handleButtonClick(e, label) {
        state.pressed_buttons.add(label);
        state.clicked_buttons.add(label);

        self.restart();
    }

    // XXX: Get rid of this (used in popups)
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
                ${buttons.map((button, idx) =>
                    html`<button type=${idx ? 'button' : 'submit'} class="af_button"
                                 ?disabled=${!button[1]} title=${button[2] || ''}
                                 @click=${button[1]}>${button[0]}</button>`)}
            </div>
        `);

        addWidget('buttons', null, render);
    };
    this.buttons.std = {
        save: (label, options = {}) => [
            [label || 'Enregistrer', self.isValid() ? self.submit : null,
                                     !self.isValid() ? 'Erreurs ou données obligatoires manquantes' : null]
        ],
        ok_cancel: (label, options = {}) => [
            [label || 'OK', self.isValid() ? self.submit : null,
                            !self.isValid() ? 'Erreurs ou données obligatoires manquantes' : null],
            ['Annuler', self.close]
        ]
    };

    this.errorList = function(options = {}) {
        options = expandOptions(options);

        let render = intf => {
            if (self.hasErrors() || options.force) {
                return html`
                    <fieldset class="af_container af_section af_section_error">
                        <legend>${options.label || 'Liste des erreurs'}</legend>
                        ${!self.hasErrors() ? 'Aucune erreur' : ''}
                        ${page.widgets.map(intf => {
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

        addWidget('errorList', null, render);
    };

    this.submit = async function() {
        if (!self.isValid()) {
            log.error('Corrigez les erreurs avant d\'enregistrer');

            state.take_delayed = new Set(page.variables.map(variable => variable.key.toString()));
            self.restart();

            return;
        }

        await self.submitHandler();
    };

    this.restart = function() {
        if (!restart) {
            setTimeout(() => self.changeHandler(self), 0);
            restart = true;
        }
    };

    function decodeKey(key, options) {
        if (typeof key === 'string' && key[0] === '*') {
            key = key.substr(1);
            options.mandatory = true;
        }

        return self.decodeKey(key);
    }

    function expandOptions(options) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);
        return options;
    }

    function makeID(key) {
        return `af_var_${page.key}_${key}`;
    }

    function renderWrappedWidget(intf, frag) {
        if (!intf.options.hidden) {
            let cls = 'af_widget';
            if (intf.errors.length)
                cls += ' error';
            if (intf.options.disable)
                cls += ' disable';
            if (intf.options.mandatory)
                cls += ' mandatory';
            if (intf.options.compact)
                cls += ' compact';

            return html`
                <div class="af_wrap">
                    <div class=${cls}>
                        ${frag}
                        ${intf.errors.length ?
                            html`<div class="af_error">${intf.errors.map(err => html`${err}<br/>`)}</div>` : ''}
                    </div>
                    ${intf.options.help ? html`<p class="af_help">${intf.options.help}</p>` : ''}
                </div>
            `;
        } else {
            return '';
        }
    }

    function makeInputStyle(options) {
        return options.wide ? 'width: 100%;' : '';
    }

    function addWidget(type, label, render, options = {}) {
        let intf = {
            type: type,
            label: label,
            render: () => {
                intf.render = () => '';
                return render(intf);
            },
            options: options,
            errors: []
        };

        widgets_ref.push(intf);
        page.widgets.push(intf);

        return intf;
    }

    function fillVariableInfo(intf, key, value, missing) {
        if (variables_map[key])
            throw new Error(`Variable '${key}' already exists`);

        Object.assign(intf, {
            page: page.key,
            key: key,
            value: value,

            missing: missing || intf.options.missing,
            changed: state.changed_variables.has(key.toString()),

            error: (msg, delay = false) => {
                if (!delay || state.take_delayed.has(key.toString())) {
                    intf.errors.push(msg || '');
                    page.errors++;
                }

                page.valid = false;

                return intf;
            }
        });
        state.changed_variables.delete(key.toString());

        if (intf.options.mandatory && intf.missing) {
            intf.error('Donnée obligatoire manquante', intf.options.missingMode !== 'error');
            if (intf.options.missingMode === 'disable')
                valid = false;
        }

        page.variables.push(intf);
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

    function readValue(key, default_value) {
        if (state.values.hasOwnProperty(key)) {
            return state.values[key];
        } else {
            return self.getValue(key, default_value);
        }
    }

    function updateValue(key, value, refresh = true) {
        state.values[key] = value;
        state.changed = true;

        state.take_delayed.delete(key.toString());
        state.changed_variables.add(key.toString());

        self.setValue(key, value);

        if (refresh)
            self.restart();
    }
}
