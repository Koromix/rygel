// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function TreeSelector(prefix)
{
    this.changeHandler = null;

    let self = this;
    let widget = null;
    let summary = null;
    let list = null;

    let depth = 0;

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

    function toggleSelection(value, select)
    {
        let checkbox = widget.querySelector('input[data-value="' + value + '"]');
        if (checkbox) {
            checkbox.checked = (select !== undefined) ? select : !checkbox.checked;

            syncGroupCheckboxes();
            updateSummary();
        }
    }

    function updateSummary()
    {
        let values = self.getValues();

        function handleSummaryOptionClick(e)
        {
            toggleSelection(this.textContent);
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
            let total = list.querySelectorAll('.tsel_option').length;
            let span = createElement('a', {}, '' + values.length + ' / ' + total);
            summary.appendChild(span);
        }
    }

    function handleGroupClick(e)
    {
        let group = this.parentNode;
        let sibling = group.nextSibling;
        while (sibling && sibling.dataset.depth > group.dataset.depth) {
            let checkbox = sibling.querySelector('input[type=checkbox]');
            checkbox.checked = this.checked;
            checkbox.indeterminate = false;
            sibling = sibling.nextSibling;
        }

        syncGroupCheckboxes();
        updateSummary();
    };

    function handleOptionClick(e)
    {
        syncGroupCheckboxes();
        updateSummary();
    }

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

    this.getValues = function() {
        let values = [];
        let checkboxes = widget.querySelectorAll('.tsel_option input[type=checkbox]:checked');
        for  (let i = 0; i < checkboxes.length; i++)
            values.push(checkboxes[i].dataset.value);

        return values;
    }

    this.getWidget = function() {
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
                createElement('div', {class: 'tsel_list'}),
                createElement('button', {class: 'tsel_validate', click: this.close}, 'Fermer')
            )
        ),
    );
    summary = widget.querySelector('.tsel_summary');
    list = widget.querySelector('.tsel_list');

    widget.object = this;
}

document.addEventListener('click', function(e) {
    let selectors = document.querySelectorAll('.tsel.active');
    for (let i = 0; i < selectors.length; i++)
        selectors[i].object.close();
});
