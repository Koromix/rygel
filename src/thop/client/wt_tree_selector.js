// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function TreeSelector(prefix)
{
    'use strict';

    this.changeHandler = null;

    let self = this;
    let widget = null;
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
        for (let checkbox of checkboxes)
            checkbox.checked = checked;
    }

    // Does not work correctly for deep hierarchies (more than 32 levels)
    function syncGroupCheckboxes()
    {
        let elements = widget.queryAll('.tsel_list > label');

        let or_state = 0;
        let and_state = 0xFFFFFFFF;
        for (let i = elements.length - 1; i >= 0; i--) {
            let el = elements[i];

            let depth = parseInt(el.dataset.depth);
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

        summary.innerHTML = '';
        summary.appendChild(document.createTextNode(prefix));
        if (!values.length) {
            let a = html('a', 'Aucune option sélectionnée');
            summary.appendChild(a);
        } else if (values.length < 8) {
            for (const value of values) {
                let a = html('a', {href: '#', click: handleSummaryOptionClick}, value);
                summary.appendChild(a);
            }
        } else {
            let a = html('a', '' + values.length + ' / ' + total);
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
        for (const checkbox of checkboxes)
            syncValue(checkbox.dataset.value, checked);

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
            list = html('div', {class: 'tsel_list'});
            view.appendChild(list);
        }

        let tab = html('a', {class: 'tsel_tab', href: '#', click: handleTabClick}, name);
        if (list.hasClass('active'))
            tab.addClass('active');
        tab.list = list;
        tabbar.appendChild(tab);

        depth = 0;
    };

    this.beginGroup = function(name) {
        let el = html('label', {class: 'tsel_group', style: 'padding-left: ' + depth + 'em;',
                                         'data-depth': depth},
            html('input', {type: 'checkbox', click: handleGroupClick}),
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
        let el = html('label', {class: 'tsel_option' + (options.disabled ? ' disabled' : ''),
                                style: 'padding-left: ' + depth + 'em;', 'data-depth': depth},
            html('input', {type: 'checkbox', click: handleOptionClick,
                           'data-value': value, checked: options.selected ? 'checked' : null}),
            name
        );

        list.appendChild(el);
    };

    this.open = function() { widget.addClass('active'); };
    this.toggle = function(state) {
        if (!widget.toggleClass('active', state) && self.changeHandler)
             setTimeout(function() { self.changeHandler.call(widget); }, 0);
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
        if (idx < tabbar.childNodes.length)
            selectTab(tabbar.childNodes[idx]);
    };

    this.getValues = function(all) {
        if (all === undefined)
            all = false;

        let values = [];
        let checkboxes = widget.queryAll('.tsel_option input[type=checkbox]');
        for (const checkbox of checkboxes) {
            if (checkbox.checked || all)
                values.push(checkbox.dataset.value);
        }

        values = values.sort().filter(function(value, i) {
            return !i || value !== values[i - 1];
        });

        return values;
    };

    this.getWidget = function() {
        total = self.getValues(true).length;

        syncGroupCheckboxes();
        updateSummary();

        return widget;
    };

    widget = html('div', {class: 'tsel', click: function(e) { e.stopPropagation(); }},
        // This dummy button catches click events that happen when a label encloses the widget
        html('button', {style: 'display: none;', click: function(e) { e.preventDefault(); }}),

        html('div', {class: 'tsel_main'},
            html('div', {class: 'tsel_rect', click: function(e) { self.toggle(); }},
                 html('div', {class: 'tsel_curtab hide'}),
                 html('div', {class: 'tsel_summary'})
            ),
            html('div', {class: 'tsel_view'},
                html('div', {class: 'tsel_tabbar hide'}),
                html('div', {class: 'tsel_shortcuts'},
                    html('a', {href: '#', click: handleCheckAll}, 'Tout cocher'), ' / ',
                    html('a', {href: '#', click: handleUncheckAll}, 'Tout décocher')
                ),
                html('button', {class: 'tsel_validate', click: self.close}, 'Fermer'),
                html('div', {class: 'tsel_list active'})
            )
        ),
    );
    curtab = widget.query('.tsel_curtab');
    summary = widget.query('.tsel_summary');
    tabbar = widget.query('.tsel_tabbar');
    view = widget.query('.tsel_view');
    list = widget.query('.tsel_list');

    widget.object = this;
}

document.addEventListener('click', function(e) {
    for (let tsel of queryAll('.tsel.active'))
        tsel.object.close();
});
