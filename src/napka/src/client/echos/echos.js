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

import { render, html } from '../../../node_modules/lit/html.js';
import { unsafeHTML } from '../../../node_modules/lit/directives/unsafe-html.js';
import { Util, Log, Net } from '../../../../web/core/base.js';
import { start, zoom, makeField, makeEdit, updateEntry, deleteEntry,
         renderMarkdown, isConnected, loadTexture } from '../map.js';

const ICONS = {
    iml1: 'static/icons/iml1.png',
    iml2: 'static/icons/iml2.png',
    sncf: 'static/icons/sncf.png'
};

function EchosProvider() {
    let entries;

    let icons = {};

    this.loadMap = async function() {
        let [data, images] = await Promise.all([
            Net.get('api/entries'),
            Promise.all(Object.values(ICONS).map(loadTexture))
        ]);

        entries = [
            ...data.iml.rows.map(row => ({ type: 'IML', ...row })),
            ...data.sncf.rows.map(row => ({ type: 'SNCF', ...row }))
        ];

        // We've preloaded images
        {
            let keys = Object.keys(ICONS);

            for (let i = 0; i < keys.length; i++)
                icons[keys[i]] = images[i];
        }
    };

    this.renderFilters = function() {
        return html`
            <div id="filters">
                <div id="count"></div>

                <div class="group">
                    <label>
                        <input type="checkbox" data-filter="this.type == 'IML' && this.hotspot"
                               checked/> Hotspots IML
                        <img src="static/icons/iml2.png" width="24" height="24" alt="" />
                    </label>
                    <label>
                        <input type="checkbox" data-filter="this.type == 'IML' && !this.hotspot"
                               checked/> Autres IML
                        <img src="static/icons/iml1.png" width="24" height="24" alt="" />
                    </label>
                    <label>
                        <input type="checkbox" data-filter="this.type == 'SNCF'"
                               checked/> SNCF
                        <img src="static/icons/sncf.png" width="24" height="24" alt="" />
                    </label>
                </div>
            </div>
        `;
    };

    this.fillMap = function(filters) {
        let markers = [];
        let total = 0;

        for (let entry of entries) {
            if (entry.address.address.startsWith('05'))
                continue;

            if (entry.address.latitude != null) {
                total++;

                if (filters.every(filtre => filtre(entry))) {
                    let marker = null;

                    if (entry.type == 'IML' && entry.hotspot) {
                        marker = {
                            latitude: entry.address.latitude,
                            longitude: entry.address.longitude,
                            cluster: `IML:${entry.lieu}`,
                            text: '1',
                            tooltip: entry.lieu,
                            priority: 2,
                            icon: icons.iml1,
                            size: 24,
                            clickable: true
                        };
                    } else if (entry.type == 'IML') {
                        marker = {
                            latitude: entry.address.latitude,
                            longitude: entry.address.longitude,
                            tooltip: entry.lieu,
                            priority: 1,
                            icon: icons.iml1,
                            size: 16,
                            clickable: true
                        };
                    } else if (entry.type == 'SNCF') {
                        marker = {
                            latitude: entry.address.latitude,
                            longitude: entry.address.longitude,
                            cluster: `SNCF:${entry.address.latitude},${entry.address.longitude}`,
                            tooltip: entry.lieu,
                            priority: 1,
                            icon: icons.sncf,
                            size: 16,
                            clickable: true
                        };
                    }

                    marker.entry = entry;

                    markers.push(marker);
                }
            }
        }

        if (markers.length < total) {
            let text = `${markers.length} / ${total}`;
            render(text, document.querySelector('#count'));
        } else if (markers.length) {
            let text = `${markers.length} entrée${markers.length > 1 ? 's' : ''}`;
            render(text, document.querySelector('#count'));
        } else {
            let text = 'Aucun résultat';
            render(text, document.querySelector('#count'));
        }

        return markers;
    };

    this.styleCluster = function(element) {
        if (element.cluster.startsWith('IML:')) {
            element.icon = icons.iml2;
            element.size = 30;
            element.priority = 3;
        } else if (element.cluster.startsWith('SNCF:')) {
            element.circle = '#878787';
            element.size = 24;
            element.priority = 3;
        }

        // We don't need to show the list
        element.markers.length = 1;
    };

    this.renderEntry = function(entry, edit_key) {
        let address = entry.address.address;
        let city = (entry.type == 'IML') ? `${address.substr(6).trim()} (${address.substr(0, 5)})` : address;

        let content = html`
            <div>
                ${entry.lieu ? html`Lieu : <b>${entry.lieu}</b><br/>` : ''}
                Ville : <b>${city}</b>
                ${entry.hotspot ? html`<br/><br/><span style="color: #db0a0a;">⚠\uFE0E Hotspot</span>` : ''}
            </div>
        `;

        return content;
    };

    function field(entry, key, view = null) {
        if (key == 'address' && entry[key] != null) {
            return makeField(entry, key, 'address');
        } else {
            return makeField(entry, key, fields[key], view);
        }
    }
}

export async function main() {
    let provider = new EchosProvider;

    let latitude = 45.4;
    let longitude = 4.8;
    let zoom = (window.innerWidth <= 580) ? 6 : 7;

    await start(provider, {
        latitude: latitude,
        longitude: longitude,
        zoom: zoom
    });
}

export { zoom }
