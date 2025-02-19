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

    let part = [];
    let widgets = model.widgets;

    model.parts.push(part);

    this.part = function(func) {
        if (part.length) {
            part = [];
            model.parts.push(part);
        }

        func();
    };

    this.output = function(content) {
        let widget = makeWidget('output', () => content);
        return widget;
    };

    this.text = function(key, label, options = {}) {
        options = expandOptions(options, {
            placeholder: null
        });

        let value = getValue(key, 'text', options);

        let widget = makeInput(key, label, 'text', () => html`
            <input type="text" value=${live(value ?? '')} placeholder=${options.placeholder ?? ''}
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
            max: null
        });

        let value = getValue(key, 'number', options);

        let widget = makeInput(key, label, 'number', () => html`
            <input type="number" value=${live(value ?? '')} placeholder=${options.placeholder ?? ''}
                   min=${options.min} max=${options.max} @input=${input} />
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

    this.enum = function(key, label, options = {}) {
        options = expandOptions(options, {
            enum: 'buttons'
        });

        switch (options.enum) {
            case 'buttons': return self.enumButtons(key, label, options);
            case 'radio': return self.enumRadio(key, label, options);
            default: throw new Error(`Invalid enum layout '${options.enum}'`);
        }
    };

    this.enumButtons = function(key, label, props, options = {}) {
        options = expandOptions(options);
        props = normalizePropositions(props);

        let value = getValue(key, 'enum', options);

        let widget = makeInput(key, label, 'enumRadio', () => html`
            <div class="enum">
                ${props.map(prop => {
                    return html`
                        <button type="button" class=${value === prop[0] ? 'active' : ''}
                                @click=${click}>${prop[1]}</button>
                    `;

                    function click(e) {
                        setValue(key, prop[0]);
                    }
                })}
            </div>
        `);

        return widget;
    };

    this.enumRadio = function(key, label, props, options = {}) {
        options = expandOptions(options);
        props = normalizePropositions(props);

        let value = getValue(key, 'enum', options);

        let widget = makeInput(key, label, 'enumButtons', id => props.map(prop => {
            return html`
                <label>
                    <input type="radio" name=${id} ?checked=${live(value === prop[0])}
                           @clicked=${click} />
                    <span>${prop[1]}</span>
               </label>
            `;

            function click(e) {
                setValue(key, prop[0]);
            }
        }));

        return widget;
    };

    this.multi = function(key, label, options = {}) {
        options = expandOptions(options, {
            multi: 'check'
        });

        switch (options.multi) {
            case 'check': return self.multiCheck(key, label, options);
            default: throw new Error(`Invalid multi layout '${options.multi}'`);
        }
    };

    this.multiCheck = function(key, label, props, options = {}) {
        options = expandOptions(options);
        props = normalizePropositions(props);

        let value = getValue(key, 'multi', options);

        let widget = makeInput(key, label, 'multiCheck', () => props.map(prop => {
            return html`
                <input type="checkbox" ?checked=${live(value === prop[0])}
                       @clicked=${click} />
            `;

            function click(e) {
                setValue(key, prop[0]);
            }
        }));

        return widget;
    };

    this.slider = function(key, label, options = {}) {
        options = expandOptions(options, {
            min: 0,
            max: 10
        });

        let value = getValue(key, 'number', options);

        let widget = makeInput(key, label, 'slider', () => html`
            <input type="slider" value=${live(value ?? '')}
                   min=${options.min} max=${options.max} @input=${input} />
        `);

        function input(e) {
            let value = e.target.value ?? undefined;
            if (value != null)
                value = parseFloat(value);
            setValue(key, value);
        }

        return widget;
    };

    function makeWidget(type, func) {
        let widget = {
            type: type,
            render: func
        };

        part.push(widget);
        widgets.push(widget);

        return widget;
    }

    function makeInput(key, label, type, func) {
        let id = 'input' + (++unique);

        let render = () => html`
            <label for=${id}>${label}</label>
            ${func(id)}
        `;

        return makeWidget(type, render);
    }

    function expandOptions(options, defaults = {}) {
        options = Object.assign({}, defaults, options);
        return options;
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
        let notes = annotate(ctx.values, key);

        if (notes == null) {
            ctx.values[key] = options.prefill ?? undefined;
            notes = annotate(ctx.values, key);
        } else if (!notes.changed) {
            ctx.values[key] = options.prefill ?? undefined;
        }

        notes.changed = !!notes.changed;
        notes.type = type;

        return ctx.values[key];
    }

    function setValue(key, value) {
        let notes = annotate(ctx.values, key);

        ctx.values[key] = value;
        notes.changed = true;

        ctx.refresh();
    }
}

export {
    FormContext,
    FormModel,
    FormBuilder
}
