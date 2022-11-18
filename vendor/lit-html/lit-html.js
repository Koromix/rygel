const { render } = require('lit/html.js');
const { directive } = require('lit/directive.js');
const { guard } = require('lit/directives/guard.js');
const { live } = require('lit/directives/live.js');
const { until } = require('lit/directives/until.js');

const html = (strings, ...values) => {
    transformValues(values);
    return {
        ['_$litType$']: 1, // HTML_RESULT
        strings,
        values
    };
};
const svg = (strings, ...values) => {
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

module.exports = {
    html: html,
    svg: svg,
    render: render,
    directive: directive,
    guard: guard,
    live: live,
    until: until
};
