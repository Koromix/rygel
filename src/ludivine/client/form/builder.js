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

import { render, html, live } from '../../../../vendor/lit-html/lit-html.bundle.js';
import { Util, Log } from '../../../web/core/base.js';
import { wrap, annotate } from './data.js';
import { ASSETS } from '../../assets/assets.js';

let unique = 0;

function FormContext(init = {}) {
    let self = this;

    let change_handler = () => {};

    let [raw, values] = wrap(init);

    Object.defineProperties(this, {
        changeHandler: { get: () => change_handler, set: handler => { change_handler = handler; }, enumerable: true },

        raw: { get: () => raw, enumerable: true },
        values: { get: () => values, enumerable: true }
    });

    this.refresh = function() {
        change_handler();
    };
}

function FormModel() {
    this.parts = [];
    this.widgets = [];
}

function FormBuilder(ctx, model) {
    let self = this;

    let ptr = ctx.values;

    let intro = '';
    let intro_idx = 0;

    let part = null;
    let widgets = model.widgets;

    let refreshing = false;

    Object.defineProperties(this, {
        values: { get: () => ptr, set: values => { ptr = values; }, enumerable: true },
        intro: { get: () => intro, set: content => { intro = content; intro_idx++; }, enumerable: true }
    });

    this.part = function(func) {
        let prev_part = part;

        try {
            part = {
                intro: intro,
                widgets: []
            };

            model.parts.push(part);

            func();
        } finally {
            part = prev_part;
        }
    };

    this.output = function(content) {
        let widget = makeWidget('output', () => content);
        return widget;
    };

    this.text = function(key, label, options = {}) {
        options = expandOptions(options, {
            placeholder: null
        });
        key = expandKey(key, options);

        let value = getValue(key, 'text', options);

        let widget = makeInput(key, label, options, 'text', () => html`
            <input type="text" ?disabled=${options.disabled}
                   .value=${live(value ?? '')} placeholder=${options.placeholder ?? ''}
                   @input=${e => setValue(key, e.target.value || undefined)} />
        `);

        function input(e) {
            let value = e.target.value || undefined;
            setValue(key, value);
        }

        return widget;
    };

    this.number = function(key, label, options = {}) {
        options = expandOptions(options, {
            placeholder: null,
            min: null,
            max: null,
            prefix: null,
            suffix: null
        });
        key = expandKey(key, options);

        let value = getValue(key, 'number', options);

        let widget = makeInput(key, label, options, 'number', () => html`
            <div class="number">
                ${makePrefixOrSuffix(options.prefix, value)}
                <input type="number" ?disabled=${options.disabled}
                       .value=${live(value ?? '')} placeholder=${options.placeholder ?? ''}
                       min=${options.min} max=${options.max} @input=${input} />
                ${makePrefixOrSuffix(options.suffix, value)}
            </div>
        `);

        function input(e) {
            let value = e.target.value ?? undefined;
            if (value != null)
                value = parseFloat(value);
            setValue(key, value);
        }

        return widget;
    };

    this.binary = function(key, label, options = {}) {
        let props = [
            [1, 'Oui'],
            [0, 'Non']
        ];

        return self.enumButtons(key, label, props, options);
    }

    this.enum = function(key, label, props, options = {}) {
        options = expandOptions(options, {
            enum: 'buttons'
        });
        key = expandKey(key, options);

        switch (options.enum) {
            case 'buttons': return self.enumButtons(key, label, props, options);
            case 'radio': return self.enumRadio(key, label, props, options);
            case 'drop': return self.enumDrop(key, label, props, options);
            default: throw new Error(`Invalid enum layout '${options.enum}'`);
        }
    };

    this.enumButtons = function(key, label, props, options = {}) {
        options = expandOptions(options);
        key = expandKey(key, options);

        props = normalizePropositions(props);

        let value = getValue(key, 'enum', options);

        let widget = makeInput(key, label, options, 'enumRadio', () => html`
            <div class="enum">
                ${props.map(prop => {
                    let active = (value === prop[0]);

                    return html`
                        <button type="button" ?disabled=${options.disabled}
                                class=${active ? 'active' : ''} @click=${click}>${prop[1]}</button>
                    `;

                    function click(e) {
                        let value = !active ? prop[0] : null;
                        setValue(key, value);
                    }
                })}
            </div>
        `);

        return widget;
    };

    this.enumRadio = function(key, label, props, options = {}) {
        options = expandOptions(options);
        key = expandKey(key, options);

        props = normalizePropositions(props);

        let value = getValue(key, 'enum', options);

        let widget = makeInput(key, label, options, 'enumButtons', id => html`
            <div>
                ${props.map(prop => {
                    let active = (value === prop[0]);

                    return html`
                        <label id=${id}>
                            <input type="radio" name=${id} ?disabled=${options.disabled}
                                   .checked=${live(active)} @click=${click} />
                            <span>${prop[1]}</span>
                       </label>
                    `;

                    function click(e) {
                        if (active)
                            e.target.checked = false;

                        let value = e.target.checked ? prop[0] : null;
                        setValue(key, value);
                    }
                })}
            </div>
        `);

        return widget;
    };

    this.enumDrop = function(key, label, props, options = {}) {
        options = expandOptions(options);
        key = expandKey(key, options);

        props = normalizePropositions(props);

        let value = getValue(key, 'enum', options);

        let widget = makeInput(key, label, options, 'enumDrop', () => html`
            <select @change=${change} ?disabled=${options.disabled}>
                <option value="" .selected=${live(value == null)}>-- Non renseigné --</option>
                ${props.map((prop, idx) => {
                    let active = (value === prop[0]);
                    return html`<option value=${idx} .selected=${live(active)}>${prop[1]}</button>`;
                })}
            </select>
        `);

        function change(e) {
            let idx = (e.target.value != '') ? parseInt(e.target.value, 10) : -1;
            let value = props[idx]?.[0];

            setValue(key, value);
        }

        return widget;
    };

    this.multi = function(key, label, props, options = {}) {
        options = expandOptions(options, {
            multi: 'check'
        });

        switch (options.multi) {
            case 'buttons': return self.multiButtons(key, label, props, options);
            case 'check': return self.multiCheck(key, label, props, options);
            default: throw new Error(`Invalid multi layout '${options.multi}'`);
        }
    };

    this.multiButtons = function(key, label, props, options = {}) {
        options = expandOptions(options);
        key = expandKey(key, options);

        props = normalizePropositions(props);

        let value = getValue(key, 'multi', options);
        let values = value;

        if (values == null) {
            values = [];
            values[key] = null;
        } else if (!Array.isArray(values)) {
            values = [values];
            values[key] = values;
        }

        let widget = makeInput(key, label, options, 'multiButtons', () => html`
            <div class="enum">
                ${props.map(prop => {
                    let active = values.includes(prop[0]);

                    return html`
                        <button type="button" ?disabled=${options.disabled}
                                class=${active ? 'active' : ''} @click=${click}>${prop[1]}</button>
                    `;

                    function click(e) {
                        values = props.map(prop => prop[0]).filter(key => {
                            if (key == prop[0])
                                return !active;

                            return values.includes(key);
                        });

                        if (!active) {
                            let allow_null = (prop[0] == null);

                            values = values.filter(value => {
                                let is_null = (value == null);
                                return is_null == allow_null;
                            });
                        }

                        if (!values.length)
                            values = null;

                        setValue(key, values);
                    }
                })}
            </div>
        `);

        return widget;
    };

    this.multiCheck = function(key, label, props, options = {}) {
        options = expandOptions(options);
        key = expandKey(key, options);

        props = normalizePropositions(props);

        let value = getValue(key, 'multi', options);
        let values = value;

        if (values == null) {
            values = [];
            values[key] = null;
        } else if (!Array.isArray(values)) {
            values = [values];
            values[key] = values;
        }

        let widget = makeInput(key, label, options, 'multiCheck', id => html`
            <div>
                ${props.map(prop => {
                    let active = values.includes(prop[0]);

                    return html`
                        <label id=${id}>
                            <input type="checkbox" ?disabled=${options.disabled}
                                   .checked=${live(values.includes(prop[0]))} @click=${click} />
                            <span>${prop[1]}</span>
                        </label>
                    `;

                    function click(e) {
                        if (active)
                            e.target.checked = false;

                        values = props.map(prop => prop[0]).filter(key => {
                            if (key == prop[0]) {
                                return e.target.checked;
                            } else {
                                return values.includes(key);
                            }
                        });

                        if (e.target.checked) {
                            let allow_null = (prop[0] == null);

                            values = values.filter(value => {
                                let is_null = (value == null);
                                return is_null == allow_null;
                            });
                        }

                        if (!values.length)
                            values = null;

                        setValue(key, values);
                    }
                })}
            </div>
        `);

        return widget;
    };

    this.slider = function(key, label, options = {}) {
        options = expandOptions(options, {
            min: 0,
            max: 10,
            prefix: null,
            suffix: null
        });
        key = expandKey(key, options);

        let value = getValue(key, 'number', options);

        let widget = makeInput(key, label, options, 'slider', id => html`
            <div class="slider">
                ${makePrefixOrSuffix(options.prefix, value)}
                <input id=${id} type="range" ?disabled=${options.disabled}
                       class=${value == null ? 'missing' : ''}
                       .value=${live(value ?? '')}
                       min=${options.min} max=${options.max} @input=${input} />
                ${makePrefixOrSuffix(options.suffix, value)}
            </div>
        `);

        function input(e) {
            let value = e.target.value ?? undefined;
            if (value != null)
                value = parseFloat(value);
            setValue(key, value);
        }

        return widget;
    };

    this.error = function(key, msg) {
        let notes = annotate(ptr, key);

        if (notes == null)
            throw new Error(`Unkwown variable '${key}'`);

        notes.error = msg;

        refresh();
    };

    function makeWidget(type, func) {
        let widget = {
            type: type,
            render: func
        };

        if (part == null)
            self.part(() => {});
        part.widgets.push(widget);

        widgets.push(widget);

        return widget;
    }

    function makeInput(key, label, options, type, func) {
        let id = 'input' + (++unique);

        let notes = annotate(key.obj, key.name);

        let render = () => {
            let error = notes.error;
            let skippable = notes.hasOwnProperty('skip') && (error == null);
            let annotated = skippable || (error != null);

            let cls = 'widget' + (annotated ? ' annotate' : '');

            let tip = options.tip ?? options.help // For compatibility

            return html`
                <div class=${cls}>
                    <label for=${id}>${label}</label>
                    ${func(id)}
                    ${tip ? html`<div class="tip">${tip}</div>` : ''}
                    ${skippable ? html`
                        <label>
                            <span>Je souhaite apporter un commentaire (non obligatoire) :</span>
                            <textarea rows="4" @input=${comment}>${notes.comment ?? ''}</textarea>
                        </label>
                        <label>
                            <input type="checkbox" ?checked=${!!notes.skip} @click=${skip} />
                            <span>Je ne souhaite pas répondre à cette question</span>
                        </label>
                    ` : ''}
                    ${error != null ? html`<div class="error">${error}</div>` : ''}
                </div>
            `;

            function skip(e) {
                notes.skip = e.target.checked;
            }

            function comment(e) {
                notes.comment = e.target.value || null;
            }
        };

        let widget = makeWidget(type, render);

        Object.assign(widget, {
            key: key,
            label: label,

            disabled: options.disabled,
            optional: options.optional
        });

        return widget;
    }

    function makePrefixOrSuffix(text, value) {
        if (typeof text == 'function')
            text = text(value);

        if (!text)
            return '';

        return html`<span>${text}</span>`;
    }

    function expandOptions(options, defaults = {}) {
        options = Object.assign({
            disabled: false,
            optional: false,
            tip: null
        }, defaults, options);

        return options;
    }

    function expandKey(key, options) {
        if (key.startsWith('?')) {
            key = key.substr(1);
            options.optional = true;
        }

        key = {
            obj: ptr,
            name: key
        };

        return key;
    }

    function normalizePropositions(props) {
        if (Util.isPodObject(props)) {
            props = Object.keys(props).map(key => [key, props[key]]);
        } else if (Array.isArray(props)) {
            props = props.map(prop => {
                if (typeof prop == 'string') {
                    prop = prop.split(':::');
                } else if (typeof prop == 'number') {
                    prop = [prop, prop];
                }

                let [key, label] = prop;
                return [key, label ?? key];
            });
        } else {
            throw new Error('Invalid propositions');
        }

        return props;
    }

    function getValue(key, type, options) {
        let notes = annotate(key.obj, key.name);

        if (notes == null) {
            key.obj[key.name] = options.prefill ?? undefined;
            notes = annotate(key.obj, key.name);
        } else if (!notes.changed) {
            key.obj[key.name] = options.prefill ?? undefined;
        }

        notes.changed = !!notes.changed;
        notes.type = type;
        notes.disabled = options.disabled;

        let value = key.obj[key.name];

        // Clear annotations
        delete notes.error;
        if (value != null)
            delete notes.skip;

        return value;
    }

    function setValue(key, value) {
        let notes = annotate(key.obj, key.name);

        key.obj[key.name] = value;
        notes.changed = true;

        refresh();
    }

    function refresh() {
        if (refreshing)
            return;

        refreshing = true;
        setTimeout(() => ctx.refresh(), 0);
    }
}

export {
    FormContext,
    FormModel,
    FormBuilder
}
