// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let dev_form = new function() {
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

    let form_state;

    this.run = async function(asset = null, args = {}) {
        if (asset) {
            current_asset = asset;
        } else {
            asset = current_asset;
        }

        // Load record (if needed)
        if (!args.hasOwnProperty('id') && asset.path !== current_record.table)
            current_record = {};
        if (args.hasOwnProperty('id') || current_record.id == null) {
            if (args.id == null) {
                current_record = g_records.create(asset.path);
            } else if (args.id !== current_record.id) {
                current_record = await g_records.load(asset.path, args.id);
            }

            form_state = new FormData(current_record.values);
        }

        // Render
        renderLayout();
        renderModes();
        switch (left_panel) {
            case 'editor': { syncEditor(); } break;
            case 'records': { dev_records.run(asset.path, current_record.id); } break;
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

        self.run();
    }

    function toggleMainPanel() {
        if (!left_panel)
            left_panel = 'editor';
        show_main_panel = !show_main_panel;

        self.run();
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
            let err_line = util.parseEvalErrorLine(err);

            log_el.textContent = `⚠\uFE0E Line ${err_line || '?'}: ${err.message}`;
            log_el.style.display = 'block';

            page_el.classList.add('dev_broken');

            return false;
        }
    }

    function executeScript() {
        let widgets = [];
        let variables = [];

        let page_builder = new FormPage(form_state, widgets, variables);
        page_builder.decodeKey = decodeFormKey;
        page_builder.changeHandler = renderForm;
        page_builder.submitHandler = saveRecordAndReset;

        // Execute user script
        let func = Function('page', 'form', current_asset.data);
        func(page_builder, page_builder);

        // Render widgets
        elements = widgets.map(intf => intf.render(intf));

        return elements;
    }

    function decodeFormKey(key) {
        let record_ids = ['foo'];
        let table_names = ['bar'];

        if (typeof key === 'string') {
            let split_idx = key.indexOf('.');

            if (split_idx >= 0) {
                key = {
                    record_id: null,
                    table: key.substr(0, key_idx),
                    variable: key.substr(key_idx + 1),

                    toString: null
                }
            } else {
                key = {
                    record_id: null,
                    table: null,
                    variable: key,

                    toString: null
                };
            }
        }

        if (!key.record_id) {
            if (record_ids.length > 1)
                throw new Error('Ambiguous key (multiple record IDs are available)');

            key.record_id = record_ids[0];
        }
        if (!key.table)
            key.table = table_names[0];

        if (!key.variable)
            throw new Error('Empty keys are not allowed');
        if (!key.table.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/) ||
                !key.variable.match(/^[a-zA-Z_][a-zA-Z0-9_]*$/))
            throw new Error('Allowed key characters: a-z, _ and 0-9 (not as first character)');

        let str = `${key.record_id}+${key.table}.${key.variable}`;
        key.toString = () => str;

        return Object.freeze(key);
    }

    async function saveRecordAndReset(values, variables) {
        let entry = new log.Entry();
        entry.progress('Enregistrement en cours');

        await g_records.save(current_record, variables);
        entry.success('Données enregistrées !');

        self.run(null, {id: null});
        // TODO: Give focus to first widget
        window.scrollTo(0, 0);
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
            let history = editor_history_cache[current_asset.path];

            if (history !== editor.session.getUndoManager()) {
                editor.setValue(current_asset.data);
                editor.setReadOnly(false);
                editor.clearSelection();

                if (!history) {
                    history = new ace.UndoManager();
                    editor_history_cache[current_asset.path] = history;
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
};
