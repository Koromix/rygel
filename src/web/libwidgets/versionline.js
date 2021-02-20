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

function VersionLine() {
    let self = this;

    this.urlBuilder = page => '#';
    this.clickHandler = (e, version) => {};

    let versions = [];
    let current_date;

    let root_el;

    this.addVersion = function(date, label, tooltip, major) {
        versions.push({
            date: date,
            label: label,
            tooltip: tooltip,
            major: major
        });
    };

    this.setDate = function(date) { current_date = date; };
    this.getDate = function() { return current_date; };

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
                    <a class=${cls} href=${self.urlBuilder.call(self, version)} @click=${e => handleNodeClick(e, version)}>
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
        if (!self.clickHandler.call(self, e, version))
            render(renderWidget(), root_el);
    }
}
