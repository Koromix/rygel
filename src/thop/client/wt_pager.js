// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function Pager(widget, active_page, last_page)
{
    this.anchorBuilder = null;
    this.changeHandler = null;

    let self = this;

    function handlePageClick(e, page)
    {
        active_page = page;
        setTimeout(self.render, 0);

        if (self.changeHandler)
            self.changeHandler.call(this, page);

        e.preventDefault();
    }
    this.handlePageClick = handlePageClick;

    function addPageLink(tr, text, page)
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

    this.render = function() {
        widget.innerHTML = '';
        widget.addClass('pagr');
        widget.appendChild(html('tr'));
        let tr = widget.firstChild;

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

        addPageLink(tr, '≪', (active_page > 1) ? (active_page - 1) : null);
        if (start_page > 1) {
            addPageLink(tr, 1, 1);
            addPageLink(tr, ' … ');
        }
        for (let i = start_page; i <= end_page; i++) {
            let td = addPageLink(tr, i, (i !== active_page) ? i : null);
            if (i === active_page)
                td.addClass('active');
        }
        if (end_page < last_page) {
            addPageLink(tr, ' … ');
            addPageLink(tr, last_page, last_page);
        }
        addPageLink(tr, '≫', (active_page < last_page) ? (active_page + 1) : null);
    }

    this.getValue = function() {
        let active = widget.query('td.active');
        return parseInt(active.textContent, 10);
    }
    this.getWidget = function() { return widget; }

    widget.object = this;
}

function computeLastPage(render_count, row_count, page_length)
{
    let last_page = Math.floor((row_count - 1) / page_length + 1);
    if (last_page === 1 && render_count === row_count)
        last_page = null;

    return last_page;
}
