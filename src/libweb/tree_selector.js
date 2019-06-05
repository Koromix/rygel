// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function TreeSelector(widget, prefix)
{
    this.changeHandler = null;

    let self = this;
    let curtab = null;
    let summary = null;
    let tabbar = null;
    let view = null;
    let list = null;

    let depth = 0;
    let total = 0;

    function syncValue(value, checked)
    {
        let checkboxes = widget.queryAll('.tsel_option input[data-value="' + value + '"]');
        checkboxes.forEach(function(checkbox) {
            checkbox.checked = checked;
        });
    }

    // Does not work correctly for deep hierarchies (more than 32 levels)
    function syncGroupCheckboxes()
    {
        let elements = widget.queryAll('.tsel_list > label');

        let or_state = 0;
        let and_state = 0xFFFFFFFF;
        for (let i = elements.length - 1; i >= 0; i--) {
            let el = elements[i];

            let depth = parseInt(el.dataset.depth, 10);
            let checkbox = el.query('input[type=checkbox]');

            if (el.hasClass('tsel_group')) {
                let check = !!(or_state & (1 << (depth + 1)));
                checkbox.indeterminate = check && !(and_state & (1 << (depth + 1)));
                checkbox.checked = check && !checkbox.indeterminate;
            }

            or_state &= (1 << (depth + 1)) - 1;
            and_state |= ~((1 << (depth + 1)) - 1);
            if (checkbox.indeterminate || checkbox.checked)
                or_state |= 1 << depth;
            if (checkbox.indeterminate || !checkbox.checked)
                and_state &= ~(1 << depth);
        }
    }

    function updateSummary()
    {
        let values = self.getValues();

        function handleSummaryOptionClick(e)
        {
            syncValue(this.textContent, false);
            syncGroupCheckboxes();
            updateSummary();

            e.preventDefault();
            e.stopPropagation();

            if (!widget.hasClass('active') && self.changeHandler)
                setTimeout(function() { self.changeHandler.call(widget); }, 0);
        }

        if (!curtab.hasClass('hide'))
            curtab.innerHTML = tabbar.childNodes[self.getActiveTab()].innerHTML;

        summary.replaceContent(prefix);
        if (!values.length) {
            let a = dom.h('a', 'Aucune sélection');
            summary.appendChild(a);
        } else if (values.length < 8) {
            for (const value of values) {
                let a = dom.h('a', {href: '#', click: handleSummaryOptionClick}, value);
                summary.appendChild(a);
            }
        } else {
            let a = dom.h('a', '' + values.length + ' / ' + total);
            summary.appendChild(a);
        }
    }

    function selectTab(tab)
    {
        widget.queryAll('.tsel_tab.active, .tsel_list.active').removeClass('active');
        tab.addClass('active');
        tab.list.addClass('active');

        syncGroupCheckboxes();
        updateSummary();
    }

    function setVisibleState(checked)
    {
        let list = widget.query('.tsel_list.active');

        let checkboxes = list.queryAll('.tsel_option input[type=checkbox]');
        checkboxes.forEach(function(checkbox) {
            syncValue(checkbox.dataset.value, checked);
        });

        syncGroupCheckboxes();
        updateSummary();
    }

    function handleTabClick(e)
    {
        selectTab(this);
        e.preventDefault();
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
        let group = this.parentNode;
        let sibling = group.nextSibling;
        while (sibling && sibling.dataset.depth > group.dataset.depth) {
            if (sibling.hasClass('tsel_option')) {
                let checkbox = sibling.query('input[type=checkbox]');
                syncValue(checkbox.dataset.value, this.checked);
            }
            sibling = sibling.nextSibling;
        }

        syncGroupCheckboxes();
        updateSummary();
    }

    function handleOptionClick(e)
    {
        syncValue(this.dataset.value, this.checked);
        syncGroupCheckboxes();
        updateSummary();
    }

    this.createTab = function(name, active) {
        if (tabbar.hasClass('hide')) {
            tabbar.removeClass('hide');
            curtab.removeClass('hide');
        } else {
            list = dom.h('div', {class: 'tsel_list'});
            view.appendChild(list);
        }

        let tab = dom.h('a', {class: 'tsel_tab', href: '#', click: handleTabClick}, name);
        if (list.hasClass('active'))
            tab.addClass('active');
        tab.list = list;
        tabbar.appendChild(tab);

        depth = 0;
    };

    this.beginGroup = function(name) {
        let el = dom.h('label', {class: 'tsel_group', style: 'padding-left: ' + depth + 'em;',
                                 'data-depth': depth},
            dom.h('input', {type: 'checkbox', click: handleGroupClick}),
            name
        );

        list.appendChild(el);
        depth++;
    };
    this.endGroup = function() {
        depth--;
    };

    this.addOption = function(name, value, options) {
        if (!options)
            options = {};
        options.disabled = options.disabled || false;
        options.selected = options.selected || false;

        options.selected &= !options.disabled;
        let el = dom.h('label', {class: 'tsel_option' + (options.disabled ? ' disabled' : ''),
                                 style: 'padding-left: ' + depth + 'em;', 'data-depth': depth},
            dom.h('input', {type: 'checkbox', click: handleOptionClick,
                            'data-value': value, checked: options.selected ? 'checked' : null}),
            name
        );

        list.appendChild(el);
    };

    this.open = function() {
        queryAll('.tsel.active').forEach(function(tsel) {
            tsel.object.close();
        });
        widget.addClass('active');
    }
    this.toggle = function(state) {
        if (!widget.hasClass('active')) {
            self.open();
        } else {
            self.close();
        }
    };
    this.close = function() {
        widget.removeClass('active');
        if (self.changeHandler)
            setTimeout(function() { self.changeHandler.call(widget); }, 0);
    };

    this.getActiveTab = function() {
        for (let i = 0; i < tabbar.childNodes.length; i++) {
            if (tabbar.childNodes[i].hasClass('active'))
                return i;
        }
        return 0;
    };
    this.setActiveTab = function(idx) {
        if (idx >= 0 && idx < tabbar.childNodes.length)
            selectTab(tabbar.childNodes[idx]);
    };

    this.getValues = function(all) {
        if (all === undefined)
            all = false;

        let values = [];
        {
            let checkboxes = widget.queryAll('.tsel_option input[type=checkbox]');
            checkboxes.forEach(function(checkbox) {
                if (checkbox.checked || all)
                    values.push(checkbox.dataset.value);
            });
        }

        values = values.sort().filter(function(value, i) {
            return !i || value !== values[i - 1];
        });

        return values;
    };

    // TODO: Only mess with the (visible?) DOM when 'rendering'
    this.render = function() {
        total = self.getValues(true).length;

        syncGroupCheckboxes();
        updateSummary();
    }

    this.getWidget = function() { return widget; }

    widget.addClass('tsel');
    widget.addEventListener('click', function(e) { e.stopPropagation(); });
    widget.replaceContent(
        // This dummy button catches click events that happen when a label encloses the widget
        dom.h('button', {style: 'display: none;', click: function(e) { e.preventDefault(); }}),

        dom.h('div', {class: 'tsel_main'},
            dom.h('div', {class: 'tsel_rect', click: function(e) { self.toggle(); }},
                 dom.h('div', {class: ['tsel_curtab', 'hide']}),
                 dom.h('div', {class: 'tsel_summary'})
            ),
            dom.h('div', {class: 'tsel_view'},
                dom.h('div', {class: ['tsel_tabbar', 'hide']}),
                dom.h('div', {class: 'tsel_shortcuts'},
                    dom.h('a', {href: '#', click: handleCheckAll}, 'Tout cocher'), ' / ',
                    dom.h('a', {href: '#', click: handleUncheckAll}, 'Tout décocher')
                ),
                dom.h('button', {class: 'tsel_validate', click: self.close}, 'Fermer'),
                dom.h('div', {class: ['tsel_list', 'active']})
            )
        )
    );
    curtab = widget.query('.tsel_curtab');
    summary = widget.query('.tsel_summary');
    tabbar = widget.query('.tsel_tabbar');
    view = widget.query('.tsel_view');
    list = widget.query('.tsel_list');

    widget.object = this;
}

document.addEventListener('click', function(e) {
    queryAll('.tsel.active').forEach(function(tsel) {
        tsel.object.close();
    });
});
