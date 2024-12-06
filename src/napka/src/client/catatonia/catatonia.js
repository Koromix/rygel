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
import { Util, Log, Net } from '../../../../web/core/common.js';
import { start, zoom, makeField, makeEdit, updateEntry, deleteEntry, renderMarkdown, isConnected } from '../map.js';

const ICONS = {
    dual: 'static/icons/dual.png',
    center: 'static/icons/center.png',
    ect: 'static/icons/ect.png'
};

function CatatoniaProvider() {
    let entries;

    let icons = {};

    this.loadMap = async function() {
        let [data, images] = await Promise.all([
            Net.get('api/entries'),
            Promise.all(Object.values(ICONS).map(url => Net.loadImage(url)))
        ]);

        entries = [
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
                        <input type="checkbox" data-filter="this.type === 'Centre' && !this.ect" checked /> Sans ECT
                        <img src="static/icons/center.png" width="32" height="32" alt="" />
                    </label>
                    <label>
                        <input type="checkbox" data-filter="this.type === 'Centre' && this.ect" checked /> Avec ECT
                        <img src="static/icons/dual.png" width="32" height="32" alt="" />
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
                        cluster: null,
                        priority: 1 + entry.ect,
                        icon: getEntryIcon(entry),
                        size: 48,
                        clickable: true
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

    function getEntryIcon(entry) {
        return entry.ect ? icons.dual : icons.center;
    }

    this.renderEntry = function(entry, edit_key) {
        let content = html`
            <div>
                ${entry.type == 'Centre' && entry.ect ? html`Centre <span class="tag" style="background: #18a059;">ECT</span><br/><br/>` : ''}

                Adresse : <b>${field(entry, 'address')}</b><br/><br/>

                <u>Pour prendre un rendez-vous :</u><br/>
                ${entry.type == 'Centre' && entry.mail ?
                    html`Adressez un courriel électronique à l'adresse <a href=${'mailto:' + entry.mail}>${entry.mail}</a><br/>` : ''}
                ${entry.type == 'Centre' && !entry.mail ? html`Prenez rendez-vous par téléphone<br/>` : ''}
                <br/>
                ${entry.mail ? html`Courriel : <a href=${'mailto:' + entry.mail}>${entry.mail}</a><br/>` : ''}
                ${entry.telephone ? html`Téléphone : <a href=${'tel:+33' + entry.telephone.substr(1)}>${entry.telephone}</a><br/>` : ''}
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
    let provider = new CatatoniaProvider;

    let latitude = (window.innerWidth > 1100) ? 47 :
                   (window.innerWidth > 580) ? 47.65 : 48.4;
    let longitude = 1.9;
    let zoom = (window.innerWidth <= 580) ? 4 : 5;

    await start(provider, {
        latitude: latitude,
        longitude: longitude,
        zoom: zoom
    });
}

export { zoom }
