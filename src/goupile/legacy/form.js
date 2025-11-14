// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2025 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, svg,
         directive, Directive, noChange, nothing } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, Mutex, LocalDate, LocalTime } from 'lib/web/base/base.js';
import { profile } from '../client/goupile.js';
import * as UI from '../client/ui.js';

function FormState(values = {}) {
    let self = this;

    this.changeHandler = model => {};

    // Mostly internal state
    this.unique_id = FormState.next_unique_id++;
    this.file_lists = new Map;
    this.click_events = new Set;
    this.invalid_numbers = new Set;
    this.take_delayed = new Set;
    this.follow_default = new Set;
    this.just_changed = new Set;
    this.obj_caches = new WeakMap;
    this.explicitly_changed = false;

    // Semi-public UI state
    this.just_triggered = false;
    this.state_tabs = {};
    this.state_sections = {};

    let proxies = new WeakMap;
    let changes = new Set;

    // Stored values
    this.values = proxyObject(values);

    this.hasChanged = function() { return !!changes.size && self.explicitly_changed; };
    this.markInteraction = function() { self.explicitly_changed = true; };

    function proxyObject(obj) {
        if (obj == null)
            return undefined;
        if (typeof obj !== 'object')
            return obj;
        if (Object.prototype.toString.call(obj) !== '[object Object]') // Don't mess with "special" objects
            return obj;

        let proxy = proxies.get(obj);

        if (proxy == null) {
            let obj_cache = {};
            let obj_changes = new Set;

            proxy = new Proxy(obj, {
                set: function(obj, key, value) {
                    value = proxyObject(value);
                    obj[key] = value;

                    let json1 = JSON.stringify(obj_cache[key]) || '{}';
                    let json2 = JSON.stringify(value) || '{}';

                    if (json2 !== json1) {
                        obj_changes.add(key);
                    } else {
                        obj_changes.delete(key);
                    }

                    if (obj_changes.size) {
                        changes.add(obj);
                    } else {
                        changes.delete(obj);
                    }

                    return true;
                }
            });

            for (let key in obj) {
                let value = proxyObject(obj[key]);

                obj[key] = value;
                obj_cache[key] = value;
            }

            proxies.set(proxy, proxy);
            self.obj_caches.set(proxy, obj_cache);
        }

        return proxy;
    }
}
FormState.next_unique_id = 0;

function FormModel() {
    let self = this;

    this.widgets = [];
    this.widgets0 = [];
    this.actions = [];
    this.variables = [];

    this.errors = 0;
    this.valid = true;
    this.triggered = false;

    this.isValid = function() { return self.valid; };
    this.hasErrors = function() { return !!self.errors; };

    this.render = function() {
        return html`
            <div class="fm_main">${self.renderWidgets()}</div>
            <div class="ui_actions" style="margin-top: 32px;">${self.renderActions()}</div>
        `;
    };
    this.renderWidgets = function() { return self.widgets0.map(intf => intf.render()); };
    this.renderActions = function() { return self.actions.map(intf => intf.render()); };
}

function FormBuilder(state, model, readonly = false) {
    let self = this;

    // Workaround for lack of some date inputs (Firefox, Safari)
    let has_input_date = (() => {
        if (typeof document !== 'undefined') {
            let el = document.createElement('input');
            el.setAttribute('type', 'date');
            el.value = '1900-01-01';
            return (el.valueAsDate != null);
        } else {
            return false;
        }
    })();
    let has_input_month = (() => {
        if (typeof document !== 'undefined') {
            let el = document.createElement('input');
            el.setAttribute('type', 'month');
            el.value = '1900-01';
            return (el.valueAsDate != null);
        } else {
            return false;
        }
    })();

    let variables_map = {};
    let options_stack = [{
        deploy: true,
        untoggle: true,
        wrap: true
    }];
    let widgets_ref = model.widgets0;

    let inline_next = false;
    let inline_widgets;

    let section_depth = 0;
    let tabs_keys = new Set;
    let tabs_ref;

    let restart = false;

    this.widgets = model.widgets;
    this.widgets0 = model.widgets0;
    this.variables = model.variables;
    this.state = state;
    this.values = state.values;

    this.hasChanged = function() { return state.hasChanged(); };
    this.markInteraction = function() { state.markInteraction(); };

    this.isValid = function() { return model.isValid(); };
    this.hasErrors = function() { return model.hasErrors(); };

    this.triggerErrors = function() {
        if (!self.isValid()) {
            state.take_delayed = new Set(model.variables.map(variable => variable.key.toString()));

            self.restart();
            state.just_triggered = true;

            throw new Error('Corrigez les erreurs avant de continuer');
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

        let value = readValue(key, options, value => (value != null) ? String(value) : undefined);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
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

        let value = readValue(key, options, value => (value != null) ? String(value) : undefined);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            <textarea id=${id} class="fm_input" style=${makeInputStyle(options)}
                   rows=${options.rows || 3} cols=${options.cols || 30}
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

        let value = readValue(key, options, value => (value != null) ? String(value) : undefined);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type="password" class="fm_input" style=${makeInputStyle(options)}
                   placeholder=${options.placeholder || ''}
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
        if (!isModifiable(variables_map[key]))
            return;

        updateValue(key, e.target.value || undefined);
    }

    this.pin = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options, value => (value != null) ? String(value) : undefined);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type="password" class="fm_input" style=${makeInputStyle(options)}
                   inputmode="none" autocomplete="new-password" .value=${value || ''}
                   placeholder=${options.placeholder || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleTextInput(e, key)} />
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
            ${!options.readonly ? html`
                <div class="fm_pin">
                    ${[7, 8, 9, 4, 5, 6, 1, 2, 3].map(i =>
                        html`<button type="button" ?disabled=${options.disabled}
                                     @click=${e => handlePinButton(e, key, value)}>${i}</button>`)}
                    <button type="button" ?disabled=${options.disabled} style="visibility: hidden;">?</button>
                    <button type="button" ?disabled=${options.disabled}
                            @click=${e => handlePinButton(e, key, value)}>0</button>
                    <button type="button" class="clear" ?disabled=${options.disabled}
                            @click=${e => handlePinClear(e, key)}>C</button>
                </div>
            `: ''}
        `);

        let intf = makeWidget('pin', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        if (value && !value.match(/^[0-9]*$/))
            intf.error('Le code doit comporter uniquement des chiffres');

        return intf;
    };

    function handlePinButton(e, key, value) {
        updateValue(key, (value || '') + e.target.textContent);
    }

    function handlePinClear(e, key) {
        updateValue(key, undefined);
    }

    this.number = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options, value => {
            value = parseFloat(value);
            if (Number.isNaN(value))
                value = undefined;
            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
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

        if (state.invalid_numbers.has(key.toString()))
            intf.error('Nombre invalide (décimales ?)');

        return intf;
    };

    function handleNumberChange(e, key) {
        if (!isModifiable(variables_map[key]))
            return;

        // Hack to accept incomplete values, mainly in the case of a '-' being typed first,
        // in which case we don't want to clear the field immediately.
        if (e.target.validity && !e.target.validity.valid) {
            state.invalid_numbers.add(key.toString());
            self.restart();
            return;
        }
        if (state.invalid_numbers.delete(key.toString()))
            self.restart();

        let value = parseFloat(e.target.value);
        if (Number.isNaN(value))
            value = undefined;
        updateValue(key, value);
    }

    this.slider = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        // Default options
        options.decimals = options.decimals || 0;
        options.min = options.min || 0;
        if (typeof options.min !== 'number')
            throw new Error('La valeur minimale (min) doit être un nombre; utilisez l\'option prefix pour du texte');
        options.max = (options.max != null) ? options.max : 10;
        if (typeof options.max !== 'number')
            throw new Error('La valeur maximale (max) doit être un nombre; utilisez l\'option suffix pour du texte');
        options.prefix = (options.prefix != null) ? options.prefix : options.min;
        options.suffix = (options.suffix != null) ? options.suffix : options.max;

        let range = options.max - options.min;
        if (range <= 0)
            throw new Error('Range (options.max - options.min) must be positive');

        let value = readValue(key, options, value => {
            value = parseFloat(value);
            if (Number.isNaN(value))
                value = undefined;
            return value;
        });

        let fix_value = (value != null) ? Util.clamp(value, options.min, options.max)
                                        : ((options.min + options.max) / 2);
        let fake_progress = (value != null) ? ((fix_value - options.min) / range) : -1;
        if (options.floating)
            fake_progress = -1;

        let ticks = [];
        if (options.ticks != null && options.ticks !== false) {
            if (options.ticks === true) {
                ticks = Array.from(Util.mapRange(options.min, options.max + 1, i => i));
            } else if (Util.isPodObject(options.ticks)) {
                for (let key in options.ticks) {
                    let pos = parseFloat(key);
                    if (Number.isNaN(pos))
                        throw new Error('La position du tiret doit être une valeur numérique');

                    let value = options.ticks[key];
                    let tick = Array.isArray(value) ? [pos, ...value] : [pos, value];

                    ticks.push(tick);
                }
            } else {
                try {
                    if (typeof options.ticks === 'function') {
                        ticks = Array.from(options.ticks());
                    } else {
                        ticks = Array.from(options.ticks);
                    }
                } catch (err) {
                    throw new Error('Option \'ticks\' must be a boolean, an object or array-like');
                }
            }
        }
        ticks = ticks.filter(tick => {
            let value = Array.isArray(tick) ? tick[0] : tick;
            return value >= options.min && value <= options.max;
        });

        // Yeah, the generated HTML is not very pretty, it is the result of trial-and-error
        // with seemingly random px and em offsets. If you want to do better, be my guest :)

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            <div class=${'fm_slider' + (value == null ? ' missing' : '') + (options.readonly ? ' readonly' : '')}
                 style=${makeInputStyle(options)}>
                ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
                <div>
                    <input id=${id} type="range" style=${`--progress: ${1 + fake_progress * 98}%; z-index: 999;`}
                           min=${options.min} max=${options.max} step=${1 / Math.pow(10, options.decimals)}
                           .value=${value} data-value=${value}
                           placeholder=${options.placeholder || ''}
                           ?disabled=${options.disabled} title=${value != null ? value.toFixed(options.decimals) : ''}
                           @click=${e => { e.target.value = fix_value; handleSliderChange(e, key); }}
                           @dblclick=${e => handleSliderClick(e, key, value, options.min, options.max)}
                           @input=${e => handleSliderChange(e, key)}/>
                    ${ticks.length ? html`
                        <div class="legend">
                            ${ticks.map(tick => {
                                if (Array.isArray(tick) && tick.length >= 3 && !tick[2])
                                    return;

                                let pos = Array.isArray(tick) ? tick[0] : tick;
                                let left = `calc(${100 * (pos - options.min) / range}% + 1px)`;

                                return html`<span class="tick" style=${'left: ' + left + ';'}></span>`;
                            })}
                            ${ticks.map(tick => {
                                let pos = Array.isArray(tick) ? tick[0] : tick;
                                let left = `calc(${100 * (pos - options.min - 0.5) / range}%)`;
                                let width = `calc(100% / ${range} + 1px)`;
                                let label = Array.isArray(tick) ? tick[1] : tick;

                                return html`<span class="label" style=${'left: ' + left + '; width: ' + width + ';'}>${label}</span>`;
                            })}
                        </div>
                    ` : ''}
                </div>
                ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
            </div>
        `);

        let intf = makeWidget('slider', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf);

        return intf;
    }

    function handleSliderChange(e, key) {
        if (!isModifiable(variables_map[key])) {
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
        if (!isModifiable(variables_map[key]))
            return;

        return UI.dialog(e, null, {}, (d, resolve, reject) => {
            let number = d.number('number', 'Valeur :', {min: min, max: max, value: value});

            d.action('Modifier', {disabled: !d.isValid()}, () => {
                updateValue(key, number.value);
                resolve(number.value);
            });
        });
    }

    this.enum = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options, value => {
            if (Array.isArray(value))
                value = (value.length === 1) ? value[0] : undefined;

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            <div class=${options.readonly ? 'fm_enum readonly' : 'fm_enum'} id=${id}>
                ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
                ${props.map((p, i) =>
                    html`<button type="button" data-value=${Util.valueToStr(p.value)}
                                 .className=${value === p.value ? 'active' : ''}
                                 ?disabled=${options.disabled}
                                 @click=${e => handleEnumChange(e, key, options.untoggle)}
                                 @keydown=${handleEnumOrMultiKey} tabindex=${i ? -1 : 0}>${p.label}</button>`)}
                ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
            </div>
        `);

        let intf = makeWidget('enum', label, render, options);
        fillVariableInfo(intf, key, value, props, false);
        addWidget(intf);

        return intf;
    };

    function handleEnumChange(e, key, allow_untoggle) {
        if (!isModifiable(variables_map[key]))
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
                updateValue(key, Util.strToValue(json));
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

        let value = readValue(key, options, value => {
            if (Array.isArray(value))
                value = (value.length === 1) ? value[0] : undefined;

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            <div style=${'display: inline-block; max-width: 100%; ' + makeInputStyle(options)}>
                ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
                <select id=${id} class="fm_select" ?disabled=${options.disabled}
                        @change=${e => handleEnumDropChange(e, key)}>
                    ${options.untoggle || !props.some(p => p != null && value === p.value) ?
                        html`<option value="undefined" .selected=${value == null}
                                     ?disabled=${options.readonly && value != null}>-- Valeur non renseignée --</option>` : ''}
                    ${props.map(p =>
                        html`<option value=${Util.valueToStr(p.value)} .selected=${value === p.value}
                                     ?disabled=${options.readonly && value !== p.value}>${p.label}</option>`)}
                </select>
                ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
            </div>
        `);

        let intf = makeWidget('enumDrop', label, render, options);
        fillVariableInfo(intf, key, value, props, false);
        addWidget(intf);

        return intf;
    };

    function handleEnumDropChange(e, key) {
        if (!isModifiable(variables_map[key]))
            return;

        updateValue(key, Util.strToValue(e.target.value));
    }

    this.enumRadio = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options, value => {
            if (Array.isArray(value))
                value = (value.length === 1) ? value[0] : undefined;

            return value;
        });

        let tab0 = !props.some(p => value === p.value);
        let tabbed = false;

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            <div class=${options.readonly ? 'fm_radio readonly' : 'fm_radio'}
                 style=${makeRadioStyle(options)} id=${id}>
                ${props.map((p, i) => {
                    let tab = !tabbed && (tab0 || value === p.value);
                    tabbed |= tab;

                    // Remember to set name (and id) after .checked, because otherwise when we update
                    // the form with a new FormState on the same page, a previously checked radio button
                    // can unset a previous one when it's name is updated but the checked value is
                    // still true, meaning '.checked=false' hasn't run yet.
                    return html`<input type="radio" value=${Util.valueToStr(p.value)}
                                       ?disabled=${options.disabled || false} .checked=${value === p.value}
                                       name=${id} id=${`${id}.${i}`}
                                       @click=${e => handleEnumRadioChange(e, key, options.untoggle && value === p.value)}
                                       @keydown=${handleRadioOrCheckKey} tabindex=${tab ? 0 : -1} />
                                <label for=${`${id}.${i}`}>${p.label}</label><br/>`;
                })}
            </div>
        `);

        let intf = makeWidget('enumRadio', label, render, options);
        fillVariableInfo(intf, key, value, props, false);
        addWidget(intf);

        return intf;
    };

    function handleEnumRadioChange(e, key, already_checked) {
        if (!isModifiable(variables_map[key]))
            return;

        if (already_checked) {
            e.target.checked = false;
            updateValue(key, undefined);
        } else {
            updateValue(key, Util.strToValue(e.target.value));
        }
    }

    this.multi = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options, value => {
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
            ${label ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            <div class=${options.readonly ? 'fm_enum readonly' : 'fm_enum'} id=${id}>
                ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
                ${props.map((p, i) =>
                    html`<button type="button" data-value=${Util.valueToStr(p.value)}
                                 .className=${value != null && value.includes(p.value) ? 'active' : ''}
                                 ?disabled=${options.disabled}
                                 @click=${e => handleMultiChange(e, key)}
                                 @keydown=${handleEnumOrMultiKey} tabindex=${i ? -1 : 0}>${p.label}</button>`)}
                ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
            </div>
        `);

        let intf = makeWidget('multi', label, render, options);
        fillVariableInfo(intf, key, value, props, true);
        addWidget(intf);

        return intf;
    };

    function handleMultiChange(e, key) {
        if (!isModifiable(variables_map[key]))
            return;

        e.target.classList.toggle('active');

        let els = e.target.parentNode.querySelectorAll('button');

        let nullify = (e.target.classList.contains('active') && e.target.dataset.value === 'null');
        let value = [];
        for (let el of els) {
            if ((el.dataset.value === 'null') != nullify)
                el.classList.remove('active');
            if (el.classList.contains('active'))
                value.push(Util.strToValue(el.dataset.value));
        }

        updateValue(key, value);
    }

    function handleEnumOrMultiKey(e) {
        if ((e.keyCode >= 48 && e.keyCode <= 57) || (e.keyCode >= 96 && e.keyCode <= 105)) {
            let digit = e.keyCode - (e.keyCode >= 96 ? 96 : 48);
            let el = e.target.parentNode.querySelector('button[data-value=\'' + digit + '\'], button[data-value=\'"' + digit + '"\']');

            if (el != null)
                el.click();
            e.preventDefault();
        } else if (e.keyCode === 37) { // left
            let prev = e.target.previousElementSibling;
            if (prev)
                prev.focus();
            e.preventDefault();
        } else if (e.keyCode === 39) { // right
            let next = e.target.nextElementSibling;
            if (next)
                next.focus();
            e.preventDefault();
        }
    }

    this.multiCheck = function(key, label, props = [], options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);
        props = normalizePropositions(props);

        let value = readValue(key, options, value => {
            let nullable = props.some(p => p.value == null);

            if (Array.isArray(value)) {
                if (!value.length)
                    value = null;
            } else {
                if (value != null)
                    value = [value];
            }
            if (value == null)
                value = nullable ? undefined : [];

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            <div class=${options.readonly ? 'fm_check readonly' : 'fm_check'}
                 style=${makeRadioStyle(options)} id=${id}>
                ${props.map((p, i) =>
                    html`<input type="checkbox" id=${`${id}.${i}`} value=${Util.valueToStr(p.value)}
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
        if (!isModifiable(variables_map[key]))
            return;

        let els = e.target.parentNode.querySelectorAll('input');

        let nullify = (e.target.checked && e.target.value === 'null');
        let value = [];
        for (let el of els) {
            if ((el.value === 'null') != nullify)
                el.checked = false;
            if (el.checked)
                value.push(Util.strToValue(el.value));
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

        let value = readValue(key, options, value => {
            if (typeof value === 'string') {
                try {
                    value = LocalDate.parse(value);
                } catch (err) {
                    value = undefined;
                }
            } else if (!(value instanceof LocalDate)) {
                value = undefined;
            }

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
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

        validateMinMax(intf, LocalDate.parse);

        return intf;
    };

    function handleDateInput(e, key) {
        if (!isModifiable(variables_map[key]))
            return;

        if (has_input_date) {
            let date = LocalDate.parse(e.target.value || null);
            updateValue(key, date || undefined);
        } else {
            try {
                let date = LocalDate.parse(e.target.value || null);

                e.target.setCustomValidity('');
                updateValue(key, date || undefined);
            } catch (err) {
                e.target.setCustomValidity('Date malformée ou ambiguë');
            }
        }
    }

    this.month = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        if (options.value != null)
            options.value = options.value.toString();

        let value = readValue(key, options, value => {
            if (typeof value === 'string') {
                try {
                    value = LocalDate.parse(value);
                } catch (err) {
                    value = undefined;
                }
            } else if (!(value instanceof LocalDate)) {
                value = undefined;
            }

            return value;
        });
        if (value != null)
            value.day = null;

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type=${has_input_month ? 'month' : 'text'}
                   class="fm_input" style=${makeInputStyle(options)}
                   .value=${value ? (has_input_month ? value.toString() : value.toLocaleString()) : ''}
                   placeholder=${!has_input_month ? 'MM/YYYY' : ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleMonthInput(e, key)}/>
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('date', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf, LocalDate.parse);

        return intf;
    };

    function handleMonthInput(e, key) {
        if (!isModifiable(variables_map[key]))
            return;

        if (has_input_month) {
            let date = LocalDate.parse(e.target.value || null);
            updateValue(key, date || undefined);
        } else {
            try {
                let date = LocalDate.parse(e.target.value || null);

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

        let value = readValue(key, options, value => {
            if (typeof value === 'string') {
                try {
                    value = LocalTime.parse(value);
                } catch (err) {
                    value = undefined;
                }
            } else if (!(value instanceof LocalTime)) {
                value = undefined;
            }

            return value;
        });

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
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

        let intf = makeWidget('time', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf, LocalTime.parse);

        return intf;
    };

    function handleTimeInput(e, key) {
        if (!isModifiable(variables_map[key]))
            return;

        if (has_input_date) {
            let time = LocalTime.parse(e.target.value || null);
            updateValue(key, time || undefined);
        } else {
            try {
                let time = LocalTime.parse(e.target.value || null);

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

        let value = readValue(key, options, value => {
            if (!(value instanceof File))
                value = undefined;

            return value;
        });

        // Setting files on input file elements is fragile. At least on Firefox, assigning
        // its own value to the property results in an empty FileList for some reason.
        class SetFiles extends Directive {
            update(part, [value]) {
                if (value === noChange || value === nothing)
                    return value;

                let el = part.element;

                if (value == null) {
                    part.element.value = '';
                } else if (value !== el.files) {
                    part.element.files = value;
                }

                return value;
            }

            render(value) {
                return value;
            }
        }
        let set_files = directive(SetFiles);

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            <input id=${id} type="file" size=${options.size || 30}
                   placeholder=${options.placeholder || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleFileInput(e, key)}
                   ${set_files(state.file_lists.get(key.toString()))} />
        `);

        let intf = makeWidget('file', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        return intf;
    };

    function handleFileInput(e, key) {
        if (!isModifiable(variables_map[key]))
            return;

        state.file_lists.set(key.toString(), e.target.files);
        updateValue(key, e.target.files[0] || undefined);
    }

    this.calc = function(key, label, value, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        options.mandatory = false;

        if (Number.isNaN(value) || value == null)
            value = undefined;
        if (value != null && typeof value !== 'string' &&
                             typeof value !== 'number' &&
                             value.constructor.name !== 'LocalDate' &&
                             value.constructor.name !== 'LocalTime')
            throw new Error('Calculated value must be a string, a number, a date or a time');

        let text;
        if (value == null) {
            text = 'Non disponible';
        } else if (options.text != null) {
            if (typeof options.text === 'function') {
                text = options.text(value);
            } else {
                text = options.text;
            }
        } else if (typeof value === 'number' && isFinite(value)) {
            if (options.decimals != null) {
                text = value.toFixed(options.decimals);
            } else {
                // This is a garbage way to round numbers
                let multiplicator = Math.pow(10, 2);
                let n = parseFloat((value * multiplicator).toFixed(11));
                text = Math.round(n) / multiplicator;
            }
        } else {
            text = value;
        }

        let id = makeID(key);
        let render = intf => renderWrappedWidget(intf, html`
            ${label != null ? html`<div class="fm_label"><label for=${id}>${label}</label></div>` : ''}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <span id=${id} class="fm_calc">${text}</span>
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('calc', label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        updateValue(key, value, false);

        return intf;
    };

    this.output = function(content, options = {}) {
        options = expandOptions(options);

        options.mandatory = false;

        // This helps avoid garbage output when the user types 'page.output(html);'
        if (content != null && content !== html && content !== svg) {
            let render = intf => {
                if (intf.options.wrap) {
                    return html`
                        <div class="fm_wrap" data-line=${intf.line}>
                            ${content}
                        </div>
                    `;
                } else {
                    return content;
                }
            };

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

        captureWidgets(widgets, 'block', func, options);

        return intf;
    };
    this.scope = this.block;

    this.section = function(label, func, options = {}) {
        options = expandOptions(options);

        if (typeof label === 'string' && options.anchor == null && !section_depth)
            options.anchor = Util.stringToID(label);

        let deploy = state.state_sections[label];
        if (deploy == null) {
            deploy = options.deploy;
            state.state_sections[label] = deploy;
        }

        let widgets = [];
        let render = intf => html`
            <fieldset class="fm_container fm_section" id=${intf.options.anchor || ''}>
                ${label ? html`<div class="fm_legend" style=${makeLegendStyle(options)}>
                                   <span @click=${e => handleSectionClick(e, label)}>${label}</span></div>` : ''}
                ${deploy ? widgets.map(intf => intf.render()) : ''}
                ${!deploy ? html`<a class="fm_deploy"
                                    @click=${e => handleSectionClick(e, label)}>(afficher le contenu)</a>` : ''}
            </fieldset>
        `;

        let intf = makeWidget('section', label, render, options);
        addWidget(intf);

        options = Object.assign({}, options);
        options.anchor = null;

        try {
            section_depth++;
            captureWidgets(widgets, 'section', func, options);
        } finally {
            section_depth--;
        }

        return intf;
    };

    function handleSectionClick(e, label) {
        state.state_sections[label] = !state.state_sections[label];
        self.restart();
    }

    this.errorList = function(label, options = {}) {
        options = expandOptions(options);

        let render = intf => html`
            <fieldset class="fm_container fm_section error">
                <div class="fm_legend" style=${makeLegendStyle(options)}>${label || 'Liste des erreurs'}</div>
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
                        html`<button type="button" class=${!tab.disabled && idx === tab_idx ? 'active' : ''}
                                     ?disabled=${tab.disabled}
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

            try {
                tabs_ref = tabs;

                captureWidgets(widgets, 'tabs', func, options);

                // Adjust current tab
                {
                    let tab_state = state.state_tabs[key];
                    if (tab_state != null) {
                        tab_idx = tabs.findIndex(tab => tab.label === tab_state.label);
                        if (tab_idx < 0)
                            tab_idx = tab_state.idx;
                    } else if (typeof options.tab === 'string') {
                        tab_idx = tabs.findIndex(tab => tab.label === options.tab);
                    } else {
                        tab_idx = options.tab;
                    }
                    if (tabs[tab_idx] == null || tabs[tab_idx].disabled)
                        tab_idx = tabs.findIndex(tab => !tab.disabled);
                    if (tab_idx < 0)
                        tab_idx = tabs.length - 1;

                    if (tab_idx >= 0) {
                        tab_state = {
                            idx: tab_idx,
                            label: tabs[tab_idx].label
                        };
                        state.state_tabs[key] = tab_state;
                    }
                }

                for (let i = 0; i < tabs.length; i++) {
                    if (i === tab_idx) {
                        let tab = tabs[i];

                        tab.active = true;
                        model.actions.push(...tab.actions);
                    }
                }
            } finally {
                tabs_ref = prev_tabs;
            }
        }

        return intf;
    };

    function handleTabClick(e, key, idx) {
        let tab_state = {
            idx: idx
        };
        state.state_tabs[key] = tab_state;

        self.restart();
    }

    this.tab = function(label, func, options = {}) {
        options = expandOptions(options);

        if (!tabs_ref)
            throw new Error('form.tab must be called from inside form.tabs');
        if (!label)
            throw new Error('Label must be specified');
        if (tabs_ref.find(tab => tab.label === label))
            throw new Error(`Tab label '${label}' is already used`);

        let tab = {
            label: label,
            disabled: options.disabled,
            active: false,
            actions: null
        };
        tabs_ref.push(tab);

        // We don't want to show actions created inside inactive tabs
        let prev_actions_len = model.actions.length;

        let widgets = captureWidgets([], 'tab', func, options);
        let render = intf => tab.active ? widgets.map(intf => intf.render()) : '';

        tab.actions = model.actions.slice(prev_actions_len);
        model.actions.length = prev_actions_len;

        let intf = makeWidget('tab', null, render, options);
        addWidget(intf);

        return intf;
    };

    this.repeat = function(key, func, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        if (!func) {
            throw new Error(`This call does not contain a function.

Make sure you did not use this syntax by accident:
    page.repeat("var"), () => { /* Do stuff here */ };
instead of:
    page.repeat("var", () => { /* Do stuff here */ });`);
        }

        let path = [...key.path, key.name];
        let values = walkPath(state.values, path);

        let widgets = [];
        let render = intf => widgets.map(intf => intf.render());

        let intf = makeWidget('repeat', undefined, render, options);
        Object.assign(intf, {
            length: null,
            add: () => handleRepeatAdd(values, intf.length),
            remove: idx => handleRepeatRemove(values, idx)
        });
        addWidget(intf);

        if (!isModifiable(intf)) {
            intf.add = null;
            intf.remove = null;
        }

        for (let i = 0;; i++) {
            if (!Object.hasOwn(values, i)) {
                intf.length = i;
                break;
            }

            options.path = [...path, i];

            let remove = intf.remove ? (() => intf.remove(i)) : null;
            captureWidgets(widgets, 'repeat', () => func(values[i], i, remove), options);
        }

        return intf;
    };

    function handleRepeatAdd(values, len) {
        values[len] = {};
        self.restart();
    }

    function handleRepeatRemove(values, idx) {
        for (;;) {
            values[idx] = values[idx + 1];
            if (!Object.hasOwn(values, idx + 1)) {
                delete values[idx];
                break;
            }
            idx++;
        }

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

        options.compact = false;
        captureWidgets(widgets, 'columns', func, options);

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

        if (typeof label === 'string' && label.startsWith('+')) {
            options.always = true;
            label = label.substr(1);
        }
        if (func == null)
            func = e => handleActionClick(e, label);

        let clicked = state.click_events.has(label);
        state.click_events.delete(label);

        let render;
        if (typeof label === 'string' && label.match(/^\-+$/)) {
            render = intf => html`<hr/>`;
            options.always = true;
        } else {
            let type = model.actions.length ? 'button' : 'submit';

            render = intf => html`<button type=${type} ?disabled=${options.disabled}
                                          class=${options.color ? 'color' : ''}
                                          style=${options.color ? `--color: ${options.color};` : ''}
                                          title=${options.tooltip || ''}
                                          @click=${UI.wrap(func)}>${label}</button>`;

            if (options.always == null)
                options.always = false;
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

    this.refresh = function() {
        if (!restart) {
            state.just_triggered = false;

            setTimeout(() => state.changeHandler(model), 0);
            restart = true;
        }
    };
    this.restart = this.refresh;

    function decodeKey(key, options) {
        // Normalize key
        if (!Array.isArray(key))
            key = [key];
        if (Array.isArray(options.path)) {
            key = [...options.path, ...key];
        } else if (options.path != null) {
            key = [options.path, ...key];
        }
        if (!key.length)
            throw new Error('Empty keys are not allowed');

        // Decode option shortcuts
        let name = key[key.length - 1];
        for (;;) {
            if (name[0] === '*') {
                options.mandatory = true;
            } else {
                break;
            }
            name = name.substr(1);
            key[key.length - 1] = name;
        }

        // Check key parts
        for (let i = 0; i < key.length; i++) {
            let part = key[i];

            if (typeof part === 'number') {
                if (part < 0 || !Number.isInteger(part))
                    throw new Error('Number keys must be positive integers');

                key[i] = part.toString();
            } else if (typeof part === 'string') {
                if (part === '')
                    throw new Error('Empty keys are not allowed');
                if (!part.match(/^[a-zA-Z0-9_]*$/))
                    throw new Error('Allowed key characters: a-z, 0-9 and _');
                if (part.startsWith('__'))
                    throw new Error('Keys must not start with \'__\'');
            } else {
                throw new Error('foobar');
            }
        }

        return {
            path: key.slice(0, key.length - 1),
            name: name,
            toString: () => key.join('.')
        };
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
            <div class=${intf.options.disabled ? 'fm_wrap disabled' : 'fm_wrap'} data-line=${intf.line}>
                <div class=${cls}>
                    ${frag}
                    ${intf.errors.length ?
                        html`<div class="fm_error">${intf.errors.map(err => html`${err}<br/>`)}</div>` : ''}
                </div>
                ${intf.options.help && Array.isArray(intf.options.help) ?
                    intf.options.help.map(help => help ? html`<div class="fm_help"><p>${help}</p></div>` : '') : ''}
                ${intf.options.help && !Array.isArray(intf.options.help) ?
                    html`<div class="fm_help"><p>${intf.options.help}</p></div>` : ''}
            </div>
        `;
    }

    function makeInputStyle(options) {
        let style = '';

        if (options.wide)
            style += 'width: 100%;';
        if (options.style)
            style += options.style;

        return style;
    }

    function makeRadioStyle(options) {
        let style = '';

        if (options.columns)
            style += `columns: ${options.columns};`;
        if (options.style)
            style += options.style;

        return style;
    }

    function makeLegendStyle(options) {
        let style = '';

        if (options.color != null)
            style += `background: ${options.color};`;

        return style;
    }

    function makeWidget(type, label, func, options = {}) {
        if (label != null) {
            // Users are allowed to use complex HTML as label. Turn it into text for storage!
            if (typeof label === 'string' || typeof label === 'number') {
                label = '' + label;
            } else if (typeof html !== 'undefined') {
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
            line: Util.locateEvalError(new Error())?.line,

            errors: [],

            render: () => {
                if (!intf.options.hidden) {
                    return func(intf);
                } else {
                    return '';
                }
            },
            toHTML: () => {
                let render = intf.render;
                intf.render = () => '';
                return render();
            }
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

            missing: !intf.options.disabled && value == null,
            changed: state.just_changed.has(key.toString()),

            error: (msg, delay = false) => {
                if (!delay || state.take_delayed.has(key.toString())) {
                    intf.errors.push(msg || '');
                    model.errors++;
                }

                model.valid = false;

                return intf;
            }
        });

        state.just_changed.delete(key.toString());

        if (props != null) {
            intf.props = props;
            intf.props_map = Util.arrayToObject(props, prop => prop.value, prop => prop.label);
            intf.multi = multi;
        }

        if (intf.options.mandatory && intf.missing) {
            intf.error('Obligatoire !', intf.options.missing_mode !== 'error');

            if (intf.options.missing_mode === 'disable')
                model.valid = false;
        }

        model.variables.push(intf);
        variables_map[key] = intf;

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
            return html`<span class=${cls}>${text(value)}</span>`;
        } else if (text != null && text !== '') {
            return html`<span class=${cls}>${text != null ? text : ''}</span>`;
        } else {
            return '';
        }
    }

    function readValue(key, options, func) {
        let ptr = walkPath(state.values, key.path);

        if (!Object.hasOwn(ptr, key.name)) {
            let value = normalizeValue(func, options.value);
            let cache = state.obj_caches.get(ptr);

            if (cache != null)
                cache[key.name] = value;
            ptr[key.name] = value;

            state.follow_default.add(key.toString());

            return value;
        } else if (options.value != null && state.follow_default.has(key.toString())) {
            let value = normalizeValue(func, options.value);
            let cache = state.obj_caches.get(ptr);

            if (cache != null)
                cache[key.name] = value;
            ptr[key.name] = value;

            return value;
        } else {
            let orig = ptr[key.name];
            let value = normalizeValue(func, orig);

            if (value !== orig)
                ptr[key.name] = value;

            return value;
        }
    }

    function normalizeValue(func, value) {
        value = func(value);
        if (value == null)
            value = undefined;
        return value;
    }

    function updateValue(key, value, refresh = true) {
        let ptr = walkPath(state.values, key.path);
        if (value === ptr[key.name])
            return;
        ptr[key.name] = value;

        state.take_delayed.delete(key.toString());
        state.follow_default.delete(key.toString());
        state.just_changed.add(key.toString());

        if (refresh) {
            state.markInteraction();
            self.restart();
        }
    }

    function walkPath(values, path) {
        for (let key of path) {
            if (!Object.hasOwn(values, key))
                values[key] = {};
            values = values[key];
        }

        return values;
    }

    function isModifiable(intf) {
        if (intf.options.disabled)
            return false;
        if (intf.options.readonly)
            return false;

        return true;
    }
}

export {
    FormState,
    FormModel,
    FormBuilder
}
