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

import { render, html } from '../../../node_modules/lit/html.js';
import { unsafeHTML } from '../../../node_modules/lit/directives/unsafe-html.js';
import { Util, Log, Net } from '../../../../web/core/base.js';
import { start, zoom, makeField, makeEdit, updateEntry, deleteEntry,
         renderMarkdown, isConnected } from '../map.js';

function ProvenanceProvider() {
    let entries;

    this.loadMap = async function() {
        let data = await Net.get('api/entries');

        entries = [
            ...data.demheter.rows.map(row => ({ type: 'demheter', ...row }))
        ];

        console.log(entries);
    };

    this.renderFilters = function() {
        return html`
            <div id="filters">
                <div id="count"></div>

                <div class="group">
                    <label>
                        <input type="checkbox" data-filter="this.profession == 'Psychiatre'"
                               checked/> Psychiatres
                    </label>
                    <label>
                        <input type="checkbox" data-filter="this.profession == 'Psychiatre libéral'"
                               checked/> Psychiatres libéraux
                    </label>
                    <label>
                        <input type="checkbox" data-filter="this.profession == 'Médecin généraliste'"
                               checked/> Médecins généralistes
                    </label>
                    <label>
                        <input type="checkbox" data-filter="this.profession == 'Autre'"
                               checked/> Autres
                    </label>
                </div>
            </div>
        `;
    };

    this.fillMap = function(filters) {
        let markers = [];
        let total = 0;

        for (let entry of entries) {
            if (entry.address.latitude != null) {
                total++;

                if (filters.every(filtre => filtre(entry))) {
                    let marker = {
                        latitude: entry.address.latitude,
                        longitude: entry.address.longitude,
                        cluster: getSectorColor(entry.name),
                        tooltip: entry.name,
                        priority: 1,
                        circle: getSectorColor(entry.name),
                        size: 16
                    };

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

    function getSectorColor(sector) {
        let prefix = sector.substr(0, 2);

        switch (prefix) {
            case '59': return '#ee8c53';
            case '62': return '#e6b34a';
            case '02': return '#55beb5';
            case '60': return '#8fa1cb';
            case '80': return '#7cc367';
        }
    }

    this.styleCluster = function(element) {
        element.priority = element.markers.length;
        element.size = 16 + 14 * Math.log(element.markers.length);
        element.clickable = false;
        element.text = '';
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
    let provider = new ProvenanceProvider;

    let latitude = 50.44;
    let longitude = 2.5;
    let zoom = 7;

    await start(provider, {
        latitude: latitude,
        longitude: longitude,
        zoom: zoom
    });
}

export { zoom }
