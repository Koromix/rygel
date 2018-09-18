// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function TreeSelector(prefix)
{
    this.changeHandler = null;

    let self = this;
    let widget = null;
    let summary = null;
    let tabbar = null;
    let view = null;
    let list = null;

    let depth = 0;
    let total = 0;

    function syncValue(value, checked)
    {
        let checkboxes = widget.querySelectorAll('.tsel_option input[data-value="' + value + '"]');
        for (let i = 0; i < checkboxes.length; i++)
            checkboxes[i].checked = checked;
    }

    // Does not work correctly for deep hierarchies (more than 32 levels)
    function syncGroupCheckboxes()
    {
        let elements = widget.querySelectorAll('.tsel_list > label');

        let or_state = 0;
        let and_state = 0xFFFFFFFF;
        for (let i = elements.length - 1; i >= 0; i--) {
            let el = elements[i];
            let depth = parseInt(el.dataset.depth);
            let checkbox = el.querySelector('input[type=checkbox]');

            if (el.classList.contains('tsel_group')) {
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

            if (!widget.classList.contains('active') && self.changeHandler)
                setTimeout(function() { self.changeHandler.call(widget); }, 0);
        }

        summary.innerHTML = '';
        summary.appendChild(document.createTextNode(prefix));
        if (!values.length) {
            let span = createElement('a', {}, 'Aucune option sélectionnée');
            summary.appendChild(span);
        } else if (values.length < 8) {
            for (let i = 0; i < values.length; i++) {
                let span = createElement('a', {href: '#', click: handleSummaryOptionClick},
                                         values[i]);
                summary.appendChild(span);
            }
        } else {
            let span = createElement('a', {}, '' + values.length + ' / ' + total);
            summary.appendChild(span);
        }
    }

    function selectTab(tab)
    {
        removeClass(widget.querySelectorAll('.tsel_tab.active, .tsel_list.active'), 'active');
        tab.classList.add('active');
        tab.list.classList.add('active');

        syncGroupCheckboxes();
        updateSummary();
    }

    function handleTabClick(e)
    {
        selectTab(this);
        e.preventDefault();
    }

    function handleGroupClick(e)
    {
        let group = this.parentNode;
        let sibling = group.nextSibling;
        while (sibling && sibling.dataset.depth > group.dataset.depth) {
            if (sibling.classList.contains('tsel_option')) {
                let checkbox = sibling.querySelector('input[type=checkbox]');
                checkbox.checked = this.checked;
                checkbox.indeterminate = false;
                syncValue(this.dataset.value, this.checked);
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
        if (tabbar.classList.contains('hide')) {
            tabbar.classList.remove('hide');
        } else {
            list = createElement('div', {class: 'tsel_list'});
            view.appendChild(list);
        }

        let tab = createElement('a', {class: 'tsel_tab', href: '#', click: handleTabClick}, name);
        if (list.classList.contains('active'))
            tab.classList.add('active');
        tab.list = list;
        tabbar.appendChild(tab);
    };

    this.beginGroup = function(name) {
        let el = createElement('label', {class: 'tsel_group', style: 'padding-left: ' + depth + 'em;',
                                         'data-depth': depth},
            createElement('input', {type: 'checkbox', click: handleGroupClick}),
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
        let el = createElement('label', {class: 'tsel_option' + (options.disabled ? ' disabled' : ''),
                                         style: 'padding-left: ' + depth + 'em;', 'data-depth': depth},
            createElement('input', {type: 'checkbox', click: handleOptionClick,
                                    'data-value': value, checked: options.selected ? 'checked' : null}),
            name
        );

        list.appendChild(el);
    };

    this.open = function() { widget.classList.add('active'); };
    this.toggle = function(state) {
        if (!widget.classList.toggle('active', state) && self.changeHandler)
             setTimeout(function() { self.changeHandler.call(widget); }, 0);
    };
    this.close = function() {
        widget.classList.remove('active');
        if (self.changeHandler)
            setTimeout(function() { self.changeHandler.call(widget); }, 0);
    };

    this.getActiveTab = function() {
        for (let i = 0; i < tabbar.childNodes.length; i++) {
            if (tabbar.childNodes[i].classList.contains('active'))
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
        let checkboxes = widget.querySelectorAll('.tsel_option input[type=checkbox]');
        for  (let i = 0; i < checkboxes.length; i++) {
            let checkbox = checkboxes[i];
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

    widget = createElement('div', {class: 'tsel', click: function(e) { e.stopPropagation(); }},
        // This dummy button catches click events that happen when a label encloses the widget
        createElement('button', {style: 'display: none;', click: function(e) { e.preventDefault(); }}),

        createElement('div', {class: 'tsel_main'},
            createElement('div', {class: 'tsel_summary', click: function(e) { self.toggle(); }}),
            createElement('div', {class: 'tsel_view'},
                createElement('div', {class: 'tsel_tabbar hide'}),
                createElement('button', {class: 'tsel_validate', click: self.close}, 'Fermer'),
                createElement('div', {class: 'tsel_list active'})
            )
        ),
    );
    summary = widget.querySelector('.tsel_summary');
    tabbar = widget.querySelector('.tsel_tabbar');
    view = widget.querySelector('.tsel_view');
    list = widget.querySelector('.tsel_list');

    widget.object = this;
}

document.addEventListener('click', function(e) {
    let selectors = document.querySelectorAll('.tsel.active');
    for (let i = 0; i < selectors.length; i++)
        selectors[i].object.close();
});
