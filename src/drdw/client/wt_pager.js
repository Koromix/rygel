// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function Pager(active_page, last_page)
{
    'use strict';

    this.anchorBuilder = null;
    this.changeHandler = null;

    let self = this;
    let widget = null;
    let tr = null;

    function handlePageClick(e, page)
    {
        active_page = page;
        render();

        if (self.changeHandler)
            self.changeHandler.call(this, page);

        e.preventDefault();
    }
    this.handlePageClick = handlePageClick;

    function addPageLink(text, page)
    {
        let content = null;
        if (page) {
            if (self.anchorBuilder) {
                content = self.anchorBuilder.call(this, '' + text, page);
            } else {
                content = html('a', {href: '#', click: function(e) { handlePageClick(e, page); }},
                               '' + text);
            }
        } else {
            content = '' + text;
        }

        let td = html('td', content);
        tr.appendChild(td);

        return td;
    }

    function render()
    {
        tr.innerHTML = '';

        let start_page, end_page;
        if (last_page < 8) {
            start_page = 1;
            end_page = last_page;
        } else if (active_page < 5) {
            start_page = 1;
            end_page = 5;
        } else if (active_page > last_page - 4) {
            start_page = last_page - 4;
            end_page = last_page;
        } else {
            start_page = active_page - 1;
            end_page = active_page + 1;
        }

        addPageLink('≪', (active_page > 1) ? (active_page - 1) : null);
        if (start_page > 1) {
            addPageLink(1, 1);
            addPageLink(' … ');
        }
        for (let i = start_page; i <= end_page; i++) {
            let td = addPageLink(i, (i !== active_page) ? i : null);
            if (i === active_page)
                td.addClass('active');
        }
        if (end_page < last_page) {
            addPageLink(' … ');
            addPageLink(last_page, last_page);
        }
        addPageLink('≫', (active_page < last_page) ? (active_page + 1) : null);
    }

    this.getValue = function() {
        let active = widget.query('td.active');
        return parseInt(active.textContent, 10);
    }

    this.getWidget = function() {
        render();
        return widget;
    }

    widget = html('table', {class: 'pagr'},
        html('tr')
    );
    tr = widget.query('tr');

    widget.object = this;
}
