// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function PageBuilder(root, widgets) {
    let self = this;

    this.changeHandler = e => {};

    let interfaces = {};

    function makeID(name) {
        return `af_var_${name}`;
    }

    function addWidget(name, id, func, value) {
        if (interfaces[name])
            throw new Error(`Variable '${name}' already exists`);

        let widget = {
            id: id,
            func: func,
            errors: []
        };
        let intf = {
            value: value,
            error: msg => widget.errors.push(msg || '')
        };

        widgets.push(widget);
        interfaces[name] = intf;

        return intf;
    }

    function wrapWidget(id, frag, options, errors) {
        return html`
            <div class=${errors.length ? 'af_var af_error' : 'af_var'} id=${id + '_div'}>
                ${frag}
                ${errors.map(err => html`<span class="af_error_msg">${err}</span>`)}
                ${options.help ? html`<p class="af_help">${options.help}</p>` : ''}
            </div>
        `;
    }

    this.find = name => interfaces[name];
    this.value = name => interfaces[name].value;
    this.error = (name, msg) => interfaces[name].error(msg);

    this.boolean = function(name, label, options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value = prev ? prev.checked : undefined;

        let func = errors => wrapWidget(id, html`
            <label for=${id}>${label}</label>
            <input id=${id} type="checkbox" checked=${value}
                   oninput=${e => self.changeHandler(e)}/>
        `, options, errors);

        return addWidget(name, id, func, value);
    };

    this.integer = function(name, label, options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value = (prev && prev.value != '') ? parseInt(prev.value, 10) : undefined;

        let func = errors => wrapWidget(id, html`
            <label for=${id}>${label}</label>
            <input id=${id} type="number" value=${value}
                   oninput=${e => self.changeHandler(e)}/>
        `, options, errors);

        let ret = addWidget(name, id, func, value);

        if (value != null) {
            if (options.min !== undefined && value < options.min)
                ret.error(`Valeur < ${options.min}`);
            if (options.max !== undefined && value > options.max)
                ret.error(`Valeur > ${options.max}`);
        }

        return ret;
    };

    this.dropdown = function(name, label, choices = [], options = {}) {
        let id = makeID(name);

        let prev = root.querySelector(`#${id}`);
        let value = prev ? prev.value : undefined;

        let func = errors => wrapWidget(id, html`
            <label for=${id}>${label}</label>
            <select id=${id} onchange=${e => self.changeHandler(e)}>
                <option value="" selected=${value == null}>-- Choisissez une option --</option>
                ${choices.map(c => html`<option value=${c[0]} selected=${value === c[0]}>${c[1]}</option>`)}
            </select>
        `, options, errors);

        return addWidget(name, id, func, value);
    };

    this.info = function(name, label, value, options = {}) {
        let id = makeID(name);

        let text = value;
        if (!options.raw) {
            if (isNaN(value) || value == null) {
                text = '';
            } else if (isFinite(value)) {
                text = value.toFixed(2);
            }
        }

        let func = errors => wrapWidget(id, html`
            <label for=${id}>${label}</label>
            <span>${text}</span>
        `, options, errors);

        return addWidget(name, id, func, value);
    };
}

function generatePage(script) {
    let root = document.querySelector('#af_form');
    let log = document.querySelector('#af_log');

    let widgets = [];
    let page = new PageBuilder(root, widgets);
    page.changeHandler = () => generatePage(script);

    try {
        Function('page', script)(page);

        render(root, () => html`${widgets.map(w => w.func(w.errors))}`);
        log.innerHTML = '';

        return true;
    } catch (err) {
        log.textContent = `⚠ Line ${err.lineNumber}: ${err.message}`;
        return false;
    }
}

function refreshAndSave() {
    let editor = ace.edit('af_editor');
    let script = editor.getValue();

    if (generatePage(script))
        localStorage.setItem('script', script);
}

document.addEventListener('readystatechange', function() {
    if (document.readyState === 'complete') {
        let editor = ace.edit('af_editor');

        editor.setTheme('ace/theme/monokai');
        editor.setShowPrintMargin(false);
        editor.setFontSize(12);
        editor.session.setMode('ace/mode/javascript');

        let script = localStorage.getItem('script');
        if (script == null) {
            script = `let sexe = page.dropdown("sexe", "Sexe",
                         [["M", "Homme"], ["F", "Femme"]]);

if (sexe.value == "F")
    page.boolean("enceinte", "Êtes-vous enceinte ?");

let alcool = page.boolean("alcool", "Consommez-vous de l'alcool ?");
if (alcool.value && sexe.value == "F" && page.value("enceinte")) {
    alcool.error("Pensez au bébé...");
    alcool.error("On peut mettre plusieurs erreurs");
    page.error("alcool", "Et de plein de manières différentes !");
}
if (alcool.value)
    page.integer("alcool_qt", "Combien de verres par semaine ?", {min: 1, max: 30});

page.integer("enfants", "Combien avez-vous d'enfants ?", {min: 0, max: 30});
page.boolean("frites", "Aimez-vous les frites ?",
             {help: "Si si, c'est important, je vous le jure !"});
`;
        }
        editor.setValue(script);
        editor.clearSelection();

        editor.on('change', refreshAndSave);
        refreshAndSave();
    }
});
