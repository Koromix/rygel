// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function VersionLine() {
    let self = this;

    this.changeHandler = date => {};
    this.hrefBuilder = page => '#';

    let versions = [];
    let current_date;

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

    function handleNodeClick(e, version) {
        let vlin_el = util.findParent(e.target, el => el.classList.contains('vlin'));
        self.changeHandler.call(self, version);
        e.preventDefault();
    }

    this.render = function() {
        if (versions.length >= 2) {
            let min_date = versions[0].date;
            let max_diff = versions[versions.length - 1].date.diff(min_date);

            // Alternate versions labels above and below line
            let text_above = false;

            return svg`
                <svg class="vlin">
                    <line class="vlin_line" x1="2%" y1="23" x2="98%" y2="23"/>
                    <g>${versions.map(version => {
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
                            <a class=${cls} href=${self.hrefBuilder(version)} @click=${e => handleNodeClick(e, version)}>
                                <circle cx=${x} cy="23" r=${radius}>
                                    <title>${version.tooltip}</title>
                                </circle>
                                ${version.major ?
                                    svg`<text class="vlin_text" x=${x} y=${(text_above = !text_above) ? 13 : 43}
                                              text-anchor="middle">${version.label}</text>` : svg``}
                            </a>
                        `;
                    })}</g>
                </svg>
            `;
        } else {
            return svg``;
        }
    };
}
