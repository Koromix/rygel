// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function MultiSelectorBuilder(prefix)
{
    let root = null;
    root = createElement('div', {class: 'msel', click: function(e) { e.stopPropagation(); }},
        // This dummy button catches click events that happen when a label encloses the widget
        createElement('button', {style: 'display: none;', click: function(e) { e.preventDefault(); }}),
        createElement('div', {class: 'msel_main'},
            createElement('div', {class: 'msel_summary',
                                  click: function(e) { root.classList.toggle('active'); }}),
            createElement('div', {class: 'msel_view'},
                createElement('div', {class: 'msel_list'}),
                createElement('button', {class: 'msel_validate',
                                         click: function(e) { root.classList.remove('active'); }},
                              'Fermer')
            )
        ),
    );
    let summary = root.querySelector('.msel_summary');
    let list = root.querySelector('.msel_list');
    let depth = 0;

    // Does not work correctly for deep hierarchies (more than 32 levels)
    function syncGroupCheckboxes()
    {
        let elements = root.querySelectorAll('.msel_list > label');

        let or_state = 0;
        let and_state = 0xFFFFFFFF;
        for (let i = elements.length - 1; i >= 0; i--) {
            let el = elements[i];
            let depth = parseInt(el.dataset.depth);
            let checkbox = el.querySelector('input[type=checkbox]');

            if (el.classList.contains('msel_group')) {
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
        let checkbox = root.querySelector('input[data-value="' + value + '"]');
        if (checkbox) {
            checkbox.checked = (select !== undefined) ? select : !checkbox.checked;

            syncGroupCheckboxes();
            updateSummary();
        }
    }

    function updateSummary()
    {
        let values = multiSelectorValues(root);

        function handleSummaryOptionClick(e)
        {
            toggleSelection(this.textContent);
            e.preventDefault();
            e.stopPropagation();
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
            let total = list.querySelectorAll('.msel_option').length;
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
        let el = createElement('label', {class: 'msel_group', style: 'padding-left: ' + depth + 'em;',
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
        let el = createElement('label', {class: 'msel_option' + (options.disabled ? ' disabled' : ''),
                                         style: 'padding-left: ' + depth + 'em;', 'data-depth': depth},
            createElement('input', {type: 'checkbox', click: handleOptionClick,
                                    'data-value': value, checked: options.selected ? 'checked' : null}),
            name
        );

        list.appendChild(el);
    };

    this.getWidget = function() {
        syncGroupCheckboxes();
        updateSummary();

        return root;
    }
}

document.addEventListener('click', function(e) {
    let multiselectors = document.querySelectorAll('.msel');
    removeClass(multiselectors, 'active');
});

function multiSelectorValues(el)
{
    let values = [];
    let checkboxes = el.querySelectorAll('.msel_option input[type=checkbox]:checked');
    for  (let i = 0; i < checkboxes.length; i++)
        values.push(checkboxes[i].dataset.value);

    return values;
}
