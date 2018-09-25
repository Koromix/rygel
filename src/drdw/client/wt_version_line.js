// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

function VersionLine()
{
    'use strict';

    this.changeHandler = null;

    let self = this;
    let widget = null;
    let g = null;

    let versions = [];
    let value = null;

    function handleNodeClick(e)
    {
        value = this.value;
        render();
        setTimeout(0, render);

        if (self.changeHandler)
            setTimeout(function() { self.changeHandler.call(widget); }, 0);
    }

    function render()
    {
        if (g)
            g.parentNode.removeChild(g);
        g = svg('g');
        widget.appendChild(g);

        if (versions.length < 2)
            return;

        let min_date = new Date(versions[0].date);
        let max_date = new Date(versions[versions.length - 1].date);

        let text_above = true;
        for (let i = 0; i < versions.length; i++) {
            let version = versions[i];

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

            let node = svg('circle', {class: 'vlin_node', cx: x, cy: 20, r: radius,
                                      fill: color, click: handleNodeClick},
                svg('title', version.label)
            );
            node.value = version.date;
            g.appendChild(node);

            if (version.major) {
                let text_y = text_above ? 10 : 40;
                text_above = !text_above;

                let text = svg('text', {class: 'vlin_text', x: x, y: text_y, 'text-anchor': 'middle',
                                        fill: color, style: 'font-weight: ' + weight + ';',
                                        click: handleNodeClick}, version.label);
                text.value = version.date;
                g.appendChild(text);
            }
        }
    }

    this.addVersion = function(date, label, major) {
        versions.push({
            date: date,
            label: label,
            major: major
        });
    };

    this.setValue = function(date) { value = date; };
    this.getValue = function() { return value; };

    this.getWidget = function() {
        render();
        return widget;
    };

    widget = svg('svg',
        svg('line', {class: 'vlin_line', x1: '2%', y1: 20, x2: '98%', y2: 20}),
    );

    widget.object = this;
}
