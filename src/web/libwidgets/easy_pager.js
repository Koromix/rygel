// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function EasyPager() {
    let self = this;

    this.urlBuilder = page => '#';
    this.clickHandler = (e, page) => {};

    let last_page;
    let current_page;

    let root_el;

    this.setLastPage = function(page) { last_page = page; };
    this.getLastPage = function() { return last_page; };

    this.setPage = function(page) { current_page = page; };
    this.getPage = function() { return current_page; };

    this.render = function() {
        if (!root_el) {
            root_el = document.createElement('div');
            root_el.className = 'epag';
        }

        // We don't return VDOM, because if we did the next render() we do after user interaction
        // would use a new binding, and replace the widget.
        render(renderWidget(), root_el);

        return root_el;
    };

    function renderWidget() {
        let start_page, end_page;
        if (last_page < 8) {
            start_page = 1;
            end_page = last_page;
        } else if (current_page < 5) {
            start_page = 1;
            end_page = 5;
        } else if (current_page > last_page - 4) {
            start_page = last_page - 4;
            end_page = last_page;
        } else {
            start_page = current_page - 1;
            end_page = current_page + 1;
        }

        return html`
            <table>
                ${makePageLink('≪', current_page > 1 ? (current_page - 1) : null)}
                ${start_page > 1 ?
                    html`${makePageLink(1, 1)}<td> … </td>` : ''}
                ${util.mapRange(start_page, end_page + 1,
                                page => makePageLink(page, page !== current_page ? page : null))}
                ${end_page < last_page ?
                    html`<td> … </td>${makePageLink(last_page, last_page)}` : ''}
                ${makePageLink('≫', current_page < last_page ? (current_page + 1) : null)}
            </table>
        `;
    }

    function makePageLink(text, page) {
        if (page) {
            return html`<td><a href=${self.urlBuilder.call(self, page)} @click=${e => handlePageClick(e, page)}>${text}</a></td>`;
        } else {
            return html`<td>${text}</td>`;
        }
    }

    function handlePageClick(e, page) {
        current_page = page;

        if (!self.clickHandler.call(self, e, page))
            render(renderWidget(), root_el);
    }
}
