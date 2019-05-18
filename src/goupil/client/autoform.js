// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormBuilder(root, widgets, mem) {
    let self = this;

    this.changeHandler = e => {};

    let interfaces = {};
    let widgets_ref = widgets;

    this.errors = [];

    function makeID(name) { return `af_var_${name}`; }

    function addWidget(render) {
        let widget = {
            render: render,
            errors: []
        };

        widgets_ref.push(widget);

        return widget;
    }

    function wrapWidget(frag, options, errors = []) {
        return html`
            <div class=${errors.length ? 'af_widget af_widget_error' : 'af_widget'}>
                ${frag}
                ${errors.length ? html`<span class="af_error">${errors.map(err => html`${err}<br/>`)}</span>` : html``}
                ${options.help ? html`<p class="af_help">${options.help}</p>` : ''}
            </div>
        `;
    }

    function addVariableWidget(name, id, render, value) {
        if (interfaces[name])
            throw new Error(`Variable '${name}' already exists`);

        let widget = addWidget(render);

        let intf = {
            value: value,
            error: msg => {
                self.errors.push(msg || '');
                widget.errors.push(msg || '');

                return intf;
            }
        };
        interfaces[name] = intf;

        return intf;
    }

    function createPrefixOrSuffix(cls, text, value) {
        if (typeof text === 'function') {
            return html`<span class="${cls}">${text(value)}</span>`;
        } else if (text) {
            return html`<span class="${cls}">${text}</span>`;
        } else {
            return html``;
        }
    }

    this.find = name => interfaces[name];
    this.value = function(name) {
        let intf = interfaces[name];
        return intf ? intf.value : undefined;
    };
    this.error = (name, msg) => interfaces[name].error(msg);

    this.text = function(name, label, options = {}) {
        let id = makeID(name);

        let value;
        {
            let prev = root.querySelector(`#${id}`);

            if (prev) {
                value = prev.value;
                mem[name] = value;
            } else {
                value = mem[name];
            }
        }

        let render = errors => wrapWidget(html`
            <label for=${id}>${label || name}</label>
            ${createPrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="text" size="${options.size || 30}" .value=${value || ''}
                   @input=${self.changeHandler}/>
            ${createPrefixOrSuffix('af_suffix', options.suffix, value)}
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    this.number = function(name, label, options = {}) {
        let id = makeID(name);

        let value;
        {
            let prev = root.querySelector(`#${id}`);

            if (prev) {
                value = parseFloat(prev.value);
                mem[name] = value;
            } else {
                value = parseFloat(mem[name]);
            }
        }

        let render = errors => wrapWidget(html`
            <label for=${id}>${label || name}</label>
            ${createPrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="number"
                   step=${1 / Math.pow(10, options.decimals || 0)} .value=${value}
                   @input=${self.changeHandler}/>
            ${createPrefixOrSuffix('af_suffix', options.suffix, value)}
        `, options, errors);

        let intf = addVariableWidget(name, id, render, value);

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

    function parseValue(str) { return (str && str !== 'undefined') ? JSON.parse(str) : undefined; }
    function stringifyValue(value) { return JSON.stringify(value); }

    this.dropdown = function(name, label, choices = [], options = {}) {
        let id = makeID(name);

        let value;
        {
            let prev = root.querySelector(`#${id}`);

            if (prev) {
                value = parseValue(prev.value);
                mem[name] = value;
            } else {
                value = mem[name];
            }
        }

        let render = errors => wrapWidget(html`
            <label for=${id}>${label || name}</label>
            <select id=${id} @change=${self.changeHandler}>
                <option value="null" .selected=${value == null}>-- Choisissez une option --</option>
                ${choices.filter(c => c != null).map(c =>
                    html`<option value=${stringifyValue(c[0])} .selected=${value === c[0]}>${c[1]}</option>`)}
            </select>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    function changeSelect(e, id, value) {
        let json = stringifyValue(value);

        let els = document.querySelectorAll(`#${id} button`);
        for (let el of els)
            el.classList.toggle('active', el.dataset.value == json && !el.classList.contains('active'));

        self.changeHandler(e);
    }

    this.choice = function(name, label, choices = [], options = {}) {
        let id = makeID(name);

        let value;
        {
            let prev = root.querySelector(`#${id}`);

            if (prev) {
                let els = prev.querySelectorAll('button');
                for (let el of els) {
                    if (el.classList.contains('active')) {
                        value = parseValue(el.dataset.value);
                        break;
                    }
                }

                mem[name] = value;
            } else {
                value = mem[name];
            }
        }

        let render = errors => wrapWidget(html`
            <label for=${id}>${label || name}</label>
            <div class="af_select" id=${id}>
                ${choices.filter(c => c != null).map(c =>
                    html`<button data-value=${stringifyValue(c[0])}
                                 .className=${value == c[0] ? 'af_button active' : 'af_button'}
                                 @click=${e => changeSelect(e, id, c[0])}>${c[1]}</button>`)}
            </div>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    this.boolean = function(name, label, options = {}) {
        return self.choice(name, label, [[true, 'Oui'], [false, 'Non']], options);
    };

    function changeRadio(e, already_checked) {
        if (already_checked)
            e.target.checked = false;

        self.changeHandler(e);
    }

    this.radio = function(name, label, choices = [], options = {}) {
        let id = makeID(name);

        let value;
        {
            let prev = root.querySelector(`#${id}`);

            if (prev) {
                let el = prev.querySelector('input:checked');
                if (el)
                    value = parseValue(el.value);

                mem[name] = value;
            } else {
                value = mem[name];
            }
        }

        let render = errors => wrapWidget(html`
            <label>${label || name}</label>
            <div class="af_radio" id=${id}>
                ${choices.filter(c => c != null).map((c, i) =>
                    html`<input type="radio" name=${id} id=${`${id}.${i}`} value=${stringifyValue(c[0])}
                                .checked=${value === c[0]}
                                @click=${e => changeRadio(e, value === c[0])}/>
                         <label for=${`${id}.${i}`}>${c[1]}</label><br/>`)}
            </div>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    function changeMulti(e) {
        let target = e.target;
        let els = target.parentNode.querySelectorAll('input');

        let nullify = (target.checked && target.value === 'null');
        for (let el of els) {
            if ((el.value === 'null') != nullify)
                el.checked = false;
        }

        self.changeHandler(e);
    }

    this.multi = function(name, label, choices = [], options = {}) {
        let id = makeID(name);

        let value;
        {
            let prev = root.querySelector(`#${id}`);

            if (prev) {
                let els = prev.querySelectorAll('input');

                value = [];
                for (let el of els) {
                    if (el.checked)
                        value.push(parseValue(el.value));
                }

                mem[name] = value;
            } else if (Array.isArray(mem[name])) {
                value = mem[name];
            } else {
                value = [];
            }
        }

        let render = errors => wrapWidget(html`
            <label>${label || name}</label>
            <div class="af_multi" id=${id}>
                ${choices.filter(c => c != null).map((c, i) =>
                    html`<input type="checkbox" name=${id} id=${`${id}.${i}`} value=${stringifyValue(c[0])}
                                .checked=${value.includes(c[0])}
                                @click=${changeMulti}/>
                         <label for=${`${id}.${i}`}>${c[1]}</label><br/>`)}
            </div>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    this.calc = function(name, label, value, options = {}) {
        let id = makeID(name);

        let text = value;
        if (!options.raw && typeof value !== 'string') {
            if (isNaN(value) || value == null) {
                text = '';
            } else if (isFinite(value)) {
                // This is a garbage way to round numbers
                let multiplicator = Math.pow(10, 2);
                let n = parseFloat((value * multiplicator).toFixed(11));
                text = Math.round(n) / multiplicator;
            }
        }

        let render = errors => wrapWidget(html`
            <label for=${id}>${label || name}</label>
            <span>${text}</span>
        `, options, errors);

        return addVariableWidget(name, id, render, value);
    };

    this.output = function(content, options = {}) {
        // Don't output function content, helps avoid garbage output when the
        // user types 'page.oupt(html);'.
        if (!content || typeof content === 'function')
            return;

        let render = () => wrapWidget(content, options);
        addWidget(render);
    };

    this.section = function(label, func) {
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

        let render = () => html`
            <fieldset class="af_section">
                ${label ? html`<legend>${label}</legend>` : html``}
                ${widgets.map(w => w.render(w.errors))}
            </fieldset>
        `;

        addWidget(render);
    };

    this.buttons = function(buttons, options = {}) {
        let render = () => wrapWidget(html`
            <div class="af_buttons">
                ${buttons.map(button =>
                    html`<button class="af_button" @click=${button[1]}>${button[0]}</button>`)}
            </div>
        `, options);

        addWidget(render);
    };
    this.buttons.std = {
        SaveCancel: func => [['Enregistrer', func || (() => {})], ['Annuler', func || (() => {})]]
    };
}

function FormExecutor() {
    this.goHandler = key => {};

    let self = this;

    let af_form;
    let af_log;

    let mem = {};

    function parseAnonymousErrorLine(err) {
        if (err.stack) {
            let m;
            if (m = err.stack.match(/ > Function:([0-9]+):[0-9]+/) ||
                    err.stack.match(/, <anonymous>:([0-9]+):[0-9]+/)) {
                // Can someone explain to me why do I have to offset by -2?
                let line = parseInt(m[1], 10) - 2;
                return line;
            } else if (m = err.stack.match(/Function code:([0-9]+):[0-9]+/)) {
                let line = parseInt(m[1], 10);
                return line;
            }
        }

        return null;
    }

    function renderForm(script) {
        let widgets = [];

        let builder = new FormBuilder(af_form, widgets, mem);
        builder.changeHandler = () => renderForm(script);


        // Prevent go() call from working if called during script eval
        let prev_go_handler = self.goHandler;
        self.goHandler = key => {
            throw new Error(`go() must be called from a callback (button click, etc.).

If you are using it for events, make sure you did not use this syntax by accident:
    go('page_key')
instead of:
    () => go('page_key')`);
        };

        try {
            Function('form', 'go', script)(builder, key => self.goHandler(key));

            render(html`${widgets.map(w => w.render(w.errors))}`, af_form);
            self.clearError();

            return true;
        } catch (err) {
            let line;
            if (err instanceof SyntaxError) {
                // At least Firefox seems to do well in this case, it's better than nothing
                line = err.lineNumber - 2;
            } else if (err.stack) {
                line = parseAnonymousErrorLine(err);
            }

            self.setError(line, err.message);

            return false;
        } finally {
            self.goHandler = prev_go_handler;
        }
    }

    this.setError = function(line, msg) {
        af_form.classList.add('af_form_broken');

        af_log.textContent = `⚠\uFE0E Line ${line || '?'}: ${msg}`;
        af_log.style.display = 'block';
    };

    this.clearError = function() {
        af_form.classList.remove('af_form_broken');

        af_log.innerHTML = '';
        af_log.style.display = 'none';
    };

    this.render = function(root, script) {
        render(html`
            <div class="af_form"></div>
            <div class="af_log" style="display: none;"></div>
        `, root);
        af_form = root.querySelector('.af_form');
        af_log = root.querySelector('.af_log');

        if (script !== undefined) {
            return renderForm(script);
        } else {
            return true;
        }
    };
}

let autoform = (function() {
    let self = this;

    let init = false;

    let editor;
    let af_menu;
    let af_page;

    let pages = new Map;
    let current_key;

    let executor;

    function loadDefaultPages() {
        pages = new Map([
            ['tuto', {
                key: 'tuto',
                title: 'Tutoriel',
                script: `form.output(html\`
    <p>Une <b>fonction</b> est composée d'un <i>nom</i> et de plusieurs <i>paramètres</i> et permet de proposer un outil de saisie (champ texte, menu déroulant ...).
    <p>Exemple : la fonction form.text("num_patient", "Numéro de patient") propose un champ de saisie texte intitulé <i>Numéro de patient</i> et le stocke dans la variable <i>num_patient</i>.
    <p>Vous pouvez copier les fonctions présentées dans la section <b>Exemples</b> dans <b>Nouvelle section</b> pour créer votre propre formulaire.
\`);

form.section("Nouvelle section", () => {
    // Copier coller les fonctions ci-dessous
    // Modifier les noms de variables. Ex : "nom" -> "prenom"

    // Remplacer ce commentaire par une première fonction

    // Remplacer ce commentaire par une deuxième fonction
});

form.section("Exemples", () => {
    // Variable texte
    form.text("nom", "Quel est votre nom ?");

    // Variable numérique
    form.number("age", "Quel est votre âge ?", {min: 0, max: 120});

    // Variable de choix (boutons)
    form.choice("sexe", "Quel est votre sexe ?", [["M", "Homme"], ["F", "Femme"]]);

    // Variable à choix unique (menu déroulant)
    form.dropdown("csp", "Quelle est votre CSP ?", [
        [1, "Agriculteur exploitant"],
        [2, "Artisan, commerçant ou chef d'entreprise"],
        [3, "Cadre ou profession intellectuelle supérieure"],
        [4, "Profession intermédiaire"],
        [5, "Employé"],
        [6, "Ouvrier"],
        [7, "Retraité"],
        [8, "Autre ou sans activité professionnelle"]
    ]);

    // Variable à choix unique (bouton radio)
    form.radio("lieu_vie", "Quel est votre lieu de vie ?", [
        ["maison", "Maison"],
        ["appartement", "Appartement"]
    ]);

    // Variable cases à cocher multiples
    form.multi("sommeil", "Présentez-vous un trouble du sommeil ?", [
        [1, "Troubles d’endormissement"],
        [2, "Troubles de maintien du sommeil"],
        [3, "Réveil précoce"],
        [4, "Sommeil non récupérateur"],
        [null, "Aucune de ces réponses"]
    ]);
});
`
            }],

            ['complicated', {
                key: 'complicated',
                title: 'Formulaire compliqué',
                script: `form.text("name", "Quel est votre nom ?");
form.number("age", "Quel est votre âge ?", {min: 0, max: 120,
                                            suffix: value => util.pluralFR(value, "an", "ans")});

let sexe = form.choice("sexe", "Quel est votre sexe ?", [["M", "Homme"], ["F", "Femme"]]);

form.dropdown("csp", "Quelle est votre CSP ?", [
    [1, "Agriculteur exploitant"],
    [2, "Artisan, commerçant ou chef d'entreprise"],
    [3, "Cadre ou profession intellectuelle supérieure"],
    [4, "Profession intermédiaire"],
    [5, "Employé"],
    [6, "Ouvrier"],
    [7, "Retraité"],
    [8, "Autre ou sans activité professionnelle"]
]);

form.radio("lieu_vie", "Quel est votre lieu de vie ?", [
    ["maison", "Maison"],
    ["appartement", "Appartement"]
]);

form.multi("sommeil", "Présentez-vous un trouble du sommeil ?", [
    [1, "Troubles d’endormissement"],
    [2, "Troubles de maintien du sommeil"],
    [3, "Réveil précoce"],
    [4, "Sommeil non récupérateur"],
    [null, "Aucune de ces réponses"]
]);

if (sexe.value == "F") {
    form.boolean("enceinte", "Êtes-vous enceinte ?");
}

form.section("Alcool", () => {
    let alcool = form.boolean("alcool", "Consommez-vous de l'alcool ?");

    if (alcool.value && form.value("enceinte")) {
        alcool.error("Pensez au bébé...");
        alcool.error("On peut mettre plusieurs erreurs");
        form.error("alcool", "Et de plein de manières différentes !");
    }

    if (alcool.value) {
        form.number("alcool_qt", "Combien de verres par semaine ?", {min: 1, max: 30});
    }
});

form.section("Autres", () => {
    form.number("enfants", "Combien avez-vous d'enfants ?", {min: 0, max: 30});
    form.boolean("frites", "Aimez-vous les frites ?",
                 {help: "Si si, c'est important, je vous le jure !"});
});

form.output(html\`On peut aussi mettre du <b>HTML directement</b> si on veut...
                 <button class="af_button" @click=\${e => go("complicated_help")}>Afficher l'aide</button>\`);
form.output("Ou bien encore mettre du <b>texte brut</b>.");
`
            }],

            ['complicated_help', {
                key: 'complicated_help',
                title: 'Formulaire compliqué (aide)',
                script: `form.output("Loreum ipsum");

form.buttons([
    ["Donner l'alerte", () => alert("Alerte générale !!")],
    ["Revenir à l'auto-questionnaire", () => go("complicated")]
]);
`
            }]
        ]);
    }

    function savePages() {
        let entries = Array.from(pages.entries());
        localStorage.setItem('goupil_af_pages', JSON.stringify(entries));
    }

    function createPage() {
        let key = prompt('Key?');
        if (key) {
            if (pages.has(key))
                throw new Error(`Page '${key}' already exists`);

            let title = prompt('Title?');
            if (title) {
                let page = {
                    key: key,
                    title: title,
                    script: ''
                };

                pages.set(key, page);
                savePages();

                self.go(key);
            }
        }
    }

    function deletePage(key) {
        if (confirm('Are you sure?')) {
            pages.delete(key);
            savePages();

            if (current_key === key && pages.size) {
                let first_key = pages.values().next().value.key;
                self.go(first_key);
            } else {
                self.go(current_key);
            }
        }
    }

    function resetPages() {
        if (confirm('Are you sure?')) {
            loadDefaultPages();
            executor = null;

            let first_key = pages.values().next().value.key;
            self.go(first_key);
        }
    }

    function renderAll() {
        let page = pages.get(current_key);

        render(html`
            <button @click=${createPage}>Ajouter</button>
            ${page ? html`<button @click=${e => deletePage(current_key)}>Supprimer</button>` : html``}
            <select @change=${e => self.go(e.target.value)}>
                ${!current_key && !pages.size ? html`<option>-- No page available --</option>` : html``}
                ${current_key && !page ?
                    html`<option value=${current_key} .selected=${true}>-- Unknown page '${current_key}' --</option>` : html``}
                ${Array.from(pages, ([_, page]) =>
                    html`<option value=${page.key} .selected=${page.key == current_key}>${page.title} (${page.key})</option>`)}
            </select>
            <button @click=${resetPages}>Réinitialiser</button>
        `, af_menu);

        if (!executor) {
            executor = new FormExecutor();
            executor.goHandler = self.go;
        }

        if (page) {
            editor.setReadOnly(false);
            editor.container.classList.remove('disabled');

            return executor.render(af_form, page.script);
        } else {
            editor.setReadOnly(true);
            editor.container.classList.add('disabled');

            executor.render(af_form);
            if (current_key) {
                executor.setError(null, `Unknown page '${current_key}'`);
            } else {
                executor.setError(null, 'No page available');
            }

            return false;
        }
    }

    function refreshAndSave() {
        let page = pages.get(current_key);

        if (page) {
            let prev_script = page.script;

            page.script = editor.getValue();

            if (renderAll()) {
                savePages();
            } else {
                // Restore working script
                page.script = prev_script;
            }
        }
    }

    function initEditor() {
        editor = ace.edit('af_editor');

        editor.setTheme('ace/theme/monokai');
        editor.setShowPrintMargin(false);
        editor.setFontSize(12);
        editor.session.setMode('ace/mode/javascript');

        editor.on('change', e => {
            // If something goes wrong during refreshAndSave(), we don't
            // want to break ACE state.
            setTimeout(refreshAndSave, 0);
        });
    }

    this.go = function(key) {
        let page = pages.get(key);

        if (page) {
            editor.setValue(page.script);
            editor.clearSelection();
        }
        current_key = key;

        renderAll();
    }

    this.activate = function() {
        document.title = 'goupil autoform';

        if (!init) {
            if (!window.ace) {
                goupil.loadScript(`${settings.base_url}static/ace.js`);
                return;
            }

            let json = localStorage.getItem('goupil_af_pages');

            if (json) {
                try {
                    pages = new Map(JSON.parse(json));
                } catch (err) {
                    console.log('Loading default pages (load error)');
                    loadDefaultPages();
                }
            } else {
                loadDefaultPages();
            }

            init = true;
        }

        let main = document.querySelector('main');

        render(html`
            <div id="af_menu"></div>
            <div id="af_editor"></div>
            <div id="af_form"></div>
        `, main);
        af_menu = document.querySelector('#af_menu');
        af_form = document.querySelector('#af_form');

        initEditor();

        if (current_key && pages.size) {
            self.go(current_key);
        } else {
            let first_key = pages.size ? pages.values().next().value.key : null;
            self.go(first_key);
        }
    };

    return this;
}).call({});
