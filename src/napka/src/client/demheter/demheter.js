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
    dual: 'static/icons/dual.png',
    demheter: 'static/icons/demheter.png',
    ect: 'static/icons/ect.png',
    psychologist: 'static/icons/psychologist.png'
};

function DemheterProvider() {
    let etablissements;

    let icons = {};

    this.loadMap = async function() {
        let [data, images] = await Promise.all([
            Net.get('api/entries'),
            Promise.all(Object.values(ICONS).map(url => Net.loadImage(url, true)))
        ]);

        etablissements = [
            ...data.psychologues.rows.map(psy => ({ type: 'Psychologue', ...psy })),
            ...data.centres.rows.map(centre => ({ type: 'Centre', ...centre }))
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
                        <input type="checkbox" data-filter="this.type === 'Centre' && this.demheter"
                               checked/> Centres DEMHETER
                        <img src="static/icons/demheter.png" width="32" height="32" alt="" />
                    </label>
                    <label>
                        <input type="checkbox" data-filter="this.type === 'Centre' && this.ect"
                               checked/> ECT
                        <img src="static/icons/ect.png" width="32" height="32" alt="" />
                    </label>
                    <label>
                        <input type="checkbox" data-filter="this.type === 'Psychologue'"
                               checked/> Psychologues
                        <img src="static/icons/psychologist.png" width="32" height="32" alt="" />
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
                        cluster: (etab.type == 'Psychologue') ? '#d352a3' : null,
                        icon: getEtabIcon(etab),
                        size: (etab.type == 'Centre') ? 48 : 40,
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
        if (etab.type == 'Centre') {
            if (etab.demheter && etab.ect) {
                return icons.dual;
            } else if (etab.demheter) {
                return icons.demheter;
            } else if (etab.ect) {
                return icons.ect;
            }
        } else if (etab.type === 'Psychologue') {
            return icons.psychologist;
        }

        return null;
    }

    this.renderPopup = function(etab, edit_key) {
        let content = html`
            <div>
                <i>${etab.type}</i>
                ${etab.type == 'Centre' && etab.demheter ? html`<span class="tag" style="background: #ff6600;">DEMHETER</span>` : ''}
                ${etab.type == 'Centre' && etab.ect ? html`<span class="tag" style="background: #18a059;">ECT</span>` : ''}
                <br/><br/>

                Adresse : <b>${field(etab, 'address')}</b><br/><br/>

                <u>Pour prendre un rendez-vous :</u><br/>
                ${etab.type == 'Psychologue' ? unsafeHTML(renderMarkdown(etab.orientation.trim())) : ''}
                ${etab.type == 'Centre' && etab.mail ?
                    html`Adressez un courriel électronique à l'adresse <a href=${'mailto:' + etab.mail}>${etab.mail}</a><br/>` : ''}
                ${etab.type == 'Centre' && !etab.mail ? html`Prenez rendez-vous par téléphone<br/>` : ''}
                <br/>
                ${etab.mail ? html`Courriel : <a href=${'mailto:' + etab.mail}>${etab.mail}</a><br/>` : ''}
                ${etab.telephone ? html`Téléphone : <a href=${'tel:+33' + etab.telephone.substr(1)}>${etab.telephone}</a><br/>` : ''}
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
    let provider = new DemheterProvider;

    let latitude = 50.44;
    let longitude = 2.5;
    let zoom = (window.innerWidth <= 580) ? 7 : 8;

    await start(provider, {
        latitude: latitude,
        longitude: longitude,
        zoom: zoom
    });
}

export { zoom }
