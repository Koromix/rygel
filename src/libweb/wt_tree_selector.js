// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function TreeSelector() {
    let self = this;

    this.changeHandler = null;

    let prefix = null;
    let tabs = [];
    let values = new Set;

    let current_tab = {
        title: null,
        options: []
    };
    let current_values = new Set;

    let current_depth = 0;

    this.setPrefix = function(str) { prefix = str; };
    this.getPrefix = function() { return prefix; };

    this.addTab = function(title) {
        let tab = {
            title: title,
            options: []
        };

        tabs.push(tab);
        current_tab = tab;
        current_depth = 0;
    };

    this.beginGroup = function(title) {
        let opt = {
            title: title,
            depth: current_depth
        };

        current_tab.options.push(opt);
        current_depth++;
    };
    this.endGroup = function() {
        current_depth--;
    };

    this.addOption = function(title, value, options = {}) {
        let opt = {
            title: title,
            value: value,
            depth: current_depth,
            disabled: !!options.disabled
        };
        let selected = !!options.selected & !opt.disabled;

        current_tab.options.push(opt);

        values.add(value);
        if (selected)
            current_values.add(value);
    };

    this.setCurrentTab = function(tab_idx) { current_tab = tabs[tab_idx]; };
    this.getCurrentTab = function() { return tabs.indexOf(current_tab); };

    this.setValues = function(values) { current_values = new Set(values); };
    this.getValues = function() { return Array.from(current_values); };

    this.render = function() {
        // The dummy button catches click events that happen when a label encloses the widget
        return html`
            <div class="tsel" @click=${e => e.stopPropagation()}>
                ${renderWidget()}
            </div>
        `;
    };

    function renderWidget() {
        return html`
            <button style="display: none;" @click=${e => e.preventDefault()}></button>

            <div class="tsel_main">
                <div class="tsel_rect" @click=${handleToggleClick}>
                    ${tabs.length ? html`<div class="tsel_curtab">${current_tab.title}</div>` : html``}
                    <div class="tsel_summary">
                        ${renderSummary()}
                    </div>
                </div>

                <div class="tsel_view">
                    <div class="tsel_tabbar">${tabs.map(tab => html`
                        <button class=${'tsel_tab' + (tab === current_tab ? ' active' : '')}
                                @click=${e => handleTabClick(e, tab)}>${tab.title}</button>
                    `)}</div>
                    <div class="tsel_shortcuts">
                        <a href="#" @click=${e => handleSetAll(e, true)}>Tout cocher</a> /
                        <a href="#" @click=${e => handleSetAll(e, false)}>Tout décocher</a>
                    </div>

                    <div class="tsel_list">${current_tab.options.map(opt => {
                        if (opt.value === undefined) {
                            return html`
                                <label class="tsel_group"
                                       style=${'padding-left: ' + opt.depth + 'em;'} data-depth=${opt.depth}>
                                    <input type="checkbox" @click=${handleGroupClick}/>
                                    ${opt.title}
                                </label>
                            `;
                        } else {
                            return html`
                                <label class=${'tsel_option' + (opt.disabled ? ' disabled' : '')}
                                       style=${'padding-left: ' + opt.depth + 'em;'} data-depth=${opt.depth}>
                                    <input type="checkbox" data-value=${util.valueToStr(opt.value)}
                                           ?disabled=${opt.disabled} @click=${handleOptionClick}/>
                                    ${opt.title}
                                </label>
                            `;
                        }
                    })}</div>

                    <button class="tsel_validate" @click=${handleToggleClick}>Fermer</button>
                </div>
            </div>
        `;
    }

    function renderSummary() {
        if (!current_values.size) {
            return html`${prefix}Aucune sélection`;
        } else if (current_values.size < 8) {
            let sorted_values = Array.from(current_values).sort();

            return html`${prefix}
                ${sorted_values.map(value => html`<a href="#" @click=${handleSummaryClick}
                                                     data-value=${util.valueToStr(value)}>${value}</a>`)}
            `;
        } else {
            return html`${prefix}${current_values.size} / ${values.size}`;
        }
    }

    function handleSummaryClick(e) {
        let root_el = util.findParent(e.target, el => el.classList.contains('tsel'));
        let summary_el = root_el.querySelector('.tsel_summary');
        let view_el = root_el.querySelector('.tsel_view');

        let value = util.strToValue(e.target.dataset.value);
        updateValue(value, false);

        render(renderSummary(), summary_el);
        syncCheckboxes(root_el);

        e.preventDefault();
        e.stopPropagation();

        if (!view_el.classList.contains('active'))
            setTimeout(() => self.changeHandler.call(self, self.getValues()), 0);
    }

    function handleTabClick(e, tab) {
        let root_el = util.findParent(e.target, el => el.classList.contains('tsel'));

        current_tab = tab;
        render(renderWidget(), root_el);
    }

    function handleSetAll(e, enable) {
        let root_el = util.findParent(e.target, el => el.classList.contains('tsel'));
        let summary_el = root_el.querySelector('.tsel_summary');

        for (let opt of current_tab.options) {
            if (opt.value !== undefined)
                updateValue(opt.value, enable);
        }

        render(renderSummary(), summary_el);
        syncCheckboxes(root_el);

        e.preventDefault();
    }

    function handleGroupClick(e) {
        let root_el = util.findParent(e.target, el => el.classList.contains('tsel'));
        let summary_el = root_el.querySelector('.tsel_summary');

        let group = e.target.parentNode;
        let sibling = group.nextElementSibling;
        while (sibling && sibling.dataset.depth > group.dataset.depth) {
            if (sibling.classList.contains('tsel_option')) {
                let input = sibling.querySelector('input[type=checkbox]');
                let value = util.strToValue(input.dataset.value);

                updateValue(value, e.target.checked);
            }
            sibling = sibling.nextElementSibling;
        }

        render(renderSummary(), summary_el);
        syncCheckboxes(root_el);
    }

    function handleOptionClick(e) {
        let root_el = util.findParent(e.target, el => el.classList.contains('tsel'));
        let summary_el = root_el.querySelector('.tsel_summary');

        let value = util.strToValue(e.target.dataset.value);
        updateValue(value, e.target.checked);

        render(renderSummary(), summary_el);
        syncCheckboxes(root_el);
    }

    function handleToggleClick(e) {
        let root_el = util.findParent(e.target, el => el.classList.contains('tsel'));
        let view_el = root_el.querySelector('.tsel_view');

        if (!view_el.classList.contains('active')) {
            if (TreeSelector.close_func)
                TreeSelector.close_func();

            TreeSelector.close_func = () => {
                view_el.classList.remove('active');
                TreeSelector.close_func = null;

                if (self.changeHandler)
                    setTimeout(() => self.changeHandler.call(self, self.getValues()), 0);
            };

            syncCheckboxes(root_el);
            view_el.classList.add('active');
        } else {
            // Only one selector can be open at the same time: this must be it!
            TreeSelector.close_func();
        }
    }

    function updateValue(value, enable) {
        if (enable && values.has(value)) {
            current_values.add(value);
        } else {
            current_values.delete(value);
        }
    }

    // Does not work correctly for deep hierarchies (more than 32 levels)
    function syncCheckboxes(root_el) {
        let labels = root_el.querySelectorAll('.tsel_list > label');

        let or_state = 0;
        let and_state = 0xFFFFFFFF;
        for (let i = labels.length - 1; i >= 0; i--) {
            let el = labels[i];

            let depth = parseInt(el.dataset.depth, 10);
            let input = el.querySelector('input[type=checkbox]');

            if (el.classList.contains('tsel_group')) {
                let check = !!(or_state & (1 << (depth + 1)));
                input.indeterminate = check && !(and_state & (1 << (depth + 1)));
                input.checked = check && !input.indeterminate;
            } else {
                let value = util.strToValue(input.dataset.value);
                input.checked = current_values.has(value);
            }

            or_state &= (1 << (depth + 1)) - 1;
            and_state |= ~((1 << (depth + 1)) - 1);
            if (input.indeterminate || input.checked)
                or_state |= 1 << depth;
            if (input.indeterminate || !input.checked)
                and_state &= ~(1 << depth);
        }
    }
}
TreeSelector.close_func = null;

document.addEventListener('click', e => {
    if (TreeSelector.close_func)
        TreeSelector.close_func();
});
