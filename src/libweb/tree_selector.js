// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function TreeSelector()
{
    let self = this;

    this.changeHandler = null;

    let root_el;
    let curtab_el;
    let summary_el;
    let tabbar_el;
    let view_el;
    let list_el;

    let prefix = null;
    let tabs = [];
    let values = new Set;

    let current_tab = {
        title: null,
        options: []
    };
    let current_values = new Set;

    let current_depth = 0;

    this.getRootElement = function() { return root_el; };

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

    this.setCurrentTab = function(tab_idx) { current_tab = tabs[tab_idx]; }
    this.getCurrentTab = function() { return tabs.indexOf(current_tab); }
    this.setValues = function(values) { current_values = new Set(values); }
    this.getValues = function() { return Array.from(current_values); }

    function updateValue(value, enable)
    {
        if (enable && values.has(value)) {
            current_values.add(value);
        } else {
            current_values.delete(value);
        }
    }

    // Does not work correctly for deep hierarchies (more than 32 levels)
    function syncCheckboxes()
    {
        let labels = root_el.queryAll('.tsel_list > label');

        let or_state = 0;
        let and_state = 0xFFFFFFFF;
        for (let i = labels.length - 1; i >= 0; i--) {
            let el = labels[i];

            let depth = parseInt(el.dataset.depth, 10);
            let input = el.query('input[type=checkbox]');

            if (el.hasClass('tsel_group')) {
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

    function handleSummaryClick(e)
    {
        let target = e.target;
        let value = util.strToValue(target.dataset.value);

        updateValue(value, false);
        syncCheckboxes();
        syncSummary();

        e.preventDefault();
        e.stopPropagation();

        if (!view_el.hasClass('active') && self.changeHandler)
            setTimeout(function() { self.changeHandler.call(self); }, 0);
    }

    function syncSummary()
    {
        if (!current_values.size) {
            render(html`${prefix}<a>Aucune sélection</a>`, summary_el);
        } else if (current_values.size < 8) {
            let sorted_values = Array.from(current_values).sort();

            render(html`${prefix}
                ${sorted_values.map(value => html`<a href="#" @click=${handleSummaryClick}
                                                     data-value=${util.valueToStr(value)}>${value}</a>`)}
            `, summary_el);
        } else {
            render(html`${prefix}<a>${current_values.size} / ${values.size}</a>`, summary_el);
        }
    }

    function setVisibleState(enable)
    {
        for (let opt of current_tab.options) {
            if (opt.value !== undefined)
                updateValue(opt.value, enable);
        }

        syncCheckboxes();
        syncSummary();
    }

    function handleTabClick(e, tab)
    {
        current_tab = tab;
        self.render(root_el);
    }

    function handleCheckAll(e)
    {
        setVisibleState(true);
        e.preventDefault();
    }

    function handleUncheckAll(e)
    {
        setVisibleState(false);
        e.preventDefault();
    }

    function handleGroupClick(e)
    {
        let target = e.target;

        let group = target.parentNode;
        let sibling = group.nextElementSibling;
        while (sibling && sibling.dataset.depth > group.dataset.depth) {
            if (sibling.hasClass('tsel_option')) {
                let input = sibling.query('input[type=checkbox]');
                let value = util.strToValue(input.dataset.value);

                updateValue(value, target.checked);
            }
            sibling = sibling.nextElementSibling;
        }

        syncCheckboxes();
        syncSummary();
    }

    function handleOptionClick(e)
    {
        let target = e.target;
        let value = util.strToValue(target.dataset.value);

        updateValue(value, target.checked);

        syncCheckboxes();
        syncSummary();
    }

    this.open = function() {
        queryAll('.tsel_view.active').forEach(el => el.intf.close());
        view_el.addClass('active');
    }
    this.toggle = function(state) {
        if (!view_el.hasClass('active')) {
            self.open();
        } else {
            self.close();
        }
    };
    this.close = function() {
        view_el.removeClass('active');
        if (self.changeHandler)
            setTimeout(function() { self.changeHandler.call(self); }, 0);
    };

    this.render = function(new_root_el) {
        root_el = new_root_el;

        // The dummy button catches click events that happen when a label encloses the widget
        render(html`
            <div class="tsel" @click=${e => e.stopPropagation()}>
                <button style="display: none;" @click=${e => e.preventDefault()}></button>

                <div class="tsel_main">
                    <div class="tsel_rect" @click=${e => self.toggle()}>
                        ${tabs.length ? html`<div class="tsel_curtab">${current_tab.title}</div>` : html``}
                        <div class="tsel_summary"></div>
                    </div>

                    <div class="tsel_view">
                        <div class="tsel_tabbar">${tabs.map(tab => html`
                            <button class=${'tsel_tab' + (tab === current_tab ? ' active' : '')}
                                    @click=${e => handleTabClick(e, tab)}>${tab.title}</button>
                        `)}</div>
                        <div class="tsel_shortcuts">
                            <a href="#" @click=${handleCheckAll}>Tout cocher</a> /
                            <a href="#" @click=${handleUncheckAll}>Tout décocher</a>
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

                        <button class="tsel_validate" @click=${self.close}>Fermer</button>
                    </div>
                </div>
            </div>
        `, root_el);
        curtab_el = root_el.query('.tsel_curtab');
        summary_el = root_el.query('.tsel_summary');
        tabbar_el = root_el.query('.tsel_tabbar');
        view_el = root_el.query('.tsel_view');
        list_el = root_el.query('.tsel_list');

        // Make it easy to find self for active selectors
        view_el.intf = self;

        syncCheckboxes();
        syncSummary();
    }
}

document.addEventListener('click', e => {
    queryAll('.tsel_view.active').forEach(el => el.intf.close());
});
