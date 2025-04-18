const { render, noChange, nothing } = require('lit/html.js');
const { directive, Directive } = require('lit/directive.js');
const { guard } = require('lit/directives/guard.js');
const { live } = require('lit/directives/live.js');
const { ref } = require('lit/directives/ref.js');
const { until } = require('lit/directives/until.js');
const { unsafeHTML } = require('lit/directives/unsafe-html.js');

const html = (strings, ...values) => {
    if (typeof document == 'undefined')
        return;

    transformValues(values);
    return {
        ['_$litType$']: 1, // HTML_RESULT
        strings,
        values
    };
};
const svg = (strings, ...values) => {
    if (typeof document == 'undefined')
        return;

    transformValues(values);
    return {
        ['_$litType$']: 2, // SVG_RESULT
        strings,
        values
    };
};

// Small addition to lit-html: objects with a toHTML() method are handled by
// calling this function before the templating stuff. This is used by goupile to
// allow the user to embed widgets in HTML code automatically.
function transformValues(values) {
    for (let i = 0; i < values.length; i++) {
        const value = values[i];

        if (value != null && typeof value.toHTML === 'function')
            values[i] = value.toHTML();
    }
}

export {
    html,
    svg,
    render,
    noChange,
    nothing,
    directive,
    Directive,
    guard,
    live,
    ref,
    until,
    unsafeHTML
};