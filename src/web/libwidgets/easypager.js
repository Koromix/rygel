// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program. If not, see https://www.gnu.org/licenses/.

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
