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

import { render, html } from '../../../node_modules/lit/html.js';
import { unsafeHTML } from '../../../node_modules/lit/directives/unsafe-html.js';
import { Util, Log, Net } from '../../../../web/libjs/common.js';
import { start, zoom, makeField, makeEdit, updateEntry, deleteEntry, renderMarkdown, isConnected } from '../map.js';

const ICONS = {
    hang: 'static/icons/hang.png'
};

function EchosProvider() {
    let etablissements;

    let icons = {};

    this.loadMap = async function() {
        let [data, images] = await Promise.all([
            Net.get('api/entries'),
            Promise.all(Object.values(ICONS).map(url => Net.loadImage(url, true)))
        ]);

        etablissements = [
            ...data.iml.rows.map(psy => ({ type: 'Suicide', ...psy }))
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
                        <input type="checkbox" data-filter="this.type == 'Suicide'"
                               checked/> Suicides
                        <img src="static/icons/hang.png" width="24" height="24" alt="" />
                    </label>
                </div>
            </div>
        `;
    };

    this.fillMap = function(filters) {
        let markers = [];
        let total = 0;

        for (let etab of etablissements) {
            if (etab.address.latitude != null) {
                total++;

                if (filters.every(filtre => filtre(etab))) {
                    let marker = {
                        latitude: etab.address.latitude,
                        longitude: etab.address.longitude,
                        cluster: '#e1572e',
                        priority: 1,
                        icon: getEtabIcon(etab),
                        size: 24,
                        clickable: true
                    };

                    marker.etab = etab;

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

    function getEtabIcon(etab) {
        return icons.hang;
    }

    this.renderPopup = function(etab, edit_key) {
        let cp = etab.address.address.substr(0, 5);
        let city = etab.address.address.substr(6).trim();

        let content = html`
            <div>
                Code postal : <b>${cp}</b><br/>
                Ville : <b>${city}</b>
                ${etab.lieu ? html`<br/><br/>Lieu : ${etab.lieu}` : ''}
            </div>
        `;

        return content;
    };

    function field(etab, key, view = null) {
        if (key == 'address' && etab[key] != null) {
            return makeField(etab, key, 'address');
        } else {
            return makeField(etab, key, fields[key], view);
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
