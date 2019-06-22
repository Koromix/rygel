// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function FormBuilder(root, unique_key, widgets, mem) {
    let self = this;

    this.changeHandler = e => {};

    let interfaces = {};
    let widgets_ref = widgets;
    let options_stack = [{untoggle: true}];

    this.errors = [];

    function makeID(key) {
        return `af_var_${unique_key}_${key}`;
    }

    function addWidget(render) {
        let intf = {
            render: render,
            errors: []
        };

        widgets_ref.push(intf);

        return intf;
    }

    function wrapWidget(frag, options, errors = []) {
        let cls = 'af_widget';
        if (options.large)
            cls += ' af_widget_large';
        if (errors.length)
            cls += ' af_widget_error';
        if (options.disable)
            cls += ' af_widget_disable';

        return html`
            <div class=${cls}>
                ${frag}
                ${errors.length && errors.every(err => err) ?
                    html`<div class="af_error">${errors.map(err => html`${err}<br/>`)}</div>` : html``}
                ${options.help ? html`<p class="af_help">${options.help}</p>` : ''}
            </div>
        `;
    }

    function addVariableWidget(key, label, render, value) {
        if (!key)
            throw new Error('Empty variable keys are not allowed');
        if (!key.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed variable key characters: a-z, _ and 0-9 (not as first character)');
        if (interfaces[key])
            throw new Error(`Variable '${key}' already exists`);

        let intf = addWidget(render);
        Object.assign(intf, {
            key: key,
            label: label,
            value: value,
            missing: value == null,
            error: msg => {
                if (!intf.errors.length)
                    self.errors.push(intf);
                intf.errors.push(msg || '');

                return intf;
            }
        });

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
    this.value = key => mem[key];
    this.missing = key => mem[key] == null;
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
                   ?disabled=${options.disable} @input=${e => handleTextInput(e, key)}/>
            ${createPrefixOrSuffix('af_suffix', options.suffix, value)}
        `, options, errors);

        return addVariableWidget(key, label, render, value);
    };

    this.password = function(key, label, options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        let id = makeID(key);
        let value = mem.hasOwnProperty(key) ? mem[key] : options.value;

        let render = errors => wrapWidget(html`
            <label for=${id}>${label || key}</label>
            ${createPrefixOrSuffix('af_prefix', options.prefix, value)}
            <input id=${id} type="password" size="${options.size || 30}" .value=${value || ''}
                   ?disabled=${options.disable} @input=${e => handleTextInput(e, key)}/>
            ${createPrefixOrSuffix('af_suffix', options.suffix, value)}
        `, options, errors);

        return addVariableWidget(key, label, render, value);
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
                   ?disabled=${options.disable} @input=${e => handleNumberChange(e, key)}/>
            ${createPrefixOrSuffix('af_suffix', options.suffix, value)}
        `, options, errors);

        let intf = addVariableWidget(key, label, render, value);

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
            <select id=${id} ?disabled=${options.disable}
                    @change=${e => handleDropdownChange(e, key)}>
                ${options.untoggle || !choices.some(c => c != null && value === c[0]) ?
                    html`<option value="null" .selected=${value == null}>-- Choisissez une option --</option>` : html``}
                ${choices.filter(c => c != null).map(c =>
                    html`<option value=${stringifyValue(c[0])} .selected=${value === c[0]}>${c[1]}</option>`)}
            </select>
        `, options, errors);

        return addVariableWidget(key, label, render, value);
    };

    function handleChoiceChange(e, key, allow_untoggle) {
        let json = e.target.dataset.value;
        if (e.target.classList.contains('active') && allow_untoggle) {
            mem[key] = undefined;
        } else {
            mem[key] = parseValue(json);
        }

        // This is useless in most cases because the new form will incorporate
        // this change, but who knows. Do it like other browser-native widgets.
        let els = e.target.parentNode.querySelectorAll('button');
        for (let el of els)
            el.classList.toggle('active', el.dataset.value === json &&
                                          (!el.classList.contains('active') || !allow_untoggle));

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
                                 ?disabled=${options.disable} .className=${value === c[0] ? 'af_button active' : 'af_button'}
                                 @click=${e => handleChoiceChange(e, key, options.untoggle)}>${c[1]}</button>`)}
            </div>
        `, options, errors);

        return addVariableWidget(key, label, render, value);
    };

    this.binary = function(key, label, options = {}) {
        return self.choice(key, label, [[1, 'Oui'], [0, 'Non']], options);
    };
    this.boolean = function(key, label, options = {}) {
        return self.choice(key, label, [[true, 'Oui'], [false, 'Non']], options);
    };

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
                                ?disabled=${options.disable} .checked=${value === c[0]}
                                @click=${e => handleRadioChange(e, key, options.untoggle && value === c[0])}/>
                         <label for=${`${id}.${i}`}>${c[1]}</label><br/>`)}
            </div>
        `, options, errors);

        return addVariableWidget(key, label, render, value);
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
                                ?disabled=${options.disable} .checked=${value.includes(c[0])}
                                @click=${e => handleMultiChange(e, key)}/>
                         <label for=${`${id}.${i}`}>${c[1]}</label><br/>`)}
            </div>
        `, options, errors);

        return addVariableWidget(key, label, render, value);
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

        return addVariableWidget(key, label, render, value);
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

    this.conclude = function(label, options = {}) {
        options = Object.assign({}, options_stack[options_stack.length - 1], options);

        if (self.errors.length) {
            let render = () => html`
                <fieldset class="af_section af_section_error">
                    <legend>Liste des erreurs</legend>
                    ${self.errors.map(intf =>
                        html`${intf.errors.length} ${intf.errors.length > 1 ? 'erreurs' : 'erreur'} sur :
                             <a href=${'#' + makeID(intf.key)}>${intf.label}</a><br/>`)}
                </fieldset>
            `;

            addWidget(render);
        }

        // TODO: Add tooltip to button
        self.buttons([
            [label || 'Enregistrer', !self.errors.length ? self.submit : null]
        ]);
    };
}

function FormExecutor() {
    this.goHandler = key => {};
    this.submitHandler = mem => {};

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
        builder.submit = () => self.submitHandler(mem);

        // Prevent go() call from working if called during script eval
        let prev_go_handler = self.goHandler;
        let prev_submit_handler = self.submitHandler;
        self.goHandler = key => {
            throw new Error(`Navigation functions (go, form.submit, etc.) must be called from a callback (button click, etc.).

If you are using it for events, make sure you did not use this syntax by accident:
    go('page_key')
instead of:
    () => go('page_key')`);
        };
        self.submitHandler = self.goHandler;

        try {
            Function('form', 'go', script)(builder, key => self.goHandler(key));

            render(widgets.map(w => w.render(w.errors)), af_form);
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
            self.submitHandler = prev_submit_handler;
        }
    }

    this.setData = function(new_mem) { mem = new_mem; };
    this.getData = function() { return mem; }

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
    let default_key;

    let executor;
    let record_id;

    function pickDefaultKey() {
        if (default_key) {
            return default_key;
        } else if (pages.length) {
            return pages[0].key;
        } else {
            return null;
        }
    }

    function createPage(key, title, is_default) {
        let page = {
            key: key,
            title: title,
            script: ''
        };

        let t = goupil.database.transaction(db => {
            db.save('pages', page);
            if (is_default)
                db.saveWithKey('settings', 'default_page', key);
        });

        t.then(() => {
            pages.push(page);
            pages.sort((page1, page2) => util.compareValues(page1.key, page2.key));
            pages_map[key] = page;

            if (is_default)
                default_key = key;

            self.go(key);
        });
    }

    function editPage(page, title, is_default) {
        let new_page = Object.assign({}, page);
        new_page.title = title;

        let t = goupil.database.transaction(db => {
            db.save('pages', new_page);
            if (is_default) {
                db.saveWithKey('settings', 'default_page', page.key);
            } else if (default_key === page.key) {
                db.delete('settings', 'default_page');
            }
        });

        t.then(() => {
            Object.assign(page, new_page);

            if (is_default) {
                default_key = page.key;
            } else if (page.key === default_key) {
                default_key = null;
            }

            renderAll();
        });
    }

    function deletePage(page) {
        let t = goupil.database.transaction(db => {
            db.delete('pages', page.key);
            if (default_key === page.key)
                db.delete('settings', 'default_page');
        });

        t.then(() => {
            // Remove from pages array and map
            let page_idx = pages.findIndex(it => it.key === page.key);
            pages.splice(page_idx, 1);
            delete pages_map[page.key];

            if (page.key === default_key)
                default_key = null;

            self.go(pickDefaultKey());
        });
    }

    function resetPages() {
        let t = goupil.database.transaction(db => {
            db.clear('pages');
            for (let page of autoform_default.pages)
                db.save('pages', page);

            db.saveWithKey('settings', 'default_page', autoform_default.key);
        });

        t.then(() => {
            pages = autoform_default.pages.map(page => Object.assign({}, page));
            pages_map = {};
            for (let page of pages)
                pages_map[page.key] = page;

            default_key = autoform_default.key;
            executor = null;

            self.go(default_key);
        });
    }

    function showCreatePageDialog(e) {
        goupil.popup(e, form => {
            let key = form.text('key', 'Clé :');
            let title = form.text('title', 'Titre :');
            let is_default = form.boolean('is_default', 'Page par défaut :',
                                          {untoggle: false, value: false});

            if (key.value) {
                if (pages.some(page => page.key === key.value))
                    key.error('Existe déjà');
                if (!key.value.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
                    key.error('Autorisé : a-z, _ et 0-9 (sauf initiale)');
            }

            if (key.value && title.value && !form.errors.length) {
                form.submit = () => {
                    createPage(key.value, title.value, is_default.value);
                    form.close();
                };
            }
            form.buttons(form.buttons.std.OkCancel('Créer'));
        });
    }

    function showEditPageDialog(e, page) {
        goupil.popup(e, form => {
            let title = form.text('title', 'Titre :', {value: page.title});
            let is_default = form.boolean('is_default', 'Page par défaut :',
                                          {untoggle: false, value: default_key === page.key});

            if (title.value) {
                form.submit = () => {
                    editPage(page, title.value, is_default.value);
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

        if (page) {
            editor.setReadOnly(false);
            editor.container.classList.remove('disabled');

            return executor.render(af_form, page.key, page.script);
        } else {
            editor.setReadOnly(true);
            editor.container.classList.add('disabled');

            executor.render(af_form);
            if (current_key)
                executor.setError(null, `Unknown page '${current_key}'`);

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

    function saveData(mem) {
        goupil.database.save('data', mem);
    }

    this.go = function(key, id) {
        if (!executor) {
            executor = new FormExecutor();
            executor.goHandler = self.go;
            executor.submitHandler = saveData;
        }

        // Load record data
        if (id != null && id !== record_id) {
            goupil.database.load('data', id).then(data => {
                if (data) {
                    executor.setData(data);
                    record_id = id;

                    self.go(key);
                } else {
                    // TODO: Trigger goupil error in this case
                }
            });

            return;
        }
        if (record_id == null) {
            // TODO: Generate ULID-like IDs
            record_id = util.getRandomInt(1, 9007199254740991);
            executor.setData({id: record_id});
        }

        // Change current page
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

            // NOTE: Migration from localStorage to IndexedDB, drop localStorage
            // support in a month or two.
            try {
                let json = localStorage.getItem('goupil_af_pages');
                let pages2 = JSON.parse(json).map(kv => kv[1]);
                let default_key2 = pages.length ? pages[0].key : null;

                let t = goupil.database.transaction(db => {
                    if (default_key2)
                        db.saveWithKey('settings', 'default_page', default_key2);
                    db.saveAll('pages', pages2);
                });

                t.then(() => {
                    localStorage.removeItem('goupil_af_pages');
                    self.activate();
                });
            } catch (err) {
                let t = goupil.database.transaction(db => {
                    db.load('settings', 'default_page').then(default_page => { default_key = default_page; });
                    db.loadAll('pages').then(pages2 => {
                        if (pages2.length) {
                            pages = Array.from(pages2);
                        } else {
                            pages = autoform_default.pages;
                        }
                        for (let page of pages)
                            pages_map[page.key] = page;
                    });
                });

                t.then(self.activate);

                init = true;
            }
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

        self.go(current_key || pickDefaultKey());
    };

    return this;
}).call({});
