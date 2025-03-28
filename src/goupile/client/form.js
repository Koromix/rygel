// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { render, html, svg,
         directive, Directive, noChange, nothing } from '../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log, Net, Mutex, LocalDate, LocalTime } from '../../web/core/base.js';
import { profile } from './goupile.js';
import * as UI from './ui.js';
import { MagicData } from './form_data.js';

import './form.css';

// Unique widget HTML identifiers
let ptr_ids = new WeakMap;
let next_id = 0;

function FormState(data = null) {
    let self = this;

    // Hook functions
    this.changeHandler = () => {};
    this.annotateHandler = null;

    // Internal widget state
    this.retain_map = new WeakMap;
    this.click_events = new Set;

    // Semi-public UI state
    this.just_triggered = false;
    this.state_tabs = {};
    this.state_sections = {};
    this.force_changed = false;
    this.has_interaction = false;

    if (!(data instanceof MagicData)) {
        if (data == null)
            data = {};
        data = new MagicData(data);
    }

    // Stored values
    this.data = data;
    this.values = data.proxy;

    this.hasChanged = function() {
        if (!data.hasChanged() && !self.force_changed)
            return false;
        if (!self.has_interaction)
            return false;

        return true;
    };

    this.markInteraction = function() { self.has_interaction = true; };
    this.markChange = function() {
        self.force_changed = true;
        self.has_interaction = true;
    };

    this.justTriggered = function() { return self.just_triggered; };
}

function FormModel() {
    let self = this;

    this.widgets = [];
    this.widgets0 = [];
    this.variables = [];
    this.actions = [];

    this.valid = true;

    this.isValid = function() { return !self.widgets.some(intf => intf.errors.length); };
    this.hasErrors = function() { return self.widgets.some(intf => intf.errors.some(err => !err.delay)); };

    this.render = function() {
        return html`
            <div class="fm_main">${self.renderWidgets()}</div>
            <div class="ui_actions" style="margin-top: 32px;">${self.renderActions()}</div>
        `;
    };
    this.renderWidgets = function() { return self.widgets0.map(intf => intf.render()); };
    this.renderActions = function() { return self.actions.map(intf => intf.render()); };
}

function FormBuilder(state, model, options = {}) {
    let self = this;

    options = Object.assign({
        deploy: true,
        untoggle: true,
        wrap: true,
        annotate: false
    }, options);

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

    let data = state.data;

    let intf_map = new WeakMap;

    data.clearNotes('errors');
    data.clearNotes('variables');

    let paths_stack = [{
        root: state.values,
        ptr: state.values
    }];

    let options_stack = [options];
    let widgets_ref = model.widgets0;

    let inline_next = false;
    let inline_widgets;

    let section_depth = 0;
    let tabs_keys = new Set;
    let tabs_ref;

    let restart = false;

    Object.defineProperties(this, {
        widgets: { get: () => model.widgets, enumerable: true },
        widgets0: { get: () => model.widgets0, enumerable: true },
        variables: { get: () => model.variables, enumerable: true },

        state: { get: () => state, enumerable: true },
        values: { get: () => state.values, enumerable: true }
    });

    this.hasChanged = function() { return state.hasChanged(); };
    this.markInteraction = function() { state.markInteraction(); };
    this.markChange = function() { state.markChange(); };

    this.justTriggered = function() { return state.justTriggered(); };

    this.isValid = function() { return model.isValid(); };
    this.hasErrors = function() { return model.hasErrors(); };

    this.pushOptions = function(options = {}) {
        if (options.hasOwnProperty('path'))
            throw new Error('Option "path" is not supported anymore');

        options = expandOptions(options);
        options_stack.push(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error('Too many popOptions() operations');

        options_stack.pop();
    };

    this.pushPath = function(key, sub = null) {
        key = decodeKey(key);

        if (key.variables.has(key.name))
            throw new Error(`Variable '${key}' already exists`);

        let path = {
            root: key.ptr[key.name],
            ptr: null
        };

        if (!Util.isPodObject(path.root)) {
            key.ptr[key.name] = {};
            path.root = key.ptr[key.name];
        }

        if (sub != null) {
            if (!Util.isPodObject(path.root[sub]))
                path.root[sub] = {};
            path.ptr = path.root[sub];
        } else {
            path.ptr = path.root;
        };

        paths_stack.push(path);
    };

    this.popPath = function() {
        if (paths_stack.length < 2)
            throw new Error('Too many popPath() operations');

        paths_stack.pop();
    };

    this.triggerErrors = function() {
        if (!self.isValid()) {
            let cleared_set = new Set;

            for (let variable of model.variables) {
                let key = variable.key;

                if (!cleared_set.has(key.retain)) {
                    key.retain.take_delayed.clear();
                    cleared_set.add(key.retain);
                }

                key.retain.take_delayed.add(key.name);
            }

            self.restart();
            state.just_triggered = true;

            throw new Error(`Vous n'avez pas répondu correctement à toutes les questions, veuillez vérifier vos réponses ou mettre des annotations`);
        }
    };

    this.find = key => {
        key = decodeKey(key);
        return key.variables.get(key.name);
    };
    this.value = (key, default_value = undefined) => {
        let intf = self.find(key);
        return (intf && !intf.missing) ? intf.value : default_value;
    };
    this.missing = key => {
        let intf = self.find(key);
        return intf.missing;
    };
    this.error = (key, msg, delay = false) => {
        let intf = self.find(key);
        intf.error(msg, delay);
    };

    this.text = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options, value => (value != null) ? String(value) : undefined);
        let extra = makeTextExtra(key, options, value);

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf, extra)}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type="text" class="fm_input" style=${makeInputStyle(options)}
                   placeholder=${options.placeholder || ''}
                   .value=${value || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleTextInput(e, key)} />
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('text', makeID(key), label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        return intf;
    };

    this.textArea = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options, value => (value != null) ? String(value) : undefined);
        let extra = makeTextExtra(key, options, value);

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf, extra)}
            <textarea id=${id} class="fm_input" style=${makeInputStyle(options)}
                   rows=${options.rows || 3} cols=${options.cols || 30}
                   placeholder=${options.placeholder || ''}
                   .value=${value || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleTextInput(e, key)}></textarea>
        `);

        let intf = makeWidget('text', makeID(key), label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        return intf;
    };

    this.password = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options, value => (value != null) ? String(value) : undefined);

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type="password" class="fm_input" style=${makeInputStyle(options)}
                   placeholder=${options.placeholder || ''}
                   .value=${value || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleTextInput(e, key)} />
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('password', makeID(key), label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        return intf;
    };

    function handleTextInput(e, key) {
        if (!isModifiable(key))
            return;

        updateValue(key, e.target.value || undefined);
    }

    this.pin = function(key, label, options = {}) {
        options = expandOptions(options);
        key = decodeKey(key, options);

        let value = readValue(key, options, value => (value != null) ? String(value) : undefined);

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
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

        let intf = makeWidget('password', makeID(key), label, render, options);
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type="number" class="fm_input" style=${makeInputStyle(options)}
                   step=${1 / Math.pow(10, options.decimals || 0)} .value=${value}
                   placeholder=${options.placeholder || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleNumberChange(e, key)}/>
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('number', makeID(key), label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf);

        if (key.retain.invalid_numbers.has(key.name))
            intf.error('Nombre invalide (décimales ?)');

        return intf;
    };

    function handleNumberChange(e, key) {
        if (!isModifiable(key))
            return;

        // Hack to accept incomplete values, mainly in the case of a '-' being typed first,
        // in which case we don't want to clear the field immediately.
        if (e.target.validity && !e.target.validity.valid) {
            key.retain.invalid_numbers.add(key.name);
            self.restart();
            return;
        }
        if (key.retain.invalid_numbers.delete(key.name))
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
            <div class=${'fm_slider' + (value == null ? ' missing' : '') + (options.readonly ? ' readonly' : '')}
                 style=${makeInputStyle(options)}>
                ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
                <div>
                    <input id=${id} type="range" style=${`--progress: ${1 + fake_progress * 98}%; z-index: 999;`}
                           min=${options.min} max=${options.max} step=${1 / Math.pow(10, options.decimals)}
                           .value=${value} data-value=${value}
                           placeholder=${options.placeholder || ''}
                           ?disabled=${options.disabled} title=${value?.toFixed?.(options.decimals) ?? ''}
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

        let intf = makeWidget('number', makeID(key), label, render, options);
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

        return UI.dialog(e, null, {}, (d, resolve, reject) => {
            let number = d.number('number', 'Valeur :', { min: min, max: max, value: value });

            d.action('Modifier', { disabled: !d.isValid() }, () => {
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
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

        let intf = makeWidget('enum', makeID(key), label, render, options);
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
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

        let intf = makeWidget('enum', makeID(key), label, render, options);
        fillVariableInfo(intf, key, value, props, false);
        addWidget(intf);

        return intf;
    };

    function handleEnumDropChange(e, key) {
        if (!isModifiable(key))
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
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

        let intf = makeWidget('enum', makeID(key), label, render, options);
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
            <div class=${options.readonly ? 'fm_enum readonly' : 'fm_enum'} id=${id}>
                ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
                ${props.map((p, i) =>
                    html`<button type="button" data-value=${Util.valueToStr(p.value)}
                                 .className=${value?.includes?.(p.value) ? 'active' : ''}
                                 ?disabled=${options.disabled}
                                 @click=${e => handleMultiChange(e, key)}
                                 @keydown=${handleEnumOrMultiKey} tabindex=${i ? -1 : 0}>${p.label}</button>`)}
                ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
            </div>
        `);

        let intf = makeWidget('multi', makeID(key), label, render, options);
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
            <div class=${options.readonly ? 'fm_check readonly' : 'fm_check'}
                 style=${makeRadioStyle(options)} id=${id}>
                ${props.map((p, i) =>
                    html`<input type="checkbox" id=${`${id}.${i}`} value=${Util.valueToStr(p.value)}
                                ?disabled=${options.disabled} .checked=${value?.includes?.(p.value)}
                                @click=${e => handleMultiCheckChange(e, key)}
                                @keydown=${handleRadioOrCheckKey} tabindex=${i ? -1 : 0} />
                         <label for=${`${id}.${i}`}>${p.label}</label><br/>`)}
            </div>
        `);

        let intf = makeWidget('multi', makeID(key), label, render, options);
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
        return { value: value, label: label || value };
    };

    function normalizePropositions(props) {
        if (!Array.isArray(props))
            props = Array.from(props);

        props = props.filter(c => c != null).map(c => {
            if (Array.isArray(c)) {
                return { value: c[0], label: c[1] || c[0] };
            } else if (typeof c === 'string') {
                let sep_pos = c.indexOf(':::');
                if (sep_pos >= 0) {
                    let value = c.substr(0, sep_pos);
                    let label = c.substr(sep_pos + 3);
                    return { value: value, label: label || value };
                } else {
                    return { value: c, label: c };
                }
            } else if (typeof c === 'number') {
                return { value: c, label: c };
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type=${has_input_date ? 'date' : 'text'}
                   class="fm_input" style=${makeInputStyle(options)}
                   .value=${value ? (has_input_date ? value.toString() : value.toLocaleString()) : ''}
                   placeholder=${!has_input_date ? 'DD/MM/YYYY' : ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleDateInput(e, key)}/>
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('date', makeID(key), label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf, LocalDate.parse);

        return intf;
    };

    function handleDateInput(e, key) {
        if (!isModifiable(key))
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <input id=${id} type=${has_input_month ? 'month' : 'text'}
                   class="fm_input" style=${makeInputStyle(options)}
                   .value=${value ? (has_input_month ? value.toString() : value.toLocaleString()) : ''}
                   placeholder=${!has_input_month ? 'MM/YYYY' : ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleMonthInput(e, key)}/>
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('month', makeID(key), label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf, LocalDate.parse);

        return intf;
    };

    function handleMonthInput(e, key) {
        if (!isModifiable(key))
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
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

        let intf = makeWidget('time', makeID(key), label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        validateMinMax(intf, LocalTime.parse);

        return intf;
    };

    function handleTimeInput(e, key) {
        if (!isModifiable(key))
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
            <input id=${id} type="file" size="${options.size || 30}"
                   placeholder=${options.placeholder || ''}
                   ?disabled=${options.disabled} ?readonly=${options.readonly}
                   @input=${e => handleFileInput(e, key)}
                   ${set_files(key.retain.file_lists.get(key.name))} />
        `);

        let intf = makeWidget('file', makeID(key), label, render, options);
        fillVariableInfo(intf, key, value);
        addWidget(intf);

        return intf;
    };

    function handleFileInput(e, key) {
        if (!isModifiable(key))
            return;

        key.retain.file_lists.set(key.name, e.target.files);
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
                             !(value instanceof LocalDate) &&
                             !(value instanceof LocalTime))
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

        let render = (intf, id) => renderWrappedWidget(intf, html`
            ${makeLabel(intf)}
            ${makePrefixOrSuffix('fm_prefix', options.prefix, value)}
            <span id="${id}" class="fm_calc">${text}</span>
            ${makePrefixOrSuffix('fm_suffix', options.suffix, value)}
        `);

        let intf = makeWidget('calc', makeID(key), label, render, options);
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
            let id = makeID(null);

            let render = intf => {
                if (intf.options.wrap) {
                    return html`
                        <div id=${id} class="fm_wrap">
                            ${content}
                        </div>
                    `;
                } else {
                    return content;
                }
            };

            let intf = makeWidget('output', id, null, render, options);
            addWidget(intf);

            return intf;
        }
    };

    this.block = function(func, options = {}) {
        options = expandOptions(options);

        let widgets = [];
        let render = intf => widgets.map(intf => intf.render());

        let intf = makeWidget('block', null, '', render, options);
        addWidget(intf);

        captureWidgets(widgets, 'block', func, options);

        return intf;
    };
    this.scope = this.block;

    this.section = function(label, func, options = {}) {
        options = expandOptions(options);

        if (typeof label == 'function') {
            if (Util.isPodObject(func))
                options = func;
            func = label;
            label = null;
        }

        if (typeof label === 'string' && options.anchor == null && !section_depth) {
            let anchor = label;

            anchor = anchor.replace(/[A-Z]/g, c => c.toLowerCase());
            anchor = anchor.replace(/[^a-zA-Z0-9]/g, c => {
                switch (c) {
                    case 'ç': { c = 'c'; } break;
                    case 'ê':
                    case 'é':
                    case 'è':
                    case 'ë': { c = 'e'; } break;
                    case 'à':
                    case 'â':
                    case 'ä':
                    case 'å': { c = '[aàâäå]'; } break;
                    case 'î':
                    case 'ï': { c = '[iîï]'; } break;
                    case 'ù':
                    case 'ü':
                    case 'û':
                    case 'ú': { c = 'u'; } break;
                    case 'n':
                    case 'ñ': { c = 'n'; } break;
                    case 'ó':
                    case 'ö':
                    case 'ô': { c = 'o'; } break;
                    case 'œ': { c = 'oe'; } break;
                    case 'ÿ': { c = 'y'; } break;

                    default: { c = '-'; } break;
                }

                return c;
            });
            anchor = anchor.replace(/-+/g, '-');

            options.anchor = anchor;
        }

        let deploy = state.state_sections[label];
        if (deploy == null) {
            deploy = options.deploy;
            state.state_sections[label] = deploy;
        }

        let widgets = [];
        let render = (intf, id) => html`
            <fieldset class=${makeClasses(options, 'fm_container', 'fm_section')} id=${id || ''}>
                ${label ? html`<div class="fm_legend" style=${makeLegendStyle(options)}>
                                   <span @click=${e => handleSectionClick(e, label)}>${label}</span></div>` : ''}
                ${deploy ? widgets.map(intf => intf.render()) : ''}
                ${!deploy ? html`<a class="fm_deploy"
                                    @click=${e => handleSectionClick(e, label)}>(afficher le contenu)</a>` : ''}
            </fieldset>
        `;

        let intf = makeWidget('section', options.anchor, label, render, options);
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
            <fieldset class=${makeClasses(options, 'fm_container', 'fm_section', 'error')}>
                <div class="fm_legend" style=${makeLegendStyle(options)}>${label || `Assurez-vous d'avoir correctement répondu à tous les items`}</div>
                ${!self.hasErrors() ? 'Aucune erreur' : ''}
                ${self.hasErrors() ? html`
                    <ul>
                        ${model.widgets.map(intf => intf.errors.length ? html`<li><a href=${'#' + intf.id}>${intf.label}</a></li>` : '')}
                    </ul>
                ` : ''}
            </fieldset>
        `;

        let intf = makeWidget('errorList', null, null, render, options);
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
            <div class=${makeClasses(options, 'fm_container', 'fm_section', 'tabs')}>
                <div class="fm_tabs">
                    ${tabs.map((tab, idx) =>
                        html`<button type="button" class=${!tab.disabled && idx === tab_idx ? 'active' : ''}
                                     ?disabled=${tab.disabled}
                                     @click=${e => handleTabClick(e, key, idx)}>${tab.label}</button>`)}
                </div>

                ${widgets.map(intf => intf.render())}
            </div>
        ` : '';

        let intf = makeWidget('tabs', null, null, render, options);
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

        let intf = makeWidget('tab', null, null, render, options);
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

        if (key.variables.has(key.name))
            throw new Error(`Variable '${key}' already exists`);
        if (!Util.isPodObject(key.ptr[key.name]))
            key.ptr[key.name] = {};

        let values = key.ptr[key.name];

        let widgets = [];
        let render = intf => widgets.map(intf => intf.render());

        let intf = makeWidget('repeat', null, null, render, options);
        fillVariableInfo(intf, key, values);
        addWidget(intf);

        Object.assign(intf, {
            length: null,
            add: isModifiable(key) ? () => handleRepeatAdd(values, intf.length) : null,
            remove: isModifiable(key) ? idx => handleRepeatRemove(values, idx) : null
        });

        for (let i = 0;; i++) {
            if (!values.hasOwnProperty(i)) {
                intf.length = i;
                break;
            }

            let prev_paths = paths_stack;

            try {
                paths_stack = [{
                    root: values,
                    ptr: values[i]
                }];

                let remove = intf.remove ? (() => intf.remove(i)) : null;
                captureWidgets(widgets, 'repeat', () => func(values[i], i, remove), options);
            } finally {
                paths_stack = prev_paths;
            }
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
            if (!values.hasOwnProperty(idx + 1)) {
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
            <div class=${makeClasses(options, 'fm_container', 'fm_columns')}>
                ${widgets.map(intf => html`
                    <div style=${options.wide ? 'flex: 1;' : ''}>
                        ${intf.render()}
                    </div>
                `)}
            </div>
        `;

        let intf = makeWidget('columns', null, null, render, options);
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
            }, { wide: wide });

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
        let prev_paths = paths_stack;
        let prev_options = options_stack;
        let prev_inline = inline_widgets;

        try {
            widgets_ref = widgets;
            paths_stack = [paths_stack[paths_stack.length - 1]];
            options_stack = [expandOptions(options)];

            inline_next = false;
            inline_widgets = null;

            func();
        } finally {
            inline_next = false;
            inline_widgets = prev_inline;

            options_stack = prev_options;
            paths_stack = prev_paths;
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

        let intf = makeWidget('action', null, label, render, options);
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

            setTimeout(() => state.changeHandler(), 0);
            restart = true;
        }
    };
    this.restart = this.refresh;

    function decodeKey(key, options = {}) {
        // Normalize key
        if (!Array.isArray(key))
            key = [null, key];
        if (key.length < 2 || key.length > 3)
            throw new Error('Invalid key type');

        let root = key[0] ?? paths_stack[paths_stack.length - 1].root;
        let ptr = (key.length >= 3 ? key[1] : key[0]) ?? paths_stack[paths_stack.length - 1].ptr;
        let name = (key.length >= 3 ? key[2] : key[1]);

        // Decode option shortcuts
        for (;;) {
            if (name[0] == '*') {
                options.mandatory = true;
            } else {
                break;
            }
            name = name.substr(1);
        }

        // Check key name
        if (typeof name == 'number') {
            if (name < 0 || !Number.isInteger(name))
                throw new Error('Number keys must be positive integers');

            name = name.toString();
        } else if (typeof name == 'string') {
            if (name === '')
                throw new Error('Empty keys are not allowed');
            if (!name.match(/^[a-zA-Z0-9_]*$/))
                throw new Error('Allowed key characters: a-z, 0-9 and _');
            if (name.startsWith('__'))
                throw new Error('Keys must not start with \'__\'');
        } else {
            throw new Error('Invalid key name, must be string or number');
        }

        let retain = state.retain_map.get(ptr);
        let variables = intf_map.get(ptr);

        // Prepare state objects
        if (retain == null) {
            retain = {
                file_lists: new Map,
                invalid_numbers: new Set,
                take_delayed: new Set,
                follow_default: new Set,
                just_changed: new Set
            };
            state.retain_map.set(ptr, retain);
        }
        if (variables == null) {
            variables = new Map;
            intf_map.set(ptr, variables);
        }

        key = {
            root: root,
            ptr: ptr,
            retain: retain,
            variables: variables,
            name: name,
            toString: () => name
        };

        return key;
    }

    function expandOptions(options) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        if (state.annotateHandler == null)
            options.annotate = false;

        return options;
    }

    function makeID(key) {
        let id = null;

        if (key != null) {
            let ids = ptr_ids.get(key.ptr);

            if (ids == null) {
                ids = {};
                ptr_ids.set(key.ptr, ids);
            }

            id = ids[key];

            if (id == null) {
                id = next_id++;
                ids[key] = id;
            }
        } else {
            id = next_id++;
        }

        return `fm_var_${id}`;
    }

    function renderWrappedWidget(intf, frag) {
        let errors = intf.errors.filter(err => !err.delay);

        let cls = 'fm_widget';

        if (errors.length)
            cls += ' error';
        if (intf.options.mandatory)
            cls += ' mandatory';
        if (intf.options.compact)
            cls += ' compact';

        return html`
            <div class=${makeClasses(intf.options, 'fm_wrap')}>
                <div class=${cls}>
                    ${frag}
                    ${errors.length ?
                        html`<div class="fm_error">${errors.map(err => html`${err.msg}<br/>`)}</div>` : ''}
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

    function makeClasses(options, ...classes) {
        classes = classes.filter(cls => cls != null);

        if (options.disabled)
            classes.push('disabled');
        if (options.cls)
            classes.push(options.cls);

        return classes.join(' ');
    }

    function makeWidget(type, id, label, func, options = {}) {
        if (label != null) {
            // Users are allowed to use complex HTML as label. Turn it into text for storage!
            if (typeof label === 'string' || typeof label === 'number') {
                label = '' + label;
            } else if (typeof render !== 'undefined') {
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
            id: id,
            type: type,

            label: label,
            options: options,
            location: Util.locateEvalError(new Error()),

            errors: [],

            render: () => {
                if (!intf.options.hidden) {
                    return func(intf, intf.id);
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
        if (key.variables.has(key.name))
            throw new Error(`Variable '${key}' already exists`);

        Object.assign(intf, {
            key: key,
            value: value,

            missing: !intf.options.disabled && value == null,
            changed: key.retain.just_changed.delete(key.name),

            error: (msg, delay = false) => {
                msg = msg || '';
                delay &&= !key.retain.take_delayed.has(key.name);

                intf.errors.push({ msg: msg, delay: delay });
                model.errors++;

                let note = data.openNote(key.ptr, 'errors', []);
                note.push({ key: key.name, message: msg });

                return intf;
            }
        });

        let variable = {
            type: intf.type,
            label: intf.label
        };

        if (props != null) {
            intf.props = props;
            intf.props_map = Util.arrayToObject(props, prop => prop.value, prop => prop.label);
            intf.multi = multi;

            variable.props = props;
        }

        if (intf.options.mandatory && intf.missing) {
            let msg = intf.options.annotate ? 'Réponse obligatoire sauf justification' : 'Réponse obligatoire';
            intf.error(msg, intf.options.missing_mode !== 'error');
        }

        model.variables.push(intf);
        key.variables.set(key.name, intf);

        let note = data.openNote(key.root, 'variables', {});
        note[key.name] = variable;

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

    function makeLabel(intf, extra = null) {
        if (intf.label == null)
            return '';

        let tags = Array.isArray(intf.options.tags) ? intf.options.tags : [];

        return html`
            <div class="fm_label">
                <label for=${intf.id}>
                    ${intf.label}
                </label>

                ${tags.length ? html`
                    <div class="fm_tags">
                        ${tags.map(tag => html` <span class="ui_tag" style=${'background: ' + tag.color + ';'}>${tag.label}</span>`)}
                    </div>
                ` : ''}

                ${intf.options.annotate ? html`<a class="fm_annotate" @click=${e => annotate(e, intf)} title="Ajouter des annotations">🖊\uFE0E</a>` : ''}
                ${extra != null ? html`<span style="font-weight: normal;">${extra}</span>` : ''}
            </div>
        `;
    }

    async function annotate(e, intf) {
        e.preventDefault();

        await state.annotateHandler(e, intf);
        self.restart();
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

    function makeTextExtra(key, options, value) {
        // Not used anymore but keep this just in case
        return '';
    }

    async function handleVocalClick(e, key, value, language) {
        if (!isModifiable(key))
            return;

        if (typeof loadVosklet === 'undefined')
            await import(`${ENV.urls.static}vosklet/Vosklet.min.js`);

        let vosklet = null;
        let stream = null;

        try {
            let ctx = new AudioContext({ sinkId: { type: 'none' } });

            vosklet = await loadVosklet();

            let url = `${ENV.urls.static}vosklet/${language}.tar`;
            let model = await vosklet.createModel(url, language, language);

            stream = await navigator.mediaDevices.getUserMedia({
                video: false,
                audio: {
                    echoCancellation: true,
                    noiseSuppression: true,
                    channelCount: 1
                }
            });

            let microphone = ctx.createMediaStreamSource(stream);
            let recognizer = await vosklet.createRecognizer(model, ctx.sampleRate);
            let transferer = await vosklet.createTransferer(ctx, 128 * 150);
            transferer.port.onmessage = ev => recognizer.acceptWaveform(ev.data);

            let textarea = document.createElement('textarea');
            textarea.className = 'fm_input';
            textarea.setAttribute('style', 'width: 400px;');
            textarea.setAttribute('readonly', true);
            textarea.rows = 12;

            let text = value ?? '';
            let lines = null;

            await UI.dialog(e, null, {}, (d, resolve, reject) => {
                if (lines == null) {
                    lines = text.split('\n');
                    lines.push('');

                    recognizer.addEventListener('partialResult', ev => {
                        lines[lines.length - 1] = JSON.parse(ev.detail).partial;
                        refresh();
                    });
                    recognizer.addEventListener('result', ev => {
                        lines[lines.length - 1] = JSON.parse(ev.detail).text;
                        lines.push('');

                        refresh();
                    });

                    microphone.connect(transferer);
                }

                d.output(html`
                    <div class="fm_wrap">
                        <div class="fm_widget">${textarea}</div>
                    </div>
                `);

                d.action('Accepter', {}, () => {
                    updateValue(key, text);
                    resolve(text);
                });

                function refresh() {
                    text = lines.join('\n').trim();
                    textarea.value = text;
                }
            });
        } finally {
            if (stream != null) {
                for (let track of stream.getTracks())
                    track.stop();
            }

            if (vosklet != null)
                vosklet.cleanUp();
        }
    }

    function readValue(key, options, func) {
        let ptr = key.ptr;

        if (!ptr.hasOwnProperty(key.name)) {
            let value = normalizeValue(func, options.value);

            ptr[key.name] = value;
            key.retain.follow_default.add(key.name);

            return value;
        } else if (options.value != null && key.retain.follow_default.has(key.name)) {
            let value = normalizeValue(func, options.value);

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
        let ptr = key.ptr;
        if (value === ptr[key.name])
            return;
        ptr[key.name] = value;

        key.retain.take_delayed.delete(key.name);
        key.retain.follow_default.delete(key.name);
        key.retain.just_changed.add(key.name);

        if (refresh) {
            state.markInteraction();
            self.restart();
        }
    }

    function isModifiable(intf) {
        if (intf.ptr != null)
            intf = intf.variables.get(intf.name);

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
