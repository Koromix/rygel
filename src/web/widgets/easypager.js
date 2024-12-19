// Copyright (C) 2024  Niels Martignène <niels.martignene@protonmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

import { Util, Log } from '../core/base.js';
import { render, html } from '../../../vendor/lit-html/lit-html.bundle.js';

import './easypager.css';

function EasyPager() {
    let self = this;

    let url_builder = page => '#';
    let click_handler = (e, page) => {};

    let last_page;
    let current_page;

    let root_el;

    Object.defineProperties(this, {
        urlBuilder: { get: () => url_builder, set: builder => { url_builder = builder; }, enumerable: true },
        clickHandler: { get: () => click_handler, set: handler => { click_handler = handler; }, enumerable: true },

        lastPage: { get: () => last_page, set: page => { last_page = page; }, enumerable: true },
        currentPage: { get: () => current_page, set: page => { current_page = page; }, enumerable: true }
    });

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
                ${Util.mapRange(start_page, end_page + 1,
                                page => makePageLink(page, page !== current_page ? page : null))}
                ${end_page < last_page ?
                    html`<td> … </td>${makePageLink(last_page, last_page)}` : ''}
                ${makePageLink('≫', current_page < last_page ? (current_page + 1) : null)}
            </table>
        `;
    }

    function makePageLink(text, page) {
        if (page) {
            return html`<td><a href=${url_builder.call(self, page)} @click=${e => handlePageClick(e, page)}>${text}</a></td>`;
        } else {
            return html`<td>${text}</td>`;
        }
    }

    function handlePageClick(e, page) {
        current_page = page;

        if (!click_handler.call(self, e, page))
            render(renderWidget(), root_el);
    }
}

export { EasyPager }
