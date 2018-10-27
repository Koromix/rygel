// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function VersionLine(widget)
{
    this.anchorBuilder = null;
    this.changeHandler = null;

    let self = this;

    let versions = [];
    let value = null;

    function handleNodeClick(e)
    {
        value = this.value;
        setTimeout(self.render, 0);

        if (self.changeHandler)
            setTimeout(function() { self.changeHandler.call(widget); }, 0);

        e.preventDefault();
    }

    this.render = function() {
        while (widget.lastChild)
            widget.removeChild(widget.lastChild);
        widget.addClass('vlin');
        widget.appendChildren([
            svg('line', {class: 'vlin_line', x1: '2%', y1: 20, x2: '98%', y2: 20}),
            svg('g')
        ]);
        let g = widget.query('g');

        if (versions.length < 2)
            return;

        let min_date = new Date(versions[0].date);
        let max_date = new Date(versions[versions.length - 1].date);

        let text_above = true;
        for (const version of versions) {
            let x = (6.0 + (new Date(version.date) - min_date) / (max_date - min_date) * 88.0).toFixed(1) + '%';
            let radius = 4 + !!version.major + (version.date === value);

            let color;
            let weight;
            if (version.date === value) {
                color = '#ed6d0a';
                weight = 'bold';
            } else if (version.major) {
                color = '#000';
                weight = 'normal';
            } else {
                color = '#888';
                weight = 'normal';
            }

            let href;
            if (self.anchorBuilder) {
                href = self.anchorBuilder(version);
            } else {
                href = '#';
            }

            let anchor = svg('a', {click: handleNodeClick});
            anchor.setAttributeNS('http://www.w3.org/1999/xlink', 'href', href);
            anchor.value = version.date;

            let node = svg('circle', {class: 'vlin_node', cx: x, cy: 20, r: radius, fill: color},
                svg('title', version.tooltip)
            );
            anchor.appendChild(node);

            if (version.major) {
                let text_y = text_above ? 10 : 40;
                text_above = !text_above;

                let text = svg('text', {class: 'vlin_text', x: x, y: text_y, 'text-anchor': 'middle',
                                        fill: color, style: 'font-weight: ' + weight + ';'},
                               version.label);
                anchor.appendChild(text);
            }

            g.appendChild(anchor);
        }
    }

    this.addVersion = function(date, label, tooltip, major) {
        versions.push({
            date: date,
            label: label,
            tooltip: tooltip,
            major: major
        });
    };

    this.setValue = function(date) { value = date; };
    this.getValue = function() { return value; };
    this.getWidget = function() { return widget; }

    widget.object = this;
}
