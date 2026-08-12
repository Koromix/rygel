// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 Niels Martignène <niels.martignene@protonmail.com>

import { render, html, live, classMap } from 'vendor/lit-html/lit-html.bundle.js';
import { Util, Log, LocalDate, LocalTime } from 'lib/web/base/base.js';
import * as Data from 'lib/web/base/data.js';

if (typeof T == 'undefined')
    globalThis.T = {};

Object.assign(T, {
    missing_value_choice: '-- Value not provided --',
    no: 'No',
    yes: 'Yes'
});

let ids = new WeakMap;
let next_id = 0;

let states = new WeakMap;

// Used in some cases to detect unset values
let unset = Symbol();

function FormState(init = {}) {
    let self = this;

    let change_handler = () => {};

    let [raw, values] = Data.wrap(init);

    Object.defineProperties(this, {
        changeHandler: { get: () => change_handler, set: handler => { change_handler = handler; }, enumerable: true },

        raw: { get: () => raw, enumerable: true },
        values: { get: () => values, enumerable: true }
    });

    this.refresh = function () {
        change_handler();
    };
}

function FormModel() {
    this.parts = [];
    this.widgets = [];
    this.variables = [];
}

function FormBuilder(state, model, base = {}) {
    let self = this;

    let ptr = state.values;

    let intro = null;
    let intro_idx = -1;

    let part = null;

    let wrap_widget = null;
    let wrap_block = false;

    let refreshing = false;

    Object.defineProperties(this, {
        values: { get: () => ptr, set: values => { ptr = values; }, enumerable: true },
        intro: { get: () => intro, set: content => { intro = content; intro_idx++; }, enumerable: true }
    });

    this.section = function (title = null, func = null) {
        if (typeof title == 'function') {
            func = title;
            title = null;
        }
        title = title || null;

        let prev_part = part;

        part = {
            intro: intro,
            intro_idx: intro_idx,

            title: title,
            widgets: [],
            variables: []
        };

        model.parts.push(part);

        if (func != null) {
            try {
                func();
            } finally {
                part = prev_part;

                wrap_widget = null;
                wrap_block = false;
            }
        }
    };

    this.wrap = function (func = null) {
        if (wrap_widget != null)
            throw new Error(T.message(`Cannot nest widget wraps`));

        if (func != null) {
            wrap_widget = null;
            wrap_block = true;

            try {
                func();
            } finally {
                wrap_widget = null;
                wrap_block = false;
            }
        } else {
            if (!part?.widgets?.length)
                throw new Error(T.message(`Cannot use form.wrap() without active widget`));

            wrap_widget = part.widgets[part.widgets.length - 1];
            wrap_block = false;
        }
    };

    this.output = function (content, options = {}) {
        options = expandOptions(options);
        makeWidget(options.wrap, false, () => html`<div>${content}</div>`);
    };

    this.text = function (key, label, options = {}) {
        options = expandOptions(options, {
            placeholder: null
        });
        key = expandKey(key, options);

        let value = getValue(key, 'text', options);

        let widget = makeInput(key, label, options, () => html`
            <input type="text" ?disabled=${options.disabled} ?readonly=${options.readonly}
                   .value=${live(value ?? '')} placeholder=${options.placeholder ?? ''}
                   @input=${input} />
        `);

        function input(e) {
            let value = e.target.value || undefined;
            setValue(key, value, options);
        }

        return widget;
    };

    this.textArea = function (key, label, options = {}) {
        options = expandOptions(options, {
            placeholder: null,
            rows: 4
        });
        key = expandKey(key, options);

        let value = getValue(key, 'text', options);

        let widget = makeInput(key, label, options, () => html`
            <textarea rows=${options.rows} ?disabled=${options.disabled} ?readonly=${options.readonly}
                      .value=${live(value ?? '')} placeholder=${options.placeholder || ''}
                      @input=${input}></textarea>
        `);

        function input(e) {
            let value = e.target.value || undefined;
            setValue(key, value, options);
        }

        return widget;
    };

    this.number = function (key, label, options = {}) {
        options = expandOptions(options, {
            placeholder: null,
            min: null,
            max: null,
            prefix: null,
            suffix: null
        });
        key = expandKey(key, options);

        let value = getValue(key, 'number', options);

        let widget = makeInput(key, label, options, () => html`
            <div class="number">
                ${makePrefixOrSuffix(options.prefix, value)}
                <input type="number" ?disabled=${options.disabled} ?readonly=${options.readonly}
                       .value=${live(value ?? '')} placeholder=${options.placeholder ?? ''}
                       min=${options.min} max=${options.max} @input=${input} />
                ${makePrefixOrSuffix(options.suffix, value)}
            </div>
        `);

        function input(e) {
            let value = e.target.value ?? undefined;

            if (value != null)
                value = parseFloat(value);
            if (Number.isNaN(value))
                value = undefined;

            setValue(key, value, options);
        }

        return widget;
    };

    this.binary = function (key, label, options = {}) {
        let props = [
            [1, T.yes],
            [0, T.no]
        ];

        return self.enumButtons(key, label, props, options);
    };

    this.enum = function (key, label, props, options = {}) {
        options = expandOptions(options, {
            enum: 'buttons'
        });

        switch (options.enum) {
            case 'buttons': return self.enumButtons(key, label, props, options);
            case 'radio': return self.enumRadio(key, label, props, options);
            case 'drop': return self.enumDrop(key, label, props, options);
            default: throw new Error(`Invalid enum layout '${options.enum}'`);
        }
    };

    this.enumButtons = function (key, label, choices, options = {}) {
        options = expandOptions(options);
        key = expandKey(key, options);

        let [, props] = normalizePropositions(choices);

        let value = getValue(key, 'enum', options);

        let widget = makeInput(key, label, options, () => html`
            <div class="enum">
                ${props.map((prop, idx) => {
                    let active = (value === prop[0]);

                    let cls = {
                        active: active,
                        readonly: options.readonly
                    };

                    return html`
                        <button type="button" ?disabled=${options.disabled}
                                class=${live(classMap(cls))} tabindex=${idx ? -1 : 0}
                                @click=${click} @keydown=${keydown}>
                            ${prop[1]}
                            ${prop[2].help ? html`<div class="sub">${prop[2].help}</div>` : ''}
                        </button>
                    `;

                    function click(e) {
                        let value = !active ? prop[0] : null;
                        setValue(key, value, options);
                    }
                })}
            </div>
        `);

        function keydown(e) {
            if (e.code == 'ArrowLeft') { // left
                let prev = e.target.previousElementSibling;
                if (prev != null)
                    prev.focus();
                e.preventDefault();
            } else if (e.code == 'ArrowRight') { // right
                let next = e.target.nextElementSibling;
                if (next != null)
                    next.focus();
                e.preventDefault();
            }
        }

        widget.props = props;

        return widget;
    };

    this.enumRadio = function (key, label, choices, options = {}) {
        options = expandOptions(options);
        key = expandKey(key, options);

        let [groups, props] = normalizePropositions(choices);
        let value = getValue(key, 'enum', options);

        let counter = 0;

        let widget = makeInput(key, label, options, id => groups.map(group => html`
            <div class=${group.title != null ? 'group' : ''}>
                ${group.title ? html`<div class="title">${group.title}</div>` : ''}
                ${group.props.map(prop => {
                    let idx = counter++;
                    let active = (value === prop[0]);

                    return html`
                        <label id=${!idx ? id : ''} class=${options.readonly ? 'readonly' : ''}>
                            <input type="radio" name=${id}
                                   ?disabled=${options.disabled}
                                   .checked=${live(active)} @click=${click} />
                            <span>
                                ${prop[1]}
                                ${prop[2].help ? html`<span class="sub">(${prop[2].help})</span>` : ''}
                            </span>
                       </label>
                    `;

                    function click(e) {
                        let value = active ? null : prop[0];

                        if (setValue(key, value, options)) {
                            if (active)
                                e.target.checked = false;
                        } else {
                            e.preventDefault();
                        }
                    }
                })}
            </div>
        `));

        widget.props = props;

        return widget;
    };

    this.enumDrop = function (key, label, choices, options = {}) {
        options = expandOptions(options, {
            placeholder: T.missing_value_choice
        });
        key = expandKey(key, options);

        let [groups, props] = normalizePropositions(choices);
        let value = getValue(key, 'enum', options);

        let counter = 0;

        let widget = makeInput(key, label, options, id => html`
            <select id=${id} @change=${change} ?disabled=${options.disabled}>
                ${options.placeholder != null ?
                    html`<option value="" .selected=${live(value == null)}>${options.placeholder}</option>` : ''}
                ${groups.map(group => {
                    let children = group.props.map(prop => {
                        let idx = counter++;
                        let active = (value === prop[0]);

                        return html`
                            <option value=${idx} ?disabled=${options.readonly && value !== p.value}
                                                 .selected=${live(active)}>
                                ${prop[1]}
                                ${prop[2].help ? html`<span class="sub">(${prop[2].help})</span>` : ''}
                            </option>`;
                    });

                    if (group.title != null) {
                        return html`
                            <optgroup label=${group.title}>
                                ${children}
                            </optgroup>
                        `
                    } else {
                        return children;
                    }
                })}
            </select>
        `);

        widget.props = props;

        function change(e) {
            let idx = (e.target.value != '') ? parseInt(e.target.value, 10) : -1;
            let value = props[idx]?.[0];

            setValue(key, value, options);
        }

        return widget;
    };

    this.multi = function (key, label, props, options = {}) {
        options = expandOptions(options, {
            multi: 'check'
        });

        switch (options.multi) {
            case 'buttons': return self.multiButtons(key, label, props, options);
            case 'check': return self.multiCheck(key, label, props, options);
            default: throw new Error(`Invalid multi layout '${options.multi}'`);
        }
    };

    this.multiButtons = function (key, label, choices, options = {}) {
        options = expandOptions(options);
        key = expandKey(key, options);

        let [, props] = normalizePropositions(choices);

        let value = getValue(key, 'multi', options);
        let values = value;

        if (values == null) {
            values = [];
            values[key] = null;
        } else if (!Array.isArray(values)) {
            values = [values];
            values[key] = values;
        }

        let widget = makeInput(key, label, options, () => html`
            <div class="enum">
                ${props.map((prop, idx) => {
                    let active = values.includes(prop[0]);

                    let cls = {
                        active: active,
                        readonly: options.readonly
                    };

                    return html`
                        <button type="button" ?disabled=${options.disabled}
                                class=${live(classMap(cls))} tabindex=${idx ? -1 : 0}
                                @click=${click} @keydown=${keydown}>
                            ${prop[1]}
                            ${prop[2].help ? html`<div class="sub">${prop[2].help}</div>` : ''}
                        </button>
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

                        setValue(key, values, options);
                    }
                })}
            </div>
        `);

        function keydown(e) {
            if (e.code == 'ArrowLeft') { // left
                let prev = e.target.previousElementSibling;
                if (prev != null)
                    prev.focus();
                e.preventDefault();
            } else if (e.code == 'ArrowRight') { // right
                let next = e.target.nextElementSibling;
                if (next != null)
                    next.focus();
                e.preventDefault();
            }
        }

        widget.props = props;

        return widget;
    };

    this.multiCheck = function (key, label, choices, options = {}) {
        options = expandOptions(options);
        key = expandKey(key, options);

        let [groups, props] = normalizePropositions(choices);

        let value = getValue(key, 'multi', options);
        let values = value;

        if (values == null) {
            values = [];
            values[key] = null;
        } else if (!Array.isArray(values)) {
            values = [values];
            values[key] = values;
        }

        let counter = 0;

        let widget = makeInput(key, label, options, id => groups.map(group => html`
            <div class=${group.title != null ? 'group' : ''}>
                ${group.title ? html`<div class="title">${group.title}</div>` : ''}
                ${group.props.map(prop => {
                    let idx = counter++;
                    let active = values.includes(prop[0]);

                    return html`
                        <label id=${!idx ? id : ''} class=${options.readonly ? 'readonly' : ''}>
                            <input type="checkbox"
                                   ?disabled=${options.disabled}
                                   .checked=${live(values.includes(prop[0]))} @click=${click} />
                            <span>
                                ${prop[1]}
                                ${prop[2].help ? html`<span class="sub">(${prop[2].help})</span>` : ''}
                            </span>
                        </label>
                    `;

                    function click(e) {
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

                        if (setValue(key, values, options)) {
                            if (active)
                                e.target.checked = false;
                        } else {
                            e.preventDefault();
                        }
                    }
                })}
            </div>
        `));

        widget.props = props;

        return widget;
    };

    this.slider = function (key, label, options = {}) {
        options = expandOptions(options, {
            min: 0,
            max: 10,
            prefix: unset,
            suffix: unset,
            label: null
        });
        key = expandKey(key, options);

        let value = getValue(key, 'number', options);

        if (options.prefix == unset)
            options.prefix = options.min;
        if (options.suffix == unset)
            options.suffix = options.max;
        if (!options.label)
            options.label = null;
        if (options.label && typeof options.label != 'function')
            options.label = value => value;

        let mark = null;

        if (options.label != null) {
            mark = document.createElement('span');

            if (value != null) {
                mark.style.setProperty('--position', position(value));
                render(options.label(value), mark);
            }
        }

        let widget = makeInput(key, label, options, id => html`
            <div class=${'slider' + (options.readonly ? ' readonly' : '')}>
                ${makePrefixOrSuffix(options.prefix, value)}
                <div>
                    <input id=${id} type="range"
                           ?disabled=${options.disabled}
                           class=${value == null ? 'missing' : ''}
                           .value=${live(value ?? '')}
                           min=${options.min} max=${options.max} @input=${input} />
                    ${mark ?? ''}
                </div>
                ${makePrefixOrSuffix(options.suffix, value)}
            </div>
        `);

        function input(e) {
            let value = e.target.value ?? undefined;

            if (value != null)
                value = parseFloat(value);
            if (Number.isNaN(value))
                value = undefined;

            if (mark != null) {
                if (value != null) {
                    mark.style.setProperty('--position', position(value));
                    render(options.label(value), mark);
                } else {
                    render('', mark);
                }
            }

            setValue(key, value, options);
        }

        function position(value) {
            let position = Util.clamp((value - options.min) / (options.max - options.min), 0, 1);
            return position;
        }

        return widget;
    };

    this.date = function (key, label, options = {}) {
        options = expandOptions(options, {
            min: null,
            max: null,
            prefix: null,
            suffix: null
        });
        key = expandKey(key, options);

        let value = getValue(key, 'date', options);
        let date = value;

        if (!(date instanceof LocalDate))
            date = null;
        if (date != null && !date.isValid())
            date = null;

        let state = useState(key, 'date');

        ready();

        let widget = makeInput(key, label, options, id => html`
            <div class="date">
                ${makePrefixOrSuffix(options.prefix, value)}
                <input id=${id + '.d'} type="text"
                       ?disabled=${options.disabled} ?readonly=${options.readonly}
                       class=${date == null ? 'day missing' : 'day'}
                       pattern="[0-9]*" inputmode="numeric" placeholder="dd" maxlength="2"
                       .value=${live(state.day ?? '')}
                       @focus=${ready} @input=${input} @keydown=${keydown} @change=${change} />
                <input id=${id + '.m'} type="text"
                       ?disabled=${options.disabled} ?readonly=${options.readonly}
                       class=${date == null ? 'month missing' : 'month'} tabindex="-1"
                       pattern="[0-9]*" inputmode="numeric" placeholder="mm" maxlength="2"
                       .value=${live(state.month ?? '')}
                       @focus=${ready} @input=${input} @keydown=${keydown} @change=${change} />
                <input id=${id + '.y'} type="text"
                       ?disabled=${options.disabled} ?readonly=${options.readonly}
                       class=${date == null ? 'year missing' : 'year'} tabindex="-1"
                       pattern="[0-9]*" inputmode="numeric" placeholder="yyyy" maxlength="4"
                       .value=${live(state.year ?? '')}
                       @focus=${ready} @input=${input} @keydown=${keydown} @change=${change} />
                ${makePrefixOrSuffix(options.suffix, value)}
            </div>
        `);

        function ready() {
            state.year ??= date?.year?.toString?.()?.padStart?.(4, '0');
            state.month ??= date?.month?.toString?.()?.padStart?.(2, '0');
            state.day ??= date?.day?.toString?.()?.padStart?.(2, '0');;
        }

        function input(e) {
            let inputs = Array.from(e.target.parentNode.querySelectorAll('input'));

            state.year = inputs[2].value;
            state.month = inputs[1].value;
            state.day = inputs[0].value;

            let [year, month, day] = [state.year, state.month, state.day].map(value => {
                value = parseInt(value, 10);
                return !Number.isNaN(value) ? value : null;
            });
            if (year < 1000 || year > 9999)
                year = undefined;

            if (year != null && month != null && day != null) {
                let date = new LocalDate(year, month, day);
                if (!date.isValid())
                    date = null;
                setValue(key, date, options);
            } else {
                setValue(key, undefined, options);
            }

            if (e.target.selectionStart == e.target.selectionEnd && e.target.selectionStart == e.target.maxLength) {
                let idx = inputs.indexOf(e.target);
                let next = inputs[idx + 1];

                if (next != null) {
                    next.setSelectionRange(0, 0);
                    next.focus();
                }
            }
        }

        function keydown(e) {
            let inputs = Array.from(e.target.parentNode.querySelectorAll('input'));

            let idx = inputs.indexOf(e.target);
            let pos = (e.target.selectionStart == e.target.selectionEnd) ? e.target.selectionStart : null;
            let length = e.target.value?.length ?? 0;

            if ((e.code == 'ArrowLeft' || e.code == 'Backspace') && idx && !pos) {
                let prev = inputs[idx - 1];
                let end = prev.value.length;

                prev.setSelectionRange(end, end);
                prev.focus();

                e.preventDefault();
            } else if (e.code == 'ArrowRight' && idx < inputs.length - 1 && pos == length) {
                let next = inputs[idx + 1];

                next.setSelectionRange(0, 0);
                next.focus();

                e.preventDefault();
            } else if (e.key >= '0' && e.key <= '9' && pos >= 0 && pos < length && length == e.target.maxLength) {
                e.target.value = e.target.value.substr(0, pos) + e.target.value.substr(pos + 1);
                e.target.setSelectionRange(pos, pos);
            }
        }

        function change() {
            let value = getValue(key, 'date', options);

            if (value instanceof LocalDate && value.isValid()) {
                resetState(key);
                refresh();
            }
        }

        return widget;
    };

    this.time = function (key, label, options = {}) {
        options = expandOptions(options, {
            min: null,
            max: null,
            prefix: null,
            suffix: null,
            seconds: false
        });
        key = expandKey(key, options);

        let value = getValue(key, 'time', options);
        let time = value;

        if (!(time instanceof LocalTime))
            time = null;
        if (time != null && !time.isValid())
            time = null;

        let state = useState(key, 'time');

        ready();

        let widget = makeInput(key, label, options, id => html`
            <div class="time">
                ${makePrefixOrSuffix(options.prefix, value)}
                <input id=${id + '.h'} type="text"
                       ?disabled=${options.disabled} ?readonly=${options.readonly}
                       class=${time == null ? 'hour missing' : 'hour'}
                       pattern="[0-9]*" inputmode="numeric" placeholder="hh" maxlength="2"
                       .value=${live(state.hour ?? '')}
                       @focus=${ready} @input=${input} @keydown=${keydown} @change=${change} />
                <input id=${id + '.m'} type="text"
                       ?disabled=${options.disabled} ?readonly=${options.readonly}
                       class=${time == null ? 'minute missing' : 'minute'} tabindex="-1"
                       pattern="[0-9]*" inputmode="numeric" placeholder="mm" maxlength="2"
                       .value=${live(state.minute ?? '')}
                       @focus=${ready} @input=${input} @keydown=${keydown} @change=${change} />
                ${options.seconds ? html`
                    <input id=${id + '.s'} type="text"
                        ?disabled=${options.disabled} ?readonly=${options.readonly}
                        class=${time == null ? 'second missing' : 'second'} tabindex="-1"
                        pattern="[0-9]*" inputmode="numeric" placeholder="ss" maxlength="2"
                        .value=${live(state.second ?? '')}
                        @focus=${ready} @input=${input} @keydown=${keydown} @change=${change} />
                ` : ''}
                ${makePrefixOrSuffix(options.suffix, value)}
            </div>
        `);

        function ready() {
            state.hour ??= time?.hour?.toString?.()?.padStart?.(2, '0');
            state.minute ??= time?.minute?.toString?.()?.padStart?.(2, '0');
            state.second ??= time?.second?.toString?.()?.padStart?.(2, '0');
        }

        function input(e) {
            let inputs = Array.from(e.target.parentNode.querySelectorAll('input'));

            state.hour = inputs[0].value;
            state.minute = inputs[1].value;
            state.second = inputs[2]?.value ?? '00';

            let [hour, minute, second] = [state.hour, state.minute, state.second].map(value => {
                value = parseInt(value, 10);
                return !Number.isNaN(value) ? value : null;
            });

            if (hour != null && minute != null && second != null) {
                let time = new LocalTime(hour, minute, second);
                if (!time.isValid())
                    time = null;
                setValue(key, time, options);
            } else {
                setValue(key, undefined, options);
            }

            if (e.target.selectionStart == e.target.selectionEnd && e.target.selectionStart == e.target.maxLength) {
                let idx = inputs.indexOf(e.target);
                let next = inputs[idx + 1];

                if (next != null) {
                    next.setSelectionRange(0, 0);
                    next.focus();
                }
            }
        }

        function keydown(e) {
            let inputs = Array.from(e.target.parentNode.querySelectorAll('input'));

            let idx = inputs.indexOf(e.target);
            let pos = (e.target.selectionStart == e.target.selectionEnd) ? e.target.selectionStart : null;
            let length = e.target.value?.length ?? 0;

            if ((e.code == 'ArrowLeft' || e.code == 'Backspace') && idx && !pos) {
                let prev = inputs[idx - 1];
                let end = prev.value.length;

                prev.setSelectionRange(end, end);
                prev.focus();

                e.preventDefault();
            } else if (e.code == 'ArrowRight' && idx < inputs.length - 1 && pos == length) {
                let next = inputs[idx + 1];

                next.setSelectionRange(0, 0);
                next.focus();

                e.preventDefault();
            } else if (e.key >= '0' && e.key <= '9' && pos >= 0 && pos < length && length == e.target.maxLength) {
                e.target.value = e.target.value.substr(0, pos) + e.target.value.substr(pos + 1);
                e.target.setSelectionRange(pos, pos);
            }
        }

        function change() {
            let value = getValue(key, 'time', options);

            if (value instanceof LocalTime && value.isValid()) {
                resetState(key);
                refresh();
            }
        }

        return widget;
    };

    this.error = function (key, msg) {
        key = Data.resolveKey(ptr, key);

        let notes = Data.annotate(key.obj, key.name);

        if (notes == null)
            throw new Error(`Unkwown variable '${key}'`);

        notes.error = msg;

        refresh();
    };

    function makeWidget(cls, annotated, func) {
        let widget = wrap_widget;

        if (widget == null) {
            widget = {
                cls: cls,
                annotated: false,
                funcs: [],
                render: () => html`
                    <div class=${widget.cls + (widget.annotated ? ' annotate' : '')}>
                        ${widget.funcs.map(func => func())}
                    </div>
                `
            };

            if (part == null)
                self.section();
            part.widgets.push(widget);
            model.widgets.push(widget);
        }

        wrap_widget = wrap_block ? widget : null;

        widget.annotated ||= annotated;
        widget.funcs.push(func);
    }

    function makeInput(key, label, options, func) {
        let id = useID(key);

        let notes = Data.annotate(key.obj, key.name);

        let error = notes.error;
        let skippable = Object.hasOwn(notes, 'skip') && (error == null);
        let annotated = skippable || (error != null);
        let sub = options.sub;
        let tip = options.tip ?? options.help; // For compatibility

        let render = () => {
            return html`
                <label for=${id} class=${options.disabled ? 'disabled' : ''}>
                    ${label}
                    ${sub ? html`<div class="question">${sub}</div>` : ''}
                </label>
                ${func(id)}
                ${tip ? html`<div class="tip">${tip}</div>` : ''}
                ${skippable ? html`
                    <div class="notes">
                        <label for=${id + '_comment'}>Je souhaite apporter un commentaire :</label>
                        <textarea id=${id + '_comment'} rows="4" @input=${comment}>${notes.comment ?? ''}</textarea>
                        <div class="tip">Non obligatoire</div>
                        <label>
                            <input type="checkbox" ?checked=${!!notes.skip} @click=${skip} />
                            <span>Je souhaite continuer sans répondre à cette question</span>
                        </label>
                    </div>
                ` : ''}
                ${error != null ? html`<div class="error">${error}</div>` : ''}
            `;

            function skip(e) {
                notes.skip = e.target.checked;
            }

            function comment(e) {
                notes.comment = e.target.value || null;
            }
        };

        makeWidget(options.wrap, annotated, render);

        let variable = {
            key: key,
            label: label,

            disabled: options.disabled,
            optional: options.optional
        };
        Object.defineProperties(variable, {
            value: { get: () => key.obj[key.name], set: value => setValue(key, value, options), enumerable: true },
            notes: { get: () => Data.annotate(key.obj, key.name), enumerable: true }
        });

        if (part == null)
            self.section();
        part.variables.push(variable);
        model.variables.push(variable);

        return variable;
    }

    function makePrefixOrSuffix(text, value) {
        if (typeof text == 'function')
            text = text(value);

        if (text == null || text === '')
            return '';

        return html`<span>${text}</span>`;
    }

    function expandOptions(options, defaults = {}) {
        defaults = Object.assign({
            wrap: 'widget',
            disabled: false,
            readonly: false,
            optional: false,
            sub: null,
            tip: null
        }, base, defaults);

        for (let key in defaults) {
            if (options[key] == null)
                options[key] = defaults[key];
        }

        return options;
    }

    function expandKey(key, options) {
        let prefix = 0;

        while (prefix < key.length) {
            let c = key[prefix];

            if (c == '?') {
                options.optional = true;
            } else if (c == '+') {
                if (wrap_widget == null)
                    self.wrap();
            } else {
                break;
            }

            prefix++;
        }

        if (prefix)
            key = key.substr(prefix);

        return Data.resolveKey(ptr, key);
    }

    function normalizePropositions(props) {
        if (Util.isPodObject(props)) {
            let keys = Object.keys(props);

            if (keys.length && typeof props[keys[0]] == 'object') {
                let groups = [];
                let all = [];

                for (let key of keys) {
                    let [, children] = normalizePropositions(props[key]);

                    groups.push({
                        title: key,
                        props: children
                    });
                    all.push(...children);
                }

                return [groups, all];
            }

            props = keys.map(key => {
                let value = props[key];
                return Array.isArray(value) ? [key, ...value] : [key, value, {}];
            });
        } else if (Array.isArray(props)) {
            props = props.map(prop => {
                if (typeof prop == 'string') {
                    prop = prop.split(':::');
                } else if (typeof prop == 'number') {
                    prop = [prop, prop];
                }

                let [key, label, options] = prop;
                return [key, label ?? key, options ?? {}];
            });
        } else {
            throw new Error('Invalid propositions');
        }

        let groups = [{ title: null, props: props }];

        return [groups, props];
    }

    function getValue(key, type, options) {
        let notes = Data.annotate(key.obj, key.name);

        if (notes == null) {
            key.obj[key.name] = options.value ?? undefined;

            notes = Data.annotate(key.obj, key.name);
            notes.changed = false;
        } else if (notes.changed == null && !Object.hasOwn(key.obj, key.name)) {
            key.obj[key.name] = options.value ?? undefined;
            notes.changed = false;
        } else if (notes.changed === false) {
            key.obj[key.name] = options.value ?? undefined;
            notes.changed = false;
        } else {
            notes.changed = true;
        }

        notes.type = type;
        notes.disabled = options.disabled;

        let value = key.obj[key.name];

        // Clear annotations
        delete notes.error;
        if (value != null)
            delete notes.skip;

        return value;
    }

    function setValue(key, value, options) {
        let update = !options.disabled && !options.readonly;

        if (update) {
            let notes = Data.annotate(key.obj, key.name);

            key.obj[key.name] = value ?? undefined;
            notes.changed = true;
        }

        refresh();

        return update;
    }

    function refresh() {
        if (refreshing)
            return;

        refreshing = true;
        setTimeout(() => state.refresh(), 0);
    }

    function useID(key) {
        let map = ids.get(key.obj);

        if (map == null) {
            map = new Map;
            ids.set(key.obj, map);
        }

        let id = map.get(key.name);

        if (id == null) {
            id = 'input' + (++next_id);
            map.set(key.name, id);
        }

        return id;
    }

    function useState(key, type, init = {}) {
        let map = states.get(key.obj);

        if (map == null) {
            map = new Map;
            states.set(key.obj, map);
        }

        let ptr = map.get(key.name);

        if (ptr?.type != type) {
            ptr = {
                type: type,
                state: Object.assign({}, init)
            };
            map.set(key.name, ptr);
        }

        return ptr.state;
    }

    function resetState(key) {
        let map = states.get(key.obj);
        if (map != null)
            map.delete(key.name);
    }
}

export {
    FormState,
    FormModel,
    FormBuilder
}
