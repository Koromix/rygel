// Copyright (C) 2025  Niels Martignène <niels.martignene@protonmail.com>
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
import { render, svg } from '../../../vendor/lit-html/lit-html.bundle.js';

import './versionline.css';

function VersionLine() {
    let self = this;

    let url_builder = page => '#';
    let click_handler = (e, version) => {};
    let current_date = null;

    let versions = [];

    let root_el;

    Object.defineProperties(this, {
        urlBuilder: { get: () => url_builder, set: builder => { url_builder = builder; }, enumerable: true },
        clickHandler: { get: () => click_handler, set: handler => { click_handler = handler; }, enumerable: true },
        currentDate: { get: () => current_date, set: date => { current_date = date; }, enumerable: true }
    });

    this.addVersion = function(date, label, tooltip, major) {
        versions.push({
            date: date,
            label: label,
            tooltip: tooltip,
            major: major
        });
    };

    this.render = function() {
        if (versions.length < 2)
            throw new Error('Cannot render VersionLine with less than 2 data points');

        if (!root_el) {
            root_el = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
            root_el.setAttribute('class', 'vlin');
        }

        // We don't return VDOM, because if we did the next render() we do after user interaction
        // would use a new binding, and replace the widget.
        render(renderWidget(), root_el);

        return root_el;
    };

    function renderWidget() {
        let min_date = versions[0].date;
        let max_diff = versions[versions.length - 1].date.diff(min_date);

        // Alternate versions labels above and below line
        let text_above = false;

        return svg`
            <line class="vlin_line" x1="2%" y1="23" x2="98%" y2="23"/>
            ${versions.map(version => {
                let x = (6.0 + version.date.diff(min_date) / max_diff * 88.0).toFixed(1) + '%';
                let radius = 4 + !!version.major + version.date.equals(current_date);

                let cls = 'vlin_node';
                if (version.date.equals(current_date)) {
                    cls += ' vlin_node_current';
                } else if (version.major) {
                    cls += ' vlin_node_major';
                } else {
                    cls += ' vlin_node_normal';
                }

                return svg`
                    <a class=${cls} href=${url_builder.call(self, version)} @click=${e => handleNodeClick(e, version)}>
                        <circle cx=${x} cy="23" r=${radius}>
                            <title>${version.tooltip}</title>
                        </circle>
                        ${version.major ?
                            svg`<text class="vlin_text" x=${x} y=${(text_above = !text_above) ? 13 : 43}
                                      text-anchor="middle">${version.label}</text>` : svg``}
                    </a>
                `;
            })}
        `;
    }

    function handleNodeClick(e, version) {
        if (!click_handler.call(self, e, version))
            render(renderWidget(), root_el);
    }
}

export { VersionLine }
