// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormBuilder(root, unique_key, widgets, mem) {
    let self = this;

    this.changeHandler = e => {};

    let interfaces = {};
    let widgets_ref = widgets;
    let options_stack = [{}];

    this.errors = [];

    function makeID(key) {
        return `af_var_${unique_key}_${key}`;
    }

    function addWidget(render) {
        let widget = {
            render: render,
            errors: []
        };

        widgets_ref.push(widget);

        return widget;
    }

    function wrapWidget(frag, options, errors = []) {
        let cls = 'af_widget';
        if (options.large)
            cls += ' af_widget_large';
        if (errors.length)
            cls += ' af_widget_error';

        return html`
            <div class=${cls}>
                ${frag}
                ${errors.length && errors.every(err => err) ?
                    html`<div class="af_error">${errors.map(err => html`${err}<br/>`)}</div>` : html``}
                ${options.help ? html`<p class="af_help">${options.help}</p>` : ''}
            </div>
        `;
    }

    function addVariableWidget(key, render, value) {
        if (!key)
            throw new Error('Empty variable keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed variable key characters: a-z, _ and 0-9 (not as first character)');
        if (interfaces[key])
            throw new Error(`Variable '${key}' already exists`);

        let widget = addWidget(render);

        let intf = {
            value: value,
            error: msg => {
                self.errors.push(msg || '');
                widget.errors.push(msg || '');

                return intf;
            }
        };
        interfaces[key] = intf;

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

    this.pushOptions = function(options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);
        options_stack.push(options);
    };
    this.popOptions = function() {
        if (options_stack.length < 2)
            throw new Error('Too many popOptions() operations');

        options_stack.pop();
    }

    function parseValue(str) { return (str && str !== 'undefined') ? JSON.parse(str) : undefined; }
    function stringifyValue(value) { return JSON.stringify(value); }

    this.find = key => interfaces[key];
    this.value = function(key) {
        let intf = interfaces[key];
        return intf ? intf.value : undefined;
    };
    this.error = (key, msg) => interfaces[key].error(msg);

    function handleTextInput(e, key) {
        let value = e.target.value;
        mem[key] = value;

        self.changeHandler(e);
    };

    this.text = function(key, label, options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        let id = makeID(key);
        let value = mem.hasOwnProperty(key) ? mem[key] : options.value;

        let render = errors => wrapWidget(html`
            <label for=${id}>${label || key}</label>
            ${createPrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="text" size="${options.size || 30}" .value=${value || ''}
                   @input=${e => handleTextInput(e, key)}/>
            ${createPrefixOrSuffix('af_suffix', options.suffix, value)}
        `, options, errors);

        return addVariableWidget(key, render, value);
    };

    function handleNumberChange(e, key)
    {
        let value = parseFloat(e.target.value);
        mem[key] = value;

        self.changeHandler(e);
    }

    this.number = function(key, label, options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        let id = makeID(key);
        let value = parseFloat(mem.hasOwnProperty(key) ? mem[key] : options.value);

        let render = errors => wrapWidget(html`
            <label for=${id}>${label || key}</label>
            ${createPrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="number"
                   step=${1 / Math.pow(10, options.decimals || 0)} .value=${value}
                   @input=${e => handleNumberChange(e, key)}/>
            ${createPrefixOrSuffix('af_suffix', options.suffix, value)}
        `, options, errors);

        let intf = addVariableWidget(key, render, value);

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

    function handleDropdownChange(e, key) {
        let value = parseValue(e.target.value);
        mem[key] = value;

        self.changeHandler(e);
    }

    this.dropdown = function(key, label, choices = [], options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        let id = makeID(key);
        let value = mem.hasOwnProperty(key) ? mem[key] : options.value;

        let render = errors => wrapWidget(html`
            <label for=${id}>${label || key}</label>
            <select id=${id} @change=${e => handleDropdownChange(e, key)}>
                <option value="null" .selected=${value == null}>-- Choisissez une option --</option>
                ${choices.filter(c => c != null).map(c =>
                    html`<option value=${stringifyValue(c[0])} .selected=${value === c[0]}>${c[1]}</option>`)}
            </select>
        `, options, errors);

        return addVariableWidget(key, render, value);
    };

    function handleChoiceChange(e, key) {
        let json = e.target.dataset.value;
        if (e.target.classList.contains('active')) {
            mem[key] = undefined;
        } else {
            mem[key] = parseValue(json);
        }

        // This is useless in most cases because the new form will incorporate
        // this change, but who knows. Do it like other browser-native widgets.
        let els = e.target.parentNode.querySelectorAll('button');
        for (let el of els)
            el.classList.toggle('active', el.dataset.value == json && !el.classList.contains('active'));

        self.changeHandler(e);
    }

    this.choice = function(key, label, choices = [], options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        let id = makeID(key);
        let value = mem.hasOwnProperty(key) ? mem[key] : options.value;

        let render = errors => wrapWidget(html`
            <label for=${id}>${label || key}</label>
            <div class="af_select" id=${id}>
                ${choices.filter(c => c != null).map(c =>
                    html`<button data-value=${stringifyValue(c[0])}
                                 .className=${value == c[0] ? 'af_button active' : 'af_button'}
                                 @click=${e => handleChoiceChange(e, key)}>${c[1]}</button>`)}
            </div>
        `, options, errors);

        return addVariableWidget(key, render, value);
    };

    this.binary = function(key, label, options = {}) {
        return self.choice(key, label, [[1, 'Oui'], [0, 'Non']], options);
    };
    // NOTE: Deprecated
    this.boolean = this.binary;

    function handleRadioChange(e, key, already_checked) {
        let value = parseValue(e.target.value);

        if (already_checked) {
            e.target.checked = false;
            mem[key] = undefined;
        } else {
            mem[key] = value;
        }

        self.changeHandler(e);
    }

    this.radio = function(key, label, choices = [], options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        let id = makeID(key);
        let value = mem.hasOwnProperty(key) ? mem[key] : options.value;

        let render = errors => wrapWidget(html`
            <label>${label || key}</label>
            <div class="af_radio" id=${id}>
                ${choices.filter(c => c != null).map((c, i) =>
                    html`<input type="radio" name=${id} id=${`${id}.${i}`} value=${stringifyValue(c[0])}
                                .checked=${value === c[0]}
                                @click=${e => handleRadioChange(e, key, value === c[0])}/>
                         <label for=${`${id}.${i}`}>${c[1]}</label><br/>`)}
            </div>
        `, options, errors);

        return addVariableWidget(key, render, value);
    };

    function handleMultiChange(e, key) {
        let els = e.target.parentNode.querySelectorAll('input');

        let nullify = (e.target.checked && e.target.value === 'null');
        let value = [];
        for (let el of els) {
            if ((el.value === 'null') != nullify)
                el.checked = false;
            if (el.checked)
                value.push(parseValue(el.value));
        }
        mem[key] = value;

        self.changeHandler(e);
    }

    this.multi = function(key, label, choices = [], options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        let id = makeID(key);
        let value;
        {
            let candidate = mem.hasOwnProperty(key) ? mem[key] : options.value;
            value = Array.isArray(candidate) ? candidate : [];
        }

        let render = errors => wrapWidget(html`
            <label>${label || key}</label>
            <div class="af_multi" id=${id}>
                ${choices.filter(c => c != null).map((c, i) =>
                    html`<input type="checkbox" id=${`${id}.${i}`} value=${stringifyValue(c[0])}
                                .checked=${value.includes(c[0])}
                                @click=${e => handleMultiChange(e, key)}/>
                         <label for=${`${id}.${i}`}>${c[1]}</label><br/>`)}
            </div>
        `, options, errors);

        return addVariableWidget(key, render, value);
    };

    this.calc = function(key, label, value, options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        let id = makeID(key);

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
            <label for=${id}>${label || key}</label>
            <span>${text}</span>
        `, options, errors);

        return addVariableWidget(key, render, value);
    };

    this.output = function(content, options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

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
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        let render = () => wrapWidget(html`
            <div class="af_buttons">
                ${buttons.map(button =>
                    html`<button class="af_button" ?disabled=${!button[1]}
                                 @click=${button[1]}>${button[0]}</button>`)}
            </div>
        `, options);

        addWidget(render);
    };
    this.buttons.std = {
        OkCancel: label => [[label || 'OK', self.submit], ['Annuler', self.close]]
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

    function renderForm(page_key, script) {
        let widgets = [];

        let builder = new FormBuilder(af_form, page_key, widgets, mem);
        builder.changeHandler = () => renderForm(page_key, script);

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

    this.render = function(root, page_key, script) {
        render(html`
            <div class="af_form"></div>
            <div class="af_log" style="display: none;"></div>
        `, root);
        af_form = root.querySelector('.af_form');
        af_log = root.querySelector('.af_log');

        if (script !== undefined) {
            return renderForm(page_key, script);
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

    // TODO: Simplify code with some kind of OrderedMap?
    let pages = [];
    let pages_map = {};
    let current_key;

    let executor;

    function loadDefaultPages() {
        pages = [
            {
                key: 'tuto',
                title: 'Tutoriel',
                script: `// Retirer le commentaire de la ligne suivante pour afficher les
// champs (texte, numérique, etc.) à droite du libellé.
// form.pushOptions({large: true});

form.output(html\`
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
            },

            {
                key: 'complicated',
                title: 'Formulaire compliqué',
                script: `form.pushOptions({large: true});

form.text("name", "Quel est votre nom ?");
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
    form.binary("enceinte", "Êtes-vous enceinte ?");
}

form.section("Alcool", () => {
    let alcool = form.binary("alcool", "Consommez-vous de l'alcool ?");

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
    form.binary("frites", "Aimez-vous les frites ?",
                {help: "Si si, c'est important, je vous le jure !"});
});

form.output(html\`On peut aussi mettre du <b>HTML directement</b> si on veut...
                 <button class="af_button" @click=\${e => go("complicated_help")}>Afficher l'aide</button>\`);
form.output("Ou bien encore mettre du <b>texte brut</b>.");
`
            },

            {
                key: 'complicated_help',
                title: 'Formulaire compliqué (aide)',
                script: `form.output("Loreum ipsum");

form.buttons([
    ["Donner l'alerte", () => alert("Alerte générale !!")],
    ["Revenir à l'auto-questionnaire", () => go("complicated")]
]);
`
            }
        ];
        pages.sort((page1, page2) => util.compareStrings(page1.key, page2.key));

        pages_map = {};
        for (let page of pages)
            pages_map[page.key] = page;

        goupil.database.transaction(db => {
            db.clear('pages');
            for (let page of pages)
                db.save('pages', page);
        });
    }

    function createPage(key, title) {
        let page = {
            key: key,
            title: title,
            script: ''
        };

        goupil.database.save('pages', page).then(() => {
            pages.push(page);
            pages.sort((page1, page2) => util.compareStrings(page1.key, page2.key));
            pages_map[page.key] = page;

            self.go(key);
        });
    }

    function editPage(page, title) {
        let new_page = Object.assign({}, page);
        new_page.title = title;

        goupil.database.save('pages', new_page).then(() => {
            Object.assign(page, new_page);
            renderAll();
        });
    }

    function deletePage(page) {
        goupil.database.delete('pages', page.key).then(() => {
            let key = page.key;

            // Remove from pages array and map
            let page_idx = pages.findIndex(page => page.key === key);
            pages.splice(page_idx, 1);
            delete pages_map[key];

            if (current_key === key && pages.length) {
                let first_key = pages[0].key;
                self.go(first_key);
            } else {
                self.go(current_key);
            }
        });
    }

    function resetPages() {
        loadDefaultPages();
        executor = null;

        let first_key = pages[0].key;
        self.go(first_key);
    }

    function showCreatePageDialog(e) {
        goupil.popup(e, form => {
            let key = form.text('key', 'Clé :');
            let title = form.text('title', 'Titre :');

            if (key.value) {
                if (pages.some(page => page.key === key.value))
                    key.error('Existe déjà');
                if (!key.value.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
                    key.error('Autorisé : a-z, _ et 0-9 (sauf initiale)');
            }

            if (key.value && title.value && !form.errors.length) {
                form.submit = () => {
                    createPage(key.value, title.value);
                    form.close();
                };
            }
            form.buttons(form.buttons.std.OkCancel('Créer'));
        });
    }

    function showEditPageDialog(e, page) {
        goupil.popup(e, form => {
            let title = form.text('title', 'Titre :', {value: page.title});

            if (title.value) {
                form.submit = () => {
                    editPage(page, title.value);
                    form.close();
                };
            }
            form.buttons(form.buttons.std.OkCancel('Modifier'));
        });
    }

    function showDeletePageDialog(e, page) {
        goupil.popup(e, form => {
            form.output(`Voulez-vous vraiment supprimer la page '${page.key}' ?`);

            form.submit = () => {
                deletePage(page);
                form.close();
            };
            form.buttons(form.buttons.std.OkCancel('Supprimer'));
        });
    }

    function showResetPagesDialog(e) {
        goupil.popup(e, form => {
            form.output('Voulez-vous vraiment réinitialiser toutes les pages ?');

            form.submit = () => {
                resetPages();
                form.close();
            };
            form.buttons(form.buttons.std.OkCancel('Réinitialiser'));
        });
    }

    function renderAll() {
        let page = pages_map[current_key];

        render(html`
            <button @click=${showCreatePageDialog}>Ajouter</button>
            ${page ? html`<button @click=${e => showEditPageDialog(e, page)}>Modifier</button>
                          <button @click=${e => showDeletePageDialog(e, page)}>Supprimer</button>` : html``}
            <select @change=${e => self.go(e.target.value)}>
                ${!current_key && !pages.length ? html`<option>-- No page available --</option>` : html``}
                ${current_key && !page ?
                    html`<option value=${current_key} .selected=${true}>-- Unknown page '${current_key}' --</option>` : html``}
                ${pages.map(page =>
                    html`<option value=${page.key} .selected=${page.key == current_key}>${page.key} (${page.title})</option>`)}
            </select>
            <button @click=${showResetPagesDialog}>Réinitialiser</button>
        `, af_menu);

        if (!executor) {
            executor = new FormExecutor();
            executor.goHandler = self.go;
        }

        if (page) {
            editor.setReadOnly(false);
            editor.container.classList.remove('disabled');

            return executor.render(af_form, page.key, page.script);
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
        let page = pages_map[current_key];

        if (page) {
            let prev_script = page.script;

            page.script = editor.getValue();

            if (renderAll()) {
                goupil.database.save('pages', page);
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
        let page = pages_map[key];

        if (page) {
            editor.setValue(page.script);
            editor.clearSelection();
        }
        current_key = key;

        renderAll();
    };

    this.activate = function() {
        document.title = `${settings.project_key} — goupil autoform`;

        if (!init) {
            if (!window.ace) {
                goupil.loadScript(`${settings.base_url}static/ace.js`);
                return;
            }

            goupil.database.loadAll('pages').then(pages2 => {
                if (pages2.length) {
                    pages = Array.from(pages2);
                    for (let page of pages)
                        pages_map[page.key] = page;
                } else {
                    loadDefaultPages();
                }
                self.activate();
            });

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

        if (current_key && pages.length) {
            self.go(current_key);
        } else {
            let first_key = pages.length ? pages[0].key : null;
            self.go(first_key);
        }
    };

    return this;
}).call({});
