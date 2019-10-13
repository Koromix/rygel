// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev_form = (function() {
    let self = this;

    let current_asset;
    let current_record = {};

    let editor_el;
    let page_el;
    let log_el;

    let left_panel = 'editor';
    let show_main_panel = true;

    let editor;
    let editor_history_cache = {};

    let form_state = new FormState;

    this.run = async function(asset, args) {
        // Load record (if needed)
        if (!args.hasOwnProperty('id') && asset.key !== current_record.table)
            current_record = {};
        if (args.hasOwnProperty('id') || current_record.id == null) {
            if (args.id == null) {
                current_record = g_records.create(asset.key);
            } else if (args.id !== current_record.id) {
                current_record = await g_records.load(asset.key, args.id);
            }

            form_state = new FormState(current_record.values);
        }
        current_asset = asset;

        // Render
        renderLayout();
        renderModes();
        switch (left_panel) {
            case 'editor': { syncEditor(); } break;
            case 'records': { dev_records.run(asset.key, current_record.id); } break;
        }
        renderForm();
    };

    function renderLayout() {
        let main_el = document.querySelector('main');

        if (left_panel === 'editor' && show_main_panel) {
            render(html`
                ${makeEditorElement('dev_panel_left')}
                <div id="dev_page" class="dev_panel_right"></div>
                <div id="dev_log" style="display: none;"></div>
            `, main_el);
        } else if (left_panel === 'records' && show_main_panel) {
            render(html`
                <div id="dev_records" class="dev_panel_left"></div>
                <div id="dev_page" class="dev_panel_right"></div>
                <div id="dev_log" style="display: none;"></div>
            `, main_el);
        } else if (left_panel === 'editor') {
            render(html`
                ${makeEditorElement('dev_panel_fixed')}
                <div id="dev_log" style="display: none;"></div>
            `, main_el);
        } else if (left_panel === 'records') {
            render(html`
                <div id="dev_records" class="dev_panel_fixed"></div>
                <div id="dev_log" style="display: none;"></div>
            `, main_el);
        } else {
            render(html`
                <div id="dev_page" class="dev_panel_page"></div>
                <div id="dev_log" style="display: none;"></div>
            `, main_el);
        }

        modes_el = document.querySelector('#dev_modes');
        log_el = document.querySelector('#dev_log');
        if (show_main_panel) {
            page_el = document.querySelector('#dev_page');
        } else {
            // We still need to render the form to test it, so create a dummy element!
            page_el = document.createElement('div');
        }
    }

    function makeEditorElement(cls) {
        if (!editor_el) {
            editor_el = document.createElement('div');
            editor_el.id = 'dev_editor';
        }

        for (let cls of editor_el.classList) {
            if (!cls.startsWith('ace_') && !cls.startsWith('ace-'))
                editor_el.classList.remove(cls);
        }
        editor_el.classList.add(cls);

        return editor_el;
    }

    function renderModes() {
        render(html`
            <button class=${left_panel === 'editor' ? 'active' : ''} @click=${e => toggleLeftPanel('editor')}>Éditeur</button>
            <button class=${left_panel === 'records' ? 'active' : ''} @click=${e => toggleLeftPanel('records')}>Données</button>
            <button class=${show_main_panel ? 'active': ''} @click=${e => toggleMainPanel()}>Aperçu</button>
        `, modes_el);
    }

    function toggleLeftPanel(type) {
        if (left_panel !== type) {
            left_panel = type;
        } else {
            left_panel = null;
            show_main_panel = true;
        }

        dev.go();
    }

    function toggleMainPanel() {
        if (!left_panel)
            left_panel = 'editor';
        show_main_panel = !show_main_panel;

        dev.go();
    }

    function renderForm() {
        try {
            let elements = executeScript();

            // Things are OK!
            log_el.innerHTML = '';
            log_el.style.display = 'none';

            page_el.classList.remove('dev_broken');
            render(elements, page_el);

            return true;
        } catch (err) {
            let line;
            if (err instanceof SyntaxError) {
                // At least Firefox seems to do well in this case, it's better than nothing
                line = err.lineNumber - 2;
            } else if (err.stack) {
                line = parseAnonymousErrorLine(err);
            }

            log_el.textContent = `⚠\uFE0E Line ${line || '?'}: ${err.message}`;
            log_el.style.display = 'block';

            page_el.classList.add('dev_broken');

            return false;
        }
    }

    function executeScript() {
        let widgets = [];
        let variables = [];

        let page_obj = new FormPage(form_state, widgets, variables);
        page_obj.changeHandler = renderForm;
        page_obj.submitHandler = saveRecordAndReset;

        // Execute user script
        let func = Function('page', 'form', current_asset.data);
        func(page_obj, page_obj);

        // Render widgets
        elements = widgets.map(intf => intf.render(intf));

        return elements;
    }

    async function saveRecordAndReset(values, variables) {
        variables = variables.map(variable => ({
            key: variable.key,
            type: variable.type
        }));

        let entry = new log.Entry();
        entry.progress('Enregistrement en cours');

        await g_records.save(current_record, variables);
        entry.success('Données enregistrées !');

        dev.go(null, {id: null});
        // TODO: Give focus to first widget
        window.scrollTo(0, 0);
    }

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

    async function syncEditor() {
        // FIXME: Make sure we don't run loadScript more than once
        if (!window.ace)
            await util.loadScript(`${env.base_url}static/ace.js`);

        if (!editor) {
            editor = ace.edit(editor_el);

            editor.setTheme('ace/theme/monokai');
            editor.setShowPrintMargin(false);
            editor.setFontSize(12);
            editor.session.setOption('useWorker', false);
            editor.session.setMode('ace/mode/javascript');

            editor.on('change', e => {
                // If something goes wrong during handleEditorChange(), we don't
                // want to break ACE state.
                setTimeout(handleEditorChange, 0);
            });
        }

        if (current_asset) {
            let history = editor_history_cache[current_asset.key];

            if (history !== editor.session.getUndoManager()) {
                editor.setValue(current_asset.data);
                editor.setReadOnly(false);
                editor.clearSelection();

                if (!history) {
                    history = new ace.UndoManager();
                    editor_history_cache[current_asset.key] = history;
                }
                editor.session.setUndoManager(history);
            }
        } else {
            editor.setValue('');
            editor.setReadOnly(true);

            editor.session.setUndoManager(new ace.UndoManager());
        }
    }

    function handleEditorChange() {
        if (current_asset) {
            let prev_script = current_asset.data;

            current_asset.data = editor.getValue();

            if (renderForm()) {
                g_assets.save(current_asset);
            } else {
                // Restore working script
                current_asset.data = prev_script;
            }
        }
    }

    return this;
}).call({});
