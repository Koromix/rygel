// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

let wt_version_line = (function() {
    function VersionLine() {
        let self = this;

        this.hrefBuilder = page => '#';
        this.changeHandler = null;

        let root_el;

        let versions = [];
        let current_date;

        this.getRootElement = function() { return root_el; }

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

        function handleNodeClick(e, date) {
            current_date = date;
            setTimeout(() => self.render(root_el), 0);

            if (self.changeHandler)
                setTimeout(() => self.changeHandler.call(self, e), 0);

            e.preventDefault();
        }

        this.render = function(new_root_el) {
            root_el = new_root_el;

            if (versions.length >= 2) {
                let min_date = versions[0].date;
                let max_diff = versions[versions.length - 1].date.diff(min_date);

                // Alternate versions labels above and below line
                let text_above = false;

                render(svg`
                    <svg class="vlin">
                        <line class="vlin_line" x1="2%" y1="20" x2="98%" y2="20"/>
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
                                <a class=${cls} href=${self.hrefBuilder(version)} @click=${e => handleNodeClick(e, version.date)}>
                                    <circle cx=${x} cy="20" r=${radius}>
                                        <title>${version.tooltip}</title>
                                    </circle>
                                    ${version.major ?
                                        svg`<text class="vlin_text" x=${x} y=${(text_above = !text_above) ? 10 : 40}
                                                  text-anchor="middle">${version.label}</text>` : svg``}
                                </a>
                            `;
                        })}</g>
                    </svg>
                `, root_el);
            } else {
                render(svg``, root_el);
            }
        };
    }

    this.create = function() { return new VersionLine(); };

    return this;
}).call({});
