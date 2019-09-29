// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function TreeSelector() {
    let self = this;

    this.changeHandler = (values, tab) => {};

    let prefix = null;
    let tabs = [];
    let values = new Set;
    let groups = [];

    let current_tab = {
        title: null,
        options: []
    };
    let current_values = new Set;

    this.setPrefix = function(str) { prefix = str; };
    this.getPrefix = function() { return prefix; };

    this.addTab = function(title) {
        let tab = {
            title: title,
            options: []
        };

        tabs.push(tab);
        current_tab = tab;
        groups.length = 0;
    };

    this.beginGroup = function(title) {
        let opt = {
            title: title,
            depth: groups.length
        };

        current_tab.options.push(opt);
        groups.push(title);
    };
    this.endGroup = function() {
        groups.pop();
    };

    this.addOption = function(title, value, options = {}) {
        if (Array.isArray(title)) {
            while (groups.length >= title.length ||
                   groups[groups.length - 1] !== title[groups.length - 1])
                self.endGroup();
            for (let i = groups.length; i < title.length - 1; i++)
                self.beginGroup(title[i]);

            title = title[title.length - 1];
        }

        let opt = {
            title: title,
            value: value,
            depth: groups.length,
            disabled: !!options.disabled
        };
        let selected = !!options.selected & !opt.disabled;

        current_tab.options.push(opt);

        values.add(value);
        if (selected)
            current_values.add(value);
    };

    this.setCurrentTab = function(title) {
        let new_tab = tabs.find(tab => tab.title === title);
        if (!new_tab)
            throw new Error(`Tab '${title}' does not exist`);
        current_tab = new_tab;
    };
    this.getCurrentTab = function() { return current_tab.title; };

    this.setValues = function(values) {
        if (values instanceof Set) {
            current_values = values;
        } else {
            current_values = new Set(values);
        }
    }
    this.getValues = function() { return current_values; };

    this.render = function() {
        let root_el = document.createElement('div');
        root_el.className = 'tsel';
        root_el.addEventListener('click', e => e.stopPropagation());

        // We don't return VDOM, because if we did the next render() we do after user interaction
        // would use a new binding, and replace the widget.
        render(renderWidget(), root_el);

        return root_el;
    };

    function renderWidget() {
        // The dummy button catches click events that happen when a label encloses the widget
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
        let label_els = root_el.querySelectorAll('.tsel_list > label');

        let or_state = 0;
        let and_state = 0xFFFFFFFF;
        for (let i = label_els.length - 1; i >= 0; i--) {
            let el = label_els[i];

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
